#!/bin/sh
# The only device folders a build may bake into its package. A release runs from
# the flashed image; development uses the single override, which the image's
# provisioner retires when it no longer matches. Any other path would leave a
# folder on the device that nothing owns, so it is refused.
# Usage: device-location.sh PATH release|override
set -eu
[ "$#" -eq 2 ] || { echo 'device-location.sh PATH release|override' >&2;exit 2; }
case "$2:$1" in
 release:/usr/share/mpclearn/mcu|release:/data/mpclearn/dev|override:/data/mpclearn/dev) exit 0;;
 release:*) echo "Refusing device location $1: use /usr/share/mpclearn/mcu (release) or /data/mpclearn/dev (development override)." >&2;;
 override:*) echo "Refusing device location $1: diagnostic builds use only the development override /data/mpclearn/dev." >&2;;
 *) echo 'device-location.sh PATH release|override' >&2;;
esac
exit 2
