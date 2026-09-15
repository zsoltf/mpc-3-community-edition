# Startup, manual control and recovery

After installing Community Edition, turn on MPC, create a blank project or
load a template/saved project, and use the X-Touch. The controller waits while
the project chooser is open. No computer or manual command is needed in normal use.

The r3 image installs its matched runtime at
`/data/mpclearn-model.mcu-perf-r3`. Earlier r2 installations use a different
directory. The selected installation is recorded in `/etc/mpclearn-boot-stage`.

For buttons and operating modes, use the [X-Touch cheat sheet](../../docs/xtouch-cheatsheet.md).
For tested scope, see the [release notes](../../docs/releases/v0.1.0-rc.1.md).

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

On MPC, inspect the selected installation and service:

```sh
cat /etc/mpclearn-boot-stage
systemctl status mpclearn-boot.service
```

For **r3**, use:

```sh
/data/mpclearn-model.mcu-perf-r3/mcu status
```

If the selector names an earlier stage, use that installation's `mcu` instead.
An explicit start or stop can restart the MPC application. **Save your project
before either command.** Starting an already owned session retains it.

```sh
/data/mpclearn-model.mcu-perf-r3/mcu start
/data/mpclearn-model.mcu-perf-r3/mcu stop
```

`stop` ends the owned controller session and restores stock MPC. It does not
disable the next boot's controller startup.

## Disable automatic startup

Save the project, then run on MPC:

```sh
/data/mpclearn-boot/mcu-boot-install.sh disable
```

This removes the writable boot selector and stops the boot-owned controller
session. The early system service remains installed but dormant. It does not
delete projects or the runtime package. The provisioner preserves this disabled
state on subsequent boots and known-version upgrades.

For a later one-session trial, the r3 `mcu start` command above remains available.
Restoring persistent autostart uses the maintainer boot installer, which requires
the matched stage and access to its systemd unit directory; do not substitute an
old stage or copy individual executables from another release.

## Return to official firmware

Reinstall the official firmware through MPC's normal update procedure. This
restores system files, but does not erase user projects or the extension's
files on the writable `/data` partition. Keep an official firmware download
available before experimenting. An image made without SSH has no SSH recovery
access; use MPC's firmware update flow.

## If controls or audio misbehave

Save musical work before troubleshooting. Record the release, MPC model,
project operation and whether X-Touch was connected. With SSH, `mcu status`
and the boot service status help distinguish a controller failure from an app
failure. Avoid posting complete settings or projects in a public issue.

The session owner validates the executable and process lifetime, handles
project replacement, and does not endlessly restart MPC after arbitrary crashes.
A firmware update can replace boot entries or invalidate native addresses;
only use a release that explicitly supports that exact firmware.

The extension does not process samples, but it consumes CPU alongside audio.
See [performance and profiling](../../docs/performance.md) for measured costs
and the distinction between component checks and musical acceptance.
