#!/usr/bin/env python3
"""Container-only production session-owner test; native MPC, protocol, route and
ALSA children are substitutes. Real shell/flock/process identity/wait/archive
paths run. A pre-owner-lock pause is the only scheduling injection.
"""
import os, pathlib, subprocess, time, signal
P=pathlib.Path
assert P('/.dockerenv').exists() and os.getuid()==0
stage=P('/data/mpclearn-model.wrapper-test');stage.mkdir(parents=True,mode=0o700)
settings=P('/media/az01-internal/Settings/MPC/MPC.settings');settings.parent.mkdir(parents=True);settings.write_text('original settings\n')
runtime=P('/run/mpclearn-'+stage.name)
source=P('/src/mcu-session.sh').read_text()
source=source.replace('owner(){\n exec 9>session.lock','owner(){\n while [ -f pause-owner ];do sleep .05;done\n exec 9>session.lock')
(stage/'mcu-session.sh').write_text(source);(stage/'mcu-session.sh').chmod(0o700)
(stage/'mcu').write_text(P('/src/mcu.sh').read_text().replace('stage=/data/mpclearn-model.manual1',f'stage={stage}'));(stage/'mcu').chmod(0o700)
(stage/'config.h').write_text(f'#define WINDOW_SECONDS 0u\n#define OBSERVER_LIBRARY "{stage}/command-observer.so"\n#define OBSERVER_LOG "{runtime}/volume.state"\n#define COMMAND_PATH "{runtime}/command.state"\n')
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
f=fopen("/run/mpclearn-mpclearn-model.wrapper-test/command.state","w");fprintf(f,"%u\n",(unsigned)getpid());fclose(f);f=fopen("/run/mpclearn-mpclearn-model.wrapper-test/volume.state","w");fclose(f);
for(int i=0;i<1100000;i++)putchar('x');puts(" MPC diagnostic tail");fflush(stdout);
while(!stopped){f=fopen("exit-request","r");if(f){int code=0;fscanf(f,"%d",&code);fclose(f);unlink("exit-request");return code;}usleep(10000);}return 0;}
''')
subprocess.run(['gcc','-D_GNU_SOURCE','/tmp/session-stub.c','-o','/usr/bin/MPC'],check=True)
for name in ['mirror-input','mpclearn-controls']:(stage/name).write_bytes(P('/usr/bin/MPC').read_bytes());(stage/name).chmod(0o700)
bin=P('/tmp/session-bin');bin.mkdir()
def script(p,text):p.write_text('#!/bin/sh\n'+text);p.chmod(0o700)
script(bin/'setarch','shift;shift;exec "$@"\n')
script(stage/'main-button','test ! -f route-fail\n')
script(bin/'systemctl',f'''case "$1" in stop) rm -f {stage}/service-active;; start) touch {stage}/service-active;; is-active) test -f {stage}/service-active;; *) exit 2;; esac\n''')
script(stage/'command-client',f'''cd {stage}
case "$1" in
 session-status) test "$2" = "{runtime}/command.state" -a "$3" = "{runtime}/volume.state" || exit 9;test -f "$2" -a -f "$3" || exit 5;test ! -f closed || exit 4;test ! -f native-intent || exit 6;test -f project-ready || exit 3;exit 0;;
 new-project-intent) test -f native-intent || exit 1;test "$2" = "{runtime}/command.state" || exit 9;test "$(cat "$2")" = "$3" || exit 1;test "$(cat native-intent)" = "$3" || exit 1;if test "${{5:-}}" = consume;then test ! -e /proc/$3/exe || exit 1;mv native-intent consumed-intent;fi;;
 stop) test "$2" = "{runtime}/command.state" -a "$3" = "{runtime}/volume.state" || exit 9;touch closed;;
 *) exit 2;; esac
