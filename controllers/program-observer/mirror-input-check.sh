#!/bin/sh
set -eu
cd "$(dirname "$0")"
fixture_dir=$(mktemp -d /tmp/mpclearn-mirror-input-check.XXXXXX)
trap 'rm -rf "$fixture_dir"' EXIT HUP INT TERM
./command-check.sh
docker run --rm --network none --memory 256m --pids-limit 32 \
 -v "$PWD:/src:ro" -v "$fixture_dir:/out" mpclearn-controls-build sh -ec '
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/mirror-input/effects-component
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/mirror-input/pad-component
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/mirror-input/midi-component
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/mirror-input/mirror-input-producer-test /out/volume.state /out/command.state /src/package/mirror-input/mirror-input-test
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/mirror-input/mirror-motor-regression /out/volume.state
set +e
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /src/package/mirror-input/mirror-read /out/volume.state --all > /out/channels.json
result=$?
[ "$result" -eq 1 ] || exit 1
'
python3 - "$fixture_dir/channels.json" <<'PYTEST'
import json,sys
j=json.load(open(sys.argv[1]))
assert j['format']=='MMV17' and j['stable'] and not j['fresh'] and not j['error']
assert j['master_count']==12 and len(j['tracks'])==12
for t in j['tracks']:
 assert t['available'] and t['volume']==0.5
 for key,value in [('pan',0.5),('mute',0),('solo',0),('solo_audio',0),('effective_mute',0),('arm',0),('name','Channel'),('color',0xffdd4488)]:
  f=t['fields'][key]
  assert f['available'] and f['value']==value and f['seed_kind']==1,(key,f)
assert j['selection']['available'] and j['selection']['value']==0
print('PASS actual MMV17 reader JSON:12 retained channel groups, defaults and independent owner/property identity; closed producer is stale')
PYTEST
