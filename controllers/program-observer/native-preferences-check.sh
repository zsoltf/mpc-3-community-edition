#!/bin/sh
# Exercise production native UI construction/callback/teardown with host substitutes.
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d /tmp/mpclearn-native-ui-check.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
clang++ --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -DMPCLEARN_NATIVE_UI_EXAMPLE=0 -c native-preferences-test.cc -o "$tmp/native-preferences-test.o"
clang++ --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -DMPCLEARN_NATIVE_UI_EXAMPLE=1 -c native-preferences-test.cc -o "$tmp/native-preferences-example-test.o"
clang++ --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -c ../../ui/mpclearn-ui.cc -o "$tmp/mpclearn-ui.o"
clang++ --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -c ../../ui/screens/global-midi-learn-groups.cc -o "$tmp/global-midi-learn-groups.o"
clang++ --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -c ../../ui/screens/global-midi-learn-screen.cc -o "$tmp/global-midi-learn-screen.o"
clang++ --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -c ../../ui/examples/toolkit-example.cc -o "$tmp/toolkit-example.o"
docker run --rm --network none -v "$tmp:/out" mpclearn-controls-build sh -ec '
arm-linux-gnueabihf-gcc /out/native-preferences-test.o /out/mpclearn-ui.o /out/global-midi-learn-groups.o /out/global-midi-learn-screen.o /out/toolkit-example.o /usr/arm-linux-gnueabihf/lib/libstdc++.so.6 -o /out/native-preferences-test
arm-linux-gnueabihf-gcc /out/native-preferences-example-test.o /out/mpclearn-ui.o /out/global-midi-learn-groups.o /out/global-midi-learn-screen.o /out/toolkit-example.o /usr/arm-linux-gnueabihf/lib/libstdc++.so.6 -o /out/native-preferences-example-test
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /out/native-preferences-test
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /out/native-preferences-example-test
'
