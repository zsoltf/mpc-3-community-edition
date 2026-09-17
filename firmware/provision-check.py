#!/usr/bin/env python3
"""Exercise the image's provisioner and in-place runtime with the image's own ARM userland.

Run inside mpclearn-build:local with a tmpfs mounted where the extracted root's
/run will be:

  docker run --rm --network none --tmpfs /provision-root/run -v "$PWD:/work" \\
    mpclearn-build:local python3 firmware/provision-check.py ROOTFS \\
    [--recipe RECIPE_DIR] [--downgrade OLDER_ROOTFS] [--provisioner FILE]

ROOTFS is a built root filesystem, or with --recipe the official root that the
browser recipe in RECIPE_DIR patches (the same span application the builder
performs). --downgrade runs an older release's own provisioner in between.
--provisioner substitutes a provisioner file, for mutation testing only.
Leftovers of earlier releases are present throughout; nothing outside
/data/mpclearn and /run/mpclearn may change and nothing may refer to them.
/data and /etc are ordinary directories and /proc is not mounted (a boot id file
stands in); systemd ordering, reboot, a real power cut and flashing are not
exercised.
"""
import base64, hashlib, json, os, pathlib, shutil, stat, subprocess, sys, tempfile
P = pathlib.Path
args = sys.argv[1:]
def option(name):
    if name not in args: return None
    i = args.index(name); value = args[i+1]; del args[i:i+2]; return value
recipe, older, substitute = option('--recipe'), option('--downgrade'), option('--provisioner')
image = P(args[0]).resolve()
root = P('/provision-root')
assert P('/.dockerenv').exists() and os.getuid() == 0
assert subprocess.run(['stat', '-f', '-c', '%T', str(root/'run')], capture_output=True, text=True).stdout.strip() == 'tmpfs', \
    'mount a tmpfs at /provision-root/run (docker --tmpfs)'
assert [p.name for p in root.iterdir()] == ['run'], 'the extracted root must start empty'
def run(cmd, **kw):
    return subprocess.run(cmd, check=True, text=True, capture_output=True, timeout=600, **kw)
scratch = P(tempfile.mkdtemp(prefix='mpclearn-provision-'))

if recipe:
    # The browser builder's own procedure: validate, write every span verbatim,
    # compare the whole-root digest.
    manifest = json.loads((P(recipe)/'manifest.json').read_text()); patch = (P(recipe)/'patch.bin').read_bytes()
    assert manifest['version'] == 1 and hashlib.sha256(patch).hexdigest() == manifest['patch_sha256']
    data = bytearray(image.read_bytes())
    assert len(data) == manifest['original_size'] and hashlib.sha256(data).hexdigest() == manifest['original_sha256']
    end = used = 0
    for span in manifest['spans']:
        assert span['length'] > 0 and span['offset'] >= end and span['patch_offset'] == used
        data[span['offset']:span['offset']+span['length']] = patch[used:used+span['length']]
        end = span['offset']+span['length']; used += span['length']
    assert used == len(patch) and hashlib.sha256(data).hexdigest() == manifest['patched_sha256']
    image = scratch/'recipe-root.ext'; image.write_bytes(data)
    print(f"PASS browser recipe ({'ssh' if manifest['ssh_enabled'] else 'no-ssh'}) applied span by span; root digest matches")

run(['debugfs', '-R', f'rdump / {root}', str(image)])
shutil.copy('/usr/bin/qemu-arm', root/'qemu-arm')
payload = root/'usr/share/mpclearn/mcu'
provisioner = root/'usr/libexec/mpclearn/provision.sh'

# Shipped modes, read from the filesystem image itself and from the extracted tree.
EXECUTABLE = 'command-client mirror-input mirror-read mcu-session.sh mcu mpclearn-controls main-button mcu-boot.sh mcu-boot-install.sh'.split()
PRIVATE = 'command-observer.so config.h session-package.sha256'.split()
PUBLIC = 'LICENSE BUILD.json payload.sha256'.split()
def inode_mode(path):
    out = run(['debugfs', '-R', f'stat {path}', str(image)]).stdout
    line = next(l for l in out.splitlines() if 'Mode:' in l)
    return int(line.split('Mode:')[1].split()[0], 8), int(out.split('User:')[1].split()[0])
expected = {n: 0o700 for n in EXECUTABLE} | {n: 0o600 for n in PRIVATE} | {n: 0o644 for n in PUBLIC}
assert sorted(p.name for p in payload.iterdir()) == sorted(expected)
for name, mode in expected.items():
    assert inode_mode('/usr/share/mpclearn/mcu/'+name) == (mode, 0), name
    assert stat.S_IMODE((payload/name).stat().st_mode) == mode, name
