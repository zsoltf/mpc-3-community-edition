#!/bin/sh
# Build the complete local v0.2.5 candidate. Nothing is flashed or published.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
cd "$repo"
out=build/v0_2_5
payload=artifacts/mcu-v0_2_5
log(){ printf '== %s [%s]\n' "$*" "$(date +%H:%M:%S)"; }

python3 firmware/patch-mpc-ce.py build/MPC "$out/MPC"
python3 - <<'PY'
import pathlib, shutil
for name in [
    'build/v0_2_5/package', 'build/v0_2_5/inputs-placeholder',
    'build/v0_2_5/inputs-owner', 'build/v0_2_5/patch-ssh',
    'build/v0_2_5/patch-no-ssh', 'build/v0_2_5/diagnostics',
    'artifacts/mcu-v0_2_5']:
    path = pathlib.Path(name)
    if path.exists(): shutil.rmtree(path)
for name in [
    'build/v0_2_5/rootfs.original.ext', 'build/v0_2_5/rootfs.ssh-placeholder.ext',
    'build/v0_2_5/rootfs.no-ssh.ext', 'build/v0_2_5/rootfs.personal.ext',
    'build/v0_2_5/decoded.ext',
    'artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-PLACEHOLDER-update.img',
    'artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-PLACEHOLDER-update.img.sha256',
    'artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-update.img',
    'artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-update.img.sha256',
    'artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-DEBUG-update.img',
    'artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-DEBUG-update.img.sha256',
    'artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-PERSONAL-SSH-update.img',
    'artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-PERSONAL-SSH-update.img.sha256']:
    path = pathlib.Path(name)
    if path.exists(): path.unlink()
PY
mkdir -p "$out/package" "$out/inputs-placeholder" "$out/inputs-owner"
for name in command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button session-package.sha256; do
    cp -p "controllers/program-observer/package/mirror-input/$name" "$out/package/$name"
done
cp inputs/MPC-3.9.1-Gen1-update.img "$out/inputs-placeholder/"
cp inputs/placeholder.pub "$out/inputs-placeholder/owner.pub"
cp inputs/MPC-3.9.1-Gen1-update.img "$out/inputs-owner/"
cp inputs/owner.pub "$out/inputs-owner/owner.pub"

log payload
python3 firmware/package.py "$out/package" "$payload"

log placeholder-image
docker run --rm --network none \
  -e MPC_IMAGE_OUTPUT=artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-PLACEHOLDER-update.img \
  -e MPC_CE_EXECUTABLE=build/v0_2_5/MPC \
  -v "$repo:/work" -v "$repo/$out/inputs-placeholder:/inputs:ro" -v "$repo/$payload:/payload:ro" \
  mpclearn-build:local sh firmware/build.sh
cp build/rootfs.original.ext "$out/rootfs.original.ext"
cp build/rootfs.final.ext "$out/rootfs.ssh-placeholder.ext"
cp build/mcu-files.json "$out/mcu-files.json"
cp build/mpc-ce-files.json "$out/mpc-ce-files.json"
cp build/ssh.inventory "$out/ssh-placeholder.inventory"
cp build/mcu.inventory "$out/mcu-placeholder.inventory"
cp build/original.inventory "$out/original.inventory"
cp artifacts/image-verification.json "$out/placeholder-image-verification.json"

log recipes-and-no-ssh
docker run --rm --network none -v "$repo:/work" -w /work mpclearn-build:local sh -ec '
set -eu
python3 firmware/make-patch.py build/v0_2_5/rootfs.original.ext build/v0_2_5/rootfs.ssh-placeholder.ext inputs/placeholder.pub build/v0_2_5/patch-ssh
python3 firmware/disable-ssh.py build/v0_2_5/rootfs.ssh-placeholder.ext build/v0_2_5/rootfs.no-ssh.ext
e2fsck -fn build/v0_2_5/rootfs.no-ssh.ext
build/fs_inventory build/v0_2_5/rootfs.no-ssh.ext > build/v0_2_5/no-ssh.inventory
python3 - <<"PY"
import pathlib
def inv(name):
    result={}
    for line in pathlib.Path(name).read_text().splitlines():
        path, fields=line.split(" ",1); result[bytes.fromhex(path).decode()]=fields
    return result
a=inv("build/v0_2_5/mcu-placeholder.inventory"); b=inv("build/v0_2_5/no-ssh.inventory")
changed={path for path in a.keys()|b.keys() if a.get(path)!=b.get(path)}
allowed={"/root/.ssh/authorized_keys","/etc/systemd/system/multi-user.target.wants/sshd.service","/usr/lib/systemd/system/sshd.service","/root/.ssh","/etc/systemd/system/multi-user.target.wants","/usr/lib/systemd/system"}
assert changed <= allowed, sorted(changed)
assert "/root/.ssh/authorized_keys" not in b
assert "/etc/systemd/system/multi-user.target.wants/sshd.service" not in b
assert b["/usr/lib/systemd/system/sshd.service"].startswith("mode:120777")
print("PASS: no-SSH root differs only at the three SSH paths and affected parents")
PY
python3 firmware/make-patch.py --no-ssh build/v0_2_5/rootfs.original.ext build/v0_2_5/rootfs.no-ssh.ext inputs/placeholder.pub build/v0_2_5/patch-no-ssh
'

