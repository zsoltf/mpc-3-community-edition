#!/usr/bin/env python3
"""Verify added bytes and metadata, permitting only necessary directory link changes."""
import hashlib, json, pathlib, subprocess, sys, tempfile

def inventory(path):
    out = {}
    for line in pathlib.Path(path).read_text().splitlines():
        name, fields = line.split(' ',1)
        name = bytes.fromhex(name).decode()
        if name in out: raise ValueError('duplicate path')
        out[name] = fields
    return out
if len(sys.argv) not in (5, 6):
    raise SystemExit("usage: verify.py OLD NEW ADDED ROOTFS [MPC_DELTA]")
old, new = map(inventory, sys.argv[1:3])
expected = json.loads(pathlib.Path(sys.argv[3]).read_text())
rootfs = sys.argv[4]
mpc = json.loads(pathlib.Path(sys.argv[5]).read_text()) if len(sys.argv) == 6 else None
base = '/usr/share/mpclearn/mcu/'
# Shipped modes, stated here independently of patch.py: the image runs these
# files in place, so a wrong mode is a runtime that cannot start.
executable = 'command-client mirror-input mirror-read mcu-session.sh mcu mpclearn-controls main-button mcu-boot.sh mcu-boot-install.sh'.split()
private = 'command-observer.so config.h session-package.sha256'.split()
public = 'LICENSE BUILD.json payload.sha256'.split()
modes = {base+n:0o100700 for n in executable} | {base+n:0o100600 for n in private} | {base+n:0o100644 for n in public}
modes |= {base.rstrip('/'):0o40700, '/usr/libexec/mpclearn/provision.sh':0o100755, '/usr/lib/systemd/system/mpclearn-provision.service':0o100644, '/usr/lib/systemd/system/mpclearn-boot.service':0o100644}
required = {base+n for n in executable+private+public} | {'/usr/libexec/mpclearn/provision.sh','/usr/lib/systemd/system/mpclearn-provision.service','/usr/lib/systemd/system/mpclearn-boot.service','/usr/lib/systemd/system/multi-user.target.wants/mpclearn-provision.service','/usr/lib/systemd/system/multi-user.target.wants/mpclearn-boot.service'}
assert {p for p,v in expected.items() if v['type'] != 'directory'} == required
for name, mode in modes.items():
    assert int(dict(x.split(':',1) for x in new[name].split())['mode'],8) == mode, ('shipped mode',name,oct(mode))
assert new.keys()-old.keys() == expected.keys()
assert not old.keys()-new.keys()
parents = {}
for name, spec in expected.items():
    if spec['type'] == 'directory':
        parent = str(pathlib.PurePosixPath(name).parent)
        parents[parent] = parents.get(parent,0)+1
for name in old:
    before = old[name].split(); after = new[name].split()
    if name in parents:
        assert before[0].startswith('mode:4')
        before[4] = 'links:'+str(int(before[4].split(':')[1])+parents[name])
    if mpc and name == mpc['path']:
        before_fields = dict(item.split(':',1) for item in before)
        after_fields = dict(item.split(':',1) for item in after)
        assert before_fields.pop('sha256') == mpc['input_sha256']
        assert after_fields.pop('sha256') == mpc['output_sha256']
        assert before_fields == after_fields, ('MPC metadata changed', before_fields, after_fields)
    else:
        assert before == after, ('unexpected stock change',name)
with tempfile.TemporaryDirectory() as temp:
    for name, spec in expected.items():
        fields = dict(x.split(':',1) for x in new[name].split())
        assert fields['uid'] == fields['gid'] == '0'
        # debugfs creates extent-backed regular files/directories (EXT4_EXTENTS_FL).
        assert int(fields['flags']) == (0 if spec['type']=='symlink' else 0x80000)
        if spec['type'] == 'directory':
            assert int(fields['mode'],8)==spec['mode']
        elif spec['type'] == 'symlink':
            assert fields['sha256']==hashlib.sha256(spec['target'].encode()).hexdigest()
            assert int(fields['mode'],8)==0o120777
        else:
            assert int(fields['mode'],8)==spec['mode'] and fields['sha256']==spec['sha256'],name
            output = pathlib.Path(temp)/'file'
            subprocess.run(['debugfs','-R',f'dump {name} {output}',rootfs],check=True,capture_output=True)
            assert hashlib.sha256(output.read_bytes()).hexdigest()==spec['sha256'],name
        if spec['type'] != 'directory': assert fields['links']=='1'
    if mpc:
        output = pathlib.Path(temp)/'MPC'
        subprocess.run(['debugfs','-R',f"dump {mpc['path']} {output}",rootfs],check=True,capture_output=True)
        data = output.read_bytes()
        assert len(data) == mpc['size']
        assert hashlib.sha256(data).hexdigest() == mpc['output_sha256']
        assert data[mpc['offset']:mpc['offset']+len(bytes.fromhex(mpc['after_hex']))] == bytes.fromhex(mpc['after_hex'])
suffix = '; exact admitted CE label delta and all other stock content unchanged' if mpc else '; stock MPC and all other stock content unchanged'
print('PASS: exact MCU payload bytes/owners and shipped modes (executables 0700, payload folder 0700); only added paths and parent links changed'+suffix)
