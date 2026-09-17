#!/bin/sh
# Container entrypoint. Inputs mounted read-only at /inputs and /payload.
set -eu
cd /work
output=${MPC_IMAGE_OUTPUT:-artifacts/MPC-3.9.1-Gen1-CE-v0.2.4-PERSONAL-SSH-update.img}
[ ! -e "$output" ] || { echo "Output already exists: $output" >&2; exit 1; }
mkdir -p build artifacts
python3 - <<'PY'
from scripts.image_format import decode, OFFICIAL_SHA
assert decode('/inputs/MPC-3.9.1-Gen1-update.img','build/rootfs.original.ext')['image_sha256']==OFFICIAL_SHA
PY
ssh-keygen -l -f /inputs/owner.pub > build/owner-key-fingerprint.txt
python3 scripts/patch_rootfs.py build/rootfs.original.ext build/rootfs.ssh.ext /inputs/owner.pub
cp build/rootfs.ssh.ext build/rootfs.custom.ext
python3 firmware/patch.py build/rootfs.custom.ext /payload build/mcu-files.json
e2fsck -fn build/rootfs.custom.ext
gcc -O2 -Wno-deprecated-declarations -o build/mpcimg2 upstream/mpcimg2.c -llzma -lcrypto
gcc -O2 -Wno-deprecated-declarations -o build/fs_inventory scripts/fs_inventory.c -lext2fs -lcom_err -lcrypto
python3 - <<'PY'
import hashlib,pathlib
assert pathlib.Path('upstream/COMMIT').read_text().strip()=='9be3e63bf03d3467cf06478b48671894dcc634f4'
assert hashlib.sha256(pathlib.Path('upstream/mpcimg2.c').read_bytes()).hexdigest()=='e28fd9a2c1e4c760483e06dea31450f9d3d1e90cfb0c3604f3a1e66829853acc'
PY
build/mpcimg2 -m /inputs/MPC-3.9.1-Gen1-update.img build/rootfs.custom.ext "$output" > build/repack.log
python3 scripts/image_format.py "$output" build/rootfs.final.ext /inputs/MPC-3.9.1-Gen1-update.img > artifacts/image-verification.json
cmp build/rootfs.custom.ext build/rootfs.final.ext
e2fsck -fn build/rootfs.final.ext
build/fs_inventory build/rootfs.original.ext > build/original.inventory
build/fs_inventory build/rootfs.ssh.ext > build/ssh.inventory
build/fs_inventory build/rootfs.final.ext > build/mcu.inventory
python3 scripts/verify_delta.py build/original.inventory build/ssh.inventory > build/ssh-delta.txt
python3 firmware/verify.py build/ssh.inventory build/mcu.inventory build/mcu-files.json build/rootfs.final.ext
sha256sum "$output" > "$output.sha256"
