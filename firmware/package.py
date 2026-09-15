#!/usr/bin/env python3
"""Copy only the accepted package files; never archive a device stage or settings."""
import hashlib, json, pathlib, shutil, sys
repo = pathlib.Path(__file__).resolve().parents[1]
source, output = map(pathlib.Path, sys.argv[1:])
names = 'command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button'.split()
manifest = {}
for line in (source/'session-package.sha256').read_text().splitlines():
    digest, name = line.split()
    if name in manifest or name not in names or len(digest) != 64:
        raise SystemExit('Invalid package manifest')
    manifest[name] = digest
if set(manifest) != set(names):
    raise SystemExit('Incomplete package')
for name in names:
    if (source/name).is_symlink() or hashlib.sha256((source/name).read_bytes()).hexdigest() != manifest[name]:
        raise SystemExit('Mismatched package: '+name)
# Freeze every executable/helper, not just the observer. A self-consistent mixed
# package is not the accepted runtime.
qualified = dict((name, digest) for digest, name in
                 (line.split() for line in (repo/'firmware/runtime.sha256').read_text().splitlines()))
if manifest != qualified:
    raise SystemExit('Not the acceptedce4ccce package; qualify a new release first')
if '#define WINDOW_SECONDS 0u' not in (source/'config.h').read_text():
    raise SystemExit('Timed verification packages cannot be released')
if '#define OBSERVER_LIBRARY "/data/mpclearn-model.mcu-perf-r3/command-observer.so"' not in (source/'config.h').read_text():
    raise SystemExit('Wrong runtime path')
output.mkdir(parents=True, exist_ok=False)
for name in names + ['session-package.sha256']:
    shutil.copyfile(source/name, output/name)
for name in ['mcu-boot.sh','mcu-boot-install.sh','mpclearn-boot.service']:
    shutil.copyfile(repo/'controllers/program-observer'/name, output/name)
shutil.copyfile(repo/'controllers/program-observer/LICENSE', output/'LICENSE')
(output/'BUILD.json').write_text(json.dumps({'runtime_source':'ce4ccce', 'firmware':'3.9.1 Gen1', 'qualified_hardware':'MPC Live II + full X-Touch MC/USB', 'stage':'/data/mpclearn-model.mcu-perf-r3', 'format':'CMD30/MMV17', 'fresh_device_setup':'MCU transport is native; Global MIDI Learn and XMM profiles are not required. Disable raw X-TOUCH_INT musical input; see guide.'}, indent=2)+'\n')
(output/'payload.sha256').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in sorted(output.iterdir())))