assert inode_mode('/usr/share/mpclearn/mcu') == (0o700, 0) and inode_mode('/usr/libexec/mpclearn/provision.sh') == (0o755, 0)
units = root/'usr/lib/systemd/system'
boot_unit = (units/'mpclearn-boot.service').read_text(); provision_unit = (units/'mpclearn-provision.service').read_text()
assert 'ConditionPathExists=!/data/mpclearn/disabled\n' in boot_unit and '/etc/mpclearn-boot-stage' not in boot_unit
assert 'ExecStart=/usr/share/mpclearn/mcu/mcu-boot.sh start\n' in boot_unit and 'ExecStopPost=/usr/share/mpclearn/mcu/mcu-boot.sh recover\n' in boot_unit
assert 'Requires=mpclearn-provision.service\nAfter=mpclearn-provision.service\n' in boot_unit and '/data/mpclearn-boot' not in boot_unit
assert 'TimeoutStartSec=30\n' in provision_unit and 'ExecStart=/usr/libexec/mpclearn/provision.sh\n' in provision_unit
for name in ['mpclearn-boot.service', 'mpclearn-provision.service']:
    assert os.readlink(units/'multi-user.target.wants'/name) == '../'+name
config = (payload/'config.h').read_text().splitlines()
for line in ['#define OBSERVER_LIBRARY "/usr/share/mpclearn/mcu/command-observer.so"', '#define OBSERVER_LOG "/run/mpclearn/state/volume.state"', '#define COMMAND_PATH "/run/mpclearn/state/command.state"', '#define WINDOW_SECONDS 0u']:
    assert line in config, line
print('PASS image payload modes (executables 0700, private files 0600, folder 0700, provisioner 0755), units and image-location config')
if substitute:
    shutil.copy(substitute, provisioner); provisioner.chmod(0o755)
    print('NOTE provisioner substituted for mutation testing:', substitute)

data, etc = root/'data', root/'etc'
RELEASES = ['mcu-direct-r2', 'mcu-perf-r3', 'mcu-perf-r4', 'mouse-r5', 'v0_2_0', 'v0_2_2']
REVISION = next(l.split('=', 1)[1] for l in provisioner.read_text().splitlines() if l.startswith('revision='))
PACKAGE = 'command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button session-package.sha256'.split()

# Nothing the image runs refers to an earlier release's files.
shipped = [provisioner, units/'mpclearn-boot.service', units/'mpclearn-provision.service'] + [payload/n for n in EXECUTABLE + ['config.h']]
for f in shipped:
    text = f.read_bytes()
    for old in [b'mpclearn-model', b'mpclearn-boot-stage', b'/data/mpclearn-boot', b'/data/mpclearn-image', b'removing']:
        assert old not in text, (f.name, old)
print('PASS no shipped script, unit or config refers to earlier release stages, boot/image folders or the old selector')

def write(path, content=b'', mode=0o600, uid=0):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content if isinstance(content, bytes) else content.encode()); os.chown(path, uid, uid); path.chmod(mode)
def folder(path, mode=0o700, uid=0):
    path.mkdir(parents=True, exist_ok=True); os.chown(path, uid, uid); path.chmod(mode); return path
def reset():
    """A device /data with the MPC's own data, user files and every earlier release's leftovers."""
    for p in list(data.iterdir()):
        shutil.rmtree(p) if p.is_dir() and not p.is_symlink() else p.unlink()
    for p in etc.iterdir():
        if p.name.startswith('mpclearn'): p.unlink()
    folder(data, 0o755)
    write(data/'Settings/MPC/MPC.settings', 'untouched-user-settings\n', 0o644); (data/'Settings').chmod(0o755)
    write(data/'system/etc/overlay/passwd', 'stock overlay\n', 0o644)
    write(data/'softwareunlock/unlock.bin', b'\x01\x02', 0o644); write(data/'sentry/report', 'x', 0o644)
    write(data/'ssh/mpclearn/ssh_host_ed25519_key', 'host key\n'); (data/'ssh/mpclearn').chmod(0o700)
    write(data/'az01-update.img', b'user image', 0o755); write(data/'user-project-sentinel', 'untouched-project\n', 0o644)
    for name in RELEASES:
        stage = folder(data/('mpclearn-model.'+name))
        for n in PACKAGE: write(stage/n, n)
        write(stage/'surface-preferences', b'MCU-SURFACE1 motors=0\n')
        write(stage/'history/1-1/volume.state', b'v'*65536, 0o644); write(stage/'session.id', '1-1\n', 0o644)
    for n in ['mcu-boot.sh', 'mcu-boot-install.sh', 'mcu-session.sh', 'mpclearn-boot.service']: write(folder(data/'mpclearn-boot')/n, n, 0o700)
    write(folder(data/'mpclearn-image')/'installed', '81f3086-v0_2_0\n'); write(data/'mpclearn-image/previous-stage', 'x'); write(data/'mpclearn-image/install.lock')
    write(folder(data/'mpclearn-v020-backup')/'session-package.sha256', 'marker\n')
    for n in ['mpclearn-boot-stage', 'mpclearn-boot-stage.next', 'mpclearn-boot-stage.tmp']: write(etc/n, '/data/mpclearn-model.v0_2_0\n')
    (root/'run/mpclearn').exists() and shutil.rmtree(root/'run/mpclearn')
