#!/bin/sh
# Explicit session owner; only an exact consumed native New Project intent permits
# one next app. Boot admission delegates here; no blind application retry.
set -eu
stage=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
adapter=$stage/mpclearn-controls
inspector=$stage/main-button
settings=/media/az01-internal/Settings/MPC/MPC.settings
cd "$stage"
case "$stage" in /data/*) ;; *) echo 'Use the exact staged native package under /data.' >&2;exit 2;; esac
[ "$(id -u)" -eq 0 ] && [ "$(stat -c %u .)" -eq 0 ] && [ "$(stat -c %a .)" = 700 ] || exit 2
[ -f session-package.sha256 ] && sha256sum -c session-package.sha256 >/dev/null || exit 1
grep -Fqx '#define WINDOW_SECONDS 0u' config.h || exit 1
grep -Fqx "#define OBSERVER_LIBRARY \"$stage/command-observer.so\"" config.h || exit 1
command=${1:-status};case "$command" in start|status|stop|bridge-start|bridge-stop) ;; *) echo 'mcu-session.sh start|status|stop|bridge-start|bridge-stop [--verbose]' >&2;exit 2;; esac
verbose=${2:-};[ -z "$verbose" ] || [ "$verbose" = --verbose ] || exit 2
exec 9>session.lock
flock 9
boot_id=$(cat /proc/sys/kernel/random/boot_id)
same_boot(){ [ ! -f session.boot-id ] || [ "$(cat session.boot-id)" = "$boot_id" ]; }
start_tick(){ sed 's/.*) //' "/proc/$1/stat" 2>/dev/null | awk '{print $20}'; }
mpc_running(){
 for exe in /proc/[0-9]*/exe;do
  case "$(readlink "$exe" 2>/dev/null || :)" in */MPC|*/MPC\ \(deleted\)) return 0;; esac
 done
 return 1
}
write_identity(){ p=$1;[ -r "/proc/$p/stat" ] || return 1;printf '%s %s\n' "$p" "$(start_tick "$p")" > "$2"; }
identity(){
 same_boot || return 1
 [ -f "$1" ] || return 1
 read -r pid tick < "$1"
 case "$pid:$tick" in *[!0-9:]*|:*) return 1;; esac
 [ "$pid" -gt 1 ] || return 1
 [ -n "$pid" ] && [ -n "$tick" ] && [ -r "/proc/$pid/stat" ] && [ "$(start_tick "$pid")" = "$tick" ] && [ "$(readlink "/proc/$pid/exe")" = "$2" ]
}
status(){ "$stage/command-client" session-status "$stage/command.state" "$stage/volume.state"; }
bridge_stop(){
 if identity bridge.pid "$stage/mirror-input";then
  kill -TERM "$pid"
  n=0;while identity bridge.pid "$stage/mirror-input";do n=$((n+1));[ "$n" -le 150 ] || { echo 'Bridge did not finish bounded drain; incomplete, no producer stop.' >&2;return 1; };sleep .1;done
 fi
 if [ -f bridge.pid ];then
  n=0;while [ ! -f bridge.exit ];do n=$((n+1));[ "$n" -le 20 ] || { echo 'Missing bridge exit result; stop is incomplete.' >&2;return 1; };sleep .1;done
  [ "$(cat bridge.exit)" = 0 ] || { echo 'Bridge reported incomplete/error; inspect source before recovery.' >&2;return 1; }
 fi
}
bridge_start(){
 identity mpc.pid /usr/bin/MPC || return 1
 if identity bridge.pid "$stage/mirror-input";then echo 'Bridge already active.' >&2;return 1;fi
 source_code=0;status >/dev/null || source_code=$?
 case "$source_code" in 0|3) ;; *) return 1;; esac
 motor_choice=$("$stage/mirror-input" --surface-status "$stage/surface-preferences") || { echo "Invalid surface preference; refusing automatic motor enable." >&2;return 1; }
 rm -f bridge.exit
 # This child only waits for its explicit bridge and records its exit status.
 # It never restarts it. The operation lock is not inherited.
 (
  set +e
  "$stage/mirror-input" "$stage/volume.state" "$stage/command.state" manual $verbose "--held-mask=${handoff_holds:-0}" "--motors=$motor_choice" "--preferences=$stage/surface-preferences" "--stop-adapter-exe=$adapter" &
  b=$!;write_identity "$b" bridge.pid || exit 1
  wait "$b";code=$?;printf '%s\n' "$code" > bridge.exit
 ) 9>&- >bridge.log 2>&1 &
 n=0
 while :;do
  if identity bridge.pid "$stage/mirror-input" && grep -Fqx "BRIDGE_READY pid=$pid" bridge.log;then
   source_code=0;status >/dev/null || source_code=$?
   case "$source_code" in 0|3) [ ! -f bridge.exit ] && identity bridge.pid "$stage/mirror-input" && break;; esac
   echo 'Bridge/source became unavailable during startup; no retry.' >&2
   tail -n 8 bridge.log >&2;return 1
  fi
  n=$((n+1))
  if [ "$n" -gt 50 ] || [ -f bridge.exit ];then
   echo 'Bridge did not complete startup; no retry. Inspect bridge.log and bridge.exit.' >&2
   tail -n 8 bridge.log >&2;return 1
  fi
  sleep .1
 done
 echo "Bridge running pid=$pid; explicit bridge-stop or stop required."
}

