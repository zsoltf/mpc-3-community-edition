# MPC 3 Community Edition

A community build of the MPC 3.9.1 firmware that adds X-Touch control and USB
mouse support to standalone MPCs. You build the image in your browser from
Akai's official update file. Nothing is uploaded, and no firmware is hosted
here.

**[Build your image](https://zsoltf.github.io/mpc-3-community-edition/)**
· **[X-Touch guide](https://zsoltf.github.io/mpc-3-community-edition/guide.html)**

## Features

- **X-Touch control.** Mixer, sends, drum pad mix, plug-ins, Q-Links and
  transport. Motor faders, track names, colors and meters follow the MPC. The
  jog wheel doubles as the MPC data wheel.
- **USB mouse.** Pointer, click, drag to select, and the scroll wheel as the
  data wheel. No right-click yet.

## Roadmap

- [x] Mackie MCU support
- [ ] Global MIDI Learn
- [x] Mouse support
- [ ] Basic VST2 support
- [ ] Bonus update 1
- [ ] Bonus update 2
- [ ] Bonus update 3

## Requirements

- MPC Live II on firmware 3.9.1. Other Gen1 models are untested. Gen2 and
  Force are not supported.
- Full-size Behringer X-Touch in MC mode over USB. Other controllers are not
  supported yet.
- The official `MPC-3.9.1-Gen1-update.img` from Akai.

## Install

1. Open the [Image Builder](https://zsoltf.github.io/mpc-3-community-edition/)
   and choose the official firmware file. Add an SSH key only if you want
   remote access.
2. Save the image to a USB drive and run the MPC's firmware update.
3. In **Preferences > MIDI/Sync**, turn off Track, Global and Control for the
   `X-TOUCH_INT` input.
4. Plug in the X-Touch and load a project. It connects on its own.

Your settings and projects are kept. Back up your projects first, and keep the
official firmware so you can go back.

This is an experimental pre-release. Known limits are in the
[release notes](docs/releases/v0.2.3.md).

## Links

- [Releases](https://github.com/zsoltf/mpc-3-community-edition/releases)
- [Report a bug](https://github.com/zsoltf/mpc-3-community-edition/issues).
  Please don't post private keys, settings or projects.
- [Building from source](docs/BUILDING.md) ·
  [Architecture](docs/mcu-architecture.md) ·
  [Recovery](controllers/program-observer/MCU-START.md) ·
  [Contributing](CONTRIBUTING.md)

## Credits and license

Built on research from
[TheKikGen / MPC-LiveXplore](https://github.com/TheKikGen/MPC-LiveXplore).
[MIT licensed](LICENSE), with [third-party notices](builder/THIRD_PARTY_NOTICES.md)
for bundled parts. Unofficial, and not affiliated with Akai, inMusic or
Behringer. Please don't share the images you build.
