#!/bin/sh
# One boot admission. The flashed image owns the runtime; the single development
# override replaces it only while it verifies and matches this image. Otherwise
# the override is ignored with a message; nothing is removed.
set -eu
image=/usr/share/mpclearn/mcu
override=/data/mpclearn/dev
session=/data/mpclearn/session
[ "$(id -u)" -eq 0 ] || exit 2
owned_dir(){ [ -d "$1" ] && [ ! -L "$1" ] && [ "$(stat -c %u "$1")" = 0 ] && [ "$(stat -c %a "$1")" = 700 ]; }
image_digest(){ set -- $(sha256sum "$image/payload.sha256");printf '%s\n' "$1"; }
ignored(){ echo "Development override $override ignored: $1; using the image runtime." >&2; }
select_location(){
 location=$image
 [ -e "$override" ] || [ -L "$override" ] || return 0
 owned_dir "$override" || { ignored 'not a root-owned 0700 folder';return 0; }
 (cd "$override" && sha256sum -c session-package.sha256 >/dev/null 2>&1) || { ignored 'package does not verify';return 0; }
 [ -f "$override/for-image" ] && [ ! -L "$override/for-image" ] && [ "$(cat "$override/for-image")" = "$(image_digest)" ] || { ignored 'installed for a different image';return 0; }
 location=$override
}
# A running session belongs to the package that started it.
session_location(){
 location=
 [ ! -f "$session/session.location" ] || IFS= read -r location <"$session/session.location" || :
 case "$location" in "$image"|"$override") ;; *) select_location;; esac
}
case "${1:-}" in
 start)
  [ "$#" -eq 1 ] || exit 2
  select_location
  # No retry and no second process owner. Children remain in this unit's cgroup.
  exec "$location/mcu" start;;
 stop)
  # Explicit disable only: stop the boot-owned session through its own package.
  [ "$#" -eq 1 ] || exit 2
  session_location || exit 2
  exec "$location/mcu" stop;;
 recover)
  [ "$#" -eq 1 ] || exit 2
  # ExecStopPost runs after systemd has stopped all startup children. Shutdown
  # never calls mcu stop: that operation intentionally starts the stock app.
  [ "${SERVICE_RESULT:-success}" != success ] || exit 0
  [ "$(systemctl is-system-running 2>/dev/null || :)" != stopping ] || exit 0
  if [ -f "$session/session.id" ] && session_location;then "$location/mcu" stop && exit 0;fi
  # A failure before owned admission needs no fabricated session cleanup.
  for exe in /proc/[0-9]*/exe;do
   case "$(readlink "$exe" 2>/dev/null || :)" in */MPC|*/MPC\ \(deleted\))
    systemctl is-active --quiet acvs.service;exit $?;;
   esac
  done
  systemctl start acvs.service;;
 *) echo 'mcu-boot.sh start|stop|recover' >&2;exit 2;;
esac