exact(){ [ -n "$1" ] && [ -n "$2" ] && [ "$(readlink "/proc/$1/exe" 2>/dev/null || :)" = "$3" ] && [ "$(start_tick "$1")" = "$2" ]; }
adapter_scan(){
 adapter_pid=;adapter_tick=
 for exe in /proc/[0-9]*/exe;do
  [ "$(readlink "$exe" 2>/dev/null || :)" = "$adapter" ] || continue
  p=${exe#/proc/};p=${p%/exe};t=$(start_tick "$p")
  exact "$p" "$t" "$adapter" || continue
  [ -z "$adapter_pid" ] || { echo 'Multiple exact adapter processes; refusing action.' >&2;return 1; }
  adapter_pid=$p;adapter_tick=$t
 done
}
stop_adapter(){
 adapter_scan || return 1
 [ -n "$adapter_pid" ] || return 0
 exact "$adapter_pid" "$adapter_tick" "$adapter" || return 1
 kill -TERM "$adapter_pid"
 n=0;while exact "$adapter_pid" "$adapter_tick" "$adapter";do n=$((n+1));[ "$n" -le 50 ] || return 1;sleep .1;done
}
start_adapter(){
 for file in "$adapter" "$inspector";do [ -x "$file" ] && [ "$(stat -c %u "$file")" = 0 ] || return 1;done
 adapter_scan && [ -z "$adapter_pid" ] || return 1
 (exec env -u LD_PRELOAD -u LD_LIBRARY_PATH LD_BIND_NOW=1 "$adapter" "--stop-bridge-exe=$stage/mirror-input") 9>&- >manual-adapter.log 2>&1 &
 launched=$!;n=0
 while :;do
  adapter_scan || return 1
  [ "$adapter_pid" = "$launched" ] && grep -q '^DISARMED ' manual-adapter.log && break
  kill -0 "$launched" 2>/dev/null || return 1
  n=$((n+1));[ "$n" -le 50 ] || return 1;sleep .1
 done
 printf '%s %s
' "$adapter_pid" "$adapter_tick" >manual-adapter-unarmed.pid
}
route(){
 adapter_scan && [ -n "$adapter_pid" ] || return 1
 "$inspector" --inspect --adapter-exe "$adapter" --action play 9>&- || return 1
 exact "$adapter_pid" "$adapter_tick" "$adapter"
}
authorized(){ [ -f session.id ] && [ "$(cat session.id)" = "$session_id" ] && [ ! -e session.revoked ]; }
arm_adapter(){
 authorized && [ -f manual-adapter-unarmed.pid ] || return 1
 read -r unarmed_pid unarmed_tick <manual-adapter-unarmed.pid
 route && [ "$adapter_pid" = "$unarmed_pid" ] && [ "$adapter_tick" = "$unarmed_tick" ] || return 1
 status >/dev/null && identity bridge.pid "$stage/mirror-input" && authorized || return 1
 read -r app_pid app_tick <mpc.pid
 if "$stage/command-client" new-project-intent command.state "$app_pid" "$app_tick" >/dev/null;then return 1;fi
 exact "$adapter_pid" "$adapter_tick" "$adapter" || return 1
 kill -USR1 "$adapter_pid"
 rm -f manual-adapter-unarmed.pid
}
archive_generation(){
 read -r prior_pid prior_tick <mpc.pid
 archive="history/$prior_pid-$prior_tick";mkdir -p history;mkdir "$archive" || return 1
 for file in mpc.pid mpc.exit mpc.log-status mpc-output.pipe bridge.pid bridge.exit command.state volume.state bridge.log mpc.log manual-adapter.log manual-adapter-unarmed.pid generation.ready generation.failed;do
  [ ! -e "$file" ] || mv "$file" "$archive/" || return 1
 done
 echo "Exact departed app and unchanged command receipts retained in $archive."
}
archive_session(){
 if [ -f mpc.pid ];then archive_generation;else archive="history/unlaunched-$(cat session.id)";mkdir -p history;mkdir "$archive";fi
 for file in settings-before settings-before.sha256 session.id session.boot-id session.revoked owner.pid owner.log stop-incomplete-status.txt incomplete-before-command.state incomplete-before-volume.state incomplete-before-bridge.log incomplete-before.sha256;do [ ! -e "$file" ] || mv "$file" "$archive/";done
}
launch_app(){
 authorized && ! mpc_running && ! systemctl is-active --quiet acvs || return 1
 mkfifo -m 600 mpc-output.pipe || return 1
 tail -c 1048576 <mpc-output.pipe >mpc.log 9>&- &
 log_pid=$!
 (ulimit -s 1024;exec setarch -R -- env LANG=C GLIBC_TUNABLES=glibc.malloc.hugetlb=2 MALLOC_ARENA_MAX=1 LD_PRELOAD="$stage/command-observer.so" /usr/bin/MPC) >mpc-output.pipe 2>&1 9>&- &
 app_child=$!;write_identity "$app_child" mpc.pid || return 1
 # The child may still be crossing exec; don't classify that interval as death.
 n=0;while ! identity mpc.pid /usr/bin/MPC;do n=$((n+1));[ "$n" -le 50 ] && kill -0 "$app_child" 2>/dev/null || return 1;sleep .1;done
}
wait_receipt(){
 # Actual wait and bounded logger join never need session.lock: explicit stop
 # can hold the lock while waiting for these receipts without a deadlock.
 app_code=0;wait "$app_child" || app_code=$?
 log_complete=1;n=0
 while kill -0 "$log_pid" 2>/dev/null;do n=$((n+1));if [ "$n" -gt 20 ];then log_complete=0;kill -TERM "$log_pid" 2>/dev/null || :;break;fi;sleep .1;done
 log_code=0;wait "$log_pid" || log_code=$?;[ "$log_code" -eq 0 ] || log_complete=0
 rm -f mpc-output.pipe
 printf 'complete=%s logger_exit=%s limit_bytes=1048576\n' "$log_complete" "$log_code" >mpc.log-status
 printf '%s\n' "$app_code" >mpc.exit
}
owner(){
 exec 9>session.lock
 flock 9
 app_child=;log_pid=
 if ! authorized || ! launch_app;then flock -u 9;[ -z "$app_child" ] || wait_receipt;return 1;fi
 flock -u 9
 while :;do
  setup=0;arm_attempted=0;intent_seen=0;initializing=0
  while identity mpc.pid /usr/bin/MPC;do
   # Never block here: stop needs this owner to reach actual wait/exit receipt.
   if flock -n 9;then
    if authorized;then
     read -r app_pid app_tick <mpc.pid
     if "$stage/command-client" new-project-intent command.state "$app_pid" "$app_tick" >/dev/null 2>&1;then
      if [ "$intent_seen" -eq 0 ];then stop_adapter || :;intent_seen=1;fi
     elif [ "$setup" -ne 2 ];then
      code=0;status >/dev/null 2>&1 || code=$?
      if [ "$code" -eq 6 ];then stop_adapter || :;intent_seen=1;fi
      if [ "$setup" -eq 0 ];then
       case "$code" in
        0|3)
         if start_adapter && bridge_start;then setup=1;printf '%s\n' ready >generation.ready
         else setup=2;echo 'Controller startup failed; no automatic retry.' >generation.failed;fi;;
        5|1) initializing=$((initializing+1));if [ "$initializing" -gt 40 ];then setup=2;echo 'Source initialization failed; no automatic retry.' >generation.failed;fi;;
        *) setup=2;echo 'Source terminal before controller startup.' >generation.failed;;
       esac
      fi
      if [ "$setup" -eq 1 ] && [ "$code" -eq 0 ] && [ "$arm_attempted" -eq 0 ];then
       arm_attempted=1
       arm_adapter || echo 'Route/source unavailable at arming; no automatic retry.' >generation.failed
      fi
     fi
    fi
    flock -u 9
   fi
   sleep .5
  done
  wait_receipt
  flock 9
  if ! authorized;then flock -u 9;return 0;fi
  stop_adapter || { flock -u 9;return 1; }
  bridge_stop || :
  if [ "$app_code" -ne 0 ] || mpc_running || systemctl is-active --quiet acvs;then flock -u 9;return 0;fi
  read -r departed_pid departed_tick <mpc.pid
  if ! "$stage/command-client" new-project-intent command.state "$departed_pid" "$departed_tick" >/dev/null;then flock -u 9;return 0;fi
  # Exact old app is dead. A failed bridge receipt can now be archived unchanged;
  # it is not a successful settlement. Its process must still be absent.
  if identity bridge.pid "$stage/mirror-input" || { [ -f bridge.pid ] && [ ! -f bridge.exit ]; };then flock -u 9;return 1;fi
  handoff_holds=0
  if [ -f bridge.pid ] && [ -f bridge.log ];then
   read -r old_bridge old_bridge_tick <bridge.pid
   final_mask=$(sed -n "s/^TOUCH_FINAL pid=$old_bridge mask=\\([0-9][0-9]*\\)$/\\1/p" bridge.log)
   case "$final_mask" in ''|*[!0-9]*) ;; *) [ "$final_mask" -le 511 ] && handoff_holds=$final_mask;; esac
  fi
  authorized && "$stage/command-client" new-project-intent command.state "$departed_pid" "$departed_tick" consume || { flock -u 9;return 1; }
  archive_generation || { flock -u 9;return 1; }
  app_child=;log_pid=
  if ! authorized || ! launch_app;then flock -u 9;[ -z "$app_child" ] || wait_receipt;return 1;fi
  flock -u 9
 done
}
case "$command" in
 status)
  if identity bridge.pid "$stage/mirror-input";then echo "bridge_running pid=$pid";else echo bridge_not_running;fi
  adapter_scan || exit 1
  echo "adapter_pid=${adapter_pid:-none}; arming state is not queried"
  motor_choice=$("$stage/mirror-input" --surface-status "$stage/surface-preferences") || { echo motor_follow_preference_invalid;exit 1; }
  echo "motor_follow=$motor_choice"
  [ -z "$adapter_pid" ] || route || exit 1
  [ ! -f session.revoked ] || echo session_revoked
  [ ! -f mpc.exit ] || echo "mpc_exit=$(cat mpc.exit) log=$(cat mpc.log-status 2>/dev/null || echo unavailable)"
  status;;
 bridge-stop) bridge_stop;;
 bridge-start)
  session_id=$(cat session.id);authorized || exit 1
  bridge_start
  # Explicit recovery of a missed chooser-to-project arming attempt. This is
  # under session.lock and reuses all live route/identity/intent checks.
  if [ -f manual-adapter-unarmed.pid ] && status >/dev/null;then
   arm_adapter || { echo 'Explicit bridge-start could not arm the existing adapter.' >&2;exit 1; }
   if printf '%s\n' 'Route/source unavailable at arming; no automatic retry.' | cmp -s - generation.failed;then rm -f generation.failed;fi
  fi;;
 start)
  if identity mpc.pid /usr/bin/MPC;then
   session_id=$(cat session.id);authorized || exit 1
   status;echo 'Existing owned session retained; no restart or signal.';exit 0
  fi
  identity bridge.pid "$stage/mirror-input" && exit 1
  if same_boot && [ -f owner.pid ];then read -r owner_pid owner_tick <owner.pid;[ "$(start_tick "$owner_pid")" != "$owner_tick" ] || { echo 'Previous session owner still finishing.' >&2;exit 1; };fi
  if [ -e command.state ] || [ -e volume.state ] || [ -e settings-before ];then
   [ -f session.id ] && [ -f settings-before.sha256 ] && sha256sum -c settings-before.sha256 >/dev/null || exit 1
  fi
  # Prior-boot locators cannot authorize signals. Preserve them before any
  # current-boot process action, then admit a new session normally.
  if ! same_boot;then [ -f session.id ] || exit 1;archive_session;fi
  stop_adapter || exit 1
  echo 'Save the project before starting: this explicit operation restarts MPC.'
  systemctl stop acvs
  ! systemctl is-active --quiet acvs && ! mpc_running || exit 1
  if [ -f session.id ];then
   archive_session
  fi
  cp -p "$settings" settings-before;sha256sum settings-before >settings-before.sha256
  session_id="$$-$(start_tick $$)";printf '%s\n' "$session_id" >session.id
  printf '%s\n' "$boot_id" >session.boot-id
  rm -f session.revoked
  (owner) 9>&- >owner.log 2>&1 &
  owner_pid=$!;write_identity "$owner_pid" owner.pid
  flock -u 9
  n=0;while [ ! -f generation.ready ];do
   [ ! -f generation.failed ] && [ ! -f session.revoked ] && [ "$(cat session.id)" = "$session_id" ] && kill -0 "$owner_pid" 2>/dev/null || { cat generation.failed 2>/dev/null || :;exit 1; }
   n=$((n+1));[ "$n" -le 250 ] || { echo 'Session startup not ready; inspect owner.log; no retry.' >&2;exit 1; };sleep .1
  done
  echo 'MCU session active. Chooser clears controls; saved User Template reconnects automatically.';;
 stop)
  [ -f session.id ] || { echo 'No owned session.' >&2;exit 1; }
  # Durable revocation precedes every slow drain/stop. Owner cannot launch or arm
  # while this lock is held and sees the revocation on its next admission.
  cp session.id session.revoked
  stop_adapter || exit 1
  clean=1;bridge_stop || clean=0
  owned_mpc=0
  if identity mpc.pid /usr/bin/MPC;then owned_mpc=1
  else
   [ -f session.id ] && [ -f settings-before ] && ! mpc_running && ! systemctl is-active --quiet acvs || { echo 'Different MPC/service or no owned session locator; recovery refused.' >&2;exit 1; }
   clean=0
  fi
  if [ "$clean" -eq 1 ] && [ "$owned_mpc" -eq 1 ];then "$stage/command-client" stop "$stage/command.state" "$stage/volume.state" || clean=0;fi
  if [ "$clean" -ne 1 ];then
   echo 'INCOMPLETE: graceful source closure was not certified; preserving evidence before exact-process recovery.' >&2
   status >stop-incomplete-status.txt 2>&1 || :
   for file in command.state volume.state bridge.log;do [ ! -f "$file" ] || cp -p "$file" "incomplete-before-$file";done
   : >incomplete-before.sha256
   for file in incomplete-before-command.state incomplete-before-volume.state incomplete-before-bridge.log;do [ ! -f "$file" ] || sha256sum "$file" >>incomplete-before.sha256;done
  fi
  # A timed-out bridge must be gone before stock recovery, not merely signaled.
  if identity bridge.pid "$stage/mirror-input";then
   clean=0;kill -KILL "$pid"
   n=0;while identity bridge.pid "$stage/mirror-input";do n=$((n+1));[ "$n" -le 50 ] || exit 1;sleep .1;done
  fi
  # With clean=1 producer is immutable/closed. TERM is application recovery, not the
  # source protocol's graceful stop or a manufactured settlement.
  if [ "$owned_mpc" -eq 1 ];then identity mpc.pid /usr/bin/MPC || exit 1;kill -TERM "$pid";fi
  n=0;while identity mpc.pid /usr/bin/MPC;do
   n=$((n+1));if [ "$n" -eq 150 ];then clean=0;echo 'Exact owned MPC did not exit after TERM; forced recovery is not graceful closure.' >&2;identity mpc.pid /usr/bin/MPC && kill -KILL "$pid";fi
   [ "$n" -le 180 ] || { echo 'MPC still running; settings not restored.' >&2;exit 1; };sleep .1
  done
  if [ -f mpc.pid ];then
   n=0;while [ ! -f mpc.exit ];do n=$((n+1));[ "$n" -le 30 ] || { echo 'MPC exit receipt unavailable; preserving incomplete diagnostics.' >&2;clean=0;break; };sleep .1;done
   [ ! -f mpc.exit ] || echo "MPC exited status=$(cat mpc.exit); $(cat mpc.log-status)"
  fi
  ! mpc_running && ! systemctl is-active --quiet acvs || exit 1
  sha256sum -c settings-before.sha256 >/dev/null
  cp -p settings-before "$settings"
  systemctl start acvs
  echo "Session recovered; clean=$clean; fresh stopped settings restored; normal acvs started. Verify UI and PCM."
  [ "$clean" -eq 1 ];;
esac
