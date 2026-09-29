# Build from source

The website and native MCU runtime have separate builds. End users need only
[the browser builder](https://zsoltf.github.io/mpc-3-community-edition/builder.html).

## Website

Install Go 1.26.4 (the release compiler), then from the repository root:

```sh
cd builder
go test ./...
./build-web.sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory dist
```

Open http://127.0.0.1:8765/ for the landing page and
http://127.0.0.1:8765/builder.html for the image builder. Deploy only
`builder/dist/`. The checked-in patch recipes already contain the matched
runtime; building the website does not require firmware, SSH keys, Docker or
rebuilding the native runtime.

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

Create the separate CE executable; this keeps the pristine extraction intact:

```sh
python3 firmware/patch-mpc-ce.py build/MPC build/v0_2_6/MPC
```

The output must be 112,222,004 bytes with SHA-256
`a7690d51ff905bfb7d637f86714596deb205dc72c02ec04dd1b038c2ad45b5a2`.

Build and run the release component tests:

```sh
./controllers/program-observer/native-preferences-build.sh \
  "$PWD/build/v0_2_6/MPC" "$PWD/build/MPC" /usr/share/mpclearn/mcu
./ui/build.sh
./ui/check.sh
./controllers/program-observer/native-preferences-check.sh
./controllers/program-observer/mirror-input-check.sh
./controllers/program-observer/location-check.sh
```

These ARM tests exercise the canonical `ui/` toolkit, its public-only third and
Global MIDI Learn assignment screens, the production native UI adapters,
observer, command mailbox and
controller logic with component substitutes. They do not render or dispatch
touch in MPC, run an MPC musical project, or prove hardware/audio acceptance.
See the [native UI recipe](native-ui-guide.md) and
[toolkit guide](../ui/README.md) for the exact host boundary and remaining UI
limits. `/usr/share/mpclearn/mcu` is where the image runs
the package from; the build writes the final package to
`controllers/program-observer/package/native-preferences/runtime/`. To create
an image of the tested release, use the canonical firmware build; see
[firmware preparation](../firmware/README.md).

### Trying a build on your own MPC

A development build has exactly one place on the device: the override folder
`/data/mpclearn/dev`. The build scripts refuse any other folder, so an
experiment cannot leave copies, backups or staging folders behind. This needs
an image made with your own SSH key.

```sh
./controllers/program-observer/native-preferences-build.sh \
  "$PWD/build/v0_2_6/MPC" "$PWD/build/MPC" /data/mpclearn/dev
```

Before staging or restarting, measure available blocks on `/data` and
`/media/az01-internal/Settings/MPC`, as well as `/tmp`. `/` is a different
filesystem and does not establish room for the development override, session
receipts or native settings. The installer temporarily keeps both runtimes.
The next start also archives the prior session. Native filmstrip generation can
consume space after startup, so remeasure before stopping; a zero-availability
data partition has prevented the session owner from writing its revocation.

Copy `package/native-preferences/runtime/` to a temporary folder on the MPC, such as
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
observer's non-loaded `.strtab` section. A different whole-file hash is a new
candidate even when loaded bytes match; do not bypass the package gate.
