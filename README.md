# MPC 3 Community Edition

A community build of the MPC 3.9.1 Gen1 firmware that adds features to
standalone MPCs. You build the image locally in your browser from Akai's
official update file. Nothing is uploaded and no firmware is hosted or
distributed here.

## Features

- **Mackie Control (MCU) integration.** Drives the mixer, sends, drum pad mix,
  inserts, Q-Links and transport. Motor faders, track names, colors and meters
  follow the MPC's own state.
- **USB mouse support.** A cursor after boot or on hot-plug, clicking,
  drag-to-select, and the scroll wheel acting as the data wheel for the
  focused control. Right-click and long-tap gestures are not implemented yet.

## Roadmap

- [x] Mackie MCU support
- [ ] Global MIDI Learn
- [x] Mouse support
- [ ] Basic VST2 support
- [ ] Bonus update 1
- [ ] Bonus update 2
- [ ] Bonus update 3

## Requirements

- **MPC:** MPC Live II (tested) or another Gen1 model on firmware 3.9.1
  (untested). Gen2 models and Force are not supported.
- **Controller:** Behringer X-Touch (full size) in MC/USB mode. Other MCU
  controllers and HUI are not supported.
- **File:** the official `MPC-3.9.1-Gen1-update.img` from Akai.

## Install

1. **Build the image.** Open the [Image Builder](https://zsoltf.github.io/mpc-3-community-edition/)
   and select the official firmware file. Optionally add an Ed25519 public key
   to enable SSH; SSH stays disabled otherwise.
2. **Flash it** with the MPC's normal firmware update procedure.
3. **Configure MIDI.** In Preferences > MIDI/Sync, disable Track, Global and
   Control for the `X-TOUCH_INT` input so raw MCU messages stay away from
   instruments.
4. **Connect.** Plug in the X-Touch and load a project. The controller starts
   on its own; no Global MIDI Learn mapping is needed.

The first boot installs the runtime under `/data`. Upgrading from an earlier
r2, r3 or r4 image keeps your settings and projects. Reinstalling official
firmware restores the stock system files.

## Status

Release v0.1.0-rc.3, a release candidate. Tested on one MPC Live II with one
X-Touch and one mouse; long sessions and other hardware are not covered. Known
limit: a fast wheel spin on some sliders overshoots. Details are in the
[release notes](docs/releases/v0.1.0-rc.3.md).

**Back up your projects and keep the official firmware available** before
trying this.

## Links

- [Image Builder](https://zsoltf.github.io/mpc-3-community-edition/)
- [X-Touch setup and cheat sheet](https://zsoltf.github.io/mpc-3-community-edition/guide.html)
- [Releases and release notes](https://github.com/zsoltf/mpc-3-community-edition/releases)
- [Startup, manual control and recovery](controllers/program-observer/MCU-START.md),
  [architecture](docs/mcu-architecture.md), [building from source](docs/BUILDING.md),
  [contributing](CONTRIBUTING.md)
- Report results and bugs through [issues](https://github.com/zsoltf/mpc-3-community-edition/issues).
  Do not post private keys, complete settings or projects.

## Credits and license

Built on research from [TheKikGen / MPC-LiveXplore](https://github.com/TheKikGen/MPC-LiveXplore).
[MIT licensed](LICENSE); [third-party notices](builder/THIRD_PARTY_NOTICES.md)
apply to bundled components. This is an unofficial project, not affiliated
with Akai, inMusic or Behringer. Do not redistribute generated images.
