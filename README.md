# MPC 3 Community Edition

A community build of the MPC 3.9.1 firmware that adds X-Touch control, USB
mouse support and built-in note selection to standalone MPCs. You build the
image in your browser from Akai's official update file. Nothing is uploaded,
and no firmware is hosted here.

**[Website](https://zsoltf.github.io/mpc-3-community-edition/)**
· **[Build your image](https://zsoltf.github.io/mpc-3-community-edition/builder.html)**
· **[X-Touch guide](https://zsoltf.github.io/mpc-3-community-edition/guide.html)**

https://github.com/user-attachments/assets/d2c28bb4-6c1e-4adf-b646-ca75fded7a5c

## Features

- **Mackie Control (MCU) integration.** Currently for the full-size Behringer
  X-Touch only. Drives the mixer, sends, drum pad mix, inserts, Q-Links and
  transport. Motor faders, track names, colors and meters follow the MPC's
  own state. The jog wheel doubles as the MPC data wheel (the Scrub button
  toggles it), the cursor cluster does data steps and Tab/Shift+Tab focus
  navigation, and the footswitches are Play and Record.
- **USB mouse support.** A cursor after boot or on hot-plug, clicking,
  drag-to-select, and the scroll wheel acting as the data wheel for the
  focused control. Right-click and long-tap gestures are not implemented yet.
- **Built-in note selection.** In the Grid, hold MPC Shift and turn the MPC
  data wheel to select the previous or next note.
- **Debug build.** An optional no-SSH image creates
  `MPCLEARN-DIAGNOSTICS/report.txt` on one connected writable USB drive and
  keeps the report current.

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
- Full-size Behringer X-Touch in MC mode over USB for controller features.
  A USB mouse and the MPC Shift+wheel feature work without an X-Touch.
- The official `MPC-3.9.1-Gen1-update.img` from Akai.

## Install

1. Open the [Image Builder](https://zsoltf.github.io/mpc-3-community-edition/builder.html)
   and choose the official firmware file. Add an SSH key if you want remote
   access or file transfer.
2. Save the image to a USB drive and run the MPC's firmware update.
3. If you use an X-Touch, turn off Track, Global and Control for its
   `X-TOUCH_INT` input in **Preferences > MIDI/Sync**.
4. Plug in the controls you use and load a project. The X-Touch connects on
   its own; a USB mouse can be used by itself.

After installing, Preferences > Info identifies the image as `MPC CE v0.2.6`
for installation checks and support.

Your settings and projects are kept. Back up your projects first, and keep the
official firmware so you can go back.

See the [v0.2.6 release notes](docs/releases/v0.2.6.md) for changes and limits.

## Files over SSH

An image built with your SSH key can also copy files. Connect with `sftp`,
`scp` or an app such as Cyberduck, WinSCP or FileZilla: user `root`, your
private key, and the address shown in the MPC's Wi-Fi settings. Your USB drive
is `/media/MPC`, named after its label, and the internal storage is
`/media/az01-internal-sd`. Copied files show up in the Browser right away.
Expect 1-2 MB/s over Wi-Fi, and don't replace a file a loaded project is using.

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
