# Startup, manual control and recovery

After installing Community Edition, turn on MPC, create a blank project or
load a template/saved project, and use the X-Touch. The controller waits while
the project chooser is open. No computer or manual command is needed in normal use.

The controller runs from the installed image. It keeps one folder,
`/data/mpclearn`, in the MPC's internal system storage: your motor-fader
setting, the last session's short logs and a small history of earlier sessions
(at most 16 MB).

For buttons and operating modes, use the [X-Touch cheat sheet](../../docs/xtouch-cheatsheet.md).
For known limits, see the [release notes](../../docs/releases/v0.2.6.md).

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

The development candidate adds **CONTROLLERS** to MPC Preferences. Its
**X-TOUCH**, **X-TOUCH MINI** and **GENERIC MCU** buttons hand the request to
the running controller session; the Preferences callback itself does not scan
MIDI or restart anything. A successful choice is saved for later sessions and
boots. **GENERIC MCU** applies only when exactly one external physical
bidirectional MIDI endpoint is present. If none or more than one is present,
the current bridge is retained and MPC remains usable.

When the CONTROLLERS tab opens, the highlighted button reflects the session
owner's confirmed active profile. A tap does not optimistically change that
highlight while the owner handles the request. Switch to another Preferences
tab and back to refresh it. An unavailable or ambiguous GENERIC MCU request
leaves the previous active profile highlighted.

For inspection and recovery, the bridge can still list ALSA MIDI endpoints and
accept the same selection from the command line. `configure` saves the
selection; `start` with controller options remains a temporary one-session
override:

```sh
/usr/share/mpclearn/mcu/mcu list-midi
/usr/share/mpclearn/mcu/mcu configure \
  --profile=generic \
  --endpoint-client='EXACT CLIENT NAME' \
  --endpoint-port='EXACT PORT NAME'
/usr/share/mpclearn/mcu/mcu start \
  --profile=generic \
  --endpoint-client='EXACT CLIENT NAME' \
  --endpoint-port='EXACT PORT NAME'
```

When starting a session with the X-Touch Mini, first confirm its MC MODE LED is
lit. If it is not, disconnect USB, hold the bottom-left MC button while
reconnecting, and release it when MC MODE stays lit. Then save the Mini profile:

```sh
/usr/share/mpclearn/mcu/mcu configure --profile=xtouch-mini
```

Return to the full X-Touch with
`/usr/share/mpclearn/mcu/mcu configure --profile=xtouch`. If a controller
session is active, `configure` drains and restarts its bridge; otherwise the
saved choice applies at the next session. The fixed 288-byte preference is
`/data/mpclearn/controller-preferences`.

The exact names are resolved again after a disconnect, so ALSA client and port
numbers may change. The match must remain unique and bidirectional. Physical
ALSA clients are required by default; `--endpoint-type=any` is an explicit
development opt-in for a virtual endpoint. The generic profile uses standard
MCU fader echo and has eight logical strips, master/global/jog controls, LCD and
time display. It does not send the X-Touch color extension. Generic meters are
disabled because MCU devices differ in meter/LCD layout; the X-Touch keeps its
existing separate-meter bytes and policy.

The `xtouch-mini` profile has a fixed physical endpoint, `X-TOUCH MINI` /
`X-TOUCH MINI MIDI 1`. It keeps eight logical V-Pots, their rings, supported
MCU buttons and the master-fader input. The observed Mini master range
0..16256 is mapped to the existing 0..16383 command range, so its captured top
position reaches unity. The profile emits no fader pitch, raw echo, LCD, time,
meter or color output and enables no touch or jog input. Motor Follow cannot
override those hardware limits. The bridge does not change the Mini's device
mode; its normal-MIDI Q-Link adapter route remains separate.

The Mini capture began after connection, so no startup exchange was observed.
Selecting this profile requires the user to put the hardware in MC mode and is
not a native MPC compatibility claim.

The saved selection survives an accepted New Project restart and reboot. A
named profile is protocol policy, not a physical compatibility claim.

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
