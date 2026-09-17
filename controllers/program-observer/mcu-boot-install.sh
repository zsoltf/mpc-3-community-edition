#!/bin/sh
# Root operations: autostart on/off and the development override. Enabling does
# not restart the live app. Writes only inside /data/mpclearn.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
image=/usr/share/mpclearn/mcu
home=/data/mpclearn
override=$home/dev
[ "$(id -u)" -eq 0 ] || exit 2
owned_dir(){ [ -d "$1" ] && [ ! -L "$1" ] && [ "$(stat -c %u "$1")" = 0 ] && [ "$(stat -c %a "$1")" = 700 ]; }
owned_dir "$home" || { echo "$home is not a root-owned 0700 folder; refusing." >&2;exit 2; }
executables='command-client mirror-input mirror-read mcu-session.sh mcu mpclearn-controls main-button'
private='command-observer.so config.h session-package.sha256'
unit_state(){ systemctl is-active mpclearn-boot.service || :; }
session_idle(){
 case "$(unit_state)" in active|activating|deactivating) echo 'Stop mpclearn-boot.service (or disable autostart) first.' >&2;return 1;; esac
 s=$home/session
 if [ -f "$s/session.id" ] && [ ! -f "$s/session.revoked" ] && [ -f "$s/session.boot-id" ] && [ "$(cat "$s/session.boot-id")" = "$(cat /proc/sys/kernel/random/boot_id)" ];then
  echo 'Run mcu stop first.' >&2;return 1
 fi
}
# Explicit developer action only, inside /data/mpclearn.
retire(){ rm -rf "$1";! [ -e "$1" ]; }
case "${1:-}" in
 enable)
  [ "$#" -eq 1 ] || exit 2
  rm -f "$home/disabled"
  echo 'Autostart enabled from the next boot. Current app unchanged.';;
 disable)
  [ "$#" -eq 1 ] || exit 2
  state=$(unit_state)
  case "$state" in activating|deactivating) echo 'Boot transition still active; wait for its bounded completion.' >&2;exit 1;; esac
  : >"$home/disabled"
  recovery=0
  if [ "$state" = active ];then "$here/mcu-boot.sh" stop || recovery=$?;fi
  systemctl stop mpclearn-boot.service
  # Incomplete receipt: retry only owned cleanup once the unit's children are gone.
  if [ "$recovery" -ne 0 ];then SERVICE_RESULT=exit-code "$here/mcu-boot.sh" recover;fi
  echo 'Autostart disabled. Stock acvs retained.';;
 override)
  [ "$#" -eq 2 ] || exit 2
  session_idle || exit 1
  retire "$override.next";retire "$override.old"
  if [ "$2" = clear ];then retire "$override";echo 'Override removed.';exit 0;fi
  source=$2
  [ -d "$source" ] && [ ! -L "$source" ] || { echo "Not a package folder: $source" >&2;exit 2; }
  for name in $executables $private;do [ -f "$source/$name" ] && [ ! -L "$source/$name" ] || { echo "Missing package file: $name" >&2;exit 1; };done
  (cd "$source" && sha256sum -c session-package.sha256 >/dev/null) || { echo 'Package manifest does not verify.' >&2;exit 1; }
  for name in $executables command-observer.so config.h;do
   awk -v n="$name" '$2==n{f=1}END{exit !f}' "$source/session-package.sha256" || { echo "Manifest does not cover $name." >&2;exit 1; }
  done
  for line in '#define WINDOW_SECONDS 0u' "#define OBSERVER_LIBRARY \"$override/command-observer.so\"" '#define OBSERVER_LOG "/run/mpclearn/state/volume.state"' '#define COMMAND_PATH "/run/mpclearn/state/command.state"';do
   grep -Fqx "$line" "$source/config.h" || { echo "Package was not built for $override (config.h lacks: $line)." >&2;exit 1; }
  done
  umask 077
  mkdir -m 700 "$override.next"
  for name in $executables $private;do cp "$source/$name" "$override.next/$name";done
  # Shipped owner/modes; a copied tar keeps the build machine's.
  chown 0:0 "$override.next" "$override.next"/*
  for name in $executables;do chmod 700 "$override.next/$name";done
  for name in $private;do chmod 600 "$override.next/$name";done
  (cd "$override.next" && sha256sum -c session-package.sha256 >/dev/null)
  set -- $(sha256sum "$image/payload.sha256")
  printf '%s\n' "$1" >"$override.next/for-image"
  [ ! -e "$override" ] || mv "$override" "$override.old"
  mv "$override.next" "$override"
  retire "$override.old"
  echo "Override installed for this image. Reboot, or run $override/mcu start.";;
 *) echo 'mcu-boot-install.sh enable|disable|override PACKAGE_DIR|override clear' >&2;exit 2;;
esac
