#!/bin/sh
# Explicit session owner; only an exact consumed native New Project intent permits
# one next app. Boot admission delegates here; no blind application retry.
set -eu
# Runs in place (image or dev override). Locators/logs stay on /data to survive
# a power-off; shared state and the settings snapshot are tmpfs.
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
home=/data/mpclearn
history=$home/history
preferences=$home/surface-preferences
runtime=/run/mpclearn
state=$runtime/state
snapshot=$runtime/settings-before
staging=$runtime/archive
adapter=$here/mpclearn-controls
inspector=$here/main-button
settings=/media/az01-internal/Settings/MPC/MPC.settings
# History bound (bytes, entries) and per-file caps; head caps are 4096 multiples.
history_budget=16777216
history_entries=8
cap_mpc_log=1048576
cap_log=262144
cap_locator=4096
cap_status_head=1835008
cap_status_tail=262144
cap_mirror_head=458752
cap_mirror_tail=65536
cap_session_head=196608
cap_session_tail=65536
case "$here" in
 /usr/share/mpclearn/mcu|/data/mpclearn/dev) ;;
 *) echo "Unsupported package location $here; use /usr/share/mpclearn/mcu or /data/mpclearn/dev." >&2;exit 2;;
esac
owned_dir(){ [ -d "$1" ] && [ ! -L "$1" ] && [ "$(stat -c %u "$1")" = 0 ] && [ "$(stat -c %a "$1")" = 700 ]; }
tmpfs_dir(){ owned_dir "$1" && [ "$(stat -f -c %T "$1")" = tmpfs ]; }
[ "$(id -u)" -eq 0 ] && owned_dir "$here" || exit 2
cd "$here"
[ -f session-package.sha256 ] && sha256sum -c session-package.sha256 >/dev/null || exit 1
grep -Fqx '#define WINDOW_SECONDS 0u' config.h || exit 1
grep -Fqx "#define OBSERVER_LIBRARY \"$here/command-observer.so\"" config.h || exit 1
grep -Fqx "#define OBSERVER_LOG \"$state/volume.state\"" config.h || exit 1
grep -Fqx "#define COMMAND_PATH \"$state/command.state\"" config.h || exit 1
command=${1:-status};case "$command" in start|status|stop|bridge-start|bridge-stop) ;; *) echo 'mcu-session.sh start|status|stop|bridge-start|bridge-stop [--verbose]' >&2;exit 2;; esac
verbose=${2:-};[ -z "$verbose" ] || [ "$verbose" = --verbose ] || exit 2
owned_dir "$home" || { echo "$home is not a root-owned 0700 folder; refusing to write there." >&2;exit 2; }
for folder in "$home/session" "$history";do
 [ -e "$folder" ] || [ -L "$folder" ] || mkdir -m 700 "$folder" 2>/dev/null || :
 owned_dir "$folder" || { echo "$folder is not a root-owned 0700 folder." >&2;exit 2; }
