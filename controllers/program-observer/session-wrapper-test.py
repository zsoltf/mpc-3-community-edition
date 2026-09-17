#!/usr/bin/env python3
"""Container-only production session-owner test; native MPC, protocol, route and
ALSA children are substitutes. Real shell/flock/process identity/wait/archive
paths run. A pre-owner-lock pause and smaller history constants are the only
injections. Run as root in a container with an init and a tmpfs /run:

  docker run --rm --init --network none --tmpfs /run \\
    -v "$PWD/controllers/program-observer:/src:ro" mpclearn-build:local \\
    python3 /src/session-wrapper-test.py

With --busybox ROOTFS (an ext4 root image mounted read-only in the container),
the session scripts run under that image's BusyBox shell and applets through
qemu-arm, as on the device.
"""
import hashlib, json, os, pathlib, shutil, signal, subprocess, sys, time
P = pathlib.Path
assert P('/.dockerenv').exists() and os.getuid() == 0
home = P('/data/mpclearn'); dev = home/'dev'; session = home/'session'; history = home/'history'
image = P('/usr/share/mpclearn/mcu')
runtime = P('/run/mpclearn'); state = runtime/'state'
settings = P('/media/az01-internal/Settings/MPC/MPC.settings')
settings.parent.mkdir(parents=True); settings.write_text('original settings\n')
assert subprocess.run(['stat', '-f', '-c', '%T', '/run'], capture_output=True, text=True).stdout.strip() == 'tmpfs', 'run with --tmpfs /run'
NAMES = 'command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button'.split()
EXECUTABLES = 'command-client mirror-input mirror-read mcu-session.sh mcu mpclearn-controls main-button'.split()

shell = '/bin/sh'
env = dict(os.environ)
if '--busybox' in sys.argv:
    rootfs = sys.argv[sys.argv.index('--busybox')+1]
    device = P('/tmp/device-root'); device.mkdir()
    for part in ('/usr', '/lib'):
        subprocess.run(['debugfs', '-R', f'rdump {part} {device}', rootfs], check=True, capture_output=True)
    busybox = device/'usr/bin/busybox.nosuid'
    bb = P('/tmp/bb'); bb.mkdir()
    for applet in 'sh awk basename cat chmod chown cmp cp cut dirname env find flock grep head id kill ls mkdir mkfifo mv printf readlink rm sed sha256sum sleep sort stat tail test touch tr wc'.split():
        (bb/applet).symlink_to(busybox)
    shell = str(bb/'sh')
    env['QEMU_LD_PREFIX'] = str(device)
    env['PATH'] = f'{bb}:' + env['PATH']
    assert 'BusyBox' in subprocess.run([shell, '-c', 'stat --help 2>&1; true'], env=env, capture_output=True, text=True).stdout
MODE = 'busybox' if shell != '/bin/sh' else 'host'
# Summary heads are read with dd in whole 4096-byte blocks.
for line in P('/src/mcu-session.sh').read_text().splitlines():
    if line.startswith('cap_') and '_head=' in line: assert int(line.split('=')[1]) % 4096 == 0, line

def script_text(path, replacements=()):
    text = P(path).read_text()
    for a, b in replacements:
        assert a in text, a
        text = text.replace(a, b)
    return text.replace('#!/bin/sh\n', f'#!{shell}\n', 1)

PAUSE = ('owner(){\n exec 9>session.lock', 'owner(){\n while [ -f pause-owner ];do sleep .05;done\n exec 9>session.lock')
def session_source(**constants):
    replacements = [PAUSE]
    for name, value in constants.items():
        line = next(l for l in P('/src/mcu-session.sh').read_text().splitlines() if l.startswith(name+'='))
        replacements.append((line+'\n', f'{name}={value}\n'))
    return script_text('/src/mcu-session.sh', replacements)

