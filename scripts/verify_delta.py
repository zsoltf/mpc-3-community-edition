#!/usr/bin/env python3
"""Compare complete libext2fs semantic inventories and reject unrelated changes."""
import pathlib,sys

def read(name):
    result={}
    for line in pathlib.Path(name).read_text().splitlines():
        path, record=line.split(' ',1)
        path=bytes.fromhex(path).decode()
        assert path not in result
        result[path]=record
    return result

a,b=map(read,sys.argv[1:])
expected={
'/etc/ssh/ssh_host_ed25519_key','/etc/ssh/ssh_host_ed25519_key.pub','/etc/ssh/ssh_host_rsa_key',
'/etc/ssh/sshd_config.d/10-az0x.conf','/root/.ssh/authorized_keys',
'/etc/systemd/system','/etc/systemd/system/multi-user.target.wants/sshd.service',
'/etc/systemd/system/sshd.service.d','/etc/systemd/system/sshd.service.d/10-mpclearn.conf',
'/etc/systemd/system/sshdgenkeys.service.d','/etc/systemd/system/sshdgenkeys.service.d/10-mpclearn.conf'}
changed={p for p in a.keys()|b.keys() if a.get(p)!=b.get(p)}
assert changed==expected,{'missing':sorted(expected-changed),'unexpected':sorted(changed-expected)}
for p in sorted(changed):
    print(p)
    print('  original:',a.get(p,'absent'))
    print('  custom:  ',b.get(p,'absent'))
print(f'PASS: {len(a)} original paths, {len(b)} custom paths, {len(changed)} expected differences; all other semantic records identical.')