def tree(base, skip=()):
    out = {}
    if not (base.exists() or base.is_symlink()): return out
    items = [base] + (sorted(base.rglob('*')) if base.is_dir() and not base.is_symlink() else [])
    for p in items:
        rel = str(p.relative_to(root))
        if any(rel == s or rel.startswith(s+'/') for s in skip): continue
        st = p.lstat()
        value = os.readlink(p) if stat.S_ISLNK(st.st_mode) else hashlib.sha256(p.read_bytes()).hexdigest() if stat.S_ISREG(st.st_mode) else None
        out[rel] = (stat.S_IFMT(st.st_mode), stat.S_IMODE(st.st_mode), st.st_uid, value)
    return out
def device(): return tree(data) | tree(etc)
def outside(): return tree(data, skip=['data/mpclearn']) | tree(etc)
def ours(): return tree(data/'mpclearn')
def provision(expected=0, path='/usr/libexec/mpclearn/provision.sh'):
    before = outside()
    r = subprocess.run(['chroot', str(root), path], capture_output=True, text=True, timeout=600)
    assert r.returncode == expected, (path, r.returncode, r.stdout, r.stderr)
    assert outside() == before, 'something outside /data/mpclearn changed'
    return r
def converged(extra=()):
    home = data/'mpclearn'
    assert sorted(p.name for p in home.iterdir()) == sorted({'image', 'session', 'history', *extra}), sorted(p.name for p in home.iterdir())
    assert all(stat.S_IMODE(p.stat().st_mode) == 0o700 and p.stat().st_uid == 0 for p in [home, home/'session', home/'history'])
    assert (home/'image').read_text() == REVISION+'\n'

# Earlier releases present: provisioning and the in-place runtime leave them alone.
reset(); snapshot = outside()
r = provision(); converged()
assert r.stdout.strip() == f'MCU {REVISION} ready. Settings and projects preserved.' and not r.stderr, r
write(root/'proc/sys/kernel/random/boot_id', 'provision-check-boot\n', 0o444)
r = subprocess.run(['chroot', str(root), '/usr/share/mpclearn/mcu/mcu', 'status'], capture_output=True, text=True, timeout=600)
assert 'motor_follow=on' in r.stdout, (r.returncode, r.stdout, r.stderr)
assert stat.S_IMODE((root/'run/mpclearn').stat().st_mode) == 0o700 and (data/'mpclearn/session/session.lock').exists()
assert outside() == snapshot, 'the runtime touched something outside its folders'
print('PASS runtime runs in place from the image (mcu status verified the package and executed mirror-input: motor_follow=on); the old motor setting was not read')
print('PASS with all six earlier stages, the old boot and image folders and the old selector (.next, .tmp) present: only /data/mpclearn and /run/mpclearn were written, nothing deleted')
state = ours(); provision(); assert ours() == state
print('PASS rerun changes nothing')

# Our folder is used only as root 0700.
for make in ('user-folder', 'symlink', 'file', 'non-root', 'bad-history'):
    reset(); home = data/'mpclearn'
    if make == 'user-folder': write(folder(home, 0o755)/'my-notes.txt', 'notes', 0o644)
    elif make == 'symlink': folder(data/'elsewhere'); home.symlink_to('elsewhere')
    elif make == 'file': write(home, 'a file', 0o700)
    elif make == 'non-root': folder(home, 0o700, uid=1000)
    else: folder(home); folder(home/'history', 0o755)  # session does not exist yet: refusal must come first
    snapshot = device(); r = provision(1)
    assert device() == snapshot and 'mpclearn:' in r.stderr, (make, r.stderr)
