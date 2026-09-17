#!/bin/sh
# One-time payload installation. The existing MCU session owner owns all runtime.
set -eu
payload=/usr/share/mpclearn/mcu
state=/data/mpclearn-image
stage=/data/mpclearn-model.v0_2_2
revision=1ffe005-v0_2_2
# Known prior releases upgrade into the separate new stage; their stages stay.
known_revisions='6d70695-mcu-direct-r2 ce4ccce-mcu-perf-r3 9000390-mcu-perf-r4 4d060dc-mouse-r5 81f3086-v0_2_0'
known_stages='/data/mpclearn-model.mcu-direct-r2 /data/mpclearn-model.mcu-perf-r3 /data/mpclearn-model.mcu-perf-r4 /data/mpclearn-model.mouse-r5 /data/mpclearn-model.v0_2_0'
known(){ for item in $2;do [ "$1" != "$item" ] || return 0;done;return 1; }
[ "$(id -u)" = 0 ] || exit 2
for path in /data /etc;do [ -d "$path" ] && [ ! -L "$path" ] || exit 2;done
[ ! -L "$state" ] || exit 2
installed_revision(){
 installed=
 if [ -e "$state/installed" ] || [ -L "$state/installed" ];then
  [ -f "$state/installed" ] && [ ! -L "$state/installed" ] && [ "$(stat -c %u "$state/installed")" = 0 ] || return 2
  installed=$(cat "$state/installed")
  [ -n "$installed" ] || { echo 'Empty image revision; refusing installation.' >&2;return 1; }
 fi
 case "$installed" in ""|"$revision") ;; *) known "$installed" "$known_revisions" || { echo 'Unknown image revision; explicit migration required.' >&2;return 1; };; esac
}
installed_revision
[ "$installed" != "$revision" ] || exit 0

(cd "$payload" && sha256sum -c payload.sha256)
umask 077
mkdir -p "$state"
chown 0:0 "$state";chmod 700 "$state"
exec 9>"$state/install.lock"
flock 9
# Another manual invocation may have completed while we waited.
installed_revision
[ "$installed" != "$revision" ] || exit 0
select_stage=1
# Never replace a custom selection, including installations without our marker.
if [ -e /etc/mpclearn-boot-stage ] || [ -L /etc/mpclearn-boot-stage ];then
 [ -f /etc/mpclearn-boot-stage ] && [ ! -L /etc/mpclearn-boot-stage ] || exit 2
 selected=$(cat /etc/mpclearn-boot-stage)
 [ "$selected" = "$stage" ] || known "$selected" "$known_stages" || { echo 'Custom stage selected; refusing to change it.' >&2;exit 1; }
elif [ -n "$installed" ];then
 # A known prior installation with no selector was deliberately disabled.
 select_stage=0
fi
temporary=$state/staging
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
[ ! -e "$temporary" ] || rm -rf "$temporary"
if [ -e "$stage" ] || [ -L "$stage" ];then
 [ -d "$stage" ] && [ ! -L "$stage" ] && [ "$(stat -c %u "$stage")" = 0 ] && [ "$(stat -c %a "$stage")" = 700 ] || exit 2
 cmp "$payload/session-package.sha256" "$stage/session-package.sha256"
 (cd "$stage" && sha256sum -c session-package.sha256)
else
 mkdir -m 700 "$temporary"
 for name in command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button session-package.sha256;do
  cp "$payload/$name" "$temporary/$name"
 done
 chown -R 0:0 "$temporary"
 chmod 700 "$temporary/command-client" "$temporary/mirror-input" "$temporary/mirror-read" "$temporary/mcu-session.sh" "$temporary/mcu" "$temporary/mpclearn-controls" "$temporary/main-button"
 (cd "$temporary" && sha256sum -c session-package.sha256)
 mv "$temporary" "$stage"
fi
boot=/data/mpclearn-boot
[ ! -L "$boot" ] || exit 2
mkdir -p "$boot";chown 0:0 "$boot";chmod 700 "$boot"
for name in mcu-boot.sh mcu-boot-install.sh mcu-session.sh mpclearn-boot.service;do
 # A known prior release's helpers are replaced by this release's versions;
 # early boot provisioning runs before any session owner. Helpers that differ
 # on an installation without our marker are custom and refused unchanged.
 if [ -e "$boot/$name" ] || [ -L "$boot/$name" ];then
  [ ! -L "$boot/$name" ] || exit 2
  if ! cmp -s "$payload/$name" "$boot/$name";then
   [ -n "$installed" ] || { echo "Conflicting boot helper $name; refusing installation." >&2;exit 1; }
   cp "$payload/$name" "$boot/$name.next";chown 0:0 "$boot/$name.next"
   chmod 700 "$boot/$name.next";mv "$boot/$name.next" "$boot/$name"
  fi
 else
  cp "$payload/$name" "$boot/$name.next";chown 0:0 "$boot/$name.next"
  chmod 700 "$boot/$name.next";mv "$boot/$name.next" "$boot/$name"
 fi
done
if [ -e /etc/mpclearn-boot-stage ] || [ -L /etc/mpclearn-boot-stage ];then
 [ -f /etc/mpclearn-boot-stage ] && [ ! -L /etc/mpclearn-boot-stage ] || exit 2
 # Retain the first selector even if a prior attempt already switched stages.
 if [ ! -e "$state/previous-stage" ];then
  cp -p /etc/mpclearn-boot-stage "$state/previous-stage.next"
  mv "$state/previous-stage.next" "$state/previous-stage"
 fi
fi
if [ "$select_stage" = 1 ];then
printf '%s\n' "$stage" > /etc/mpclearn-boot-stage.next
chown 0:0 /etc/mpclearn-boot-stage.next;chmod 600 /etc/mpclearn-boot-stage.next
mv /etc/mpclearn-boot-stage.next /etc/mpclearn-boot-stage
fi
printf '%s\n' "$revision" > "$state/installed.next"
mv "$state/installed.next" "$state/installed"
echo "Installed MCU $revision. Settings and projects preserved."
