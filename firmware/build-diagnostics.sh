#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
out=${1:-"$repo/build/usb-diagnostics"}
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd -P)
docker run --rm --network none -v "$repo:/src:ro" -v "$out:/out" mpclearn-controls-build sh -ec '
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm /src/firmware/usb-diagnostics.c -o /out/usb-diagnostics
file /out/usb-diagnostics
arm-linux-gnueabihf-readelf -l /out/usb-diagnostics | sed -n "/interpreter/p"
arm-linux-gnueabihf-readelf -d /out/usb-diagnostics | sed -n "/NEEDED\|RPATH\|RUNPATH/p"
arm-linux-gnueabihf-readelf -A /out/usb-diagnostics
'
sha256sum "$out/usb-diagnostics" >"$out/usb-diagnostics.sha256"
