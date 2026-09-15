# MPC 3 Community Edition

A community build of the MPC 3.9.1 Gen1 firmware that adds features to
standalone MPCs. You build the image yourself in a browser from Akai's official
update file; nothing is uploaded and no firmware is distributed here.

Shipped so far: Mackie Control (MCU) support, tested with an MPC Live II and a
full Behringer X-Touch. The controller drives mixer, sends, drum pad mix,
inserts, Q-Links and transport through MPC's own state, so motor faders,
names, colors and meters follow whatever changes on the MPC. The roadmap below
lists what comes next.

- Image builder: https://zsoltf.github.io/mpc-3-community-edition/
- X-Touch setup and cheat sheet: https://zsoltf.github.io/mpc-3-community-edition/guide.html
- Releases: https://github.com/zsoltf/mpc-3-community-edition/releases

## Roadmap

- [x] Mackie MCU support
- [ ] Global MIDI Learn
- [ ] Mouse support
- [ ] Basic VST2 support
- [ ] Bonus update 1
- [ ] Bonus update 2
- [ ] Bonus update 3

## Requirements

- MPC Live II (tested) or another Gen1 model on firmware 3.9.1 (untested).
  Gen2 models and Force are not supported.
- Behringer X-Touch (full size) in MC/USB mode. Other MCU controllers and HUI
  are not supported.
- The official `MPC-3.9.1-Gen1-update.img` from Akai.

## Install

1. Open the image builder and select the official firmware file. Optionally
   select an Ed25519 public key to enable SSH; SSH is disabled otherwise.
2. Install the generated image with MPC's normal firmware update procedure.
3. In MPC, disable Track, Global and Control for the `X-TOUCH_INT` input in
   Preferences > MIDI/Sync. This keeps raw MCU messages away from instruments.
4. Connect the X-Touch and create or load a project. The controller starts on
   its own; no Global MIDI Learn mapping is needed.

The first boot installs the runtime under `/data` and selects it for startup.
Upgrading from an earlier r2 or r3 image keeps the previous installation and
your settings and projects. Reinstalling official firmware restores stock
system files.

## Status

This is a release candidate. The current image (r4) fixes audible clicks that
r3 produced in a large project when several faders moved at once. On the test
device it played cleanly with eight faders moving, after unplugging and
reconnecting the controller, and in Drum Mix. Long sessions and other hardware
are not covered by testing. Details and limits are in the
[release notes](docs/releases/v0.1.0-rc.2.md) and
[performance measurements](docs/performance.md).

Back up your projects and keep the official firmware available before trying
this.

## Documentation

- [X-Touch cheat sheet](docs/xtouch-cheatsheet.md)
- [Startup, manual control and recovery](controllers/program-observer/MCU-START.md)
- [Architecture](docs/mcu-architecture.md)
- [Building from source](docs/BUILDING.md), [firmware image build](firmware/README.md),
  [contributing](CONTRIBUTING.md)

Report hardware results and bugs through
[issues](https://github.com/zsoltf/mpc-3-community-edition/issues). Do not
post private keys, complete settings or projects.

## Credits and license

Built on research from [TheKikGen / MPC-LiveXplore](https://github.com/TheKikGen/MPC-LiveXplore).
The code here is [MIT licensed](LICENSE); [third-party notices](builder/THIRD_PARTY_NOTICES.md)
apply to bundled components. This is an unofficial project, not affiliated
with Akai, inMusic or Behringer. Do not redistribute generated images.