log diagnostics-root
sh firmware/build-diagnostic-root.sh

log no-ssh-and-diagnostics-images
docker run --rm --network none -v "$repo:/work" -w /work mpclearn-build:local sh -ec '
set -eu
build/mpcimg2 -m inputs/MPC-3.9.1-Gen1-update.img build/v0_2_5/rootfs.no-ssh.ext artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-update.img > build/v0_2_5/repack-no-ssh.log
python3 scripts/image_format.py artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-update.img build/v0_2_5/decoded.ext inputs/MPC-3.9.1-Gen1-update.img > build/v0_2_5/no-ssh-image-verification.json
cmp build/v0_2_5/rootfs.no-ssh.ext build/v0_2_5/decoded.ext
build/mpcimg2 -m inputs/MPC-3.9.1-Gen1-update.img build/v0_2_5/diagnostics/rootfs.diagnostics.ext artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-DEBUG-update.img > build/v0_2_5/repack-diagnostics.log
python3 scripts/image_format.py artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-DEBUG-update.img build/v0_2_5/decoded.ext inputs/MPC-3.9.1-Gen1-update.img > build/v0_2_5/diagnostics-image-verification.json
cmp build/v0_2_5/diagnostics/rootfs.diagnostics.ext build/v0_2_5/decoded.ext
sha256sum artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-update.img > artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-update.img.sha256
sha256sum artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-DEBUG-update.img > artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-DEBUG-update.img.sha256
'

log personal-image
docker run --rm --network none \
  -e MPC_IMAGE_OUTPUT=artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-PERSONAL-SSH-update.img \
  -e MPC_CE_EXECUTABLE=build/v0_2_5/MPC \
  -v "$repo:/work" -v "$repo/$out/inputs-owner:/inputs:ro" -v "$repo/$payload:/payload:ro" \
  mpclearn-build:local sh firmware/build.sh
cp artifacts/image-verification.json "$out/personal-image-verification.json"
cp build/rootfs.final.ext "$out/rootfs.personal.ext"

log independent-verification
docker run --rm -i --network none -v "$repo:/work" -w /work mpclearn-build:local python3 - <<'PY'
import json, pathlib, subprocess
from scripts.image_format import decode
cases=[
 ("artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-update.img","build/v0_2_5/rootfs.no-ssh.ext","no-ssh"),
 ("artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-DEBUG-update.img","build/v0_2_5/diagnostics/rootfs.diagnostics.ext","diagnostics"),
 ("artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-PERSONAL-SSH-update.img","build/v0_2_5/rootfs.personal.ext","ssh")]
results=[]
for image, root, variant in cases:
    result=decode(image,"build/v0_2_5/decoded.ext","inputs/MPC-3.9.1-Gen1-update.img")
    result["variant"]=variant
    result["exact_root_match"]=subprocess.run(["cmp","build/v0_2_5/decoded.ext",root]).returncode==0
    assert result["exact_root_match"]
    results.append(result)
pathlib.Path("build/v0_2_5/decoded.ext").unlink()
pathlib.Path("build/v0_2_5/release-independent-verification.json").write_text(json.dumps(results,indent=1)+"\n")
print(json.dumps([{k:r[k] for k in ("variant","image_sha256","rootfs_sha256","exact_root_match")} for r in results],indent=1))
PY

log recipe-provision-checks
docker run --rm --network none --tmpfs /provision-root/run -v "$repo:/work" -w /work mpclearn-build:local python3 firmware/provision-check.py build/v0_2_5/rootfs.original.ext --recipe build/v0_2_5/patch-no-ssh
docker run --rm --network none --tmpfs /provision-root/run -v "$repo:/work" -w /work mpclearn-build:local python3 firmware/provision-check.py build/v0_2_5/rootfs.original.ext --recipe build/v0_2_5/patch-ssh

log builder-resources
cp "$out/patch-ssh/manifest.json" "$out/patch-ssh/patch.bin" builder/resources/ssh/
cp "$out/patch-no-ssh/manifest.json" "$out/patch-no-ssh/patch.bin" builder/resources/no-ssh/
cp "$out/diagnostics/recipe/manifest.json" "$out/diagnostics/recipe/patch.bin" builder/resources/diagnostics/
(cd builder && go test ./... && sh build-web.sh)

sha256sum "$out/MPC" "$payload/payload.sha256" \
  "$out/rootfs.ssh-placeholder.ext" "$out/rootfs.no-ssh.ext" "$out/diagnostics/rootfs.diagnostics.ext" \
  builder/resources/ssh/manifest.json builder/resources/ssh/patch.bin \
  builder/resources/no-ssh/manifest.json builder/resources/no-ssh/patch.bin \
  builder/resources/diagnostics/manifest.json builder/resources/diagnostics/patch.bin \
  artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-update.img \
  artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-DEBUG-update.img \
  artifacts/MPC-3.9.1-Gen1-CE-v0.2.5-PERSONAL-SSH-update.img > "$out/SHA256SUMS"
cat "$out/SHA256SUMS"
log DONE
