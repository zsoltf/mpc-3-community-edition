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

## Report after the live website flash

The device owner successfully built and flashed r3 from the published website.
A roughly seven-track project with synths and drums then produced CPU spikes
and audible clicks during simultaneous X-Touch movements; moving all eight
faders produced clicks on each reported attempt. The MPC audio-load display
was around50% before the spikes. This heavier workload is an open performance
issue; earlier clean playback tests do not establish its acceptance. SSH was restored for a same-process comparison. A 99Hz cpu-clock profile
measured the original connected bridge at about14% of one core while idle,
with one-second movement peaks35-36%. MPC CPU increased as well; bridge cost
alone has not been established as the entire cause. The saved X-TOUCH_INT
input had Track, Global and Control disabled.

The ARM snapshot is788052 bytes. Every matched input event both initialized
it and called a reader that cleared its entire capacity again. The candidate
removes the duplicate initialization and clears only metadata plus populated
rows. It still reads current source fields for every event and preserves
identity, revision, lifetime and command-settlement checks. Rows outside
count/pad_count are unspecified and must never be consumed. Poisoned-buffer,
shrinking-topology and existing ARM input/motor regressions passed.

With the candidate bridge and unchanged MPC PID1453, a21.28-second native
/proc sample measured bridge9.45% of one core and5784KiB RSS; MPC68.47% and
1087384KiB RSS. The profile showed reduced clearing cost. These are idle
measurements, not acceptance of the concurrent-control audio workload.
Local raw evidence is in build/full-load-profile (not distributed).

The candidate still clicked during the owner's eight-fader test, despite
sampled bridge movement peaks decreasing to20.2% of one core. Physical fader
movement with the bridge stopped was reported clean. A subsequent snapshot
showed Playing=0: idle/movement native CPU comparisons did not yet establish
continuous playback, so the MPC-side increase cannot be attributed to command
overhead alone. Preserve this distinction when using these early captures.

## Heavy-project repair (CMD31 candidate)

Source commit 02d9abf batches the bridge's source snapshot per MIDI input batch,
refreshes it once per tick, validates only the target scalar before each motor
message, copies pads only in Drum Mix, keeps identity and ALSA discovery at
their 100 ms cadences, limits meter demand to the visible eight strips plus
master, skips the command-association scan in meter-only hooks, and maps the
two live state files on tmpfs under `/run` instead of the ext4 `/data`
partition.

The candidate package was installed on the same Live II and the same roughly
40-track synth/drum project that clicked with r3. A one-second `/proc` sampler
(local raw evidence in `build/fable-performance`, not distributed) recorded
338 seconds with the project loaded, including a period of stationary playback
and the owner's instructed test of ten seconds still, fifteen to twenty
seconds of all eight faders, then a fader and pan together, all during
playback. Playing state was read from the observer with each sample.

| Measurement (one core = 100%)            | r3 bridge, Sep 14 | CMD31 candidate, Sep 15 |
|------------------------------------------|-------------------|-------------------------|
| Bridge idle, connected                   | about 14%         | 4 to 5%                 |
| Bridge peak during eight-fader movement  | 35 to 36%         | 6%, capture peak 8%     |
| MPC process, stationary playback         | 136 to 144%       | 137 to 148%, peak 185%  |
| MPC process, movement window             | 150 to 179%       | 137%, peak 142%         |
| Audio Processing thread, movement window | not isolated      | 25%, peak 27%           |
| Minor faults per second, movement window | not isolated      | 0 on bridge, MPC, audio |
| Meter tokens with project loaded         | 40                | 8                       |

The MPC-side CPU rise that accompanied fader movement on r3 did not appear in
this capture. The session status after the test showed 3276 requests published
and 3276 reclaimed with no pending or error state. The device owner reported no
clicks during this eight-fader test. The movement windows are inferred from the
instruction time; the sampler does not observe fader events. This capture is
one project and one passage, not an all-features stress test, a long-session
guarantee, a controller-absent or reconnect comparison, or a flashed image.

The r4 release package was built from commit 9000390 for stage
`/data/mpclearn-model.mcu-perf-r4`. Its bridge, client, reader, adapter,
button helper and session script were byte-identical to the tested candidate;
its observer differed only in the compiled stage path (three bytes) and the
resulting build-id note. `firmware/runtime.sha256` pinned those nine files at
the r4 release; it now pins the r5 package (runtime `4d060dc`, stage
`/data/mpclearn-model.mouse-r5`), whose bridge, client, reader, adapter,
button helper and session script are byte-identical to these r4 files. The
measurements above are the r4 measurements and still describe that shared
controller code.
