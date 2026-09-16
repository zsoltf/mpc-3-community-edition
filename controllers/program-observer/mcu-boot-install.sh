#!/bin/sh
# Explicit root enable/disable operation; enabling does not restart the live app.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
[ "$(id -u)" -eq 0 ] || exit 2
boot_copy(){
 boot_copy_tmp="$2.tmp.$$"
 trap 'rm -f "$boot_copy_tmp"' EXIT HUP INT TERM
 cp "$1" "$boot_copy_tmp"
 chown 0:0 "$boot_copy_tmp"
 chmod "$3" "$boot_copy_tmp"
 mv -f "$boot_copy_tmp" "$2"
}
case "${1:-}" in
 enable)
  [ "$#" -le 3 ] || exit 2
  unit_directory=${3:-/usr/lib/systemd/system}
  case "$unit_directory" in /usr/lib/systemd/system|/etc/systemd/system) ;; *) echo 'Unsupported systemd unit directory.' >&2;exit 2;; esac
  # The caller owns any required root remount. Never remount from this helper.
  [ -d "$unit_directory" ] && [ -w "$unit_directory" ] || { echo "Unit directory is not writable: $unit_directory" >&2;exit 1; }
  state=$(systemctl is-active mpclearn-boot.service || :)
  case "$state" in active|activating|deactivating) echo 'Disable the existing boot session before changing its selection.' >&2;exit 1;; esac
  stage=${2:-/data/mpclearn-model.mouse-r5}
  case "$stage" in /data/mpclearn-model.*) ;; *) exit 2;; esac
  case "$stage" in *[!a-zA-Z0-9/._-]*) exit 2;; esac
  [ -d "$stage" ] && [ ! -L "$stage" ] && [ "$(stat -c %u "$stage")" = 0 ] && [ "$(stat -c %a "$stage")" = 700 ] || exit 2
  (cd "$stage" && sha256sum -c session-package.sha256 && grep -Fqx '#define WINDOW_SECONDS 0u' config.h && grep -Fqx "#define OBSERVER_LIBRARY \"$stage/command-observer.so\"" config.h) || exit 1
  # Migration of a running pre-boot-integration package must be explicit: its
  # historical PID files alone cannot identify their originating boot.
  if [ -f "$stage/session.id" ] && [ ! -f "$stage/session.boot-id" ];then
   echo 'Selected legacy session needs a verified current-boot stamp before enabling.' >&2;exit 1
  fi
  cmp -s "$here/mcu-session.sh" "$stage/mcu-session.sh" || { echo 'Install the matching boot-aware session helper and refresh its package manifest first.' >&2;exit 1; }
  umask 077
  mkdir -p /data/mpclearn-boot
  chown 0:0 /data/mpclearn-boot
  chmod 700 /data/mpclearn-boot
  if [ "$here" != /data/mpclearn-boot ];then
   for file in mcu-boot.sh mcu-boot-install.sh mcu-session.sh;do boot_copy "$here/$file" "/data/mpclearn-boot/$file" 700;done
   boot_copy "$here/mpclearn-boot.service" /data/mpclearn-boot/mpclearn-boot.service 600
  fi
  boot_copy "$here/mpclearn-boot.service" "$unit_directory/mpclearn-boot.service" 644
  mkdir -p "$unit_directory/multi-user.target.wants"
  chown 0:0 "$unit_directory/multi-user.target.wants"
  chmod 755 "$unit_directory/multi-user.target.wants"
  ln -sfn ../mpclearn-boot.service "$unit_directory/multi-user.target.wants/mpclearn-boot.service"
  if [ "$unit_directory" != /etc/systemd/system ];then
   # Retire this installer's old overlay-only unit, which otherwise shadows
   # the early-visible version after /etc mounts.
   rm -f /etc/systemd/system/multi-user.target.wants/mpclearn-boot.service /etc/systemd/system/mpclearn-boot.service
  fi
  printf '%s\n' "$stage" >/etc/mpclearn-boot-stage.tmp
  chown 0:0 /etc/mpclearn-boot-stage.tmp
  chmod 600 /etc/mpclearn-boot-stage.tmp
  mv /etc/mpclearn-boot-stage.tmp /etc/mpclearn-boot-stage
  systemctl daemon-reload
  echo "Enabled next-boot controller session: $stage; unit/link in $unit_directory. Current app unchanged.";;
 disable)
  [ "$#" -eq 1 ] || exit 2
  state=$(systemctl is-active mpclearn-boot.service || :)
  case "$state" in activating|deactivating) echo 'Boot transition still active; wait for its bounded completion.' >&2;exit 1;; esac
  stage=
  if [ -f /etc/mpclearn-boot-stage ];then
   SERVICE_RESULT=success /data/mpclearn-boot/mcu-boot.sh recover
   IFS= read -r stage </etc/mpclearn-boot-stage
   rm /etc/mpclearn-boot-stage
  fi
  recovery=0
  if [ "$state" = active ];then
   [ -n "$stage" ] || { echo 'Active boot session has no validated selector; stop refused.' >&2;exit 1; }
   "$stage/mcu" stop || recovery=$?
  fi
  # A vendor-directory link can remain on the read-only root. The selector is
  # the unit's native ConditionPathExists gate; removing it disables boot
  # execution without a hidden root remount.
  systemctl stop mpclearn-boot.service
  # An incomplete receipt is not a failed stock restoration. Finish disabling
  # after the unit's children are gone, and retry only owned cleanup, not start.
  if [ "$recovery" -ne 0 ];then SERVICE_RESULT=exit-code /data/mpclearn-boot/mcu-boot.sh recover "$stage";fi
  echo 'Boot selector removed; early unit/link remain dormant. Stock acvs retained.';;
 *) echo 'mcu-boot-install.sh enable [MATCHED_STAGE] [UNIT_DIRECTORY]|disable' >&2;exit 2;;
esac
