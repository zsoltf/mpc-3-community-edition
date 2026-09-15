#!/usr/bin/env python3
"""Default browser recipe: remove authorization and mask the SSH system service."""
import os, pathlib, shutil, subprocess, sys
os.environ['E2FSPROGS_FAKE_TIME']='1783599420'
source,target=map(pathlib.Path,sys.argv[1:]);assert source!=target
shutil.copyfile(source,target)
for command in ['rm /root/.ssh/authorized_keys','rm /etc/systemd/system/multi-user.target.wants/sshd.service','rm /usr/lib/systemd/system/sshd.service','symlink /usr/lib/systemd/system/sshd.service /dev/null']:
 r=subprocess.run(['debugfs','-w','-R',command,str(target)],check=True,capture_output=True,text=True)
 if any(s in r.stderr.lower() for s in ['not found','error','already exists','usage:']):raise RuntimeError(r.stderr)
