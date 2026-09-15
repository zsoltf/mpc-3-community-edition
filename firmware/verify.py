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
old, new = map(inventory, sys.argv[1:3])
expected = json.loads(pathlib.Path(sys.argv[3]).read_text())
rootfs = sys.argv[4]
base = '/usr/share/mpclearn/mcu/'
files = 'command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button session-package.sha256 mcu-boot.sh mcu-boot-install.sh mpclearn-boot.service LICENSE BUILD.json payload.sha256'.split()
required = {base+n for n in files} | {'/usr/libexec/mpclearn/provision.sh','/usr/lib/systemd/system/mpclearn-provision.service','/usr/lib/systemd/system/mpclearn-boot.service','/usr/lib/systemd/system/multi-user.target.wants/mpclearn-provision.service','/usr/lib/systemd/system/multi-user.target.wants/mpclearn-boot.service'}
assert {p for p,v in expected.items() if v['type'] != 'directory'} == required
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
print('PASS: exact MCU payload bytes/modes/owners; only added paths and parent links changed; stock MPC and all other stock content unchanged')