P('/tmp/session-stub.c').write_text(r'''#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "/src/surface-preferences.h"
static volatile sig_atomic_t stopped;
static void stop(int sig){(void)sig;stopped=1;}
static void arm(int sig){(void)sig;int fd=open("armed",O_WRONLY|O_CREAT|O_APPEND,0600);if(fd>=0){write(fd,"1",1);close(fd);}}
int main(int argc,char **argv){int bridge=strstr(argv[0],"mirror-input")!=0;
if(bridge&&argc==3&&!strcmp(argv[1],"--surface-status")){unsigned enabled;if(!surface_preferences_read(argv[2],&enabled))return 1;puts(enabled?"on":"off");return 0;}
if(strstr(argv[0],"mpclearn-controls")){signal(SIGUSR1,arm);signal(SIGTERM,stop);puts("DISARMED fixture");fflush(stdout);while(!stopped)pause();return 0;}
if(bridge){signal(SIGTERM,stop);printf("BRIDGE_READY pid=%u\n",(unsigned)getpid());for(int i=1;i<argc;i++)printf("ARG %s\n",argv[i]);fflush(stdout);while(!stopped){FILE*f=fopen("mpc.pid","r");int p=0;if(f){fscanf(f,"%d",&p);fclose(f);}if(p>0&&kill(p,0))break;usleep(10000);}printf("TOUCH_FINAL pid=%u mask=3\n",(unsigned)getpid());return stopped?0:1;}
prctl(PR_SET_NAME,"MPC Main Thread",0,0,0);signal(SIGTERM,stop);
FILE*f=fopen("launches","a");fputs("1",f);fclose(f);unlink("project-ready");unlink("native-intent");unlink("closed");
f=fopen("/run/mpclearn/state/command.state","w");fprintf(f,"%u\n",(unsigned)getpid());fclose(f);f=fopen("/run/mpclearn/state/volume.state","w");fclose(f);
for(int i=0;i<1100000;i++)putchar('x');puts(" MPC diagnostic tail");fflush(stdout);
while(!stopped){f=fopen("exit-request","r");if(f){int code=0;fscanf(f,"%d",&code);fclose(f);unlink("exit-request");return code;}usleep(10000);}return 0;}
''')
subprocess.run(['gcc', '-D_GNU_SOURCE', '/tmp/session-stub.c', '-o', '/usr/bin/MPC'], check=True)
stub_bin = P('/tmp/session-bin'); stub_bin.mkdir()
def script(p, text, mode=0o700):
    p.write_text('#!/bin/sh\n'+text); p.chmod(mode)
script(stub_bin/'setarch', 'shift;shift;exec "$@"\n')
script(stub_bin/'systemctl', f'''case "$*" in *mpclearn-boot*) case "$1" in is-active) echo inactive;exit 3;; *) exit 0;; esac;; esac
case "$1" in stop) rm -f {session}/service-active;; start) touch {session}/service-active;; is-active) test -f {session}/service-active;; *) exit 2;; esac\n''')
env['PATH'] = f'{stub_bin}:' + env['PATH']
COMMAND_CLIENT = f'''cd {session}
case "$1" in
 session-status) test "$2" = "{state}/command.state" -a "$3" = "{state}/volume.state" || exit 9;test -f "$2" -a -f "$3" || exit 5;test ! -f closed || exit 4;test ! -f native-intent || exit 6;test -f project-ready || exit 3;exit 0;;
 status) test "$2" = "{state}/command.state" || exit 9
  awk -v n="$(cat status-events 2>/dev/null || echo 2)" 'BEGIN{{printf "{{\\"format\\":\\"CMD31\\",\\"error\\":0,\\"requests\\":[{{\\"slot\\":0,\\"rejected\\":0}}],\\"events\\":[";for(i=0;i<n;i++)printf "%s{{\\"slot\\":0,\\"lane\\":0,\\"index\\":%d,\\"kind\\":1,\\"lr\\":1234567}}",(i?",":""),i;print "],\\"snapshot_atomic\\":false,\\"new_project_intent\\":0,\\"navigation\\":{{\\"watch\\":0}},\\"admission\\":{{\\"category\\":\\"none\\",\\"raw_result\\":0,\\"site\\":77}}}}"}}';;
 new-project-intent) test -f native-intent || exit 1;test "$2" = "{state}/command.state" || exit 9;test "$(cat "$2")" = "$3" || exit 1;test "$(cat native-intent)" = "$3" || exit 1;if test "${{5:-}}" = consume;then test ! -e /proc/$3/exe || exit 1;mv native-intent consumed-intent;fi;;
 stop) test "$2" = "{state}/command.state" -a "$3" = "{state}/volume.state" || exit 9;touch closed;;
 *) exit 2;; esac
'''
MIRROR_READ = f'''test "$1" = "{state}/volume.state" -a "$2" = --all || exit 9
echo '{{"format":"MMV17","stable":true,"fresh":false,"tracks":[]}}';exit 1
'''
def sha(path): return hashlib.sha256(P(path).read_bytes()).hexdigest()
def write_package(folder, location, source):
    folder.mkdir(parents=True, exist_ok=True)
    (folder/'mcu-session.sh').write_text(source)
    (folder/'mcu').write_text(script_text('/src/mcu.sh'))
    (folder/'config.h').write_text(f'#define OBSERVER_LOG "{state}/volume.state"\n#define COMMAND_PATH "{state}/command.state"\n#define OBSERVER_LIBRARY "{location}/command-observer.so"\n#define WINDOW_SECONDS 0u\n')
    script(folder/'command-client', COMMAND_CLIENT)
    script(folder/'mirror-read', MIRROR_READ)
    script(folder/'main-button', 'test ! -f route-fail\n')
    for name in ['mirror-input', 'mpclearn-controls']:
        (folder/name).write_bytes(P('/usr/bin/MPC').read_bytes())
    (folder/'command-observer.so').write_bytes(b'')
    manifest(folder)
