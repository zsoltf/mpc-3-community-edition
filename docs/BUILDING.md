# Build from source

The website and native MCU runtime have separate builds. End users need only
[the browser builder](https://zsoltf.github.io/mpc-3-community-edition/).

## Website

Install Go 1.26.4 (the release compiler), then from the repository root:

```sh
cd builder
go test ./...
./build-web.sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory dist
```

Open http://127.0.0.1:8765/. Deploy only `builder/dist/`. The checked-in patch
recipes already contain the matched runtime; building the website does not
require firmware, SSH keys, Docker or rebuilding the native runtime.

## Native MCU runtime

Use Docker, Python 3 and an official MPC 3.9.1 Gen1 USB image. On Linux, running
ARM component tests also requires configured ARM user-mode emulation; Docker
Desktop's Linux VM supplies this on the tested development platform.

From the repository root, put the official download at
`inputs/MPC-3.9.1-Gen1-update.img`, then extract your own MPC executable:

```sh
mkdir -p inputs build
docker build -t mpclearn-build:local .
docker build -t mpclearn-controls-build controllers
docker run --rm --network none -v "$PWD:/work" mpclearn-build:local sh -ec '
python3 scripts/image_format.py inputs/MPC-3.9.1-Gen1-update.img build/rootfs.original.ext
debugfs -R "dump /usr/bin/MPC /work/build/MPC" build/rootfs.original.ext
'
```

Expected official image SHA-256:
`4c0f7797a006313fad206f9513eca8b3823e834c9e0941a6b5f088386cb835d6`.
Expected extracted MPC SHA-256:
`bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426`.
Preparation rejects a different executable or unexpected native instructions.

Build and run the release component tests:

```sh
./controllers/program-observer/mirror-input-build.sh \
  "$PWD/build/MPC" /usr/share/mpclearn/mcu manual
./controllers/program-observer/mirror-input-check.sh
./controllers/program-observer/location-check.sh
```

These ARM tests exercise the observer, command mailbox and controller logic
with component substitutes. They do not run an MPC musical project or prove
hardware/audio acceptance. `/usr/share/mpclearn/mcu` is where the image runs
the package from; the build writes
`controllers/program-observer/package/mirror-input/`. To create an image of the
tested release, use its published runtime archive; see
[firmware preparation](../firmware/README.md).

### Trying a build on your own MPC

A development build has exactly one place on the device: the override folder
`/data/mpclearn/dev`. The build scripts refuse any other folder, so an
experiment cannot leave copies, backups or staging folders behind. This needs
an image made with your own SSH key.

```sh
./controllers/program-observer/mirror-input-build.sh \
  "$PWD/build/MPC" /data/mpclearn/dev manual
```

Copy `package/mirror-input/` to a temporary folder on the MPC, such as
`/tmp/mcu-dev`, then on the MPC:

```sh
/usr/share/mpclearn/mcu/mcu-boot-install.sh disable
/usr/share/mpclearn/mcu/mcu-boot-install.sh override /tmp/mcu-dev
rm -rf /tmp/mcu-dev
/usr/share/mpclearn/mcu/mcu-boot-install.sh enable
reboot
```

`disable` stops the running session and restores the stock app first; save the
project before it. The installer checks the package and sets root ownership and
the shipped modes.
The override runs only on the image it was installed for; with a different
image it is ignored and the image runtime runs. Remove it yourself with
`/usr/share/mpclearn/mcu/mcu-boot-install.sh override clear`. An override is
built for the override folder, so the exact release bytes are tested only by
flashing an image.

`firmware/package.py` is a release qualification gate, not a general installer.
It accepts only the nine files
pinned by `firmware/runtime.sha256`, built for the image location with the
manual lifetime.
A changed compiler or source may produce different bytes. Such a build is a
new candidate, not the tested release: do not bypass the package check or update
pins merely to make it pass. Native lifecycle, controller and playback tests
are required before qualifying a changed runtime.

## Source layout

- `controllers/program-observer/`: native state service, command execution,
  MCU bridge, boot/session helpers and component regressions.
- `controllers/adapter.c`, `main-button.c`: matched auxiliary tools; full MCU
  transport uses native commands, not Global MIDI Learn.
- `firmware/`: packaging, image patching, provisioning and verification.
- `builder/`: shared Go image engine, browser UI and embedded patch recipes.
- `scripts/`: independent image decoding and filesystem verification.

See [firmware preparation](../firmware/README.md) for rebuilding patch data,
[architecture](mcu-architecture.md) for ownership and
[performance](performance.md) before changing recurring runtime work.

The current toolchain can vary a temporary assembler object name in the
observer's non-loaded `.strtab` section. The cleanup build matched all other
bytes and the other eight packaged files, but this metadata still changes the
whole-file hash. The release retains the original qualified binary and pins;
do not claim byte-for-byte reproducibility or bypass admission checks.
