# Startup, manual control and recovery

After installing Community Edition, turn on MPC, create a blank project or
load a template/saved project, and use the X-Touch. The controller waits while
the project chooser is open. No computer or manual command is needed in normal use.

The controller runs from the installed image. It keeps one folder,
`/data/mpclearn`, in the MPC's internal system storage: your motor-fader
setting, the last session's short logs and a small history of earlier sessions
(at most 16 MB).

For buttons and operating modes, use the [X-Touch cheat sheet](../../docs/xtouch-cheatsheet.md).
For known limits, see the [release notes](../../docs/releases/v0.2.4.md).

## Connection checklist

1. Set the full X-Touch to **MC / USB** and connect it by USB.
2. Disable **Track, Global and Control** for **X-TOUCH_INT input** in MPC's
   MIDI/Sync preferences. Leave other musical MIDI ports configured normally.
3. Create or load a project. A fresh blank project does not need to be saved
   before the controller connects.

Global MIDI Learn is not required. Mini and Launch Control mappings are a
separate optional feature and are not configured by this image.

## Inspect or start manually

These commands require an image created with your own SSH public key. Log in
from your computer, replacing the key filename and address:

```sh
ssh -i ~/.ssh/YOUR_MPC_KEY root@YOUR_MPC_IP
```

On MPC:

```sh
systemctl status mpclearn-boot.service
/usr/share/mpclearn/mcu/mcu status
```

An explicit start or stop can restart the MPC application. **Save your project
before either command.** Starting an already owned session retains it.

```sh
/usr/share/mpclearn/mcu/mcu start
/usr/share/mpclearn/mcu/mcu stop
```

`stop` ends the controller session and restores stock MPC with the MPC
settings from when the session started. After the MPC has been switched off in
between, it leaves the current settings as they are. It does not disable
startup at the next boot.

## Disable automatic startup

Save the project, then run on MPC:

```sh
/usr/share/mpclearn/mcu/mcu-boot-install.sh disable
```

This stops the controller session, returns to the stock app and creates
`/data/mpclearn/disabled`, which keeps the controller off at every boot. To turn it back on from the next boot:

```sh
/usr/share/mpclearn/mcu/mcu-boot-install.sh enable
```

## Trying a development build

A development package can replace the image's runtime without reflashing. It
has exactly one place, `/data/mpclearn/dev`, and runs only on the image it was
installed for; with any other image it is ignored. See
[building from source](../../docs/BUILDING.md#trying-a-build-on-your-own-mpc).
Remove it with `/usr/share/mpclearn/mcu/mcu-boot-install.sh override clear`.

## Return to official firmware

Reinstall the official firmware through MPC's normal update procedure. This
restores the system files. Your projects and settings are not touched, and the
`/data/mpclearn` folder stays behind; nothing uses it. On an image made with
your own SSH key you can delete it before reinstalling: disable automatic
startup as above, then run `rm -rf /data/mpclearn`. That image also keeps its
SSH host key in `/data/ssh/mpclearn`. An image made without SSH has no SSH access; use MPC's
firmware update flow.

## If controls or audio misbehave

Save musical work before troubleshooting. Record the release, MPC model,
project operation and whether X-Touch was connected. With SSH, `mcu status`
and the boot service status help distinguish a controller failure from an app
failure; `/data/mpclearn/session` and `/data/mpclearn/history` hold the logs.
Avoid posting complete settings or projects in a public issue.

The session owner validates the executable and process lifetime, handles
project replacement, and does not endlessly restart MPC after arbitrary crashes.
A firmware update can replace boot entries or invalidate native addresses;
only use a release that explicitly supports that exact firmware.

The extension does not process samples, but it consumes CPU alongside audio.
See [performance and profiling](../../docs/performance.md) for measured costs
and the distinction between component checks and musical acceptance.
