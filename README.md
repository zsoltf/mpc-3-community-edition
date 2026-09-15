# MPC 3 Community Edition

Use a Behringer X-Touch with your standalone MPC: motorized faders, track names
and colors, drum-pad mixing, effects, Q-Links and transport. Change something on
the MPC and the controller follows. No computer is needed while making music.

[**Build your image**](https://zsoltf.github.io/mpc-3-community-edition/)
| [Controller guide](https://zsoltf.github.io/mpc-3-community-edition/guide.html)
| [Downloads](https://github.com/zsoltf/mpc-3-community-edition/releases)

## Try it

Use the image builder with the official **MPC 3.9.1 Gen1** firmware from Akai.
Everything is processed in your browser. SSH is off unless you supply your own
public key. Install the generated image, connect X-Touch in **MC/USB** mode,
and create or load a project. Controller startup is automatic.

Follow the [setup guide](https://zsoltf.github.io/mpc-3-community-edition/guide.html),
including the MPC MIDI-input settings. No Global MIDI Learn mapping is required.

## Before you start

This is an **experimental release candidate**, tested with **MPC Live II and
the full X-Touch**. Other Gen1 models need testing; Gen2, Force, other MCU
controllers and HUI are not supported by this release.

The r3 runtime passed playback checks, but the rebuilt r3 image still needs
its final flash/boot test. Back up your projects and keep the official firmware
available. [Release status and limitations](docs/releases/v0.1.0-rc.1.md).

## Learn more or help out

- [Button cheat sheet](docs/xtouch-cheatsheet.md) and [recovery](controllers/program-observer/MCU-START.md)
- [How it works](docs/mcu-architecture.md) and [audio performance](docs/performance.md)
- [Build from source](docs/BUILDING.md) and [contribute](CONTRIBUTING.md)
- [Report a bug or hardware result](https://github.com/zsoltf/mpc-3-community-edition/issues)

Built on research shared by [TheKikGen / MPC-LiveXplore](https://github.com/TheKikGen/MPC-LiveXplore)
and the open-source music community. Our code is [MIT licensed](LICENSE);
[third-party notices](builder/THIRD_PARTY_NOTICES.md) apply separately.

Unofficial; not affiliated with Akai, inMusic or Behringer. Akai firmware is not
included. Please do not share generated firmware or personalized SSH images.