def manifest(folder):
    (folder/'session-package.sha256').write_text(''.join(f'{sha(folder/n)}  {n}\n' for n in NAMES))
def set_modes(folder):
    os.chown(folder, 0, 0); folder.chmod(0o700)
    for n in NAMES+['session-package.sha256']:
        os.chown(folder/n, 0, 0); (folder/n).chmod(0o700 if n in EXECUTABLES else 0o600)

def until(pred, why, seconds=15):
    end = time.monotonic()+seconds
    while time.monotonic() < end:
        try:
            if pred(): return
        except FileNotFoundError:  # a locator between archive and relaunch
            pass
        time.sleep(.05)
    log = session/'owner.log'
    raise AssertionError(why+'\n'+(log.read_text() if log.exists() else '(no owner.log)'))
def call(path, *args, expected=0):
    p = subprocess.run([str(path), *args], env=env, capture_output=True, text=True, timeout=60)
    assert p.returncode == expected, (str(path), args, p.returncode, p.stdout, p.stderr)
    return p
def run(op, expected=0, where=dev):
    return call(where/('mcu-session.sh' if op.startswith('bridge-') else 'mcu'), op, expected=expected)
def count(name): return len((session/name).read_text()) if (session/name).exists() else 0
def app_pid(): return int((session/'mpc.pid').read_text().split()[0])
def owner_gone():
    try: p = int((session/'owner.pid').read_text().split()[0]); return not P(f'/proc/{p}/exe').exists()
    except FileNotFoundError: return True
def depart(intent, code=0):
    if intent: (session/'native-intent').write_text(str(app_pid()))
    (session/'exit-request').write_text(str(code))
def entries(): return sorted(p.name for p in history.iterdir())
def du(path): return int(subprocess.run(['du', '-sb', str(path)], capture_output=True, text=True, check=True).stdout.split()[0])
def no_raw_state():
    found = [str(p) for p in home.rglob('*.state')]
    assert not found, found
def cleanup():
    for exe in P('/proc').glob('[0-9]*/exe'):
        try:
            if os.readlink(exe) == '/usr/bin/MPC' or os.readlink(exe).endswith(('/mirror-input', '/mpclearn-controls')): os.kill(int(exe.parent.name), signal.SIGKILL)
        except (FileNotFoundError, ProcessLookupError): pass

def outside():
    out = {}
    for base in [P('/data'), P('/etc')]:
        for f in sorted(base.rglob('*')):
            if base.name == 'etc' and not f.name.startswith('mpclearn'): continue
            if str(f) == str(home) or str(f).startswith(str(home)+'/') or f.is_dir() and not f.is_symlink(): continue
            out[str(f)] = (f.lstat().st_mode, f.read_bytes() if f.is_file() and not f.is_symlink() else os.readlink(f) if f.is_symlink() else None)
    return out
# Leftovers of earlier releases on the development device: inert under this design.
for name in ['mpclearn-model.v0_2_0', 'mpclearn-model.v0_2_2', 'mpclearn-model.mouse-r5']:
    old = P('/data')/name; (old/'history/1').mkdir(parents=True)
    for f in NAMES+['session-package.sha256', 'session.id', 'mpc.pid']: (old/f).write_text(f'old {f}\n')
    (old/'surface-preferences').write_text('MCU-SURFACE1 motors=0\n')
    (old/'history/1/volume.state').write_bytes(b'v'*4096)