''')
for name in ['mirror-read','command-observer.so']:(stage/name).write_bytes(b'')
env=dict(os.environ,PATH=str(bin)+':'+os.environ['PATH'])
with (stage/'session-package.sha256').open('wb') as f:subprocess.run(['sha256sum','command-observer.so','command-client','mirror-input','mirror-read','config.h','mcu-session.sh','mcu'],cwd=stage,stdout=f,check=True)
def until(pred,why,seconds=12):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if pred():return
  time.sleep(.05)
 raise AssertionError(why+'\n'+(stage/'owner.log').read_text())
def run(op,expected=0):
 p=subprocess.run([str(stage/('mcu-session.sh' if op.startswith('bridge-') else 'mcu')),op],env=env,capture_output=True,text=True,timeout=35)
 assert p.returncode==expected,(op,p.returncode,p.stdout,p.stderr)
 return p
def count(name):return len((stage/name).read_text()) if (stage/name).exists() else 0
def app_pid():return int((stage/'mpc.pid').read_text().split()[0])
def owner_gone():
 try:p=int((stage/'owner.pid').read_text().split()[0]);return not P(f'/proc/{p}/exe').exists()
 except FileNotFoundError:return True
def depart(intent,code=0):
 if intent:(stage/'native-intent').write_text(str(app_pid()))
 (stage/'exit-request').write_text(str(code))
def cleanup():
 for exe in P('/proc').glob('[0-9]*/exe'):
  try:
   if os.readlink(exe) in ('/usr/bin/MPC',str(stage/'mirror-input'),str(stage/'mpclearn-controls')):os.kill(int(exe.parent.name),signal.SIGKILL)
  except FileNotFoundError:pass
try:
 # Runtime admission refuses symlinks, unknown entries and unowned state.
 runtime.symlink_to('/tmp',target_is_directory=True);run('start',1);runtime.unlink()
 runtime.mkdir(mode=0o700);(runtime/'unexpected').touch();run('start',1);(runtime/'unexpected').unlink()
 (runtime/'command.state').symlink_to('/tmp/missing-state');run('start',1);(runtime/'command.state').unlink()
 (runtime/'command.state').write_text('unowned');run('start',1);(runtime/'command.state').unlink()
 assert not (stage/'launches').exists()
 # Stop wins even before the owner has acquired its launch admission.
 (stage/'pause-owner').touch();started=subprocess.Popen([str(stage/'mcu'),'start'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 until(lambda:(stage/'owner.pid').exists(),'owner launch locator');run('stop',1);(stage/'pause-owner').unlink();started.communicate(timeout=10)
 until(owner_gone,'revoked unlaunched owner exits');assert count('launches')==0 and (stage/'service-active').exists()
 # The canceled initial generation has no app locator; a new explicit start
 # must retain its settings/session diagnostics before replacing them.
 run('start');until(lambda:(stage/'generation.ready').exists(),'chooser controller ready');assert count('launches')==1 and count('armed')==0
 assert '--motors=on' in (stage/'bridge.log').read_text()
 (stage/'surface-preferences').write_text('MCU-SURFACE1 motors=0\n');(stage/'surface-preferences').chmod(0o600)
 original=(stage/'settings-before').read_bytes()
 # Reproduce the native miss: bridge deliberately absent when chooser becomes
 # ready, then explicit bridge-start must repair arming under the same owner.
 run('bridge-stop');(stage/'project-ready').touch()
 arming_failure='Route/source unavailable at arming; no automatic retry.\n'
 until(lambda:(stage/'generation.failed').exists(),'missing bridge arming failure')
 assert (stage/'generation.failed').read_text()==arming_failure and count('armed')==0 and (stage/'manual-adapter-unarmed.pid').exists()
 (stage/'route-fail').touch();run('bridge-start',1)
 assert count('armed')==0 and (stage/'manual-adapter-unarmed.pid').exists() and (stage/'generation.failed').read_text()==arming_failure
 run('bridge-stop');(stage/'route-fail').unlink();run('bridge-start');until(lambda:count('armed')==1,'explicit bridge-start restores missed arming')
 assert not (stage/'generation.failed').exists() and not (stage/'manual-adapter-unarmed.pid').exists()
 state_inodes=[(runtime/n).stat().st_ino for n in ('command.state','volume.state')]
 run('bridge-stop');run('bridge-start');assert count('armed')==1
 assert state_inodes==[(runtime/n).stat().st_ino for n in ('command.state','volume.state')]
 run('start');assert count('launches')==1
 print('PASS explicit bridge-start: actual missed chooser arming recovery, route-failure refusal, exact failure cleanup and no duplicate signal (native adapter/protocol substituted)')
 for expected in (2,3):
  previous=app_pid();depart(True);until(lambda:count('launches')==expected and (stage/'generation.ready').exists() and app_pid()!=previous,'accepted New Project restarts once')
  assert count('armed')==expected-1 and (stage/'settings-before').read_bytes()==original
  assert '--held-mask=3' in (stage/'bridge.log').read_text()
  assert '--motors=off' in (stage/'bridge.log').read_text()
  if expected==2:
   run('bridge-stop');run('bridge-start');assert count('armed')==expected-1 # chooser stays disarmed
   run('bridge-stop');(stage/'project-ready').touch();until(lambda:(stage/'generation.failed').exists(),'second missed arming')
   (stage/'generation.failed').write_text('Different retained failure.\n');run('bridge-start')
   until(lambda:count('armed')==expected,'explicit recovery with other failure')
   assert (stage/'generation.failed').read_text()=='Different retained failure.\n'
  else:
   (stage/'project-ready').touch();until(lambda:count('armed')==expected,'new saved template arms')
 assert len(list((stage/'history').iterdir()))>=2
 assert len(list((stage/'history').glob('*/command.state')))>=2 and not (stage/'command.state').exists()
 print('PASS actual owner: repeatable accepted exit -> distinct chooser bridge/disarmed adapter -> saved template arm; old receipts/archive and original settings retained (native protocol and MIDI substituted)')
 # Positive marker cannot authorize a crash restart; generic zero exit neither.
 for positive,code in ((True,17),(False,0)):
  before=count('launches');depart(positive,code);until(owner_gone,'non-NewProject owner ends');assert count('launches')==before
  run('stop',1);assert settings.read_text()=='original settings\n';run('start');assert '--motors=off' in (stage/'bridge.log').read_text();(stage/'project-ready').touch();until(lambda:count('armed')==before+1,'fresh explicit session arms')
 # Durable stop during a pending handoff leaves no later launch/arming.
 depart(True);run('stop',1);until(owner_gone,'stop winner owner exits');before=count('launches');time.sleep(.7);assert count('launches')==before
 assert (stage/'session.revoked').exists() and (stage/'service-active').exists()
 assert settings.read_text()=='original settings\n'
 print('PASS actual owner: crash and generic exit do not restart; explicit stop revokes before launch and during handoff; exact child waits and settings recovery (native protocol substituted)')
 # Simulate reboot: only tmpfs mappings vanish. Old locators/settings remain,
 # and cannot authorize a current process signal or a fabricated close receipt.
 for name in ('command.state','volume.state'):(runtime/name).unlink(missing_ok=True)
 (stage/'session.boot-id').write_text('previous-boot\n');run('start')
 assert (runtime/'command.state').exists() and not (stage/'command.state').exists()
 (stage/'project-ready').touch();until(lambda:not (stage/'manual-adapter-unarmed.pid').exists(),'reboot session arming');run('stop')
 print('PASS real tmpfs admission, unchanged mappings on bridge restart, cross-filesystem generation archive, and vanished reboot mappings (boot id and native app substituted)')
finally:cleanup()
