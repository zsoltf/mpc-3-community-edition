#!/bin/sh
set -eu
cd "$(dirname "$0")"
fixture_dir=$(mktemp -d /tmp/mpclearn-command-check.XXXXXX)
trap 'rm -rf "$fixture_dir"' EXIT HUP INT TERM
docker run --rm --network none --memory 256m --pids-limit 32 -v "$PWD:/src:ro" -v "$fixture_dir:/out" mpclearn-controls-build sh -ec '
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/command/processor-component /out/processor.state
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/command/project-client-test /out/intent.state
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/command/effects-client-test /out/effects-command.state /out/effects-mirror.state /out/effects-request.bin
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/command/command-component /out/volume.state /out/command.state
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/command/command-client status /out/command.state > /out/command.json
library=$(sed -n '\''s/^#define OBSERVER_LIBRARY "\(.*\)"$/\1/p'\'' /src/package/command/config.h)
mkdir -p "$(dirname "$library")"
cp /src/package/command/command-observer.so "$library"
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -I/src/package/command /src/startup-test.c -o /tmp/startup-test
env LD_PRELOAD="$library" /usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /tmp/startup-test
'
python3 - "$fixture_dir/command.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
assert j['error']==0 and not j['trace_error'] and j['closed']
assert j['format']=='CMD31' and j['published']==33 and j['reclaimed']==33
r=j['requests'];assert len(r)==1 and r[0]['seq']==33 and r[0]['rejected']==4 and r[0]['reclaimed']
assert not r[0]['dispatched'] and not r[0]['matching_body_completed'] and not r[0]['client_settled'] and not r[0]['sealed'] and not j['events']
assert not j['snapshot_atomic'] and j['pending_count']==0
assert not j['call_return_is_ack']
print('PASS actual ARM client reads shared request/dispatch/queued-body/commit evidence; rejected transaction33 releases its slot without native settlement after32 settled reused transactions')
PY