done
[ -e "$runtime" ] || [ -L "$runtime" ] || mkdir -m 700 "$runtime" 2>/dev/null || :
tmpfs_dir "$runtime" || { echo "$runtime is not a root-owned 0700 tmpfs folder." >&2;exit 1; }
cd "$home/session"
exec 9>session.lock
flock 9
# Only live shared pages use RAM; receipts/logs stay in the session folder.
# Refuse aliases and unexpected files rather than replacing another producer.
state_check(){
 tmpfs_dir "$state" || return 1
 for file in "$state"/* "$state"/.[!.]* "$state"/..?*;do
  [ -e "$file" ] || { [ ! -L "$file" ] || return 1;continue; }
  case "$file" in "$state/command.state"|"$state/volume.state") [ ! -L "$file" ] && [ -f "$file" ] && [ "$(stat -c %u "$file")" = 0 ] || return 1;; *) return 1;; esac
 done
}
if [ -e "$state" ] || [ -L "$state" ];then state_check || exit 1
elif [ "$command" = start ];then mkdir -m 700 "$state" && state_check || exit 1
fi
boot_id=$(cat /proc/sys/kernel/random/boot_id)
same_boot(){ [ ! -f session.boot-id ] || [ "$(cat session.boot-id)" = "$boot_id" ]; }
# A live session belongs to the package that started it (identity uses its paths).
if same_boot && [ -f session.location ] && [ ! -f session.revoked ] && [ "$(cat session.location)" != "$here" ];then
 echo "The current session belongs to $(cat session.location); use its mcu." >&2;exit 2
fi
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
status(){ "$here/command-client" session-status "$state/command.state" "$state/volume.state"; }
bridge_stop(){
 if identity bridge.pid "$here/mirror-input";then
  kill -TERM "$pid"
  n=0;while identity bridge.pid "$here/mirror-input";do n=$((n+1));[ "$n" -le 150 ] || { echo 'Bridge did not finish bounded drain; incomplete, no producer stop.' >&2;return 1; };sleep .1;done
 fi
 if [ -f bridge.pid ];then
  n=0;while [ ! -f bridge.exit ];do n=$((n+1));[ "$n" -le 20 ] || { echo 'Missing bridge exit result; stop is incomplete.' >&2;return 1; };sleep .1;done
  [ "$(cat bridge.exit)" = 0 ] || { echo 'Bridge reported incomplete/error; inspect source before recovery.' >&2;return 1; }
 fi
}
bridge_start(){
 identity mpc.pid /usr/bin/MPC || return 1
 if identity bridge.pid "$here/mirror-input";then echo 'Bridge already active.' >&2;return 1;fi
 source_code=0;status >/dev/null || source_code=$?
 case "$source_code" in 0|3) ;; *) return 1;; esac
 motor_choice=$("$here/mirror-input" --surface-status "$preferences") || { echo "Invalid surface preference; refusing automatic motor enable." >&2;return 1; }
 rm -f bridge.exit
 # This child only waits for its explicit bridge and records its exit status.
 # It never restarts it. The operation lock is not inherited.
 (
  set +e
  "$here/mirror-input" "$state/volume.state" "$state/command.state" manual $verbose "--held-mask=${handoff_holds:-0}" "--motors=$motor_choice" "--preferences=$preferences" "--stop-adapter-exe=$adapter" &
  b=$!;write_identity "$b" bridge.pid || exit 1
  wait "$b";code=$?;printf '%s\n' "$code" > bridge.exit
 ) 9>&- >bridge.log 2>&1 &
 n=0
 while :;do
  if identity bridge.pid "$here/mirror-input" && grep -Fqx "BRIDGE_READY pid=$pid" bridge.log;then
   source_code=0;status >/dev/null || source_code=$?
   case "$source_code" in 0|3) [ ! -f bridge.exit ] && identity bridge.pid "$here/mirror-input" && break;; esac
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
 (exec env -u LD_PRELOAD -u LD_LIBRARY_PATH LD_BIND_NOW=1 "$adapter" "--stop-bridge-exe=$here/mirror-input") 9>&- >manual-adapter.log 2>&1 &
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
 status >/dev/null && identity bridge.pid "$here/mirror-input" && authorized || return 1
 read -r app_pid app_tick <mpc.pid
 if "$here/command-client" new-project-intent "$state/command.state" "$app_pid" "$app_tick" >/dev/null;then return 1;fi
 exact "$adapter_pid" "$adapter_tick" "$adapter" || return 1
 kill -USR1 "$adapter_pid"
 rm -f manual-adapter-unarmed.pid
}
# History is text only: capped logs, locators and summaries; never raw state.
generation_files='mpc.pid mpc.exit mpc.log-status bridge.pid bridge.exit bridge.log mpc.log manual-adapter.log manual-adapter-unarmed.pid generation.ready generation.failed'
session_files='session.id session.boot-id session.location session.revoked owner.pid owner.log stop-incomplete-status.txt incomplete-command-status.json incomplete-command-status.json.truncated incomplete-mirror-read.json incomplete-mirror-read.json.truncated incomplete-bridge.log'
cap_for(){
 case "$1" in
  mpc.log) echo "$cap_mpc_log";;
  *.log|stop-incomplete-status.txt) echo "$cap_log";;
  incomplete-command-status.json) echo $((cap_status_head+cap_status_tail+256));;
  incomplete-mirror-read.json) echo $((cap_mirror_head+cap_mirror_tail+256));;
  *) echo "$cap_locator";;
 esac
}
# Keep head+tail around a marker line; a cut file is not JSON (.truncated says so).
# Runs where set -e does not apply. BusyBox head lacks -c, so dd reads 4K blocks.
cap_summary(){
 size=$(stat -c %s "$1") || return 1
 if [ "$size" -le $(($3+$4)) ];then rm -f "$2.truncated" && mv "$1" "$2";return;fi
 { dd if="$1" bs=4096 count=$(($3/4096)) 2>/dev/null && printf '\n[mpclearn history: %s bytes omitted here; this file is not valid JSON]\n' $((size-$3-$4)) && tail -c "$4" "$1"; } >"$2" || return 1
 printf 'original_bytes=%s kept_head=%s kept_tail=%s\n' "$size" "$3" "$4" >"$2.truncated" || return 1
 rm -f "$1"
}
# Summaries into DIR; mirror-read exits 1 on a closed producer, still evidence.
summarize_state(){
 raw=$runtime/summary.raw
 if [ -f "$state/command.state" ];then
  "$here/command-client" status "$state/command.state" >"$raw" 2>&1 || :
  cap_summary "$raw" "$1/$2" "$cap_status_head" "$cap_status_tail" || return 1
 fi
 if [ -f "$state/command.state" ] || [ -f "$state/volume.state" ];then
  "$here/command-client" session-status "$state/command.state" "$state/volume.state" >"$raw" 2>&1 || :
  cap_summary "$raw" "$1/$3" "$cap_session_head" "$cap_session_tail" || return 1
 fi
 if [ -f "$state/volume.state" ];then
  "$here/mirror-read" "$state/volume.state" --all >"$raw" 2>&1 || :
  cap_summary "$raw" "$1/$4" "$cap_mirror_head" "$cap_mirror_tail" || return 1
 fi
}
tree_bytes(){
 # Apparent bytes, each folder 4096, so tmpfs and ext4 measure alike.
 find "$1" \( -type f -o -type d \) -exec stat -c '%F %s' {} + | awk '{if($1=="directory")n+=4096;else n+=$NF}END{print n+0}'
}
history_names(){
 for entry in "$history"/* "$history"/.[!.]* "$history"/..?*;do
  [ -e "$entry" ] || [ -L "$entry" ] || continue
  printf '%s\n' "${entry##*/}"
 done | sort -n
}
# Drop a crash leftover, measure, prune oldest until it fits, copy, rename.
history_commit(){
 owned_dir "$history" || { echo "$history is not a root-owned 0700 folder; entry not written." >&2;return 1; }
 rm -rf "$history/.new" && [ ! -e "$history/.new" ] || return 1
 new_bytes=$(tree_bytes "$staging") || return 1
 [ $((4096+new_bytes)) -le "$history_budget" ] || { echo "History entry of $new_bytes bytes exceeds the budget; not retained." >&2;archive=;return 0; }
 while :;do
  total=4096;count=0;oldest=;last=0
  for name in $(history_names);do
   total=$((total+$(tree_bytes "$history/$name")));count=$((count+1))
   [ -n "$oldest" ] || oldest=$name
   # Strip leading zeros: shell arithmetic reads them as octal.
   number=${name%%-*};case "$number" in ''|*[!0-9]*) ;; *) number=${number#"${number%%[!0]*}"};[ "${number:-0}" -le "$last" ] || last=$number;; esac
  done
  [ $((total+new_bytes)) -gt "$history_budget" ] || [ $((count+1)) -gt "$history_entries" ] || break
  [ -n "$oldest" ] || return 1
  rm -rf "$history/$oldest";[ ! -e "$history/$oldest" ] && [ ! -L "$history/$oldest" ] || return 1
 done
 boot=$(cut -c1-8 session.boot-id 2>/dev/null || :);case "$boot" in ''|*[!0-9a-f]*) boot=unknown;; esac
 archive=$history/$(printf '%06d' $((last+1)))-$boot-$1
 mkdir -m 700 "$history/.new" || return 1
 for file in "$staging"/*;do [ ! -f "$file" ] || cp "$file" "$history/.new/" || return 1;done
 mv "$history/.new" "$archive"
}
stage_file(){
 [ -f "$1" ] && [ ! -L "$1" ] || return 0
 tail -c "$(cap_for "$1")" "$1" >"$staging/$1"
}
archive_entry(){
 rm -rf "$staging";mkdir -m 700 "$staging" || return 1
 generation=0
 if [ -f mpc.pid ];then
  generation=1
  read -r prior_pid prior_tick <mpc.pid
  label=$prior_pid-$prior_tick
  for file in $generation_files;do stage_file "$file" || return 1;done
  summarize_state "$staging" command-status.json session-status.txt mirror-read.json || return 1
 else
  label=unlaunched-$(cat session.id)
 fi
 if [ "$1" = session ];then for file in $session_files;do stage_file "$file" || return 1;done;fi
 case "$label" in *[!0-9a-z-]*) label=unnamed;; esac
 history_commit "$label" || return 1
 rm -rf "$staging" || return 1
 if [ "$generation" = 1 ];then
  for file in $generation_files mpc-output.pipe;do rm -f "$file";done
  rm -f "$state/command.state" "$state/volume.state"
 fi
 if [ "$1" = session ];then
  for file in $session_files;do rm -f "$file";done
  rm -f "$snapshot" "$snapshot.sha256"
 fi
}
archive_generation(){
 archive_entry generation || return 1
 echo "Exact departed app retained as text summaries in ${archive:-no history entry}."
}
archive_session(){ archive_entry session; }
launch_app(){
 state_check && [ ! -e "$state/command.state" ] && [ ! -e "$state/volume.state" ] || return 1
 authorized && ! mpc_running && ! systemctl is-active --quiet acvs || return 1
 mkfifo -m 600 mpc-output.pipe || return 1
 tail -c "$cap_mpc_log" <mpc-output.pipe >mpc.log 9>&- &
 log_pid=$!
 (ulimit -s 1024;exec setarch -R -- env LANG=C GLIBC_TUNABLES=glibc.malloc.hugetlb=2 MALLOC_ARENA_MAX=1 LD_PRELOAD="$here/command-observer.so" /usr/bin/MPC) >mpc-output.pipe 2>&1 9>&- &
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
 printf 'complete=%s logger_exit=%s limit_bytes=%s\n' "$log_complete" "$log_code" "$cap_mpc_log" >mpc.log-status
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
     if "$here/command-client" new-project-intent "$state/command.state" "$app_pid" "$app_tick" >/dev/null 2>&1;then
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
  if ! "$here/command-client" new-project-intent "$state/command.state" "$departed_pid" "$departed_tick" >/dev/null;then flock -u 9;return 0;fi
  # Exact old app is dead. A failed bridge receipt can now be archived unchanged;
  # it is not a successful settlement. Its process must still be absent.
  if identity bridge.pid "$here/mirror-input" || { [ -f bridge.pid ] && [ ! -f bridge.exit ]; };then flock -u 9;return 1;fi
  handoff_holds=0
  if [ -f bridge.pid ] && [ -f bridge.log ];then
   read -r old_bridge old_bridge_tick <bridge.pid
   final_mask=$(sed -n "s/^TOUCH_FINAL pid=$old_bridge mask=\\([0-9][0-9]*\\)$/\\1/p" bridge.log)
   case "$final_mask" in ''|*[!0-9]*) ;; *) [ "$final_mask" -le 511 ] && handoff_holds=$final_mask;; esac
  fi
  authorized && "$here/command-client" new-project-intent "$state/command.state" "$departed_pid" "$departed_tick" consume || { flock -u 9;return 1; }
  archive_generation || { flock -u 9;return 1; }
  app_child=;log_pid=
  if ! authorized || ! launch_app;then flock -u 9;[ -z "$app_child" ] || wait_receipt;return 1;fi
  flock -u 9
 done
}
case "$command" in
 status)
  if identity bridge.pid "$here/mirror-input";then echo "bridge_running pid=$pid";else echo bridge_not_running;fi
  adapter_scan || exit 1
  echo "adapter_pid=${adapter_pid:-none}; arming state is not queried"
  motor_choice=$("$here/mirror-input" --surface-status "$preferences") || { echo motor_follow_preference_invalid;exit 1; }
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
   # The status is reported here, not acted on: this branch is the idempotent
   # "already running" answer and exits 0. Under set -eu an unhealthy source
   # (session-status exits non-zero, and on some failures prints no JSON at
   # all) killed the script before the outcome line, so a dead command lane
   # looked like a silent exit 1. Say what the source did instead. The identity
   # and authorization gates on the two lines above are untouched, and
   # bridge_start still refuses anything but 0 or 3.
   source_code=0;status || source_code=$?
   case "$source_code" in 0|3) ;; *) echo "Session source status exit $source_code; the surface stays down until the source is healthy again.";; esac
   echo 'Existing owned session retained; no restart or signal.';exit 0
  fi
  identity bridge.pid "$here/mirror-input" && exit 1
  if same_boot && [ -f owner.pid ];then read -r owner_pid owner_tick <owner.pid;[ "$(start_tick "$owner_pid")" != "$owner_tick" ] || { echo 'Previous session owner still finishing.' >&2;exit 1; };fi
  if [ -e "$state/command.state" ] || [ -e "$state/volume.state" ] || [ -e "$snapshot" ];then
   [ -f session.id ] && [ -f "$snapshot.sha256" ] && sha256sum -c "$snapshot.sha256" >/dev/null || exit 1
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
  cp -p "$settings" "$snapshot";sha256sum "$snapshot" >"$snapshot.sha256"
  session_id="$$-$(start_tick $$)";printf '%s\n' "$session_id" >session.id
  printf '%s\n' "$boot_id" >session.boot-id
  printf '%s\n' "$here" >session.location
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
   [ -f session.id ] && [ -f "$snapshot" ] && ! mpc_running && ! systemctl is-active --quiet acvs || { echo 'Different MPC/service or no owned session locator; recovery refused.' >&2;exit 1; }
   clean=0
  fi
  if [ "$clean" -eq 1 ] && [ "$owned_mpc" -eq 1 ];then "$here/command-client" stop "$state/command.state" "$state/volume.state" || clean=0;fi
  if [ "$clean" -ne 1 ];then
   echo 'INCOMPLETE: graceful source closure was not certified; preserving evidence before exact-process recovery.' >&2
   # Text only, in the session folder, so it survives a power-off.
   summarize_state . incomplete-command-status.json stop-incomplete-status.txt incomplete-mirror-read.json || :
   [ ! -f bridge.log ] || tail -c "$cap_log" bridge.log >incomplete-bridge.log || :
  fi
  # A timed-out bridge must be gone before stock recovery, not merely signaled.
  if identity bridge.pid "$here/mirror-input";then
   clean=0;kill -KILL "$pid"
   n=0;while identity bridge.pid "$here/mirror-input";do n=$((n+1));[ "$n" -le 50 ] || exit 1;sleep .1;done
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
  sha256sum -c "$snapshot.sha256" >/dev/null
  cp -p "$snapshot" "$settings"
  systemctl start acvs
  echo "Session recovered; clean=$clean; fresh stopped settings restored; normal acvs started. Verify UI and PCM."
  [ "$clean" -eq 1 ];;
esac
