#!/bin/sh
# Obtain the pinned reference from the owner's local clone; no source relicensing.
set -eu
[ "$#" = 1 ] || { echo 'prepare-upstream.sh /path/to/MPC-LiveXplore-clone' >&2;exit 2; }
reference=$(CDPATH= cd -- "$1" && pwd)
cd "$(dirname "$0")/.."
mkdir -p upstream
revision=9be3e63bf03d3467cf06478b48671894dcc634f4
for file in mpcimg2.c Makefile readme.md;do
 git -C "$reference" show "$revision:imgmaker/$file" > "upstream/$file"
done
printf '%s\n' "$revision" > upstream/COMMIT
