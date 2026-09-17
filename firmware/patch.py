#!/usr/bin/env python3
"""Add the frozen MCU payload, with its shipped modes, and early boot entries to the SSH-patched rootfs."""
import hashlib, json, os, pathlib, subprocess, sys, tempfile
os.environ['E2FSPROGS_FAKE_TIME'] = '1783599420'
repo = pathlib.Path(__file__).resolve().parents[1]
rootfs, payload, expectation = map(pathlib.Path, sys.argv[1:])
added = {}
def debug(command, write=True):
    cmd = ['debugfs'] + (['-w'] if write else []) + ['-R', command, str(rootfs)]
    r = subprocess.run(cmd, check=True, capture_output=True, text=True)
    if write and any(x in r.stderr.lower() for x in ['not found','error','already exists','usage:','could not','no free']):
        raise RuntimeError(r.stderr)
    return r

PAYLOAD = '/usr/share/mpclearn/mcu'
# The runtime runs in place from the image, so the image carries the shipped
# modes; the browser recipes copy these inode bytes verbatim.
MODES = dict.fromkeys('command-client mirror-input mirror-read mcu-session.sh mcu mpclearn-controls main-button mcu-boot.sh mcu-boot-install.sh'.split(), 0o100700)
MODES.update(dict.fromkeys('command-observer.so config.h session-package.sha256'.split(), 0o100600))
MODES.update(dict.fromkeys('LICENSE BUILD.json payload.sha256'.split(), 0o100644))

def directory(path):
    if 'Inode:' in debug('stat '+path, False).stdout:
        return
    parent = str(pathlib.PurePosixPath(path).parent)
    directory(parent)
    debug('mkdir '+path)
    mode = 0o40700 if path == PAYLOAD else 0o40755
    for k,v in [('mode',mode),('uid',0),('gid',0)]: debug(f'set_inode_field {path} {k} {v}')
    added[path] = {'type':'directory','mode':mode}

def file(source, destination, mode=0o100644):
    if 'Inode:' in debug('stat '+destination, False).stdout:
        raise RuntimeError('Unexpected existing file: '+destination)
    directory(str(pathlib.PurePosixPath(destination).parent))
    debug(f'write {source} {destination}')
    for k,v in [('mode',mode),('uid',0),('gid',0)]: debug(f'set_inode_field {destination} {k} {v}')
    added[destination] = {'type':'file','mode':mode,'sha256':hashlib.sha256(source.read_bytes()).hexdigest()}

def symlink(path, target):
    directory(str(pathlib.PurePosixPath(path).parent))
    debug(f'symlink {path} {target}')
    added[path] = {'type':'symlink','target':target}

if {p.name for p in payload.iterdir()} != set(MODES): raise RuntimeError('Unexpected payload file set')
for p in sorted(payload.iterdir()):
    if not p.is_file() or p.is_symlink(): raise RuntimeError('Only plain payload files allowed')
    file(p, PAYLOAD+'/'+p.name, MODES[p.name])
file(repo/'firmware/provision.sh', '/usr/libexec/mpclearn/provision.sh', 0o100755)
file(repo/'firmware/mpclearn-provision.service', '/usr/lib/systemd/system/mpclearn-provision.service')
with tempfile.TemporaryDirectory() as temp:
    source = repo/'controllers/program-observer/mpclearn-boot.service'
    unit = pathlib.Path(temp)/'unit'
    unit.write_text(source.read_text().replace('[Unit]\n','[Unit]\nRequires=mpclearn-provision.service\nAfter=mpclearn-provision.service\n',1))
    file(unit, '/usr/lib/systemd/system/mpclearn-boot.service')
for name in ['mpclearn-provision.service','mpclearn-boot.service']:
    symlink('/usr/lib/systemd/system/multi-user.target.wants/'+name, '../'+name)
expectation.write_text(json.dumps(added,indent=2)+'\n')