for f in ['mpclearn-boot/mcu-boot.sh', 'mpclearn-boot/mcu-session.sh', 'mpclearn-image/installed', 'mpclearn-image/previous-stage']:
    (P('/data')/f).parent.mkdir(exist_ok=True); (P('/data')/f).write_text('old\n')
for f in ['mpclearn-boot-stage', 'mpclearn-boot-stage.next', 'mpclearn-boot-stage.tmp']:
    (P('/etc')/f).write_text('/data/mpclearn-model.v0_2_0\n')
before_outside = outside()

try:
    # Our folder is adopted only as root 0700; nothing is written into anything else.
    home.mkdir(parents=True); home.chmod(0o755)
    write_package(dev, dev, session_source()); set_modes(dev)
    p = run('status', 2)
    assert 'not a root-owned 0700 folder' in p.stderr and not session.exists() and not history.exists() and not runtime.exists()
    home.chmod(0o700); shutil.rmtree(dev)
    # The image package, and a development package built on another machine
    # (non-root owner, permissive modes) that only the installer may place.
    write_package(image, image, session_source())
    for n in ['mcu-boot.sh', 'mcu-boot-install.sh']: (image/n).write_text(script_text('/src/'+n))
    set_modes(image)
    for n in ['mcu-boot.sh', 'mcu-boot-install.sh']: (image/n).chmod(0o700)
    (image/'payload.sha256').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(image.iterdir())))
    source = P('/tmp/dev-package'); write_package(source, dev, session_source())
    for f in [source, *source.iterdir()]: os.chown(f, 1000, 1000); f.chmod(0o755 if f.is_dir() else 0o644)
    wrong = P('/tmp/wrong-package'); write_package(wrong, image, session_source())
    p = call(image/'mcu-boot-install.sh', 'override', str(wrong), expected=1); assert 'not built for /data/mpclearn/dev' in p.stderr
    shutil.copytree(source, '/tmp/tampered'); (P('/tmp/tampered')/'mirror-input').write_bytes(b'tampered')
    call(image/'mcu-boot-install.sh', 'override', '/tmp/tampered', expected=1)
    assert not dev.exists()
    call(image/'mcu-boot-install.sh', 'override', str(source))
    assert all(os.stat(dev/n).st_uid == 0 and (dev/n).stat().st_mode & 0o777 == (0o700 if n in EXECUTABLES else 0o600) for n in NAMES+['session-package.sha256'])
    assert dev.stat().st_mode & 0o777 == 0o700 and (dev/'for-image').read_text() == sha(image/'payload.sha256')+'\n'
    # Boot admission ignores an override for another image, says so, and removes nothing.
    good = (dev/'for-image').read_text()
    for bad in ('0'*64+'\n', None):
        if bad is None: (dev/'for-image').unlink()
        else: (dev/'for-image').write_text(bad)
        p = call(image/'mcu-boot.sh', 'stop', expected=1)
        assert 'ignored: installed for a different image; using the image runtime' in p.stderr and 'No owned session' in p.stderr, p.stderr
        assert (dev/'mcu').exists() and not (session/'launches').exists()
    (dev/'for-image').write_text(good)
    p = call(image/'mcu-boot.sh', 'stop', expected=1); assert 'ignored' not in p.stderr
    # Only the image and the override location may run.
    stray = P('/data/mpclearn-model.wrapper-test'); shutil.copytree(dev, stray); stray.chmod(0o700)
    p = call(stray/'mcu', 'status', expected=2); assert 'Unsupported package location' in p.stderr
    assert sorted(x.name for x in stray.iterdir()) == sorted(x.name for x in dev.iterdir()); shutil.rmtree(stray)
    # Status before any start creates only the tmpfs folder; the preference path is fixed.
    p = run('status', 5)
    assert 'motor_follow=on' in p.stdout and runtime.stat().st_mode & 0o777 == 0o700 and not state.exists()
    print(f'PASS [{MODE}] fixed locations: 0700 folder adoption, override install with shipped owner/modes, mismatched override ignored and kept, stray location refusal, status on fresh tmpfs')
    # Runtime admission refuses symlinks, unknown entries and unowned state.
    state.symlink_to('/tmp', target_is_directory=True); run('start', 1); state.unlink()
    state.mkdir(mode=0o700); (state/'unexpected').touch(); run('start', 1); (state/'unexpected').unlink()
    (state/'command.state').symlink_to('/tmp/missing-state'); run('start', 1); (state/'command.state').unlink()
    (state/'command.state').write_text('unowned'); os.chown(state/'command.state', 1000, 1000); run('start', 1); (state/'command.state').unlink()
    assert not (session/'launches').exists()
    # Stop wins even before the owner has acquired its launch admission.
    (session/'pause-owner').touch(); started = subprocess.Popen([str(dev/'mcu'), 'start'], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    until(lambda: (session/'owner.pid').exists(), 'owner launch locator'); run('stop', 1); (session/'pause-owner').unlink(); started.communicate(timeout=10)
    until(owner_gone, 'revoked unlaunched owner exits'); assert count('launches') == 0 and (session/'service-active').exists()
    # The canceled initial generation has no app locator; a new explicit start
    # must retain its session diagnostics before replacing them. Boot admission
    # selects the verified override.
    call(image/'mcu-boot.sh', 'start'); until(lambda: (session/'generation.ready').exists(), 'chooser controller ready'); assert count('launches') == 1 and count('armed') == 0
    assert any('-unlaunched-' in e for e in entries()), entries()
    bridge_log = (session/'bridge.log').read_text()
    assert '--motors=on' in bridge_log and '--preferences=/data/mpclearn/surface-preferences' in bridge_log
    assert (session/'session.location').read_text() == f'{dev}\n'
    p = run('status', 2, where=image); assert 'belongs to /data/mpclearn/dev' in p.stderr
    (home/'surface-preferences').write_text('MCU-SURFACE1 motors=0\n'); (home/'surface-preferences').chmod(0o600)
    original = (runtime/'settings-before').read_bytes()
    # Reproduce the native miss: bridge deliberately absent when chooser becomes
    # ready, then explicit bridge-start must repair arming under the same owner.
    run('bridge-stop'); (session/'project-ready').touch()
    arming_failure = 'Route/source unavailable at arming; no automatic retry.\n'
    until(lambda: (session/'generation.failed').exists(), 'missing bridge arming failure')
    assert (session/'generation.failed').read_text() == arming_failure and count('armed') == 0 and (session/'manual-adapter-unarmed.pid').exists()
    (session/'route-fail').touch(); run('bridge-start', 1)
    assert count('armed') == 0 and (session/'manual-adapter-unarmed.pid').exists() and (session/'generation.failed').read_text() == arming_failure
    run('bridge-stop'); (session/'route-fail').unlink(); run('bridge-start'); until(lambda: count('armed') == 1, 'explicit bridge-start restores missed arming')
    assert not (session/'generation.failed').exists() and not (session/'manual-adapter-unarmed.pid').exists()
    state_inodes = [(state/n).stat().st_ino for n in ('command.state', 'volume.state')]
    run('bridge-stop'); run('bridge-start'); assert count('armed') == 1
    assert state_inodes == [(state/n).stat().st_ino for n in ('command.state', 'volume.state')]
    run('start'); assert count('launches') == 1
    print(f'PASS [{MODE}] explicit bridge-start: actual missed chooser arming recovery, route-failure refusal, exact failure cleanup and no duplicate signal (native adapter/protocol substituted)')
    for expected in (2, 3):
        previous = app_pid(); before = set(entries()); depart(True)
        until(lambda: count('launches') == expected and (session/'generation.ready').exists() and app_pid() != previous, 'accepted New Project restarts once')
        assert count('armed') == expected-1 and (runtime/'settings-before').read_bytes() == original
        bridge_log = (session/'bridge.log').read_text()
        assert '--held-mask=3' in bridge_log and '--motors=off' in bridge_log
        entry = history/(set(entries())-before).pop()
        assert entry.name.split('-', 2)[2] == f'{previous}-' + entry.name.split('-')[-1]
        assert {'mpc.pid', 'mpc.exit', 'mpc.log', 'bridge.log', 'command-status.json', 'session-status.txt', 'mirror-read.json'} <= {p.name for p in entry.iterdir()}
        json.loads((entry/'command-status.json').read_text()); json.loads((entry/'mirror-read.json').read_text())
        assert not (entry/'command-status.json.truncated').exists()
        assert (entry/'mpc.log').stat().st_size == 1048576 and (entry/'mpc.log').read_bytes().endswith(b' MPC diagnostic tail\n')
        if expected == 2:
            run('bridge-stop'); run('bridge-start'); assert count('armed') == expected-1 # chooser stays disarmed
            run('bridge-stop'); (session/'project-ready').touch(); until(lambda: (session/'generation.failed').exists(), 'second missed arming')
            (session/'generation.failed').write_text('Different retained failure.\n'); run('bridge-start')
            until(lambda: count('armed') == expected, 'explicit recovery with other failure')
            assert (session/'generation.failed').read_text() == 'Different retained failure.\n'
        else:
            (session/'project-ready').touch(); until(lambda: count('armed') == expected, 'new saved template arms')
    no_raw_state()
    print(f'PASS [{MODE}] actual owner: repeatable accepted exit -> distinct chooser bridge/disarmed adapter -> saved template arm; text-only generation archive and original settings retained (native protocol and MIDI substituted)')
    # Positive marker cannot authorize a crash restart; generic zero exit neither.
    for positive, code in ((True, 17), (False, 0)):
        before = count('launches'); depart(positive, code); until(owner_gone, 'non-NewProject owner ends'); assert count('launches') == before
        run('stop', 1); assert settings.read_text() == 'original settings\n'
        # Incomplete-stop evidence is text in the session folder, never raw state.
        assert (session/'stop-incomplete-status.txt').exists() and json.loads((session/'incomplete-command-status.json').read_text())['admission']['site'] == 77
        assert (session/'incomplete-mirror-read.json').exists() and (session/'incomplete-bridge.log').read_text() == (session/'bridge.log').read_text()
        assert not list(session.glob('incomplete-before*'))
        names = set(entries()); run('start'); new = history/(set(entries())-names).pop()
        assert {'incomplete-command-status.json', 'stop-incomplete-status.txt', 'incomplete-mirror-read.json', 'incomplete-bridge.log', 'session.revoked', 'session.location'} <= {p.name for p in new.iterdir()}
        assert not (session/'incomplete-command-status.json').exists()
        assert '--motors=off' in (session/'bridge.log').read_text(); (session/'project-ready').touch(); until(lambda: count('armed') == before+1, 'fresh explicit session arms')
    # Durable stop during a pending handoff leaves no later launch/arming.
    # The owner may or may not have relaunched before stop takes the lock (a slower
    # shell changes the order); either way nothing launches after the stop.
    depart(True); p = subprocess.run([str(dev/'mcu'), 'stop'], env=env, capture_output=True, text=True, timeout=60); assert p.returncode in (0, 1), p
    until(owner_gone, 'stop winner owner exits'); before = count('launches'); time.sleep(.7); assert count('launches') == before
    assert (session/'session.revoked').exists() and (session/'service-active').exists()
    assert settings.read_text() == 'original settings\n'
    # A stopped session no longer binds its location.
    p = subprocess.run([str(image/'mcu'), 'status'], env=env, capture_output=True, text=True, timeout=60)
    assert 'motor_follow=off' in p.stdout and 'belongs to' not in p.stderr, (p.stdout, p.stderr)
    print(f'PASS [{MODE}] actual owner: crash and generic exit do not restart; explicit stop revokes before launch and during handoff; exact child waits, settings recovery and text incomplete-stop evidence (native protocol substituted)')
    # Simulate power-off: tmpfs vanishes entirely; the session folder on /data stays
    # and the next boot's start archives it, including the bridge's last lines.
    with (session/'bridge.log').open('a') as log: log.write('INPUT STOP error=10 reason=source_failure pending_request=1\n')
    last_bridge = (session/'bridge.log').read_text(); last_id = (session/'session.id').read_text()
    shutil.rmtree(runtime)
    (session/'session.boot-id').write_text('previous-boot\n'); names = set(entries()); run('start')
    rebooted = history/sorted(set(entries())-names)[0]
    assert (rebooted/'bridge.log').read_text() == last_bridge and last_bridge.endswith('pending_request=1\n') and (rebooted/'session.id').read_text() == last_id
    assert (rebooted/'session.boot-id').read_text() == 'previous-boot\n' and (rebooted/'mpc.exit').exists() and not (rebooted/'command-status.json').exists()
    assert (state/'command.state').exists() and not (session/'command.state').exists()
    (session/'project-ready').touch(); until(lambda: not (session/'manual-adapter-unarmed.pid').exists(), 'reboot session arming'); run('stop')
    no_raw_state()
    assert len(entries()) <= 8 and du(history) <= 16777216
    print(f'PASS [{MODE}] real tmpfs admission, unchanged mappings on bridge restart, power-off evidence archived from /data at the next boot, no raw state on /data (boot id and native app substituted)')

    # History budget with injected small constants.
    budget = 400000
    def small(**extra):
        values = dict(history_budget=budget, history_entries=4, cap_mpc_log=65536, cap_status_head=8192, cap_status_tail=2048)
        values.update(extra)
        (dev/'mcu-session.sh').write_text(session_source(**values)); manifest(dev)
    def within():
        assert du(history) <= budget, (du(history), entries())
        assert not (history/'.new').exists()
    small()
    for e in entries(): shutil.rmtree(history/e)
    for name in ('000001-aaaaaaaa-old', '000002-aaaaaaaa-old'):
        (history/name).mkdir(mode=0o700); (history/name/'bridge.log').write_bytes(b'o'*170000)
    (history/'.new').mkdir(); (history/'.new'/'partial').write_bytes(b'p'*500000)
    (session/'status-events').write_text('3000')
    run('start')
    within()
    assert '000001-aaaaaaaa-old' not in entries() and '000002-aaaaaaaa-old' in entries(), entries()
    newest = history/entries()[-1]; assert newest.name.startswith('000003-')
    cut = (newest/'command-status.json').read_text()
    assert cut.startswith('{"format":"CMD31","error":0,"requests":[{"slot":0,"rejected":0}],"events":[') and '[mpclearn history: ' in cut
    assert cut.endswith('"admission":{"category":"none","raw_result":0,"site":77}}\n') and 'original_bytes=' in (newest/'command-status.json.truncated').read_text()
    assert len(cut) < 8192+2048+200 and (newest/'mpc.log').stat().st_size == 65536
    print(f'PASS [{MODE}] history budget: crash leftover removed, oldest pruned before the write, du <= budget, head-and-tail status summary keeps the closing admission object')
    # Zero-padded 000008/000009 are invalid octal: the sequence must stay decimal.
    for n in (8, 9, 10, 11, 12, 13, 14):
        (history/f'{n:06d}-aaaaaaaa-tiny').mkdir(mode=0o700)
    (session/'project-ready').touch(); until(lambda: count('armed') >= 1 and not (session/'manual-adapter-unarmed.pid').exists(), 'budget session arms')
    for _ in range(3):
        before = count('launches'); previous = app_pid(); depart(True)
        until(lambda: count('launches') == before+1 and app_pid() != previous and (session/'generation.ready').exists(), 'handoff within budget')
        within(); assert len(entries()) <= 4, entries()
        (session/'project-ready').touch()
    seq = [int(e.split('-')[0]) for e in entries()]; assert seq == sorted(seq) and seq[-1] == 17, entries()
    # A write that fails refuses the successor launch; nothing is retried.
    shutil.rmtree(history); history.write_text('not a folder')
    before = count('launches'); depart(True); until(owner_gone, 'owner refuses successor after failed archive')
    assert count('launches') == before and (session/'mpc.pid').exists()
    history.unlink(); history.mkdir(mode=0o700)
    run('stop', 1)
    # An entry that cannot fit alone is dropped without pruning anything.
    (history/'000001-aaaaaaaa-keep').mkdir(mode=0o700)
    (history/'.new').mkdir(); (history/'.new'/'partial').write_bytes(b'p'*100)  # removed even when nothing is pruned
    small(history_budget=40000)
    p = run('start'); assert 'exceeds the budget; not retained' in p.stderr, p.stderr
    assert entries() == ['000001-aaaaaaaa-keep'] and du(history) <= 40000
    (session/'project-ready').touch(); until(lambda: not (session/'manual-adapter-unarmed.pid').exists(), 'oversize session arms')
    run('stop')
    no_raw_state()
    print(f'PASS [{MODE}] history budget: entry count bound over handoffs, failed write refuses the successor, oversized entry dropped without pruning')
    # Override removal goes through our scratch folder only.
    small(); call(image/'mcu-boot-install.sh', 'override', 'clear')
    assert not dev.exists() and sorted(p.name for p in home.iterdir()) == ['history', 'session', 'surface-preferences'], list(home.iterdir())
    # Nothing outside /data/mpclearn and /run/mpclearn changed, although earlier
    # release folders and the old selector were present throughout.
    assert outside() == before_outside
    print(f'PASS [{MODE}] explicit override clear; earlier release folders, old boot/image folders and old selector untouched and unused')
finally:
    cleanup()
