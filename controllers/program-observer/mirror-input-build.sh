#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ "$#" -ne 3 ];then echo 'mirror-input-build.sh /local/exact/MPC /native/owned/stage SECONDS|manual' >&2;exit 2;fi
# Reuse the unchanged command producer build; accepted prior packages remain
# in their root-owned frozen candidate directories before this regeneration.
./command-build.sh "$1" "$2" "$3"
mkdir -p package/mirror-input
cp package/command/* package/mirror-input/
docker run --rm --network none -v "$PWD/..:/controllers:ro" -v "$PWD:/src:ro" -v "$PWD/../stop-route.h:/stop-route.h:ro" -v "$PWD/package/mirror-input:/out" mpclearn-controls-build sh -ec '
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon -DMIRROR_INPUT -DX_TOUCH_BLOCKING_ENABLED=0 /src/mirror-motor.c -o /out/mirror-input -L/usr/lib/arm-linux-gnueabihf -lasound -lm
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon /src/mirror-input-test.c -o /out/mirror-input-test -L/usr/lib/arm-linux-gnueabihf -lasound -lm
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon -DVOLUME_MIRROR -DMIRROR_COMMAND -DCOMMAND_COMPONENT -no-pie -I/src/package/mirror-input /src/mirror-input-producer-test.c /src/component.S /src/gate.S /src/transport-queue.c -o /out/mirror-input-producer-test -pthread -ldl
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon /src/mirror-motor-test.c -o /out/mirror-motor-regression -L/usr/lib/arm-linux-gnueabihf -lasound -lm
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon -DVOLUME_MIRROR -DMIRROR_COMMAND -DCOMMAND_COMPONENT -no-pie -I/src/package/mirror-input /src/pad-component.c /src/component.S /src/gate.S /src/transport-queue.c -o /out/pad-component -pthread -ldl -lm
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon -DVOLUME_MIRROR -DMIRROR_COMMAND -DCOMMAND_COMPONENT -no-pie -I/src/package/mirror-input /src/midi-component.c /src/component.S /src/gate.S /src/transport-queue.c -o /out/midi-component -pthread -ldl -lm
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon -DVOLUME_MIRROR -DMIRROR_COMMAND -DCOMMAND_COMPONENT -no-pie -I/src/package/mirror-input /src/effects-component.c /src/component.S /src/gate.S /src/transport-queue.c -o /out/effects-component -pthread -ldl -lm
file /out/mirror-input /out/mirror-input-test /out/mirror-input-producer-test
'

docker run --rm --network none -v "$PWD/..:/controllers:ro" -v "$PWD/package/mirror-input:/out" mpclearn-controls-build sh -ec '
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror /controllers/adapter.c -o /out/mpclearn-controls -L/usr/lib/arm-linux-gnueabihf -lasound
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror /controllers/main-button.c -o /out/main-button -L/usr/lib/arm-linux-gnueabihf -lasound
'
cp mcu-session.sh package/mirror-input/
sed -e "s|^stage=.*|stage=${2%/}|" mcu.sh >package/mirror-input/mcu
chmod 700 package/mirror-input/mcu
(cd package/mirror-input && sha256sum command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button > session-package.sha256)
