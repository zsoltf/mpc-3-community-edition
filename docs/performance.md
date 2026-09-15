# Audio performance and profiling

Controller responsiveness alone does not establish suitability for making
music. This extension shares CPU and memory with MPC's audio engine even though
it does not process audio samples.

## Disconnected-controller repair in r3

The old bridge repeatedly opened and closed ALSA sequencer clients while
X-Touch was absent. It also continued copying complete mixer snapshots.
A native capture measured about 8% of one CPU core and 129 ALSA announcements
in 10 seconds. Stopping the bridge while retaining the same MPC process and
project made the reported glitches stop.

The repair in `mirror-motor.c` retains a portless discovery client. After
outstanding commands settle, it parks full snapshot copying but retains
bounded process identity, heartbeat and mailbox checks. Reconnection requires
fresh state before feedback. Connected input and motor cadence were not slowed.

Fixed disconnected measurements were 0.85% of one core over 10 seconds and
0.72% over 40 seconds, with no recurring device announcements. The device owner
reported clean audio while disconnected, then after reconnecting and operating
faders and pan together on the same MPC process.

A separate connected live-control sample measured bridge CPU at 14.8% and MPC
at 119.57% of one core. These workloads differ; subtracting them does not give
an A/B overhead estimate. Linux per-core CPU and MPC's audio-load display are
different measurements. A second 20-second profile again identified snapshot
copying/clearing as the principal bridge cost, without justifying another
observer change for this release.

## Constraints for future changes

- Keep expensive executable hashing at admission, not in recurring monitoring.
- No motor output does not mean no work: skip unnecessary snapshot preparation.
- Preserve command settlement and lifetime checks before parking work.
- Do not mask overload by delaying control input.
- Effects, Q-Link and I/O already use expiring interest gates.
- Connected full-track/pad copying and retained meter subscriptions remain
  optimization candidates. Meter service also disposes tokens; indiscriminate
  throttling can delay lifecycle cleanup.

## Compare under musical load

Use the same project and passage with the bridge stopped, controller absent,
controller connected, simultaneous controls and reconnect. Record native CPU,
memory, duration, process identity and relevant ALSA activity separately from
the musician's audible result. Bound profiling captures and remove temporary
profiling tools when finished.

The measurements above are bounded Live II tests. They do not establish
all-feature stress behavior, long-session stability, other hardware, or a
physical flash/boot of the rebuilt r3 image.
