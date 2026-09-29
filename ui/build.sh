#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p package
flags='--target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon -std=c++14 -O2 -Wall -Wextra -Werror -fPIC -fvisibility=hidden -fexceptions -funwind-tables -Iinclude -Iprivate -Iexamples -Iscreens'
clang++ $flags -c mpclearn-ui.cc -o package/mpclearn-ui.o
clang++ $flags -c examples/third-screen.cc -o package/third-screen.o
clang++ $flags -c examples/slider-screen.cc -o package/slider-screen.o
clang++ $flags -c examples/toolkit-example.cc -o package/toolkit-example.o
clang++ $flags -c screens/global-midi-learn-groups.cc -o package/global-midi-learn-groups.o
clang++ $flags -c screens/global-midi-learn-screen.cc -o package/global-midi-learn-screen.o
clang++ $flags -c tests/component.cc -o package/component.o
docker run --rm --network none -v "$PWD/package:/out" mpclearn-controls-build sh -ec '
  arm-linux-gnueabihf-ar rcs /out/libmpclearn-ui.a /out/mpclearn-ui.o
  arm-linux-gnueabihf-gcc /out/component.o /out/third-screen.o \
    /out/slider-screen.o /out/toolkit-example.o \
    /out/global-midi-learn-groups.o \
    /out/global-midi-learn-screen.o /out/libmpclearn-ui.a \
    /usr/arm-linux-gnueabihf/lib/libstdc++.so.6 -o /out/ui-component -Wl,-z,now
  file /out/libmpclearn-ui.a /out/ui-component
  arm-linux-gnueabihf-readelf -d /out/ui-component > /out/ui-component-dynamic.txt
  sed -n "/NEEDED\|RPATH\|RUNPATH/p" /out/ui-component-dynamic.txt
  arm-linux-gnueabihf-readelf -u /out/ui-component > /out/ui-component-unwind.txt
  arm-linux-gnueabihf-nm -g --defined-only /out/libmpclearn-ui.a > /out/libmpclearn-ui-symbols.txt
'
(cd package && sha256sum libmpclearn-ui.a third-screen.o slider-screen.o toolkit-example.o \
  global-midi-learn-groups.o global-midi-learn-screen.o ui-component > manifest.sha256)
cat package/manifest.sha256
