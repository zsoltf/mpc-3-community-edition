# Building native MPC UI surfaces

This project can add small MPC standalone screens by constructing widgets from
the MPC process's own JUCE runtime. The working integration is exact to the
Gen1 MPC 3.9.1 executable; it is not a general JUCE plug-in toolkit.

Screen authors should start with the [public toolkit quickstart](../ui/README.md#start-here-building-a-screen).
The ABI details below are for maintaining the host adapter, not for writing screens.

## Pinned host

The stock executable has SHA-256
`bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426`.
The current CE input used by the build has SHA-256
`a7690d51ff905bfb7d637f86714596deb205dc72c02ec04dd1b038c2ad45b5a2`.
Both are 112,222,004 bytes and retain build ID
`8371199f945ecc40f79cb19c17ff5de72810a91c` and note `3.9.1.2
(f3af35cdc95)`. The build compares every native UI range between these two
files before emitting the runtime guards. The static JUCE reports 5.4.4; the
matching official source revision used to interpret contracts is
`JUCE@8ec56d15896d687711be348aaace08311ecabcc8`. Raw ELF virtual addresses and
segment mapping are authoritative. The loader bias is resolved and every used
range, relocated vtable slot, detour word, and branch into a displaced second
word is checked before any patch is installed.

## Shared host-widget recipe

[`ui/include/mpclearn-ui.h`](../ui/include/mpclearn-ui.h) and
[`ui/mpclearn-ui.cc`](../ui/mpclearn-ui.cc) are the canonical shared code
toolkit used by the Controllers panel, Global MIDI Learn shell and
[`third-screen.cc`](../ui/examples/third-screen.cc). The native Label/Slider
example is [`slider-screen.cc`](../ui/examples/slider-screen.cc). The private exact-host
profile and adapter API live under [`ui/private/`](../ui/private/). Each
consumer compiles the same implementation into its existing admitted module;
the toolkit owns no settings, mappings, musical state, thread, IPC endpoint or
second JUCE runtime. It provides exact-size host allocation, JUCE String,
TextButton, Label, Slider and ListBox construction, bounds,
per-control visibility, selected state, qualified z-order, parent adapters and
ordered teardown. TextButton uses primary-vtable callback/paint replacement;
the Label and Slider retain their native host vtables. `mpc_ui_button_text` replaces the binding's host-constructed
String, destroys the previous host String and repaints on the admitted UI
thread; public screens never write a TextButton field directly. Raw
`0x00b397a0` is the qualified
`Component::setColour(int, Colour)` entry. Four independently checked MPC calls pass the
Component in `r0`, colour ID in `r1`, and ARGB word in `r2`; the function body
matches the pinned JUCE source and calls the virtual `colourChanged` slot after
a property change. The exact MPC theme table at raw `0x04d29f10` supplies black
TextButton text in both states. Native repair-3 then showed that MPC's inherited
TextButton painting still ignored injected component colours. The shared helper
therefore overrides only the copied TextButton `paintButton` slot and uses the
host's `Graphics::fillAll`, `setColour`, `setFont(float)` and integer-bounds
`drawFittedText` entries at raw `0x00a4d0a4`, `0x00a4c268`, `0x00a4cae0` and
`0x00aa85e8`. The TextButton label remains the host-constructed String at
object `+0xa8`; no duplicate UI runtime or text owner is introduced. Repair-4
rendered this path with full dark/rose fills and legible white labels.

The production adapters live in
[`native-preferences.cc`](../controllers/program-observer/native-preferences.cc):
the Preferences adapter inserts the Controllers tab and delegates
selection requests to the existing MCU session. Completion is reflected on the
next already-qualified stock command UI drain; the callback itself only
publishes the request. The launcher constructor hook binds the exact overlay and
outer Page only for lifetime, context, navigation and destructor ownership. It
runs before the stock constructor stores that Page in the overlay and before the
Page has a parent or useful bounds, so it does not create or register an app
host. Observer state records the overlay, Page, parent and four bounds words at
this constructor phase. The later completed-layout hook records the same five
words again; only after the existing `launcher_second_page` hierarchy guard
proves that the same Page is parented at local `(0,0,1280,690)` does the adapter
register its one consumer and create the launcher entry. This phase split is
covered by ARM composition and the native public-navigation observation.
After that first guarded attachment, repeated layout callbacks
recognize the same live entry screen, parent, control and app host through their
owned instance. They return before applying the pre-insertion 40-child guard to
the now 41-child second page. A partial identity is an error; it does not create
a second entry or relax the first-admission hierarchy proof. The Page's
eight-byte child-record vector at `+0xc8` identifies the
shared rebuild receivers. Repair-7 proved that a child receiver's epilogue is
still too early to inspect the complete parent tree. The hook therefore runs at
raw `0x034560dc`, the captured outer Page layout's common epilogue after it has
laid out the viewport and returned from every child rebuild. Repair-8 corrected
the timing but established that `LDRD r4,r5,[r5,#0xc8]` overwrites the original
Page: both registers hold the record-vector end at the populated-vector
epilogue. The adapter therefore identifies relevance first by matching this
exact iterator to one already-bound Page's guarded begin/end/capacity envelope;
an empty-vector pass still carries Page directly because it branches before
the `LDRD`. The native Component capture established the actual owner chain:
outer Page Component, grid, viewport, two-page content, then two 904px pages at
content-local x=0 and x=904. The native second-page render has seven stock app
entries: its row-two
column-four slot is empty, while the captured tree retains the slot's stock
cell/button pair at local `(678,136,226,136)` and `(681,142,220,124)`. The
adapter attaches one full-paint TextButton to that second page at the latter
bounds. The first page, its sidebar and its New Project/Save/Project/Preferences
footer remain untouched. Repair-9 rendered that exact placement, but its live
ordered child capture then explained why delivered touches did not dispatch:
the injected button was child index 0 and the transparent stock empty-slot
Button at the same bounds was child index 16. JUCE hit testing walks children
from last to first, so the later stock rebuild left the stock Button in front.
The launcher adapter now calls the exact host
`Component::setAlwaysOnTop(bool)` entry at raw `0x00bc2868` after attachment.
Its pinned body changes bit 0 of byte `+0x69` (word `+0x68` mask `0x100`), calls
`toFront(false)`, and notifies the hierarchy. The already-qualified host
`addChildComponent` inserts later normal children before trailing always-on-top
children. This keeps this single launcher above the empty stock slot across a
later rebuild without a scheduler, polling loop, second hook or state owner.
The bit location is proved for this exact executable; it is not inferred from
a source-level bitfield ordinal. No EngineMode or stock receiver record changes.
The shell controls attach as siblings of the outer Page through its qualified
overlay owner. Focus hides the Page and therefore its nested launcher entry;
Back publishes intent, and the next qualified UI drain closes the screen and
restores the two-page app list. It opens without registering a musical
EngineMode. Admission is generated by
[`native-preferences-prepare.py`](../controllers/program-observer/native-preferences-prepare.py),
installed by
[`native-preferences-patch.c`](../controllers/program-observer/native-preferences-patch.c),
and built by
[`native-preferences-build.sh`](../controllers/program-observer/native-preferences-build.sh).

All bounds passed to `setBounds` are local to the immediate Component parent.
The viewport begins at screen x=302 from the 228px grid offset plus its own
74px offset, and at screen y=110 from the overlay owner. When the content is
swiped to its second 904px page, the launcher target's page-local
`(681,142,220,124)` bounds are expected at screen `(983,252,220,124)`.
Screen coordinates are diagnostic results, not layout inputs; repair-6's guard
failed because it searched the overlay owner using screen-space utility bounds.

A minimal public screen follows this shape after its adapter has supplied an
admitted context and parent on the owning UI thread:

```cpp
MpcUiScreen *screen = mpc_ui_screen_create(context, parent,
                                            {0, 0, 1280, 690});
MpcUiControl *back = mpc_ui_button_create(screen, "BACK", BACK_INTENT,
                                           clicked, owner);
if (!screen || !back ||
    !mpc_ui_button_style(back, {0xff2f2f34, 0xffd31145, 0xffffffff}) ||
    !mpc_ui_control_place(back, {64, 578, 240, 64}) ||
    !mpc_ui_screen_show(screen))
    mpc_ui_screen_close(&screen);
```

Call `mpc_ui_control_keep_in_front` only when live ordered-child evidence shows that
a later normal stock child occupies the same hit-test area. The launcher needs
it; the Controllers buttons and shell controls do not.

The public Label/Slider example follows the same owner boundary:

```cpp
MpclearnSliderScreen example = {};
if (!mpclearn_slider_screen_open(&example, context, parent, bounds, 64.0,
                                  action_intent, slider_intent, owner))
    return;

/* Refresh from the consumer's existing owner without publishing an intent. */
mpclearn_slider_screen_set_title(&example, "OWNER VALUE: 72");
mpclearn_slider_screen_set_value(&example, 72.0);

/* Run only after callbacks have returned through the owner's UI drain. */
mpclearn_slider_screen_close(&example);
```

The screen receives only opaque public handles. It has no firmware address,
object size, native header or product state. Its Label uses inherited host
painting, ignores mouse input and accepts no-notification text replacement.
Its Slider publishes one consumer-defined intent with `VALUE_CHANGED`,
`DRAG_STARTED` and `DRAG_ENDED` events and the current double value. Owner
refresh uses the native no-notification setter and therefore publishes no
event. The toolkit invalidates the binding and removes the borrowed listener
before detach, complete host destruction and freeing the binding. The ARM
component fixture exercises those paths. The combined public example also
rendered the inherited Label and Slider on MPC. Action One silently set the
owner presentation to 32 and Action Two to 96. One held drag published
`DRAG_STARTED`, `VALUE_CHANGED` and `DRAG_ENDED` in that order; the owner intent
count advanced once for each event. Back balanced the first combined screen,
and reopening produced a fresh Slider at its initial value of 64. This observes
native appearance, silent owner refresh, input order, deferred close and reopen
for the app-list adapter. The later public-navigation checkpoint also observes
stock Menu suspension, tile resume of the same value-32 instance and idle
suspended-owner destruction before the stock destructor.

The private MPC 3.9.1 profile and normal observer admission guard the exact
Label constructor/destructor/text/justification/mouse/paint ranges and Slider
constructor/destructor/style/range/text-box/listener/getter/setter ranges used
by this implementation. The same admission also guards stock silent-refresh,
listener-dispatch and drag-end callers. These exact-build facts remain private;
future screen authors use only the public API and do not repeat the ABI work.
The public app-host facade now exposes one consumer slot over the existing
qualified launcher owner. Its callbacks receive only opaque context, parent,
bounds and app-host handles. Back requests deferred close; the owning UI drain
calls prepare-close and retains the consumer while it reports pending or while
native detach fails. Forced owner destruction requires the consumer screen to
detach and finish destruction before the app host, parent and context are
released. The adapter invalidates the separate launcher-entry screen before its
close attempt, so callback-active refusal cannot leave that stock-parented
button live. If forced detach or prepare-close fails, the toolkit invalidates
every binding and the launcher adapter retains the app host, screen, parents and
context instead of freeing referenced state. Once the stock owner is gone that
graph is terminal: no later drain, detach or reopen touches its dead native
parent. The retained allocation and live/failure counters make this exceptional
path observable until process exit. The Global MIDI Learn adapter and public
examples compile through this facade. The combined public example's native
open, Back, reopen and repeated layout are observed. Ordinary New Project also
entered the real launcher-overlay destructor with an idle suspended consumer
open. The retained old-inode capture records UI-thread and observer-running
eligibility, callback depth and busy both zero, cleanup before the stock
destructor, stock-destructor return, no retained graph, all toolkit live counts
zero and `failures=0`. Destruction entered from an active callback, with pending
Learn or with a file action remains unobserved. Dropdowns and editable text
controls remain library gaps.

An app host may own multiple screens. On a later completed stock Menu layout,
the adapter now suspends an open idle consumer: the private host records which
parent-owned screens are visible, hides those screens through the existing
qualified focus operation, and leaves screens that were already hidden alone.
The same page-two tile queues resume to the existing UI drain and restores only
the recorded screens, retaining the consumer and its draft. Suspension refuses
an active callback and unwinds a focus refusal; resume similarly hides any
partially restored screens before reporting failure. The Global adapter leaves
the app presented while a file action or Learn arm/cancel state is unsettled so
the existing cancellation path remains reachable. Component and production
composition checks exercise multiple screens, hidden-page preservation,
refusal unwind, queued resume and close while suspended. Native MPC observation
then opened the stock Menu with the public example logically live, rendered the
stock app list, and resumed the same value-32 instance from its existing tile.
The normal Global editor retained its Copy, selected Q-Link 1 target 24 and an
unapplied channel-2 draft across the same stock Menu/tile route, without another
screen, target-selection or edit event. Ordinary New Project destroyed a second
suspended public consumer and a second suspended Global consumer through their
real stock owner.

The existing observer state records each real overlay destructor entry, UI-thread
match, observer-running value, whether a consumer was open, pending/file-action
busy state, toolkit callback depth, successful cleanup before the stock
destructor, stock-destructor return and terminal retention. These are ordering
diagnostics; process exit or a balanced screen counter does not substitute for
the destructor entry and cleanup record. The retained native records are
`build/native-ui-library-qualification/public-navigation-1/native/`:
`example-suspended-menu.log`, `example-resume.log`, `normal-draft.log`,
`normal-menu.log`, `normal-resume.log`, `example-forced-capture.log`,
`example-forced-state.bin`, `normal-forced-capture.log` and
`normal-forced-state.bin`.

[`toolkit-example.cc`](../ui/examples/toolkit-example.cc) is the combined
public-only qualification consumer: one native Label, one Slider, Action One,
Action Two and Back. A development observer selects it in the same existing
page-two slot with `MPCLEARN_NATIVE_UI_EXAMPLE=1`; the default build selects
Global MIDI Learn. This changes no tile count, navigation parent, focus owner or
UI drain. The authored consumer includes only the public header and has no
firmware address or private host type.

## Global MIDI Learn owner readback

The assignment screen in
[`global-midi-learn-screen.cc`](../ui/screens/global-midi-learn-screen.cc) is a
public toolkit composition. Its production adapter remains inside
`native-preferences.cc`, where the exact MPC mapping owner is available. The
screen holds only transient page and selection state. It neither parses XMM
files nor owns mappings, MIDI input, saving or Learn capture.

The grouped replacement follows the stock MPC split between a selectable list
and the selected item's editor. The public model in
[`global-midi-learn-groups.cc`](../ui/screens/global-midi-learn-groups.cc)
covers target IDs 0 through 119 with 14 contiguous categories: Pads, Pad Banks,
Q-Links, Transport & Navigation, Global Controls, Sequence & Track, Pad
Performance, Looper, Step Sequencer, Editor Tabs, Modes, Panels, Browser and
Additional Targets. Those ranges match the repository action catalogue, while
the adapter still asks MPC's owner for every displayed target label; grouping
does not replace or synthesize target identity. At the 1280 by 690 app-parent
size, it reserves a 248-pixel
group list, a 420-pixel target-name list and a 516-pixel selected-assignment
detail column. Learn/Cancel and Back remain separate footer actions. Channel,
message and mode metadata appear only in the selected-assignment detail, rather
than being repeated in every target row.

Both columns now use the canonical toolkit's native ListBox wrapper. Each list
owns one native 172-byte widget and one toolkit presentation model; mapping and
session state remain with the consumer. Owner refresh calls `updateContent`,
`selectRow` and `repaint` while selection intent is suppressed. The repaint is
required when two groups have the same row count: grouped-list-2 updated its
model from Pads to Q-Links but otherwise kept the old Pad pixels. A real user
selection alone publishes the group or target intent. Each list gets its
borrowed native Viewport and enables the host drag listener with the stock
8-pixel threshold. Close invalidates the binding, disables that listener
outside an active callback, calls `setModel(nullptr)` while the model remains
alive, hides and detaches the widget, then runs the native complete destructor
and frees the model. A failed detach retains an inert widget/model pair for the
existing close retry.

Grouped-list-2 demonstrated both lists rendering on MPC, then exposed three
host-behavior gaps: a row drag selected Pad 8 and did not scroll, a three-row
group left the unused ListBox viewport white, and a same-count group change
left old Pad pixels. Grouped-list-3 observed the corresponding production
repairs. Forward and reverse swipes moved both the target and group lists
without incrementing owner selection counters; later taps selected one row
once. Pads to Q-Links repainted immediately, Additional Targets kept the unused
viewport dark, and selected Q-Link 1 showed `CH1 CC13 ABS` in the separate
detail. Two released-swipe, Back and reopen cycles ended with screens and
shells balanced `2/2`, `failures=0`, and the learner idle.

One attempted tap immediately after reconnecting the development viewer had no
recorded effect. Later top-row, scrolled-row and post-reopen first taps worked.
This observation qualifies the listed gestures and lifetime cycles, not broad
input reliability or every viewer reconnect. The implementation is in
[`mpclearn-ui.cc`](../ui/mpclearn-ui.cc), with exact addresses and sizes in
[`mpc-3.9.1-profile.h`](../ui/private/mpc-3.9.1-profile.h); the product layout
and intent adapter remain in
[`global-midi-learn-screen.cc`](../ui/screens/global-midi-learn-screen.cc).

This ListBox adapter is exact-build code, not a source-layout guess. The native
widget is 172 bytes; raw constructor `0x00be5920` takes
`(this, const HostString*, ListBoxModel*)`, and raw complete destructor
`0x00bd6a38` does not own the model stored at widget `+0x78`. The wrapper calls
raw `setModel` `0x00bc7a94`, `updateContent` `0x00bc71c0`, `setRowHeight`
`0x00bc770c` and `selectRow` `0x00bfadc8`. Raw `getViewport` `0x00b67470`
returns a ListBox-owned borrowed pointer. MPC's host-specific
`setScrollOnDragEnabled` at `0x00b89c64` takes `(Viewport*, bool, float)`; the
third argument is required and qualified stock callers pass `8.0f`. This
differs from the matching public JUCE declaration, so consumers must use the
private exact-build adapter rather than a source-level two-argument call. The
wrapper uses the existing admitted Component `setColour` with the matching
JUCE `ListBox::backgroundColourId` `0x01002800` to fill empty viewport space.
It copies the 68-byte model table at
VA `0x068ca8c4` and supplies the destructor pair, row count, row painter,
null-component refresh, item-click and selected-row slots. The remaining native
defaults include the no-drag result `0x00b275e8`, empty tooltip `0x00b26774` and
normal cursor `0x00b2608c`. Stock Preferences raw `0x030f818c` independently
shows the split widget/model construction. Production admission guards every
called range and compares this table's raw bytes between both pinned inputs.
Because the loader relocates this `.data.rel.ro` table before observer startup,
live admission checks its RTTI and every retained default slot as relocated
pointers rather than comparing the live table to raw file bytes. The executable
identity remains stock SHA-256
`bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426`.
The matching source contract is JUCE
`8ec56d15896d687711be348aaace08311ecabcc8`,
`modules/juce_gui_basics/widgets/juce_ListBox.{h,cpp}`.

The adapter captures the stock owner at raw `0x00e40628`, immediately after
the stock `MidiLearnViewController` constructor. Before replaying the displaced
`mov r0,#72; str r5,[r4,#0x7cc]`, it validates owner `[r4+0x3c0] == r7`, view
controller owner `[r5+0x138] == r7`, view-controller file handler
`[r5+0x13c] == r6`, and handler getter `handler(r7+0x19c) == r6`. The constructor
loads the stack owner/handler arguments into r5/r6, saves them at stack `+0x30`
and `+0x34`, then its SIMD store at raw `0x02ad8648` places them at those two
view-controller offsets. It records the UI thread at this owner entry. An
exhaustive direct A32 branch scan found no branch into the second displaced
word. The guarded startup range is `0x00e405c0+0x70`, SHA-256
`0dfb7cacb874b07f9ad9399eea29962b51e926f48e41836f1cc5da352b97d4d4`.

For each fixed target ID 0 through 119, the adapter performs this bounded read
sequence on that thread:

1. Decode the stock label through raw `0x02fa6e84` into a constructed host
   String, copy it into the 64-byte row view and destroy the String.
2. Construct the 12-byte target through raw `0x018bdd90` and call lookup raw
   `0x018b5840` with `owner+0x19c`, never a guessed file field.
3. Treat the returned mapping pointer as borrowed. Copy type `+0x9c`, control
   `+0xa0`, channel `+0xa4`, data1 `+0xa8` and reverse `+0xa9` immediately.
   A present mapping is displayable only when type is nonzero and channel is
   not `-1`.
4. Format a valid mapping through raw `0x018bed00`, copy its constructed host
   String, and combine it with the synchronously copied metadata in a bounded
   assignment description kept separately from the target-name list. The
   adapter names the native-observed type 1 as `NOTE`,
   type 2 as `CC`, type 3 as `PITCH BEND`, and control mode 5 as `ABS`; other
   values remain explicit numeric `TYPE`/`MODE` values instead of guessed
   names. It then destroys the
   host String and target and retains no pairing pointer across a refresh or
   selected-file change.

The lookup result is eight bytes `{mapping pointer, found byte, padding}` and
is zeroed before every call because the miss path writes only the found byte.
Raw `0x018b49f8` returns the file handler from the owner collection; it does not
return a selected file. The selected file is the borrowed pointer at handler
`+0x70`. The host String destructor is raw `0x00911f58`. Admission also guards the stock Global table caller at
`0x02fa54b0+0x32c`, whose call order and object lifetimes establish these ABIs.
The assignment-format-1 native checkpoint loaded observer
`1156b136963335bf7123b12e60550ce4a761ff730f43d0d3a02f0ea5c75e162a`
in MPC PID 5324. Startup captured one owner, view controller and file handler
with zero failures. The real page-two route rendered Q-Link 1-6 as
`CH1 CC13 ABS` through `CH1 CC18 ABS`, matching the retained XMM's type 2,
channel 1, data 13-18 and control 5 fields. That checkpoint used the superseded
six-button presentation: four Next touches reached targets 25-30, row selection
updated the selected target to 25, and two
Back/reopen/Back cycles ended with assignment screens and shells both at `2/2`,
no live assignment screen and zero failures. That observation did not invoke
Learn, Cancel or persistence. Playback resource cost and audible behavior were
also not exercised. It does not establish native rendering or interaction for
the current grouped ListBox presentation.

## Global MIDI Learn intent and persistence chain

The adapter keeps the captured stock `MidiLearnViewController` borrowed and
app-owned. The startup hook runs before the displaced store at raw
`0x00e4062c`; after replay, `[app+0x7cc]` names the same view controller,
`[vc+0x138]` names the captured mapping owner, and all calls remain on the
captured UI thread. A row click constructs the same fixed 12-byte Target used
for readback and calls raw `0x018737dc(owner+0x38, &target)`, the property path
used by stock row selection. Selection alone does not arm Learn.

Learn and Cancel call only raw `0x02ad5e0c(vc, bool)`. That entry increments
the pending word at `vc+0x10` and queues through the scheduler at `vc+0x0c`.
Its owner closure at raw `0x02ad7da4` calls the bool Property setter on
`owner+0x0c`, then balances the pending count. The registered owner listener at
raw `0x02ad53f4` queues mirror closure `0x02ad7d5c`, which updates the view
controller's UI state at `vc+0x14`; its current mirror byte is `vc+0x23`.
The adapter never calls either closure or the bool Property setter directly.
The stock touch hide path at raw `0x0353acd8` removes its listeners and
tail-calls the same false intent at `0x0353af68`, corroborating cancellation
through the view controller rather than a direct field write.

`owner+0x34` is the Learn owner value. It is distinct from
`(owner+0x19c)+0x34`, a field inside the selected-file collection; the earlier
inference that treated those as one property was withdrawn. Intent return is
not acknowledgement. The existing bounded UI drain checks `vc+0x10`,
`owner+0x34` and `vc+0x23`; it accepts the requested state only when pending is
zero and both bytes equal the request. While pending or armed, the adapter
holds the selected target and page fixed. Back requests false and retains the
screen and callback binding until the same false/drained condition is observed.
An unsettled bounded wait leaves Cancel visible and does not fabricate a close
or an armed receipt.

The arm-cancel-1 native checkpoint loaded observer
`9046309ea22c7dc66eb49210bf608628c861f093c84c69e0dc13a8e3bf511a0d`
in MPC PID 16064. A touch selected target 1 once. Learn request 1 settled with
pending zero, owner value 1 and mirror value 1. Touching another row while
armed left target 1 selected and left the target-selection count at one.
Cancel request 2 settled with pending zero and both values false. A second
Learn settled, then Back issued cancel request 4; false settlement completed
before the assignment screen was destroyed. Reopening showed idle state, and
the second Back left assignment screens and shells balanced at `2/2` with zero
failures and zero timeouts. The test delivered no controller gesture. Every
pre-existing mapping remained byte-identical; the only additional file was the
root-owned disposable test copy, which also stayed byte-identical throughout
this checkpoint.

The automatic persistence path is statically mapped. A controller gesture at
raw `0x018a1550` copies the selected
Target from `owner+0x60` and commits through
`0x018b7558(owner+0x19c, &Target, &Mapping)`. The collection change signal
queues `0x018b454c`, which reaches `0x018b44f0`; the collection's separate
pending word is at `collection+4`. Raw `0x018b49f8(collection)` returns the
file handler stored at `collection+0x50`; the selected file is at
`handler+0x70`. The selected file's XMM virtual write
slot `+0x3c` resolves to raw `0x018a87b4`, then raw `0x00987a70`. The final
writer bool is discarded, so queue drain is not evidence of disk durability or
save success. No separate Save ABI is qualified, and the candidate exposes no
Save control. Grouped-list-3 delivered one physical Mini knob gesture while
target 24 was selected and Learn was armed. Its open assignment changed from
`CH1 CC13 ABS` to `CH1 CC16 ABS` with `pairing_changes=1` and
`row_repaints=1`; independent XMM parsing found target 24 was the only changed
pairing among all 120 in the MPC's automatically created disposable copy.
Explicit Cancel then settled request 2 with pending, owner and mirror all zero.
A second Learn followed by Back settled false as request 4 before the screen
closed. MPC PID 30856 then exited normally with the observer session complete
and clean. Fresh MPC PID 8453 loaded the owner-written disposable copy and the
grouped screen rendered target 24 as `CH1 CC16 ABS`, selected and idle. This
observes the owner commit, open-screen repaint, explicit cancellation,
cancel-before-Back and disk-backed fresh-process reload. Original mapping and
selection hashes were subsequently restored and both disposable XMM files were
removed. Playback resource cost and audible behavior were not exercised.

## Mapping-file controls

The current candidate adds a mapping-file header above the grouped target
editor. It shows the selected file name and exposes previous/next selection,
New, Copy, Import and a conditional Export. Clear Assignment and a compact
manual draft for type, channel, number, control mode and reverse stay in the
selected-assignment detail. These controls publish public screen intents; the
production adapter resolves them against the captured view controller,
collection and file handler on the same admitted UI thread. Manual Apply and
Export are enabled only for a transient New or Copy that this screen observed
MPC create. They are not enabled for an arbitrary imported desktop preset.

The handler owns an eight-byte-entry vector at `+0xa4..+0xa8`. Each entry
contains a borrowed file pointer and its native control pointer. The adapter
takes only a bounded synchronous snapshot, compares the selected pointer from
`handler+0x70`, and retains neither entry field. Raw `0x018a7da8` returns the
selected file's name as a constructed host String; the adapter copies it into
its 64-byte view buffer and destroys the String immediately. Previous and Next
refresh the collection before dispatch and call raw `0x02ad71e0(vc,index)`, so
the native owner promotes and selects the indexed entry. Direct selected-pointer
writes and directory ordering are not used.

New calls raw `0x02ad7adc(vc)`. Copy calls raw `0x02ad5f88(vc)`. Import enters
the same chooser at `[vc+0x130]`, vtable slot `+8`, raw `0x01801cd4`, from the
ordinary toolkit TextButton callback. The chooser runs a synchronous nested
modal dispatch. The adapter marks the action active before entry, suppresses
recurring screen refresh and every other action during the nested loop, and
leaves the screen and callback binding alive until it returns. An empty returned
host String means only that no file was added; it is not labelled Cancel.

The stock Import wrapper is unsuitable for an Akai desktop preset on this MPC:
it registers the source first, chooses `/usr/share/Akai/SME0/Midi Learn` from
Manufacturer, and then fails its copy because `/dev/root` is read-only. Native
observation showed the error while the registered source still became selected
and rendered its supported Q-Link assignment. The current candidate instead
constructs the source File from the chooser result, requires a regular source,
constructs the same basename below MPC's writable user MIDI directory, and
refuses an existing destination before calling the native byte-copy. It opens
the copied destination through raw `0x018bed5c`, registers the returned shared
file through `0x018ad794`, finds that exact object in the handler vector, selects
its actual index through raw `0x018b3420`, releases the temporary shared
reference and destroys every File and host String in reverse lifetime order.
It never routes through Manufacturer, the native writer or an adapter-owned
parser. No-addition, collision, added-and-selected and failed outcomes remain
distinct. Native observation byte-copied the exact Akai desktop fixture into the
writable user directory and selected it. Repeating the same import showed the
destination-collision state without changing the destination bytes, catalog
count or selection. A fresh process subsequently reopened the edited native
Copy/New results described below.

Clear constructs the same 12-byte Target and calls raw
`0x02ad60f0(vc,&target)`. MPC's own restriction remains authoritative:
`handler+0x9c` mirrors whether the selected file's Manufacturer is non-User,
and the control is hidden when that byte is true. The adapter confirms Clear by
reading the selected assignment again. It does not write pairing fields or an
XMM file itself.

Manual Apply constructs a 24-byte native MidiMessage for Note, CC or Pitch,
constructs the 172-byte native Mapping through raw `0x018ba570`, and commits it
through raw `0x018b7558(collection,target,mapping)`. It then applies the stock
control-mode and reverse setters at `0x018b6220` and `0x018b6360`. The temporary
Mapping's owned allocation and three signal blocks, the MidiMessage and copied
Target are destroyed in the order proven by stock callers. The adapter retains
none of them, reads the authoritative assignment back, and reports settlement
only when type, channel, number, mode and reverse match. No MIDI message is sent
to an input path. The full-editor native checkpoint applied CC75 to target 24
of a Copy whose imported source value was CC74; comparison found target 24 was
the only changed pairing, and a fresh MPC process reopened CC75. Exception
behavior outside the observed valid inputs remains unobserved.

The earlier file-controls checkpoint opened and returned from the chooser twice
with unchanged count, index, mapping and pairing hashes. The later full-editor
checkpoint imported the exact Akai fixture into the writable user directory and
matched source and destination bytes. Copy plus Apply changed only target 24
from CC74 to CC75. Clear changed only target 25 to native unassigned defaults.
New MIDI Mapping 9 saved target 24 as CC1, and Export produced destination bytes
identical to that native common-subset source. A fresh MPC process reopened the
Copy, Clear and New results with those assignments. The state reader distinguishes
Import no-addition from added registration, collision and failure; the native
destination-collision branch preserved the existing file and catalog state.

The stock Export wrapper forwards an empty chooser result to its writer. Native
observation of that path produced an error alert for an empty filename. The
current adapter invokes the same admitted chooser directly, tests the returned
host String before constructing a File, and reports an empty result as a
completed no-write cancellation. A nonempty result still enters the existing
native writer and treats its false return as failure. Component composition
exercises both branches. Native observation of the repaired build returned from
Cancel with no alert, no destination and unchanged mapping bytes, then completed
a named Export whose destination bytes matched the selected Copy. A following
Import Cancel also settled without changing the catalogue.

The native desktop codec has a known compatibility limit. The stock writer
path `0x018a87b4 -> 0x018baf40` skips targets whose signed control ID exceeds
119. Its tree-copy helper preserves device and payload children but does not
generically preserve desktop preamble, sync and Version metadata, and it
normalizes Manufacturer to User. Copy invokes that writer before selecting its
new file. The direct Import candidate byte-copies before opening and registering
the destination, so merely importing and inspecting its supported targets does
not authorize a later lossy save. Desktop
Korg nanoKONTROL2 mappings contain 251 pairings, while an inspected MPK mini3
mapping has 117 pairings plus device and synchronization metadata. The screen
therefore presents only MPC's 120 exposed targets, does not rebuild a file from
those rows, and does not claim that Copy or a later native write is lossless.
Lossless desktop editing is optional future work. Standalone editing and the
native common subset are the accepted priority; preserve the original desktop
file when its extra targets or metadata matter. No sidecar or second serializer
has been added to the primary implementation.

The retained native records for this grouped-screen sequence are under
`build/global-midi-ui-qualification/grouped-list-3/`: `physical-gesture.log`,
`physical-cancelled.log`, `armed-back.log`,
`saved-semantic-comparison.json`, `fresh-process-selected.log` and
`fresh-process.png`; `restoration-hashes.log` records the final restored hashes
and disposable-file cleanup.

These raw ranges are admitted only for the pinned stock MPC executable SHA-256
`bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426`
and build ID `8371199f945ecc40f79cb19c17ff5de72810a91c`. Code virtual addresses equal
file offsets in the executable segment; Ghidra's imported view adds `0x10000`.
The intent entry and its two closures have exact 32-byte guards in addition to
the full entry/listener/property ranges used by production admission. The
arm-cancel-1 checkpoint establishes native selection, Learn, explicit Cancel,
selection lock while armed and cancel-before-Back through this chain. It does
not establish cancellation while MIDI delivery is queued or automatic
one-gesture cancellation. Grouped-list-3 observes persistence through a fresh
MPC process as described above, but does not establish playback behavior. The
current adapter uses the existing UI drain to inspect one
displayed pairing every four drains. While armed it prioritizes the fixed
selected row. The poll constructs and destroys one fixed Target, performs the
qualified lookup and compares only the copied type, control, channel, data1 and
reverse scalars. It does not decode labels or format unchanged mappings. A
changed mapping is formatted while its pointer is borrowed, copied into the
bounded assignment buffer and repainted through the existing toolkit control. The
poll emits no Learn intent and owns no pairing state beyond this presentation
cache. Its native gesture repaint and fresh-process persistence are observed as
described above. Polling cost during playback and launcher destruction while
busy or inside an active callback remain unqualified. Idle suspended-owner
destruction is observed through ordinary New Project.

The app-screen sequence above is demonstrated on the exact integrated source
checkpoint `6c179370b18d02122479ffdbd492d6e91e1386ff`, with loaded observer
`956d0a4723a6f5adf90c9d490165400cfac92d1f3770834f66a0b7f14a4ffd8b`
in MPC PID 8742. Retained native captures show the stock first app page, one
Global MIDI Learn entry on page two, the legible named shell, and the rendered
public-only third screen. Delivered touches opened each screen, produced Action
Two and Action One intents in that order, and used Back to return to the shell
and then page two. Repeating the flow ended with `shells=2/2`, `examples=2/2`,
the shell and example live counts at zero and `failures=0`. The images
establish the rendered states and return path; the state transitions distinguish
the two action callbacks and record the balanced screen lifetimes.

The later combined public consumer used the same page-two slot and owner. It
rendered the native Label and Slider beside Action One, Action Two and Back.
The actions silently refreshed the owner presentation to 32 and 96. A held
Slider drag delivered start, value-change and end events in order. Back balanced
the first combined screen, reopening restored the initial value 64, and repeated
Main/Menu layout retained one entry with zero failures. These observations add
the public Label, Slider and one-consumer app-host facade to the qualified
app-list path without qualifying a different stock parent.

This qualifies the public `MpcUiScreen`/`MpcUiControl` construction, button
style, absolute/grid layout, intent callback and deferred-Back recipe for the
observed app-list adapter. It does not qualify attachment to an arbitrary stock
owner. Those earlier cycles kept the launcher overlay live. The later
public-navigation checkpoint captured complete idle owner teardown for both the
combined public example and the normal Global consumer before each stock
destructor returned.

The owning adapter closes its toolkit screen, then destroys its parent and
context on the same stock-owned UI thread before the host parent starts its own
teardown. The toolkit invalidates callbacks, detaches children, restores the
stock primary vptr, runs the host complete destructor, calls the matching sized
delete and frees the copied table. Secondary vptrs remain host-installed.
Callbacks stay small: controller selection publishes one validated request to
the existing session owner, and Back publishes navigation intent. The already
qualified command UI drain performs close after the callback returns. This
introduces no `callAsync`, message-loop shim or thread.

For each new host class or entry point:

1. Identify the stock owner, real UI-thread entry, object size, constructor,
   complete destructor, primary vtable, parent, bounds, visibility and callback
   slot from the pinned ELF and matching source.
2. Guard all called code ranges and live relocated vtable targets. Guard both
   displaced words and reject direct branches into the second word.
3. Construct through the host allocator and host String/widget methods. Copy
   only the primary vtable and change only the required callback/destructor
   slots.
4. Use the exact stock navigation parent and z-order. A visible component with
   the wrong parent can paint yet miss touch or focus.
5. Remove children, restore vptrs, destroy objects and free allocations before
   chaining the stock owner's destructor. Retain counters for created,
   destroyed and live objects.
6. Freeze one observer/session build, then inspect the real rendered content,
   touch callbacks, repeated open/close, stock actions before and after, native
   CPU/RSS during playback, and audible behavior. Component tests remain
   substitutions for MPC rendering, focus, touch and stock ownership.

## Capability matrix

| Capability | Status | Boundary |
| --- | --- | --- |
| Current canonical Controllers consumer | Tested | On the integrated source checkpoint, the panel renders without the stock placeholder, with complete dark/rose buttons, legible white labels and a visible selected marker. Native Generic MCU and X-Touch Mini touches produced owner requests 1 and 2, each claimed and completed with `status=active` and `failures=0`; the marker repainted on the open tab, and restoring Mini restored the exact pre-run preferences SHA-256. Full stock navigation remains open. |
| Explicit full-button fill, white 22px fitted label and selected accent | Tested | The integrated Controllers panel, shell and public example render complete dark/rose buttons with legible white labels; Controllers also demonstrates selected-state painting and repaint on the open tab. |
| Host Component/TextButton construction, app-screen layout, callback/paint-vtable replacement and screen teardown | Tested | The integrated app-list path rendered and accepted touch through the shared controls. Two shell/example cycles balanced at `2/2` with zero live screens and zero failures. A later ordinary New Project capture observed cleanup of an open, suspended consumer before the real stock launcher destructor returned. Busy and active-callback destructor paths remain unobserved. |
| One second-page Global MIDI Learn app entry, shell and Back with stock receiver records unchanged | Tested | The integrated native captures preserved the visible stock page-one and page-two layout, showed exactly one page-two entry, opened the shell by touch and returned to page two through deferred Back. The production composition separately retains the stock receiver records. The qualified always-on-top operation keeps the transparent stock empty-slot Button behind it. |
| Live Global MIDI Learn target/assignment screen | Readback, grouped native-list render/input/lifetime, Learn/Cancel, owner-committed repaint and fresh-process persistence tested | Assignment-format-1 rendered live labels and readable pairings through the earlier button view. Grouped-list-3 scrolled both native lists in both directions without owner selection, selected later taps once, repainted Pads to Q-Links at the same row count, kept Additional Targets' unused area dark and balanced two released-swipe Back/reopen cycles at screens and shells `2/2` with zero failures and an idle learner. One physical Mini gesture changed selected target 24 from `CH1 CC13 ABS` to `CH1 CC16 ABS` on the open grouped screen; only that target changed among all 120 pairings. Explicit Cancel settled owner/mirror/pending false. A second Learn followed by Back settled false before close. A fresh MPC process loaded the owner-written copy and rendered target 24 as `CH1 CC16 ABS`, selected and idle. One attempted tap immediately after viewer reconnect had no effect, so broad input reliability is not established. No Save control is exposed; the stock owner persists the gesture. Cancellation while MIDI delivery is queued, automatic one-gesture cancellation, busy or active-callback launcher teardown, playback and complete review remain unqualified. Idle suspended launcher teardown is observed. |
| Mapping file controls, Clear and manual assignment draft | Native writable Import, collision refusal, Copy, Clear, manual Apply, New save, Export, no-write Export Cancel and fresh-process reopen tested for the common subset | The full-editor checkpoint byte-copied the exact Akai fixture to the writable user directory. Re-import preserved the existing destination, catalog count and selection. Copy plus Apply changed only target 24 from CC74 to CC75; Clear changed only target 25; New saved target 24 as CC1; Export matched its native common-subset source bytes. A fresh MPC process reopened the Copy, Clear and New results. The repaired empty-result Export Cancel returned without an alert or destination and left the selected Copy bytes unchanged; a subsequent named Export remained byte-identical. Copy/Export still do not promise preservation of desktop-only targets or metadata through the native writer. |
| Public-only third toolkit screen through the launcher owner | Tested | The button-only `ui/examples/third-screen.cc` rendered on MPC, produced distinct Action Two and Action One intents, returned through Back and balanced two cycles. The combined public consumer then exercised the same actions with the native Label and Slider, balanced one Back and reopened fresh. This proves the observed app-list adapter, not arbitrary owner attachment. |
| Native Label and Slider public wrappers | Tested on the app-list adapter | `ui/examples/slider-screen.cc` and `ui/examples/toolkit-example.cc` use only opaque public handles. MPC rendered both native controls. Action One silently refreshed the owner presentation to 32 and Action Two to 96 without a Slider intent. One held drag delivered `DRAG_STARTED`, `VALUE_CHANGED`, then `DRAG_ENDED`; Back balanced the screen and reopen restored the initial value 64. Stock Menu suspension and tile resume retained the same value-32 instance; ordinary New Project then destroyed a second idle suspended instance before the stock owner returned. |
| Public app registration/open/close facade | Open/Back/reopen, stock Menu suspend/resume and idle forced-owner teardown tested on the one app-list consumer slot | The private launcher adapter supplies one opaque host from its admitted context, navigation parent and local bounds. The combined public example registered without firmware details, opened, handled button and Slider input, requested Back from the active callback, closed through the owner drain and reopened. Main/Menu exposed the stock list while retaining the consumer; the existing tile resumed the same value-32 instance. The normal Global consumer retained an unapplied draft through the same route. Ordinary New Project captured cleanup before the stock destructor with open consumers, all live counts zero and no failures. Component checks additionally cover multiple screens, hidden-page preservation, focus-refusal unwind, pending-close refusal and close while suspended. Busy and active-callback owner destruction remain unobserved; arbitrary tile placement is not exposed. |
| Distinct launcher icon or first-class EngineMode | Unknown | The shell intentionally does not add an EngineMode or claim a custom icon. |
| Stock `SequenceSettingsDialogue` Name text-entry flow | Static only | The exact host factory, default session construction, three callback paths, two outer launcher routes and destruction order are mapped below. No callback has been invoked by this project, accept/cancel are not mapped, and focus, soft-keyboard ownership and backing-owner lifetime remain unobserved. |
| CE-created TextEditor or arbitrary text-entry flow | Unknown | The stock Name flow does not expose a safe general constructor or callback contract. Session strings, scalar/flag meanings and callback 2/3 signatures remain unknown. |
| ComboBox, editable text or arbitrary custom Graphics paint | Unknown | Label and Slider do not qualify popup ownership, keyboard/focus behavior or a general painting surface. Each requires its own exact ABI and native lifetime observation. |

This checkpoint supplies the narrow exact-firmware MPC screen code toolkit and
three production consumers. The canonical Controllers panel, launcher, shell,
Global MIDI Learn editor and public examples are demonstrated through their
real render, touch and owner callbacks. The grouped editor additionally
demonstrates native list scrolling, owner-queued Learn/Cancel, live assignment
refresh and owner-written fresh-process recall. The newer file controls and
manual common-subset assignment path are also observed through fresh-process
reload; their desktop-only preservation and collision limits remain as listed
above. Stock Menu suspend/resume and idle suspended-owner destruction are
observed on both the public example and normal editor. Busy or active-callback
owner destruction, playback resource/audible qualification and the complete
milestone review remain separate acceptance work. General-purpose
attachment to other stock owners, arbitrary launcher placement, dropdowns,
text entry, keyboard ownership, custom Graphics and plug-in editor hosting
require their own ABI and native observations.

## Static stock text-entry map

The stock `SequenceSettingsDialogue` Name field is the narrowest known benign
text-entry path in this exact executable. This section is an ABI map for a
future finite observation. It is not a callable toolkit API, and none of these
methods or callback paths has been invoked by this project.

The dialogue's screen-control builder constructs a host-default `0x48`-byte
session whose callbacks are initially null. The stock `TouchTextEditor` factory
installs only callback 1. Callback 1 and callback 2 each invoke their target and
then close the launcher. Callback 3 is copied into the editor binding and has a
separate nonterminal invoke path, but its source is null in this Name session.
These neutral callback numbers are the static session-field ordering; matching
JUCE names such as `onReturnKey` and `onEscapeKey` do not establish which one
means accept or cancel in the MPC wrapper.

All ranges below are raw ELF virtual addresses. They map to the same file
offsets in the executable's first `PT_LOAD` segment. Their hashes were computed
from the pinned stock executable named above.

| Stock ABI locator | Range and SHA-256 | Static finding |
| --- | --- | --- |
| `SequenceSettingsDialogue` screen-control builder | `0x02bbee14..0x02bbf15c`; `0c7e10e29fe48a36ce62a4bce1051de5f531db1a6e2b15aa6b6fa920e76186c8` | Builds the stock Name control and session path. RTTI uses vtable raw `0x068b06e8`, mapped RVA `0x068b16e8`, with typeinfo name `24SequenceSettingsDialogue`. |
| Session default constructor | `0x01aef8e4..0x01aef99c`; `f845a51e692113f6bcb8858f3749ebbd8ec29c617424153577bdf022021726bb` | Initializes the `0x48`-byte session, including null callback state. |
| Callback 1 terminal path | `0x035d8078..0x035d80cc`; `30c2c18acafbbc2cec74bbc916d641d1167cbf9c16cb415a7df33556572bacd1` | Invokes callback 1 and then closes. Its user meaning is not yet observed. |
| Callback 2 terminal path | `0x035d80cc..0x035d8120`; `bcef8fb1fb57b4f32eb5de25fe09df3e3186edbb861f3b113f7e163167029d14` | Invokes callback 2 and then closes. Its user meaning is not yet observed. |
| Callback 3 propagation | `0x036732b8..0x036735e4`; `5bd425dbaee7fae1a00e492d2eaa4d69febdf8b4c9a6521d76a8f103f0d9e248` | Copies the third callback into the editor binding. Its signature is unknown. |
| Callback 3 bound invoke | `0x0367171c..0x03671968`; `4646d9063c6f7f9df40a256ffa0f1e20dc8e263b83a40868be04826b4ccf8dbe` | Invokes the bound nonterminal path. It is not safe to synthesize. |
| `TouchTextEditor` complete destructor | `0x034d1eec..0x034d2088`; `4e5d58331e6dda43942c5ca13c905bba17c6bf4b65db0bee7445b068b9b2dc15` | Closes its active launcher before destroying its callback storage in the order 3, 2 and 1. |

The active `TouchTextEditor` launcher pointer comes from the outer factory
source at offset `+0x80` on wrapper route A or `+0x3e8` on wrapper route B. The
earlier `+0x3f4` attribution was incorrect and must not be reused. Static
analysis has not identified which route the stock Name action uses at runtime.

The source interpretation is pinned to
`JUCE@8ec56d15896d687711be348aaace08311ecabcc8`. Exact useful source locators
are `modules/juce_gui_basics/widgets/juce_TextEditor.h:41-43` for the
`Component`/`TextInputTarget` inheritance, the same file at `328-340` for the
text, return, escape and focus callbacks,
`modules/juce_gui_basics/widgets/juce_TextEditor.cpp:872-896` for construction
and destruction, and the same file at `2059-2133` for focus and callback
dispatch. `modules/juce_gui_basics/keyboard/juce_TextInputTarget.h:41-95`
defines the input-target and virtual-keyboard contract. These source locators
help interpret host code; they do not prove the MPC-private
`TouchTextEditor` wrapper's field meanings or lifetime.

A future native qualification should stay lifecycle-only and use the stock
Sequence Settings Name action:

1. Record only hook counts, object identities, thread IDs, route choice and
   lifecycle order. Do not capture, log or hash any text or text bytes.
2. Distinguish wrapper route A (`+0x80`) from route B (`+0x3e8`) at the outer
   factory source before interpreting launcher ownership.
3. Exercise the normal accept and cancel gestures in separate finite runs and
   observe which terminal callback fires, whether it fires once, and whether
   close follows on the same owning UI thread.
4. Keep callback 3 unqualified: it is null in the stock Name session, so this
   flow cannot exercise the mapped nonterminal invoke. Before using it,
   identify a separate stock flow with callback 3 present and qualify its
   signature, owner and interaction there. Do not replace or synthesize it.
5. Close and reopen the stock dialogue, then navigate away while it is active;
   verify that the `TouchTextEditor` closes its launcher before destroying its
   callback storage in the order 3, 2 and 1, with no duplicate or live object
   left behind.
6. Verify focus transfer, the visible soft keyboard, touch delivery and return
   to a usable stock screen without inspecting entered content.

Until that observation completes, accept/cancel mapping, source owner and UI
thread, launcher and backing-owner lifetimes, focus/modal behavior, session
string ownership, scalar/flag meanings, callback 2/3 signatures and callback
reentrancy are unknown. The maximum statically supported future construction
experiment is the host default session constructor followed by the stock
factory's callback-1 replacement. It is not authorization or evidence for a
CE-created text editor, a reusable soft keyboard or a general text-entry API.

## UI families and plug-in boundary

Injected CE host widgets are process-owned additions to a known MPC screen, as
used here. MPC-generated project and plug-in parameter editors are a different
family: the generated VST2 Gain editor has been touched and its parameter was
recalled from an unchanged project, and MPC contains
`MpcGenericAudioProcessorEditor`. A plug-in-supplied custom editor is a third
family. The existing work does not establish that MPC delivers
`effEditGetRect`, `effEditOpen`, or `effEditClose` to an external editor, nor
does it establish the editor's parent, geometry, render/input/focus path, or
close/unload/project-change lifetime. Static analysis also identifies a
compiled framebuffer constraint: the known `VSTPluginWindow` native-handle
route would reach an aborting `LinuxFbComponentPeer` slot, while a known remote
peer returns null. The ordinary MPC editor route and its actual peer remain
unknown; no crash or universal custom-editor impossibility has been observed.
See [`vst-feasibility.md`](vst-feasibility.md),
[`mpc-app-map.md`](mpc-app-map.md) and the compiled framebuffer editor
constraint in `docs/native-ui-research.md` at local commit `c1dcda9` before
applying this recipe to a plug-in. That static constraint identifies one
compiled host route; it does not identify the ordinary MPC editor route or its
actual peer.

## Failure localization

- Zero hook entries points to executable identity, live bias, admission, or
  detour installation.
- A hook plus a failure counter points to frame shape, vector bounds,
  constructor, parent, or teardown validation.
- Visible content without touch points to parent, bounds, ordered child hit-test
  priority, callback or focus. Repair-9 specifically required reading the live
  child array after rebuild; paint alone did not reveal the transparent stock
  Button in front.
- Stock text behind custom controls means the actual placeholder child was not
  detached; its stock ownership pointer must remain intact.
- A stale selected marker points to the completed-sequence handoff or qualified
  UI-drain refresh, not the request callback or button painting.
- Duplicate tiles or a close crash points to instance keys, vptr restoration,
  destructor ordering or unbalanced counts.
- Audio concerns require callback and recurring-work inspection plus the same
  playback workload measured with CPU/RSS and judged separately by ear.
