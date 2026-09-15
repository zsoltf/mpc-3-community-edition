#!/usr/bin/env python3
"""Patch a copy with debugfs, retaining the original ext filesystem geometry."""
import os, pathlib, shutil, subprocess, sys, tempfile
os.environ["E2FSPROGS_FAKE_TIME"] = "1783599420"
original, candidate, public = map(pathlib.Path, sys.argv[1:])
key = public.read_text().strip()
assert key.startswith('ssh-ed25519 ') and '\n' not in key
assert candidate != original
shutil.copyfile(original, candidate)

def debug(command):
    result = subprocess.run(['debugfs','-w','-R',command,str(candidate)],check=True,capture_output=True,text=True)
    if any(s in result.stderr.lower() for s in ['not found', 'error', 'already exists', 'usage:']):
        raise RuntimeError(result.stderr)

with tempfile.TemporaryDirectory() as temp:
    def write(path, text, mode=0o100644, replace=False):
        source = pathlib.Path(temp)/'content'; source.write_text(text)
        if replace: debug('rm '+path)
        debug(f'write {source} {path}')
        for field,value in [('mode',mode),('uid',0),('gid',0)]:
            debug(f'set_inode_field {path} {field} {value}')
    write('/root/.ssh/authorized_keys', key+'\n', 0o100600, True)
    debug('set_inode_field /root/.ssh mode 16832') # directory 0700
    write('/etc/ssh/sshd_config.d/10-az0x.conf', '''# Local custom firmware: dedicated public-key root access.
HostKey /data/ssh/mpclearn/ssh_host_ed25519_key
HostKeyAlgorithms ssh-ed25519
PermitRootLogin prohibit-password
AllowUsers root
PubkeyAuthentication yes
AuthenticationMethods publickey
PasswordAuthentication no
KbdInteractiveAuthentication no
''', replace=True)
    for name in ['ssh_host_ed25519_key','ssh_host_ed25519_key.pub','ssh_host_rsa_key']:
        blocks = subprocess.run(['debugfs','-R','blocks /etc/ssh/'+name,str(candidate)], check=True, capture_output=True, text=True).stdout.split()
        for block in blocks:
            assert block.isdigit()
            debug('zap_block '+block)
        debug('rm /etc/ssh/'+name)
    debug('mkdir /etc/systemd/system/sshdgenkeys.service.d')
    write('/etc/systemd/system/sshdgenkeys.service.d/10-mpclearn.conf', '''[Unit]
RequiresMountsFor=/data

[Service]
ExecStartPre=/usr/bin/mkdir -p /data/ssh/mpclearn
ExecStartPre=/usr/bin/chown 0:0 /data/ssh/mpclearn
ExecStartPre=/usr/bin/chmod 0700 /data/ssh/mpclearn
''')
    debug('mkdir /etc/systemd/system/sshd.service.d')
    write('/etc/systemd/system/sshd.service.d/10-mpclearn.conf', '''[Unit]
Requires=sshdgenkeys.service
RequiresMountsFor=/data
''')
    debug('symlink /etc/systemd/system/multi-user.target.wants/sshd.service /usr/lib/systemd/system/sshd.service')
