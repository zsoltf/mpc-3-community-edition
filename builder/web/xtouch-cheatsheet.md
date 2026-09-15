# X-Touch cheat sheet

For the full Behringer X-Touch in MC/USB mode on MPC Live II 3.9.1.
Applies to Community Edition r4 (CMD31/MMV17). See the
[release notes](https://github.com/zsoltf/mpc-3-community-edition/releases/tag/v0.1.0-rc.2) for tested scope and limitations.

Turn on MPC, create a blank project or load your User Template or saved
project, and play. Automatic startup is installed. The X-Touch Mini and a
connected computer are optional. The controller waits at Select Project and
connects when creation or loading completes; a blank project needs no save.

## Tracks and mixing

| Control | Action |
| --- | --- |
| Channel Left / Right | Select the previous / next MPC track. Also changes the parent track in Send, Drum Mix, I/O and Q-Link views. |
| Bank Left / Right | Change fixed banks of eight strips. In Drum Mix, changes pad banks; in Plug-In, changes parameter pages; in EQ, selects Q-Links 1-8 / 9-16. No banking in Send. |
| Select beneath a fader | Select that visible track in the normal mixer. In Send/Drum Mix, Select lights identify the parent track; pad selection is not implemented. |
| Eight channel faders | Track volume in the normal mixer; the active page and Flip can assign other targets. |
| Master fader | Global master output level. |
| Eight encoders | Pan in the normal mixer, or the active page's controls. Push centers ordinary pan. |
| Mute / Solo | Toggle the strip's mute / solo. In Drum Mix, controls the pad. |
| Rec/Rdy | Toggle track recording arm where available. Pad arm is not implemented. |
| Track | Return to the normal track mixer with track names and volume readouts. |
| Shift + Track | open the selected track's native type chooser. Choose on the MPC touchscreen or MPC data knob. Does not create a new track. |
| Instrument (top assignment group, next to EQ) | Open native Add Track chooser. Choose Drum, MIDI or another type on the MPC touchscreen/data knob. Audio Inst in the mixer-view group remains a track filter. |
| Pan | Pan assignment and pan value presentation. |
| Send | Selected track's four sends on faders; corresponding return levels on encoders. |
| Flip | Exchange the active fader/encoder targets. Supported in mixer, Send, Drum Mix, plug-in parameter and Q-Link pages. |
| Shift + Flip | Toggle motor following On/Off; this preference persists. Control input remains available with following Off. |
| Name/Value | Switch label/value presentation. Editing a control temporarily shows its value. |

Audio levels display MPC's dB scale: 0 dB default, +6 dB maximum. MIDI track volume
uses 0-127. MIDI pan/sends and CV volume remain unavailable. Motors follow real
MPC state automatically; touching a fader first is not required.

## Mixer views

| Button | View |
| --- | --- |
| MIDI Tracks | MIDI tracks |
| Audio Tracks | Audio tracks |
| Audio Inst | Instrument tracks |
| Busses | Submixes |
| Outputs | Outputs |
| Global View / User | All channel types |
| Inputs | Selected-track I/O; see below |
| Aux | Drum Mix for the selected Drum track; press again to leave |
| Shift + Aux | Returns mixer |
| Shift + Audio Inst | Alias for Drum Mix |
| Shift + Inputs | Diagnostic input-type filter; its population is not qualified |

Drum Mix shows pad addresses A01-H16. Faders control volume and encoders pan;
Flip swaps them. Selecting a non-Drum track leaves the pad strips blank.
Pad sample names, colors, meters and pad Select/Arm are not implemented.

## Plug-In and Q-Links

| Control | Action |
| --- | --- |
| Plug-In | Show the selected track's four insert slots. Press again from parameters to return to slots. |
| Push an occupied slot's encoder | Open its parameter pages. |
| Shift + push an occupied slot's encoder | Toggle that insert's bypass, from the slot list. |
| Push an empty slot's encoder | Open MPC's native Add Effect chooser. Finish choosing on MPC. |
| Shift + Plug-In, while editing an insert | Open MPC's native Replace Effect chooser. Finish choosing on MPC. |
| Turn encoder in parameters | Adjust the displayed parameter. |
| Push encoder in parameters | Toggle parameters with two declared states. Continuous parameters do not reset. |
| Bank Left / Right in parameters | Previous / next authored group or parameter page. |
| Flip in parameters | Parameters on faders; bank track volumes on encoders. |
| EQ | Q-Links on encoders; initially 1-8. EQ effects remain ordinary inserts. |
| Bank Left / Right on EQ | Q-Links 1-8 / 9-16. |
| Shift + Bank Left / Right on EQ | Previous / next MPC Q-Link mode. |
| Flip on EQ | Q-Links on faders; bank track volumes on encoders. |

Q-Link encoder push has no reset/toggle assignment. Plug-in presets, reliable
parameter-default reset and synth-instance parameter pages are not implemented.
Native chooser navigation remains on MPC. Add/Replace selects an installed
effect; it does not install new plug-in binaries.

## Inputs: selected-track I/O

Turn each encoder through MPC's applicable choices. Faders retain track volumes;
encoder push and Flip do not change routing.

| Encoder | Field |
| --- | --- |
| 1 | Monitor |
| 2 | MIDI In Port |
| 3 | MIDI In Channel |
| 4 | Send To Track |
| 5 | Audio Out |
| 6 | MIDI Out Port |
| 7 | MIDI Out Channel |
| 8 | Audio In |

Unavailable fields show N/A/dashes. Send To Track is internal MIDI routing,
separate from the four audio sends on the Send page.

## Transport, editing and automation

| Control | Action |
| --- | --- |
| Play | Play from the current position. |
| Stop | Stop playback immediately. a distinct press while already stopped returns to the beginning and remains stopped; no double-click deadline. |
| Record | Native MPC recording toggle. |
| Rewind / Fast Forward | Move backward / forward in beats; holding repeats. |
| Jog wheel | Move by beats. |
| Shift + jog wheel | Fine position movement in pulses/ticks. |
| Cycle | Toggle loop. |
| Click | Toggle metronome. |
| Save | Native Save action. |
| Undo | Native Undo; includes edits MPC puts in its undo history. |
| Shift + Undo | Redo. |
| Read/Off | Automation Read. |
| Write | Automation Write. |
| Group | Automation Off. |
| Cursor arrows | Native context-dependent navigation. |
| Zoom | Toggle arrows between ordinary navigation and horizontal/vertical zoom. |
| Enter / Cancel | Native context-dependent key actions; not qualified for every popup. Use MPC for chooser selection/cancel. |
| Shift | Modifier for the combinations listed here. |

The position display uses bars/beats/ticks; SMPTE switching is not implemented.

## F-button page shortcuts

| Button | MPC page |
| --- | --- |
| F1 | Main |
| F2 | Arrange |
| F3 | Clip |
| F4 | Track Mix |
| F5 | Pad Mix |
| F6 | Track Edit |
| F7 | Sample Edit |
| F8 | Step Sequencer |

F6 is Track Edit; **Shift + Track** opens the current track's type chooser.

## Buttons without assignments

SMPTE/Beats, Option, Control, Alt, Trim, Touch, Latch, Marker, Nudge, Drop,
Replace, the global Solo button near transport, Scrub and footswitch inputs
have no assignment. Channel Solo buttons do work. Unlisted modifier
combinations do not imply an additional action.

## Other controllers

Only the full X-Touch is qualified for this MCU bridge. Faders, relative
encoders, buttons, LCD text and much feedback use MCU messages, but discovery
currently requires the X-Touch device/port names. Display colors use an X-Touch
extension and motor policies were tested on this hardware. Other MCU devices
need a device profile and hardware checks. HUI is not implemented.
