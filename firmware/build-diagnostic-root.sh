#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
[ "$#" -eq 0 ] || { echo "usage: $0 (writes build/v0_2_5/diagnostics)" >&2; exit 2; }
out=$repo/build/v0_2_5/diagnostics
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd -P)
base=$repo/build/v0_2_5/rootfs.no-ssh.ext
original=$repo/build/v0_2_5/rootfs.original.ext
collector=$out/usb-diagnostics
sh "$repo/firmware/build-diagnostics.sh" "$out"
test ! -e "$out/rootfs.diagnostics.ext"
cp "$base" "$out/rootfs.diagnostics.ext"
base_sha=$(sha256sum "$base" | awk '{print $1}')
docker run --rm --network none -v "$repo:/work" -w /work mpclearn-build:local sh -ec '
python3 firmware/diagnostics-patch.py build/v0_2_5/diagnostics/rootfs.diagnostics.ext build/v0_2_5/diagnostics/usb-diagnostics build/v0_2_5/diagnostics/diagnostic-files.json "$1"
build/fs_inventory build/v0_2_5/rootfs.no-ssh.ext > build/v0_2_5/diagnostics/base.inventory
build/fs_inventory build/v0_2_5/diagnostics/rootfs.diagnostics.ext > build/v0_2_5/diagnostics/diagnostic.inventory
python3 firmware/diagnostics-verify.py build/v0_2_5/diagnostics/base.inventory build/v0_2_5/diagnostics/diagnostic.inventory build/v0_2_5/diagnostics/diagnostic-files.json build/v0_2_5/diagnostics/rootfs.diagnostics.ext firmware/mpclearn-diagnostics.service
e2fsck -fn build/v0_2_5/diagnostics/rootfs.diagnostics.ext
' sh "$base_sha"
test ! -e "$out/recipe"
python3 "$repo/firmware/make-patch.py" --no-ssh "$original" "$out/rootfs.diagnostics.ext" /dev/null "$out/recipe"
