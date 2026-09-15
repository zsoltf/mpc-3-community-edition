#!/bin/sh
# One boot admission; the selected matched package owns the complete session.
set -eu
selection=/etc/mpclearn-boot-stage
[ "$(id -u)" -eq 0 ] || exit 2
if [ "${1:-}" = recover ] && [ "$#" -eq 1 ] && [ "${SERVICE_RESULT:-success}" = success ] && [ ! -e "$selection" ];then exit 0;fi
if [ "${1:-}" = recover ] && [ "$#" -eq 2 ];then
 # Explicit disable has already validated and removed the boot selector.
 stage=$2
else
 [ "$#" -eq 1 ] || exit 2
 [ -f "$selection" ] && [ ! -L "$selection" ] && [ "$(stat -c %u "$selection")" = 0 ] && [ "$(stat -c %a "$selection")" = 600 ] || exit 2
 IFS= read -r stage <"$selection"
fi
case "$stage" in /data/mpclearn-model.*) ;; *) exit 2;; esac
case "$stage" in *[!a-zA-Z0-9/._-]*) exit 2;; esac
[ -d "$stage" ] && [ ! -L "$stage" ] && [ "$(stat -c %u "$stage")" = 0 ] && [ "$(stat -c %a "$stage")" = 700 ] || exit 2
case "${1:-}" in
 start)
  # No retry and no second process owner. Children remain in this unit's cgroup.
  exec "$stage/mcu" start;;
 recover)
  # ExecStopPost runs after systemd has stopped all startup children. Shutdown
  # never calls mcu stop: that operation intentionally starts the stock app.
  [ "${SERVICE_RESULT:-success}" != success ] || exit 0
  [ "$(systemctl is-system-running 2>/dev/null || :)" != stopping ] || exit 0
  if [ -f "$stage/session.id" ];then "$stage/mcu" stop && exit 0;fi
  # A failure before owned admission needs no fabricated session cleanup.
  for exe in /proc/[0-9]*/exe;do
   case "$(readlink "$exe" 2>/dev/null || :)" in */MPC|*/MPC\ \(deleted\))
    systemctl is-active --quiet acvs.service;exit $?;;
   esac
  done
  systemctl start acvs.service;;
 *) echo 'mcu-boot.sh start|recover [MATCHED_STAGE]' >&2;exit 2;;
esac
