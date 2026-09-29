#!/bin/sh
# Build the native Preferences runtime for the image or the one development override.
# This only creates a local package; it never installs or contacts the MPC.
set -eu
cd "$(dirname "$0")"
if [ "$#" -ne 3 ];then echo 'native-preferences-build.sh /local/current/CE/MPC /local/stock/MPC /usr/share/mpclearn/mcu|/data/mpclearn/dev' >&2;exit 2;fi
current=$1;stock=$2;location=$3;out=package/native-preferences
./device-location.sh "$location" release
./mirror-input-build.sh "$current" "$location" manual
rm -rf "$out";mkdir -p "$out/runtime" "$out/tools"
for name in command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button;do cp "package/mirror-input/$name" "$out/runtime/$name";done
python3 -B native-preferences-prepare.py "$current" "$stock" "$out/native-preferences-pinned.h"
printf '#define NATIVE_PREFERENCES_STATE "/run/mpclearn/state/native-preferences.state"\n' >> "$out/runtime/config.h"
cp "$out/runtime/config.h" "$out/config.h"
clang++ --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon -std=c++14 -O2 -Wall -Wextra -Werror -fPIC -fvisibility=hidden -fexceptions -funwind-tables -c native-preferences.cc -o "$out/native-preferences.o"
docker run --rm --network none -v "$PWD:/src:ro" -v "$PWD/$out:/out" mpclearn-controls-build sh -ec '
flags="-O2 -Wall -Wextra -Werror -marm -mfpu=neon -DVOLUME_MIRROR -DMIRROR_COMMAND -DNATIVE_PREFERENCES -fPIC -fvisibility=hidden -pthread -I/out -I/src/package/mirror-input -I/src"
arm-linux-gnueabihf-gcc -std=c11 $flags -c /src/observer.c -o /tmp/observer.o
arm-linux-gnueabihf-gcc -std=c11 -D_GNU_SOURCE $flags -c /src/command-capture.c -o /tmp/command-capture.o
arm-linux-gnueabihf-gcc -std=c11 $flags -c /src/patch.c -o /tmp/patch.o
arm-linux-gnueabihf-gcc $flags -c /src/gate.S -o /tmp/gate.o
arm-linux-gnueabihf-gcc -std=c11 $flags -c /src/transport-queue.c -o /tmp/transport-queue.o
arm-linux-gnueabihf-gcc -std=c11 $flags -c /src/native-preferences-patch.c -o /tmp/native-preferences-patch.o
arm-linux-gnueabihf-gcc -shared /tmp/observer.o /tmp/command-capture.o /tmp/patch.o /tmp/gate.o /tmp/transport-queue.o /tmp/native-preferences-patch.o /out/native-preferences.o /usr/arm-linux-gnueabihf/lib/libstdc++.so.6 -o /out/runtime/command-observer.so -pthread -ldl -Wl,-z,now
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon -I/src /src/native-preferences-read.c -o /out/tools/native-preferences-read
arm-linux-gnueabihf-readelf -u /out/runtime/command-observer.so > /out/tools/native-preferences-unwind.txt
arm-linux-gnueabihf-readelf -d /out/runtime/command-observer.so > /out/tools/command-observer-dynamic.txt
arm-linux-gnueabihf-nm -S /out/runtime/command-observer.so | grep native_preferences > /out/tools/native-preferences-symbols.txt
file /out/runtime/command-observer.so /out/tools/native-preferences-read
'
rm -f "$out/native-preferences.o"
(grep -F '<native_preferences_hook>:' "$out/tools/native-preferences-unwind.txt" &&
 grep -F '<native_preferences_tab_deleting_destructor>:' "$out/tools/native-preferences-unwind.txt") >/dev/null
(cd "$out/runtime" && sha256sum command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button > session-package.sha256)
(cd "$out/runtime" && sha256sum -c session-package.sha256 >/dev/null)
grep -Fqx '#define WINDOW_SECONDS 0u' "$out/runtime/config.h"
grep -Fqx "#define OBSERVER_LIBRARY \"$location/command-observer.so\"" "$out/runtime/config.h"
grep -Fqx '#define NATIVE_PREFERENCES_STATE "/run/mpclearn/state/native-preferences.state"' "$out/runtime/config.h"
grep -Fq ' native_preferences_install' "$out/tools/native-preferences-symbols.txt"
echo "Native Preferences package for $location: $PWD/$out/runtime; state reader: $PWD/$out/tools/native-preferences-read"
