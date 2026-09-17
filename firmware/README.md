# MCU image and release candidate

Runtime source: `c4906a3`, CMD31/MMV17, run in place from `/usr/share/mpclearn/mcu`.
Qualified combination: MPC Live II, firmware3.9.1 Gen1, full Behringer X-Touch
in MC/USB mode. Do not infer other MPC or MCU hardware compatibility from the
Gen1 image header. This is an unofficial experimental integration.

## Deliverables

- A **personal** firmware image containing the exact accepted MCU binaries,
  which run in place from the image, the boot/session helpers and a boot-time
  provisioner.
- A standalone MCU payload without Akai firmware, credentials, settings,
  projects, plug-ins or session data.
- The source and build instructions. No firmware redistribution permission is
  implied by source availability.

The browser builder disables SSH by default, including a system service mask.
An optional owner-supplied Ed25519 public key enables SSH for that owner to log
in as root. Such a personalized image must not be shared as a generic community image.
Private client keys never enter the image. Each MPC generates its own persistent
SSH host identity. Password authentication and the vendor CA route are disabled.

## Developer build environment

These Docker commands are for maintainers preparing and verifying patch data.
The browser-only end-user builder is developed separately under `builder/`; its users
must not need Docker, Python, a compiler, VM, WSL or terminal commands.

### Build the reference image locally

Work from the repository root on `main`. First follow the complete
[runtime build instructions](../docs/BUILDING.md). Docker is required. Supply your own official3.9.1 Gen1 USB update, pinned SHA256:
`4c0f7797a006313fad206f9513eca8b3823e834c9e0941a6b5f088386cb835d6`.
Do not substitute another firmware version.

```sh
mkdir -p inputs
# Copy your official download to inputs/MPC-3.9.1-Gen1-update.img
# Copy your own Ed25519 PUBLIC key to inputs/owner.pub
# Obtain the image tool from its original upstream repository first:
git clone https://github.com/TheKikGen/MPC-LiveXplore.git ../MPC-LiveXplore
sh firmware/prepare-upstream.sh ../MPC-LiveXplore
docker build -t mpclearn-build:local .
# Download mpc3-ce-mcu-c4906a3.tar.gz from this repository's release.
# Verify it against the release SHA256SUMS before extracting.
mkdir -p artifacts/mcu-c4906a3-v0_2_3
tar -xzf mpc3-ce-mcu-c4906a3.tar.gz --strip-components=1 -C artifacts/mcu-c4906a3-v0_2_3
docker run --rm --network none \
  -v "$PWD:/work" -v "$PWD/inputs:/inputs:ro" \
  -v "$PWD/artifacts/mcu-c4906a3-v0_2_3:/payload:ro" \
  mpclearn-build:local sh firmware/build.sh
```

`package.py` accepts only the qualified observer and matched nine-file package.
It intentionally refuses an arbitrary unqualified runtime. Rebuild that runtime
with the existing controller build guide and `mirror-input-build.sh`, using the
exact MPC executable extracted from your own official image, location
`/usr/share/mpclearn/mcu` and lifetime `manual`.

The image builder reuses TheKikGen's pinned image tool at
`9be3e63bf03d3467cf06478b48671894dcc634f4`, then independently decodes the output.
See `upstream/COMMIT`. A source release must resolve that tool's distribution
license or require users to obtain it from upstream; do not silently relicense it.

Generated files are under `artifacts/`; the USB image name retains `-update.img`.
Verification checks the exact official input, XZ/SHA1 image structure, geometry,
filesystem consistency, full semantic delta, each payload byte/mode/owner and
unchanged stock MPC executable. It does not prove the image flashed or booted.

### Browser patch variants

Maintainers prepare the SSH-enabled reference root with the canonical 81-byte
Ed25519 placeholder instead of a real owner's key. `make-patch.py` emits only
changed byte spans and verifies exact reconstruction. For the default variant,
`disable-ssh.py` removes `authorized_keys` and the SSH boot link, and replaces
the `sshd.service` unit with a `/dev/null` mask. Run the filesystem check and
compare its inventory: only those three paths may differ from the enabled root.
Generate that recipe with `make-patch.py --no-ssh`; its manifest must declare
`ssh_enabled: false`, `key_offset: -1`, and an empty `key_placeholder`.

The browser embeds both verified recipes. No selected key means the disabled
recipe. A valid selected Ed25519 public key selects the enabled recipe, whose
placeholder is replaced locally before the final image verification.

## First boot and rollback

The runtime runs in place from `/usr/share/mpclearn/mcu`. The image sets its
modes (executables 0700, observer, `config.h` and manifest 0600, folder 0700);
`verify.py` asserts them by name and the browser recipes carry the same inode
bytes. The boot unit starts the session unless `/data/mpclearn/disabled` exists.

An early unit runs `provision.sh` on every boot, after writable `/data` and
before the session unit. It checks the payload once per image revision and
prepares exactly one folder, `/data/mpclearn` (root, 0700), with `session/`,
`history/` and the `image` marker. It uses an existing `/data/mpclearn` only if
it is a root-owned 0700 folder; anything else there is refused with a message
and nothing is changed. It reads, writes and deletes nothing else. A
development override in `/data/mpclearn/dev` is used only while it matches the
image; otherwise it is ignored and left in place.

Reinstalling the official firmware restores its root filesystem and leaves
`/data/mpclearn` (and, on SSH images, `/data/ssh/mpclearn`) behind. Use the
[startup/recovery guide](../controllers/program-observer/MCU-START.md) for
normal disable/start/stop.

`provision-check.py` runs the image's provisioner and runtime with the image's
own ARM userland in an extracted root:

```sh
docker run --rm --network none --tmpfs /provision-root/run -v "$PWD:/work" \
  mpclearn-build:local python3 firmware/provision-check.py build/rootfs.final.ext
```

Add `--recipe DIR` to check the root that a browser recipe produces from the
official root instead, and `--downgrade OLDER_ROOT` to also run an earlier
release's provisioner beside `/data/mpclearn`.

**MIDI setup:** MCU uses native MPC commands and does not require Global MIDI
Learn, a selected XMM profile or the virtual adapter's Control input. In MPC
Preferences > MIDI/Sync, disable Track, Global and Control for X-TOUCH_INT to
prevent raw MCU messages from playing instrument notes or bending pitch. Leave
other MIDI ports configured as usual. The image preserves user settings.
Global MIDI Learn and the optional Mini/Launch Control mappings remain a
separate feature; the MCU image does not install a learned profile.
See the [release notes](../docs/releases/v0.2.3.md) for known limits.
