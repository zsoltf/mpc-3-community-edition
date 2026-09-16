# MCU image and release candidate

Runtime source: `4d060dc`, native transport with the disconnected-CPU repair, the heavy-project snapshot/meter repair and USB mouse support (pointer enable and wheel to data wheel), CMD31/MMV17.
Qualified combination: MPC Live II, firmware3.9.1 Gen1, full Behringer X-Touch
in MC/USB mode. Do not infer other MPC or MCU hardware compatibility from the
Gen1 image header. This is an unofficial experimental integration.

## Deliverables

- A **personal** firmware image containing the exact accepted MCU binaries,
  existing boot/session helpers and a one-time payload installer.
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
# Download mpc3-ce-mcu-4d060dc.tar.gz from this repository's release.
# Verify it against the release SHA256SUMS before extracting.
mkdir -p artifacts/mcu-4d060dc-r5
tar -xzf mpc3-ce-mcu-4d060dc.tar.gz --strip-components=1 -C artifacts/mcu-4d060dc-r5
docker run --rm --network none \
  -v "$PWD:/work" -v "$PWD/inputs:/inputs:ro" \
  -v "$PWD/artifacts/mcu-4d060dc-r5:/payload:ro" \
  mpclearn-build:local sh firmware/build.sh
```

`package.py` accepts only the qualified observer and matched nine-file package.
It intentionally refuses an arbitrary unqualified runtime. Rebuild that runtime
with the existing controller build guide and `mirror-input-build.sh`, using the
exact MPC executable extracted from your own official image and stage
`/data/mpclearn-model.mouse-r5`, lifetime `manual`.

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

An early unit invokes `provision.sh` after writable `/data` and `/etc` mount.
It copies the immutable payload to the versioned `/data` stage, installs the
existing helper files, and selects the existing MCU session owner. The marker
makes this one-time: later boots do not copy or hash the payload, and removing
the boot selector to disable MCU remains effective.

An identical existing package is retained, including its session files.
Known r2, r3 and r4 images upgrade into the separate r5 stage, preserving the
old stages, and replace their boot helpers with this release's versions.
A disabled installation stays disabled. Unknown revisions and custom stage
selections require explicit migration. Conflicting package/helper bytes cause installation to stop; stock firmware
files and user settings are not repaired or replaced speculatively. A previous
stage selection is retained at `/data/mpclearn-image/previous-stage`.
Use the [startup/recovery guide](../controllers/program-observer/MCU-START.md)
for normal disable/start/stop. Reinstalling the official firmware restores its
root filesystem, but does not erase the MCU payload or user data in `/data`.

**MIDI setup:** MCU uses native MPC commands and does not require Global MIDI
Learn, a selected XMM profile or the virtual adapter's Control input. In MPC
Preferences > MIDI/Sync, disable Track, Global and Control for X-TOUCH_INT to
prevent raw MCU messages from playing instrument notes or bending pitch. Leave
other MIDI ports configured as usual. The image preserves user settings.
Global MIDI Learn and the optional Mini/Launch Control mappings remain a
separate feature; the MCU image does not install a learned profile.
See [release notes](../docs/releases/v0.1.0-rc.3.md) for qualification.

## Hardware qualification

The device owner confirmed that the earlier r2 image flashed successfully, MPC
Live II booted, and the controller worked with both User Template and blank
project. The preceding native runtime test confirmed transport and a fader
with Global MIDI Learn and virtual-input Control disabled. This post-flash
check does not establish every control, other hardware or sustained workloads.

The r3 image was built from the published site and flashed by the device owner;
it booted and ran the controller. Under a roughly 40-track synth/drum project
it clicked during concurrent fader moves. The r4 runtime repairs that load
(see [performance](../docs/performance.md)); the owner reported no clicks with
eight faders, unplug/reconnect and Drum Mix on that project. The r4 stage,
upgrade provisioner and helper replacement are tested in the extracted ARM
filesystem, and the owner then flashed the r4 image over r3 on the Live II:
it booted, selected the r4 stage, replaced the boot helpers, connected the
controller and played the heavy project cleanly.

The r5 runtime adds USB mouse support on top of that r4 runtime. The owner
ran it in the r4 stage on the Live II: a cursor with no manual step after a
boot with the mouse plugged in and after hot-plug, pointer clicks and
drag-select, and the wheel driving the focused control's data wheel at slow
and fast speeds. The r5 image itself, its stage and its upgrade from r4 have
not been flashed or booted on hardware; the provisioner and image checks here
are filesystem-level only.