reset(); home = folder(data/'mpclearn'); write(home/'surface-preferences', 'MCU-SURFACE1 motors=1\n'); write(folder(folder(home/'history')/'000001-abc-1-2')/'bridge.log', 'kept')
provision(); converged(['surface-preferences']); assert (home/'history/000001-abc-1-2/bridge.log').read_text() == 'kept'
print('PASS our folder: a 0755 user folder, a symlink, a file, a non-root folder and a malformed subfolder are refused with nothing written; a root 0700 folder is used with its contents kept')

# A development override is never removed; a mismatched one is reported.
digest = hashlib.sha256((payload/'payload.sha256').read_bytes()).hexdigest()
for variant in ('match', 'stale', 'missing', 'symlink', 'mode'):
    reset(); home = folder(data/'mpclearn'); dev = home/'dev'
    if variant == 'symlink': folder(data/'devtarget'); dev.symlink_to('../devtarget')
    else:
        folder(dev, 0o755 if variant == 'mode' else 0o700)
        if variant != 'missing': write(dev/'for-image', ('0'*64 if variant == 'stale' else digest)+'\n')
    before = tree(dev); r = provision()
    assert tree(dev) == before and ('ignored' in r.stderr) == (variant in ('stale', 'missing', 'symlink')), (variant, r.stderr)
print('PASS development override kept in every state; a mismatched one is reported as ignored')

# Power loss after each provisioner step: the next boot converges.
reset(); provision(); reference = device()
lines = provisioner.read_text().splitlines(); points = 0
for k, line in enumerate(lines):
    text = line.strip()
    if not text or text.startswith('#') or k < 5 or text.endswith(('\\', 'then', 'do', 'else', '||', '&&', '|', ';;')):
        continue
    variant = '\n'.join(lines[:k+1] + ['exit 99'] + lines[k+1:]) + '\n'
    if subprocess.run(['sh', '-n'], input=variant, text=True).returncode: continue
    write(root/'crash.sh', variant, 0o755)
    reset(); r = subprocess.run(['chroot', str(root), '/crash.sh'], capture_output=True, text=True, timeout=600)
    assert r.returncode in (0, 99), (k, line, r.returncode, r.stderr)
    provision(); provision(); assert device() == reference, (k, line)
    points += 1
(root/'crash.sh').unlink()
assert points >= 12, points
print(f'PASS power loss after each of {points} provisioner steps: the next boot converges to the uninterrupted result')

# An older image flashed afterwards installs itself beside our folder, and this one again leaves it alone.
reset(); provision()
if older:
    old = P('/provision-old'); old.mkdir()
    run(['debugfs', '-R', f'rdump / {old}', older]); shutil.copy('/usr/bin/qemu-arm', old/'qemu-arm')
    for n in ['data'] + [p.name for p in etc.iterdir() if p.name.startswith('mpclearn')]:
        target = old/('data' if n == 'data' else 'etc/'+n)
        shutil.rmtree(target) if target.is_dir() else target.unlink(missing_ok=True)
    shutil.copytree(data, old/'data', symlinks=True)
    shutil.rmtree(old/'data/mpclearn-model.v0_2_2')  # the fixture's dummy v0.2.2 stage would conflict with the real one
    for p in etc.iterdir():
        if p.name.startswith('mpclearn'): shutil.copy2(p, old/'etc'/p.name)
    r = subprocess.run(['chroot', str(old), '/usr/libexec/mpclearn/provision.sh'], capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, (r.stdout, r.stderr)
    assert (old/'data/mpclearn-model.v0_2_2/session-package.sha256').exists()
    def listing(base): return {str(p.relative_to(base)): (p.lstat().st_mode, p.read_bytes() if p.is_file() else None) for p in base.rglob('*')}
    assert listing(old/'data/mpclearn') == listing(data/'mpclearn'), 'the older provisioner changed /data/mpclearn'
    shutil.rmtree(data); shutil.copytree(old/'data', data, symlinks=True)
    for p in (old/'etc').iterdir():
        if p.name.startswith('mpclearn'): shutil.copy2(p, etc/p.name)
    state = ours(); provision(); assert ours() == state
    print('PASS the real older provisioner runs beside /data/mpclearn; this provisioner then changes nothing outside it and nothing inside it')

shutil.rmtree(scratch)
print('Boundary: extracted root; /data, /etc and /proc are plain directories; systemd ordering, reboot, a real power cut inside a write and flashing remain untested.')
