#!/usr/bin/env python3
"""Exercise the installed provisioner with shipped ARM userland in an extracted rootfs.
/data and /etc are ordinary test directories; systemd and a firmware flash are not exercised.
"""
import hashlib, pathlib, shutil, subprocess, sys, tempfile
image=pathlib.Path(sys.argv[1]).resolve()
def run(args):
    return subprocess.run(args,check=True,text=True,capture_output=True,timeout=120)
with tempfile.TemporaryDirectory(prefix='mpclearn-provision-') as tmp:
    root=pathlib.Path(tmp)/'root';root.mkdir()
    run(['debugfs','-R',f'rdump / {root}',str(image)])
    shutil.copy('/usr/bin/qemu-arm',root/'qemu-arm')
    stage=root/'data/mpclearn-model.mcu-perf-r3'
    state=root/'data/mpclearn-image'
    selection=root/'etc/mpclearn-boot-stage'
    original=root/'media/az01-internal/Settings/MPC/MPC.settings'
    original.parent.mkdir(parents=True,exist_ok=True);original.write_bytes(b'untouched-user-settings\n')
    saved=root/'data/user-project-sentinel';saved.write_bytes(b'untouched-project\n')
    def provision(): return run(['chroot',str(root),'/bin/sh','/usr/libexec/mpclearn/provision.sh'])
    def hashes(): return {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in stage.iterdir() if p.is_file()}
    provision()
    assert selection.read_text()=='/data/mpclearn-model.mcu-perf-r3\n'
    assert stage.stat().st_mode&0o777==0o700 and selection.stat().st_mode&0o777==0o600
    run(['chroot',str(root),'/bin/sh','-c','cd /data/mpclearn-model.mcu-perf-r3 && sha256sum -c session-package.sha256'])
    baseline=hashes();(stage/'session-test-sentinel').write_text('preserve current session\n')
    provision();assert all(hashes()[k]==v for k,v in baseline.items())
    selection.unlink();provision();assert not selection.exists(), 'repeat boot re-enabled disabled MCU'
    # First image install on an already configured device reuses the exact package.
    (state/'installed').unlink();selection.write_text('/data/mpclearn-model.mcu-direct-r2\n')
    provision();assert (state/'previous-stage').read_text()=='/data/mpclearn-model.mcu-direct-r2\n'
    assert (stage/'session-test-sentinel').read_text()=='preserve current session\n'
    # Model interruption after switching selector but before success marker.
    (state/'installed').unlink();provision()
    assert (state/'previous-stage').read_text()=='/data/mpclearn-model.mcu-direct-r2\n'

    # Upgrade the known prior image into a separate stage, preserving old data.
    old_stage=root/'data/mpclearn-model.mcu-direct-r2';old_stage.mkdir()
    (old_stage/'previous-session').write_bytes(b'old-stage-untouched\n')
    old_revision='6d70695-mcu-direct-r2\n'
    new_revision='ce4ccce-mcu-perf-r3\n'
    shutil.rmtree(stage);(state/'installed').write_text(old_revision)
    selection.write_text('/data/mpclearn-model.mcu-direct-r2\n');provision()
    assert selection.read_text()=='/data/mpclearn-model.mcu-perf-r3\n'
    assert (state/'installed').read_text()==new_revision
    assert (old_stage/'previous-session').read_bytes()==b'old-stage-untouched\n'
    assert all(hashes()[k]==v for k,v in baseline.items())
    # A disabled prior installation stays disabled across the upgrade and reboot.
    shutil.rmtree(stage);(state/'installed').write_text(old_revision);selection.unlink()
    provision();assert not selection.exists() and (state/'installed').read_text()==new_revision
    provision();assert not selection.exists()
    # Resume after a selector switch but before writing the completion marker.
    (state/'installed').write_text(old_revision);selection.write_text('/data/mpclearn-model.mcu-perf-r3\n')
    provision();assert (state/'installed').read_text()==new_revision
    # Unknown revision and custom selection must not be silently repointed.
    for marker,selected in [('', '/data/mpclearn-model.mcu-perf-r3\n'), ('\n', '/data/mpclearn-model.mcu-perf-r3\n'), ('unknown-version\n','/data/mpclearn-model.mcu-perf-r3\n'), (old_revision,'/data/my-custom-stage\n'), (None,'/data/my-custom-stage\n')]:
        if marker is None: (state/'installed').unlink()
        else: (state/'installed').write_text(marker)
        selection.write_text(selected)
        failed=subprocess.run(['chroot',str(root),'/bin/sh','/usr/libexec/mpclearn/provision.sh'],capture_output=True,timeout=120)
        assert failed.returncode and selection.read_text()==selected
        assert not (state/'installed').exists() if marker is None else (state/'installed').read_text()==marker
    (state/'installed').write_text(new_revision)
    assert original.read_bytes()==b'untouched-user-settings\n' and saved.read_bytes()==b'untouched-project\n'
    # A conflicting package is rejected before selecting it or marking success.
    (state/'installed').unlink();selection.unlink();(stage/'mirror-input').write_bytes(b'conflicting package')
    failed=subprocess.run(['chroot',str(root),'/bin/sh','/usr/libexec/mpclearn/provision.sh'],capture_output=True,timeout=120)
    assert failed.returncode and not selection.exists() and not (state/'installed').exists()
    print('PASS: shipped ARM provisioner fresh install, same-package migration, rerun, disabled-state preservation, known-revision upgrade, disabled upgrade, interrupted upgrade, settings/project preservation and conflict refusal')
    print('Boundary: extracted rootfs directories replace mounted /data and /etc; actual systemd ordering, boot and flash remain untested.')
