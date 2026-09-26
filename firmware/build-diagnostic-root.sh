#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
[ "$#" -le 1 ] || { echo "usage: $0 [OUTPUT_ROOT]" >&2; exit 2; }
release_root=${1:-$repo/build/v0_2_6}
out=$release_root/diagnostics
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd -P)
base=$release_root/rootfs.no-ssh.ext
original=$release_root/rootfs.original.ext
collector=$out/usb-diagnostics
sh "$repo/firmware/build-diagnostics.sh" "$out"
test ! -e "$out/rootfs.diagnostics.ext"
cp "$base" "$out/rootfs.diagnostics.ext"
base_sha=$(sha256sum "$base" | awk '{print $1}')
rel=${release_root#"$repo"/}
docker run --rm --network none -v "$repo:/work" -w /work mpclearn-build:local sh -ec '
release_root=$1
python3 firmware/diagnostics-patch.py "$release_root/diagnostics/rootfs.diagnostics.ext" "$release_root/diagnostics/usb-diagnostics" "$release_root/diagnostics/diagnostic-files.json" "$2"
build/fs_inventory "$release_root/rootfs.no-ssh.ext" > "$release_root/diagnostics/base.inventory"
build/fs_inventory "$release_root/diagnostics/rootfs.diagnostics.ext" > "$release_root/diagnostics/diagnostic.inventory"
python3 firmware/diagnostics-verify.py "$release_root/diagnostics/base.inventory" "$release_root/diagnostics/diagnostic.inventory" "$release_root/diagnostics/diagnostic-files.json" "$release_root/diagnostics/rootfs.diagnostics.ext" firmware/mpclearn-diagnostics.service
e2fsck -fn "$release_root/diagnostics/rootfs.diagnostics.ext"
' sh "$rel" "$base_sha"
test ! -e "$out/recipe"
python3 "$repo/firmware/make-patch.py" --no-ssh "$original" "$out/rootfs.diagnostics.ext" /dev/null "$out/recipe"
