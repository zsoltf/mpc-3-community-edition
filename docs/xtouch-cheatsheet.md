# X-Touch cheat sheet

For the full-size Behringer X-Touch in MC mode over USB. It connects when a
project loads and follows the MPC: faders, names, colors and meters update as
you work.

## Setup

1. Set the X-Touch to MC over USB. Hold channel 1 Select while powering it
   on, set encoder 1 to MC and encoder 2 to USB, then press Select. Plug it
   straight into the MPC.
2. In Preferences > MIDI/Sync, turn off Track, Global and Control for the
   `X-TOUCH_INT` input. No MIDI Learn setup is needed.
3. Load a project. The controller connects when it finishes loading, and
   reconnects if you replug it.

Shift is the modifier for every combination below. Shift+Flip turns motor
faders on or off.

## Channel strips

| Control | Action |
| --- | --- |
| Fader | Track volume, or whatever the current view puts on faders. |
| Encoder | Pan, or the current view's encoder control. Push to center pan. |
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
| Save | Not enabled. Save from the MPC. |
| Undo | Undo. Shift+Undo is Redo. |
| Read/Off | Automation Read. |
| Write | Automation Write. |
| Group | Automation Off. |
| Enter / Cancel | The MPC's Enter and Cancel. Some popups ignore them. |
| Footswitch 1 / 2 | Play and Record. If a switch acts pressed when released, change the X-Touch footswitch polarity. |

The position display shows bars, beats and ticks.

## Jog wheel and cursor keys

The Scrub button switches the jog wheel and the cursor cluster between two
modes. Its LED is lit in scrub mode. The mode resets to scrub when a project
loads or the controller reconnects.

| Control | Scrub mode (LED on) | Data-wheel mode (LED off) |
| --- | --- | --- |
| Jog wheel | Move by beats. Shift+jog moves by ticks. | The MPC data wheel. One click is one step on whatever is selected. |
| Up / Down | Cursor up / down. | One step up / down, like the MPC's plus and minus. |
| Left / Right | Cursor left / right. | Previous / next control. |
| Center (Zoom) | Switch the arrows between cursor and zoom. | No action. |

A hard flick can keep moving briefly after you stop.

## Not available

- Pad names, colors, meters, Select and Arm in Drum Mix; MIDI pan and sends;
  CV track volume.
- Chooser navigation from the controller: Add Track, Add Effect and Replace
  Effect open on the MPC and finish there.
- Plug-in presets, parameter reset to default, and parameter pages for synth
  instances.
- SMPTE display, HUI, and any controller other than the full-size X-Touch.
