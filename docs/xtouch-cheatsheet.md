# X-Touch cheat sheet

MPC 3 Community Edition, Mackie Control feature. Only the full-size Behringer
X-Touch in MC mode over USB is supported right now. X-Touch Compact, One and
Extender, other Mackie Control surfaces and HUI are not. The controller
connects on its own once a project is loaded and follows the MPC: motor faders,
names, colors and meters track whatever changes on the screen.

## Setup

1. Put the X-Touch in MC mode over USB. Power it off, hold channel 1 Select
   while powering on, set encoder 1 to MC and encoder 2 to USB, press Select
   to save. Connect its USB port directly to the MPC.
2. On the MPC, open Preferences > MIDI/Sync and turn off Track, Global and
   Control for the `X-TOUCH_INT` input. No Global MIDI Learn or XMM profile is
   involved; other MIDI ports stay as they are.
3. Load a project: create a blank one, or load a saved project or your User
   Template. The controller connects when loading completes. Unplugging and
   replugging it reconnects.

Shift on the X-Touch is the modifier for every combination below. Shift+Flip
turns motor following off or on; the setting is remembered.

## Channel strips

| Control | Action |
| --- | --- |
| Fader | Track volume, or the control the current view puts on faders. MPC dB scale: 0 dB default, +6 dB maximum. MIDI track volume is 0 to 127. |
| Encoder | Pan, or the current view's encoder control. Push centers pan. |
| Select | Select that track. In Send and Drum Mix the lit Select shows the parent track. |
| Mute / Solo | Toggle mute or solo. In Drum Mix they act on the pad. |
| Rec | Toggle record arm where the track supports it. |
| Master fader | Master output level. |
| Flip | Swap what is on faders and encoders in the current view. |
| Name/Value | Show names or values on the scribble strips. Editing a control shows its value briefly. |
| Channel < > | Previous or next track. Also moves the parent track in Send, Drum Mix, I/O and Q-Link views. |
| Bank < > | Previous or next bank of eight. Drum Mix: pad banks. Plug-in pages: parameter pages. EQ: Q-Links 1 to 8 or 9 to 16. |

## Views

| Button | Strips show |
| --- | --- |
| Track | The normal track mixer. |
| Pan | Pan values. |
| Send | The selected track's four sends on faders, return levels on encoders. |
| Aux | Drum Mix for the selected Drum track: pads A01 to H16, volume on faders, pan on encoders. Press again to leave. |
| Inputs | The selected track's I/O on the encoders (see below). |
| Plug-In | The selected track's four inserts (see below). |
| EQ | Q-Links on the encoders. |

| Button | Mixer filter |
| --- | --- |
| MIDI Tracks | MIDI tracks. |
| Audio Tracks | Audio tracks. |
| Audio Inst | Instrument tracks. |
| Busses | Submixes. |
| Outputs | Outputs. |
| Global View / User | All channel types. |
| Shift + Aux | Returns. |
| Instrument | Opens the MPC's Add Track chooser; pick the type on the MPC. |
| Shift + Track | Opens the selected track's type chooser on the MPC. |

Inputs view, encoders 1 to 8: Monitor, MIDI In Port, MIDI In Channel, Send To
Track (internal MIDI routing), Audio Out, MIDI Out Port, MIDI Out Channel,
Audio In. Turn an encoder to step through the MPC's choices. Faders keep track
volume; fields that do not apply show dashes.

## Plug-ins and Q-Links

| Control | Action |
| --- | --- |
| Plug-In | Show the selected track's four insert slots. Press again while editing parameters to return to the slots. |
| Push an occupied slot's encoder | Open that insert's parameter pages. Shift+push toggles bypass instead. |
| Push an empty slot's encoder | Open the MPC's Add Effect chooser; finish on the MPC. |
| Shift + Plug-In while editing | Open the Replace Effect chooser; finish on the MPC. |
| Turn a parameter encoder | Adjust that parameter. Push toggles a two-state parameter. |
| Bank < > in parameters | Previous or next parameter page. |
| Flip in parameters | Parameters on faders, track volumes on encoders. |
| EQ | Q-Links 1 to 8 on the encoders. Bank switches to 9 to 16. Shift+Bank changes the MPC's Q-Link mode. Flip puts Q-Links on faders. |

## Transport and editing

| Button | Action |
| --- | --- |
| Play | Play from the current position. |
| Stop | Stop. Press again while stopped to return to the start. |
| Record | The MPC's record toggle. |
| Rewind / Fast Fwd | Move by beats; hold to repeat. |
| Cycle | Loop on or off. |
| Click | Metronome on or off. |
| Save | Save the project. |
| Undo | Undo. Shift+Undo is Redo. |
| Read/Off | Automation Read. |
| Write | Automation Write. |
| Group | Automation Off. |
| Enter / Cancel | The MPC's Enter and Cancel keys. Not every popup accepts them; use the screen when in doubt. |
| Footswitch 1 / 2 | Play / Record, for hands-free looping. Check the X-Touch footswitch polarity setting if a switch reads as pressed at rest. |

The position display shows bars, beats and ticks.

## Jog wheel and cursor keys

The Scrub button switches the jog wheel and the cursor cluster between two
modes. Its LED is lit in scrub mode. The mode resets to scrub when a project
loads or the controller reconnects.

| Control | Scrub mode (LED on) | Data-wheel mode (LED off) |
| --- | --- | --- |
| Jog wheel | Move the position by beats. Shift+jog moves by ticks. | The MPC data wheel: one detent is one step on the focused control, on lists, fields and sliders alike. |
| Up / Down | Cursor up / down. | One data-wheel step up / down, like the MPC's plus and minus. |
| Left / Right | Cursor left / right. | Previous / next control, like Shift+Tab and Tab. |
| Center (Zoom) | Toggle the arrows between cursor moves and zoom. | No action yet. Select/confirm from the controller is planned. |

In data-wheel mode a fast spin is delivered in short bursts of up to four
steps so the MPC's own acceleration behaves. Total travel is kept, so a hard
flick can keep moving for a moment after your hand stops. A step the MPC
refuses while it is busy is dropped, like other controller commands.

## Not available

- Pad names, colors, meters, Select and Arm in Drum Mix; MIDI pan and sends;
  CV track volume.
- Chooser navigation from the controller: Add Track, Add Effect and Replace
  Effect open on the MPC and finish there.
- Plug-in presets, parameter reset to default, and parameter pages for synth
  instances.
- SMPTE display, HUI, and any controller other than the full-size X-Touch.
