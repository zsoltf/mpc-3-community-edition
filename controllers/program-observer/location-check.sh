#!/bin/sh
# Every build that bakes a device path accepts only the image location and the
# development override, and refuses anything else before doing any work.
# Runs on the host; docker and python are replaced by recorders.
set -eu
cd "$(dirname "$0")"
work=$(mktemp -d /tmp/mpclearn-location-check.XXXXXX)
trap 'rm -rf "$work"' EXIT HUP INT TERM
python3 - "$PWD" "$work" <<'PY'
import os, pathlib, re, shutil, subprocess, sys
source, work = map(pathlib.Path, sys.argv[1:])
IMAGE, DEV = '/usr/share/mpclearn/mcu', '/data/mpclearn/dev'
REFUSED = ['/data/mpclearn-model.v0_2_3', '/data/mpclearn-model.manual1', '/data/mpclearn', '/data/mpclearn/', '/data/mpclearn/dev/',
           '/data/mpclearn/devx', '/data/mpclearn/dev/../x', '/data/mpclearn/dev/sub', '/data/mpclearn-v020-backup', '/usr/share/mpclearn/mcu/',
           '/usr/share/mpclearn', '/tmp/stage', '/data', 'data/mpclearn/dev', '', ' /data/mpclearn/dev']

# The guard itself.
for kind, allowed in (('release', {IMAGE, DEV}), ('override', {DEV})):
    for path in [IMAGE, DEV] + REFUSED:
        r = subprocess.run(['sh', str(source/'device-location.sh'), path, kind], capture_output=True, text=True)
        assert (r.returncode == 0) == (path in allowed), (kind, path, r.returncode, r.stderr)
        assert r.returncode in (0, 2) and (r.returncode == 0 or 'Refusing device location' in r.stderr), (kind, path, r.stderr)
print('PASS device-location.sh admits only the image location (release builds) and the development override')

# Every build script that writes OBSERVER_LIBRARY, and the one that delegates to
# command-build.sh, must be covered below; a new one fails this check.
builders = sorted(p.name for p in source.glob('*.sh') if re.search(r"(?m)^printf '#define OBSERVER", p.read_text()))
covered = {'build.sh': 'override', 'command-build.sh': 'duration', 'mirror-build.sh': 'override',
           'model-build.sh': 'override', 'ui-witness-build.sh': 'override'}
assert set(builders) == set(covered), builders
bin = work/'bin'; bin.mkdir()
for tool in ('docker', 'python3'):
    (bin/tool).write_text('#!/bin/sh\necho "$0" >>"$RECORD"\nexit 97\n'); (bin/tool).chmod(0o755)
def attempt(script, *args):
    folder = work/'tree'
    shutil.rmtree(folder, ignore_errors=True); folder.mkdir()
    for name in list(covered) + ['mirror-input-build.sh', 'device-location.sh', 'mirror-guard.sh']:
        shutil.copy2(source/name, folder/name)
    record = work/'record'; record.unlink(missing_ok=True)
    env = dict(os.environ, PATH=f'{bin}:/usr/bin:/bin', RECORD=str(record))
    r = subprocess.run(['sh', str(folder/script), '/nonexistent/MPC', *args], capture_output=True, text=True, env=env, cwd=work)
    reached = record.exists()
    wrote = (folder/'package').exists()
    return r, reached, wrote
def refused(script, *args):
    r, reached, wrote = attempt(script, *args)
    assert r.returncode == 2 and 'Refusing device location' in r.stderr and not reached and not wrote, (script, args, r.returncode, r.stderr)
def accepted(script, *args):
    r, reached, _ = attempt(script, *args)
    assert r.returncode == 97 and reached and 'Refusing' not in r.stderr, (script, args, r.returncode, r.stderr)
for script in ('command-build.sh', 'mirror-input-build.sh'):
    for path in (IMAGE, DEV): accepted(script, path, 'manual')
    accepted(script, DEV, '600')
    refused(script, IMAGE, '600')
    for path in REFUSED: refused(script, path, 'manual')
for script, mode in covered.items():
    if mode != 'override': continue
    accepted(script, DEV, '600') if script != 'mirror-build.sh' else accepted(script, DEV)
    for path in [IMAGE] + REFUSED:
        refused(script, path, '600') if script != 'mirror-build.sh' else refused(script, path)
print('PASS every package build refuses other device folders before any build step or package write')

# Developer documentation shows only these locations in build commands.
docs = [source/'README.md', source/'MCU-START.md', source/'../../docs/BUILDING.md', source/'../../firmware/README.md']
command = re.compile(r'(?:build\.sh|command-build\.sh|mirror-input-build\.sh)\s*\\?\s*\n?\s*("?[^\s"]+"?)\s+(\S+)')
checked = 0
for doc in docs:
    if not doc.exists(): continue
    for match in command.finditer(doc.read_text()):
        location = match.group(2)
        if location.startswith('/') or location.startswith('data/'):
            assert location in (IMAGE, DEV), (doc.name, match.group(0))
            checked += 1
assert checked, 'no documented build commands found'
print(f'PASS {checked} documented build commands use only the image location or the development override')
PY
