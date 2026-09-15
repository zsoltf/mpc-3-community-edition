#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ "$#" -lt 2 ] || [ "$#" -gt 3 ];then echo 'command-build.sh /local/exact/MPC /native/owned/stage [60..900 seconds|manual]' >&2;exit 2;fi
case "$2" in /*) ;; *) exit 2;; esac
case "$2" in *[!A-Za-z0-9_./-]*|*/../*|*/..|*//*) exit 2;; esac
duration=${3:-360};[ "$duration" != manual ] || duration=0;case "$duration" in *[!0-9]*|'') exit 2;; esac
[ "$duration" -eq 0 ] || { [ "$duration" -ge 60 ] && [ "$duration" -le 900 ]; } || exit 2
mkdir -p package/command
python3 -B model-prepare.py "$1" package/command/pinned.h --command
state_dir=${2%/}
if [ "$duration" -eq 0 ];then
 case "$state_dir" in /data/*) name=${state_dir#/data/};; *) exit 2;; esac
 case "$name" in ''|*/*|.|..) exit 2;; esac
 state_dir=/run/mpclearn-$name
fi
printf '#define OBSERVER_LOG "%s/volume.state"\n#define COMMAND_PATH "%s/command.state"\n#define OBSERVER_LIBRARY "%s/command-observer.so"\n#define OBSERVER_ARM_PATH "%s/unused-ARM"\n#define WINDOW_SECONDS %su\n' "$state_dir" "$state_dir" "${2%/}" "${2%/}" "$duration" > package/command/config.h
docker run --rm --network none -v "$PWD:/src:ro" -v "$PWD/package/command:/out" mpclearn-controls-build sh -ec '
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon -DVOLUME_MIRROR -DMIRROR_COMMAND -fPIC -fvisibility=hidden -I/src/package/command /src/observer.c /src/command-capture.c /src/patch.c /src/gate.S /src/transport-queue.c -shared -o /out/command-observer.so -pthread -ldl -Wl,-z,now
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon /src/command-client.c -o /out/command-client
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon /src/project-client-test.c -o /out/project-client-test
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon /src/effects-client-test.c -o /out/effects-client-test
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon -DVOLUME_MIRROR -DMIRROR_COMMAND -DCOMMAND_COMPONENT -no-pie -I/src/package/command /src/command-component.c /src/component.S /src/gate.S /src/transport-queue.c -o /out/command-component -pthread -ldl
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon /src/mirror-read.c -o /out/mirror-read
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon -DVOLUME_MIRROR -DMIRROR_COMMAND -I/src/package/command /src/processor-component.c -o /out/processor-component -pthread -lm
file /out/command-observer.so /out/command-client /out/command-component
arm-linux-gnueabihf-readelf -l /out/command-observer.so | sed -n "/TLS/p"
'
