# Contributing

This project makes standalone MPC more useful with external controllers.
Contributions can be musical testing, documentation, hardware compatibility
reports or source improvements.

## Report a problem

Use the repository's issue template. Include the MPC model and firmware,
Community Edition revision, controller/mode, steps to reproduce and expected
behavior. Say whether the issue involves project loading, reconnection or
audio glitches. A short description or controller/screen recording is useful.
Do not post credentials, complete settings or private projects.

## Source changes

Read [the architecture](docs/mcu-architecture.md) and
[build instructions](docs/BUILDING.md). Keep runtime, generated parameter
metadata, firmware patches and documentation consistent. Explain which real
hardware paths were tested and which were not.

The native integration is pinned to one MPC executable. Do not loosen its
hash, instruction, lifetime or completion checks simply to accept another
firmware. Supporting another version requires qualifying its native paths.

Audio performance is a functional requirement. Preserve the idle path and
test with a controller absent, connected, reconnected and multiple controls
in use. See [profiling guidance](docs/performance.md). Component tests cannot
certify glitch-free playback or motor behavior.

## Release scope

The public source focuses on the current MCU runtime and its build/test tools.
Standalone research experiments, agent notes and private development history
are not release inputs. New controllers, HUI and additional plugin features
need their own implementation and hardware evidence.

Our contributions use the repository's MIT license. Preserve third-party
notices and credits. Do not attach full Akai firmware images or personalized
SSH images to issues, pull requests or releases.
