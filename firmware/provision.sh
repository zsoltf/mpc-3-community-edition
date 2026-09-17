#!/bin/sh
# Every boot, before the session unit: check the image payload once per
# revision and prepare the one folder the runtime writes, /data/mpclearn.
# It reads and writes nothing else and deletes nothing.
set -eu
payload=/usr/share/mpclearn/mcu
home=/data/mpclearn
# Set to <runtime commit>-v0_2_4 when the runtime is pinned for release.
revision=7fce0eb-v0_2_4
owned_dir(){ [ -d "$1" ] && [ ! -L "$1" ] && [ "$(stat -c %u "$1")" = 0 ] && [ "$(stat -c %a "$1")" = 700 ]; }
present(){ [ -e "$1" ] || [ -L "$1" ]; }
umask 077
[ "$(id -u)" = 0 ] || exit 2
[ -d /data ] && [ ! -L /data ] || exit 2
# Use the folder only if this project made it: root-owned and 0700.
if present "$home";then
 owned_dir "$home" || { echo "mpclearn: $home was not created by this project; nothing was changed. Rename it to use the controller." >&2;exit 1; }
else
 mkdir -m 700 "$home"
fi
for folder in session history;do
 ! present "$home/$folder" || owned_dir "$home/$folder" || { echo "mpclearn: $home/$folder is not a root-owned 0700 folder." >&2;exit 1; }
done
for folder in session history;do present "$home/$folder" || mkdir -m 700 "$home/$folder";done
recorded=
if [ -f "$home/image" ] && [ ! -L "$home/image" ];then recorded=$(cat "$home/image");fi
if [ "$recorded" != "$revision" ];then
 (cd "$payload" && sha256sum -c payload.sha256 >/dev/null)
 printf '%s\n' "$revision" >"$home/image.next"
 mv "$home/image.next" "$home/image"
fi
# A development override is used only while it matches this image; never removed here.
if present "$home/dev";then
 set -- $(sha256sum "$payload/payload.sha256")
 [ "$(cat "$home/dev/for-image" 2>/dev/null || :)" = "$1" ] || echo "mpclearn: $home/dev was installed for a different image; it is ignored." >&2
fi
echo "MCU $revision ready. Settings and projects preserved."
