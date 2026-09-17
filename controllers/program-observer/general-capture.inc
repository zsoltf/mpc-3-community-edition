static unsigned type_open_native(Context*,const CommandRequest*,uint32_t);
static unsigned new_track_open_native(Context*,const CommandRequest*,uint32_t);
static unsigned type_open_selected(const CommandRequest*);
typedef struct {uint32_t seq,at,epoch,root,project,project_owner,editor,editor_owner,responder,function,holder,operation;} TransportJob;
static TransportJob transport_jobs[COMMAND_SLOTS];
static _Atomic uint32_t transport_pending;
/* Native synchronous global operations. Completion is facade completion, not
 * persisted-file proof, requested-value equality, or automation recording. */
static __thread struct {uint32_t id,owner,property,sp,phase,generation,mode,mixed,count;} global_call __attribute__((tls_model("initial-exec")));
static void global_initialize(void){memset(&global_call,0,sizeof(global_call));memset(transport_jobs,0,sizeof(transport_jobs));atomic_store(&transport_pending,0);
#ifdef COMMAND_COMPONENT
 transport_queue_component_reset();
#endif
}
static int global_quiescent(void){return !atomic_load_explicit(&transport_pending,memory_order_seq_cst);}
static void global_changed(void){
 GeneralState *g=&mirror_state->general;unsigned rev=atomic_load(&g->automation_generation);
 if(rev==UINT32_MAX){atomic_store(&g->automation_error,CH_WRAP);return;}
 atomic_store_explicit(&g->automation_generation,rev+1,memory_order_release);
 if(global_call.id){unsigned at=command_find(command_state,global_call.id);if(at<COMMAND_SLOTS&&accepted[at].request.reserved==CF_AUTOMATION)command_fail(C_SOURCE_CHANGED);}
}
static void global_auto_copy(uint32_t g,unsigned lane){
 unsigned id=channel_owner_id(g);if(!id)return;
 GeneralState *state=&mirror_state->general;AutomationSample *s=state->automation+lane;
 unsigned mode=word(g+0x34),mixed=*(const volatile unsigned char*)(uintptr_t)(g+0xb8),members=word(g+0xb4),rev=atomic_load(&s->revision);
 if(mode>2||mixed>1||rev>UINT32_MAX-2){atomic_store(&state->automation_error,CH_LIFETIME);return;}
 atomic_store_explicit(&s->revision,rev+1,memory_order_relaxed);atomic_thread_fence(memory_order_release);
 atomic_store(&s->owner,id);atomic_store(&s->epoch,atomic_load(&mirror_state->epoch));atomic_store(&s->tick,atomic_load(&mirror_state->heartbeat));atomic_store(&s->mode,mode);atomic_store(&s->mixed,mixed);atomic_store(&s->count,members);atomic_store(&s->generation,atomic_load(&state->automation_generation));
 atomic_store_explicit(&s->revision,rev+2,memory_order_release);
 if(global_call.id&&global_call.owner==g){global_call.mode=mode;global_call.mixed=mixed;global_call.count=members;}
}
static void global_root(unsigned site){
 GeneralState *s=&mirror_state->general;
 if(site==M_ROOT_DESTROY){channel_owner_death(atomic_load(&s->zoom));atomic_store(&s->zoom,0);atomic_store(&s->zoom_owner,0);return;}
 if(site!=M_RESET||!root)return;
 uint32_t controls=word(root+0x388),z=pointer(controls)&&controls<UINT32_MAX-0x80?word(controls+0x78):0;
 if(pointer(z)&&z<UINT32_MAX-0x154){
  if(!channel_owner_id(z))channel_owner_birth(z,CO_ZOOM);
  atomic_store(&s->zoom,z);atomic_store_explicit(&s->zoom_owner,channel_owner_id(z),memory_order_release);
 }
}
static void global_point(Context *c,unsigned site,unsigned lane){
 GeneralState *s=&mirror_state->general;
 if(site==GL_EDITOR_PUBLISH){
  uint32_t editor=c->r[5],e=c->r[4]+0xd0;
  if(token()!=command_owner||!typed(e,0x68d7bf8)||!typed(editor,0x695395c)||!typed(editor+0x230,0x6953b1c)||word(editor+0x37c)!=e)return;
  channel_owner_birth(editor,CO_EDITOR);atomic_store(&s->editor_root,e);atomic_store(&s->editor,editor);atomic_store_explicit(&s->editor_owner,channel_owner_id(editor),memory_order_release);return;
 }
 if(site==GL_EDITOR_DEATH){channel_owner_death(c->r[0]);if(c->r[0]==atomic_load(&s->editor)){atomic_store(&s->editor_owner,0);atomic_store(&s->editor,0);}return;}
 uint32_t g=site==GL_AUTO_STATE?c->r[7]:site==GL_AUTO_FANOUT||site==GL_AUTO_EMPTY?c->r[4]:c->r[0];ChannelOwner *o=channel_owner(g);
 if(!o||atomic_load(&o->kind)!=CO_AUTOMATION||!atomic_load(&o->live))return;
 if(token()!=command_owner){atomic_store(&s->automation_error,CH_OWNER);if(global_call.id)command_fail(C_OWNER);return;}
 if(site==GL_AUTO_DEATH||site==GL_AUTO_DELETE){global_changed();channel_owner_death(g);return;}
 if(site==GL_AUTO_REGISTER||site==GL_AUTO_UNREGISTER||site==GL_AUTO_CLEAR||site==GL_AUTO_CALLBACK){global_changed();return;}
 if(site==GL_AUTO_STATE||site==GL_AUTO_EMPTY)global_auto_copy(g,lane);
 if(!global_call.id)return;
 unsigned at=command_find(command_state,global_call.id);
 if(at<COMMAND_SLOTS&&accepted[at].request.reserved!=CF_AUTOMATION)return;
 if(at==COMMAND_SLOTS||g!=global_call.owner||source_requests[at]!=global_call.id){trace_fail(C_TRACE_AMBIGUOUS);return;}
 if(site==GL_AUTO_ENTER){if(global_call.phase){trace_fail(C_TRACE_AMBIGUOUS);return;}global_call.sp=source_sp(c);global_call.phase=1;}
 else if(site==GL_AUTO_FANOUT){if(global_call.phase!=1||source_sp(c)+8!=global_call.sp){trace_fail(C_TRACE_AMBIGUOUS);return;}global_call.phase=2;}
 else if(site==GL_AUTO_STATE||site==GL_AUTO_EMPTY){
  if(global_call.phase!=2||source_sp(c)+(site==GL_AUTO_EMPTY?8:24)!=global_call.sp||(site==GL_AUTO_EMPTY?global_call.count!=0:global_call.count==0)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  global_call.phase=3;event(c,CE_GLOBAL_STATE,global_call.id,global_call.count,global_call.mixed,g,0,CF_AUTOMATION,global_call.mode,g+0xc,epoch);
 }
}
/* The request identifies a live native intent owner, never its copied value.
 * Record's facade receiver R differs from its enrolled state owner RP. */
static unsigned global_toggle_target(const CommandRequest *r,uint32_t *target,uint32_t *function,uint32_t *property,unsigned *value){
 if(r->reserved==GLOBAL_LOOP_TOGGLE){
  /* Same nonretained UI-owner path as native Learn/hardware. Native notifier
   * linkage is not a separate foreign-destructor exclusion mechanism. */
  uint32_t sequence=word(project+0xe4);
  if(r->global_owner!=r->project_owner||r->field_incarnation||!typed(sequence,0x6934b18)||sequence>UINT32_MAX-0x23c)return C_IDENTITY;
  unsigned raw=*(const volatile unsigned char*)(uintptr_t)(sequence+0x238);
  if(raw>1||word(project+0xe4)!=sequence)return C_IDENTITY;
  *target=sequence+0x210;*property=*target;*function=observer_image_bias+0xe298bc;*value=raw;return 0;
 }
 uint32_t a=word(root+0x2e4),owner=0;unsigned field;
 if(!pointer(a)||a>UINT32_MAX-0x400)return C_IDENTITY;
 if(r->reserved==GLOBAL_RECORD_TOGGLE){
  uint32_t seq=word(a+0x3bc),rec;
  if(!pointer(seq)||seq>UINT32_MAX-0x400)return C_IDENTITY;
  rec=word(seq+0x3ec);if(!typed(rec,0x68a0318)||rec>UINT32_MAX-0x9c)return C_IDENTITY;
  owner=word(rec+0x98);if(!typed(owner,0x68a0340)||owner>UINT32_MAX-0x52c)return C_IDENTITY;
  *target=rec;*function=observer_image_bias+0x1c27e90;*property=owner+0x500;field=CF_RECORD_MODE;
 }else if(r->reserved==GLOBAL_CLICK_TOGGLE){
  owner=word(a+0x3c0);uint32_t metronome=word(a+0x3b8);
  if(!typed(owner,0x689fed4)||owner>UINT32_MAX-0x68||!typed(metronome,0x689febc)||word(owner+0x60)!=metronome||word(owner+0x64)!=word(a+0x3bc))return C_IDENTITY;
  *target=owner;*function=observer_image_bias+0x1beaad0;*property=owner+0xc;field=CF_CLICK;
 }else return C_NOT_READY;
 ChannelCell *cell=channel_cell(*property);ChannelOwner *o=channel_owner(owner);
 if(!o||atomic_load(&o->kind)!=channel_kind(field)||channel_owner_id(owner)!=r->global_owner||!cell||!atomic_load(&cell->live)||atomic_load(&cell->owner_incarnation)!=r->global_owner||atomic_load(&cell->incarnation)!=r->field_incarnation||channel_fault(atomic_load(&mirror_state->channel.errors[field])))return C_IDENTITY;
 unsigned raw=field==CF_RECORD_MODE?word(owner+0x528):*(const volatile unsigned char*)(uintptr_t)(owner+0x34);
 if(raw>(field==CF_RECORD_MODE?4u:1u))return C_SOURCE_FAILURE;
 *value=field==CF_RECORD_MODE?(raw==1||raw==3):raw;return 0;
}
static void global_toggle_commit(Context *c,unsigned site,unsigned at){
 const CommandRequest *r=&accepted[at].request;
 if(c->r[5]!=accepted[at].property)return;
 /* Both configured Loop and Click's native listener call e298bc: its commit
  * at e29938 is M_MUTE, despite the broader bool-shaped hook names. */
 unsigned expected=r->reserved==GLOBAL_RECORD_TOGGLE?CH_RECORD_COMMIT:M_MUTE;
 if(site!=expected)return;
 unsigned raw=c->r[7],value=r->reserved==GLOBAL_RECORD_TOGGLE?(raw==1||raw==3):raw;
 if(global_call.id!=r->seq||global_call.phase!=1||token()!=command_owner||!atomic_load(&command_in_hook)||raw>(r->reserved==GLOBAL_RECORD_TOGGLE?4u:1u)||value!=accepted[at].native_bits){trace_fail(C_TRACE_AMBIGUOUS);return;}
 global_call.phase=2;
 event(c,CE_GLOBAL_STATE,r->seq,accepted[at].property,raw,accepted[at].program,0,r->reserved,value,0,epoch);
}
static unsigned transport_current(const CommandRequest *r,uint32_t *target,uint32_t *function,uint32_t *holder,uint32_t *editor_out){
 GeneralState *s=&mirror_state->general;uint32_t editor=atomic_load_explicit(&s->editor,memory_order_acquire);
 if(!ready||loading||depth||reset_pending||root<0xd0||!project||!command_queues||root>UINT32_MAX-0x388||editor>UINT32_MAX-0x380)return C_NOT_READY;
 if(r->epoch!=epoch||r->project_owner!=channel_owner_id(project)||atomic_load(&s->editor_root)!=root||r->global_owner!=atomic_load(&s->editor_owner)||channel_owner_id(editor)!=r->global_owner)return C_IDENTITY;
 if(word(root+0x24c)!=project||word(root+0xf8)!=command_queues||word(root-0xd0+0x904)!=editor||!typed(editor,0x695395c)||!typed(editor+0x230,0x6953b1c)||word(editor+0x37c)!=root)return C_NATIVE_MEMBERSHIP;
 uint32_t controls=word(root+0x388),audio=word(root+0x2e4),h=editor+0x2c8;
 if(!pointer(controls)||controls>UINT32_MAX-0xf4||!pointer(audio)||audio>UINT32_MAX-0x400||word(h)!=command_queues)return C_NATIVE_MEMBERSHIP;
 uint32_t responder=word(controls+0xf0),sequencer=word(audio+0x3bc),timeline=word(audio+0x3a4);
 if(!pointer(responder)||responder>UINT32_MAX-0x234||!pointer(sequencer)||sequencer>UINT32_MAX-0x404||!typed(timeline,0x69091b0)||word(sequencer+0x400)!=timeline||word(responder+0x1f0)!=sequencer)return C_NATIVE_MEMBERSHIP;
 *target=responder;*function=observer_image_bias+(r->reserved==GLOBAL_PLAY?0xf2358c:0xf23628);*holder=h;*editor_out=editor;return 0;
}
static unsigned global_resolve(Context *c,const CommandRequest *r,uint32_t *target,uint32_t *function){
 unsigned now=atomic_load(&mirror_state->heartbeat);
 if(!command_global(r->reserved)||r->serial||r->binding||r->incarnation||r->track_owner||r->program_owner||!r->global_owner||(r->reserved==CF_AUTOMATION?r->bits>2:r->bits!=0))return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 if(r->created>=command_tick_limit(WINDOW_SECONDS)||r->expires<r->created||r->expires-r->created>120000||r->expires<now)return C_EXPIRED;
 if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 if(!active()||atomic_load(&enrollment_fault)||atomic_load(&command_state->trace_error))return C_SOURCE_FAILURE;
 if(!ready||loading||depth||reset_pending||!root||!project||!command_queues)return C_NOT_READY;
 if(r->epoch!=epoch||r->project_owner!=channel_owner_id(project))return C_IDENTITY;
 for(unsigned i=0;i<count;i++)if(tracks[i].transition)return C_NOT_READY;
 if(c->r[0]!=command_queues+4||word(c->r[0]+0xf0)||word(root+0x24c)!=project||word(root+0xf8)!=command_queues)return C_NOT_READY;
 if(r->reserved==CF_AUTOMATION){
  uint32_t g=word(project+0xaac);ChannelCell *cell=channel_cell(g+0xc);
  if(!typed(g,0x68ac48c)||channel_owner_id(g)!=r->global_owner||!cell||!atomic_load(&cell->live)||atomic_load(&cell->incarnation)!=r->field_incarnation||atomic_load(&mirror_state->general.automation_error))return C_IDENTITY;
  *target=g;*function=observer_image_bias+0x238f5d8;
 }else if(command_toggle(r->reserved)){
  uint32_t property;unsigned value;return global_toggle_target(r,target,function,&property,&value);
 }else if((r->reserved==GLOBAL_TRACK_NEW||r->reserved==GLOBAL_TRACK_TYPE||command_transport(r->reserved)||command_history(r->reserved)||r->reserved==GLOBAL_SAVE||command_page(r->reserved)||command_key(r->reserved)||command_sequence_duplicate(r->reserved))){
  if(r->reserved==GLOBAL_TRACK_TYPE&&!type_open_selected(r))return C_IDENTITY;
  GeneralState *s=&mirror_state->general;uint32_t editor=atomic_load(&s->editor);
  if(atomic_load(&s->editor_root)!=root||r->global_owner!=atomic_load(&s->editor_owner)||channel_owner_id(editor)!=r->global_owner||root<0xd0||word(root-0xd0+0x904)!=editor||!typed(editor,0x695395c)||!typed(editor+0x230,0x6953b1c)||word(editor+0x37c)!=root)return C_IDENTITY;
  if(command_transport(r->reserved)){
   uint32_t holder,checked_editor;return transport_current(r,target,function,&holder,&checked_editor);
  /* Duplicate Sequence names the application's own CopySequence Confirm, the
   * one native entry this operation calls, so the identity re-check before and
   * after the call covers the entry as well as the receiver. */
  }else {*target=editor;*function=observer_image_bias+(command_key(r->reserved)?0xbc18ec:command_sequence_duplicate(r->reserved)?SEQ_DUP_CONFIRM_RVA:0x2c70f20);}
 }else{
  uint32_t controls=word(root+0x388),z=pointer(controls)&&controls<UINT32_MAX-0x80?word(controls+0x78):0;
  if(!pointer(z)||z!=atomic_load(&mirror_state->general.zoom)||channel_owner_id(z)!=r->global_owner||r->global_owner!=atomic_load(&mirror_state->general.zoom_owner))return C_IDENTITY;
  static const unsigned funcs[]={0x10383c4,0x10383e4,0x1038424,0x1038444};*target=z;*function=observer_image_bias+funcs[r->reserved-GLOBAL_ZOOM_IN];
 }
 return 0;
}
/* Synthetic key presses in the shape of the application's own keyboard path.
 *
 * The X-Touch cursor cluster's Enter, Tab and Shift+Tab reach JUCE through the
 * entry a real USB keyboard reaches, ComponentPeer::handleKeyPress (bc18ec).
 * Calling only that entry was not enough on the device. MPC 3.9.1's peer
 * keyboard handler bc1cfc does three more things around the same call, and
 * JUCE's Tab traversal inside bc18ec runs only when the focused-component slot
 * is set, so a press that skips them reaches nothing. Verified by disassembly
 * of bc1cfc and its blocks bc2110..bc22b0, bc239c and bc23f4:
 *
 *  - Shift is a real modifier word, not a KeyPress field: a modifier keysym
 *    going down edits ModifierKeys::currentModifiers at G+0x10 in place
 *    (bc23fc orr #1) and the handler then tells the peer through
 *    ComponentPeer::handleModifierKeysChange (b899b8) whenever that word
 *    differs from the value it sampled on entry (bc2258..bc2274).
 *  - The keysDown bitmap at 6b221a8, one bit per keysym, is set before the
 *    delivery (bc1e24) and cleared on release (bc21cc). JUCE reads it for
 *    KeyPress::isKeyCurrentlyDown, so a key delivered without it is a key the
 *    application can never observe as held.
 *  - ComponentPeer::handleKeyUpOrDown (b74600) brackets the press (bc23a4),
 *    once with true and once with false; a release delivers that call alone.
 *  - handleKeyPress is then handed {keyCode, currentModifiers & ~0x70,
 *    textCharacter} by pointer (bc2288..bc22ac). That by-pointer triple is the
 *    shape this lane already used.
 *
 * xkb_state_update_key is deliberately NOT called. That is the keyboard
 * device's own xkb state for translating future scancodes, and this lane has
 * no device and no scancode; every value the application reads back from a
 * press is the modifier word, the bitmap and the peer callbacks above.
 *
 * G is the JUCE peer static table at 6b220a8: the modifier word at +0x10,
 * Component::currentlyFocusedComponent at +0x50 and juce::Desktop's own
 * singleton pointer at +0x44, all decoded from the application's own
 * pc-relative literals rather than restated.
 *
 * The receiver is the application's own top-level peer, not the enrolled
 * Editor's. Calling Component::getPeer (b740bc) on the Editor returned null on
 * the device, so every injected key was refused at the identity pin and none of
 * the sequence above ever ran: b740bc tests the heavyweight flag at
 * component+0x68, walks parentComponent at +0xc and tail-calls
 * ComponentPeer::getPeerFor (b74074), which returns null for a component with
 * no desktop-attached ancestor. The MpcEditor this lane enrolls (vtable
 * 695395c, juce::Component at offset 0 through ComponentStack) is not such a
 * component in the running application.
 *
 * bc1cfc resolves its receiver instead through Desktop::getInstance (b73474),
 * Desktop::findComponentAt (b9a904) at the cursor position and then the same
 * b740bc. This lane takes the same objects minus the cursor: b73474 only reads
 * [G+0x44] and constructs when it is null, so the slot is read and never
 * constructed, and b9a904's own desktopComponents list at [desktop+0x34] with
 * the count at [desktop+0x3c] supplies the top-level component whose peer
 * b740bc returns. The cursor lookup is deliberately not replicated: its point
 * comes from [G+0x2c], the lazily created cursor object that exists only while
 * a pointer device is present (docs/mpc-app-map.md#mouse-pointer-gate), which
 * would make the X-Touch keys depend on a plugged-in mouse.
 *
 * The identity pin 688e864 is unchanged and was verified against the image:
 * juce::LinuxFbComponentPeer (typeinfo 68c8870) is the image's only
 * juce::ComponentPeer subclass, derives from it publicly at offset 0, and
 * 688e864 is its primary vtable (offset-to-top 0, typeinfo 68c8870), so a live
 * peer's first word is exactly that address plus the load bias. The lane also
 * requires [peer+4], the component getPeerFor matches on, to be the same
 * top-level component.
 *
 * bc18ec targets the focused component and only falls back to [peer+4] when it
 * is null, so this receiver is the one the application's own keyboard path
 * would have used for the same press.
 *
 * Every word this borrows is put back: the bit is cleared and the modifier
 * word is restored before the operation completes, with the application told
 * again if the restore changed it. The whole cost is one command's worth of
 * loads, four native calls and two stores, on the owner thread inside the
 * existing lane. */
#define KEY_STATICS_RVA 0x6b220a8u
#define KEY_MODIFIERS_OFFSET 0x10u
#define KEY_FOCUS_OFFSET 0x50u
/* juce::Desktop's singleton pointer, the slot b73474 reads and only writes when
 * it is null; the desktopComponents vector b9a904 walks lives inside it. */
#define KEY_DESKTOP_OFFSET 0x44u
#define KEY_DESKTOP_LIST 0x34u
#define KEY_DESKTOP_COUNT 0x3cu
/* juce::Component::flags, bit 0 hasHeavyweightPeerFlag, and the component a
 * juce::ComponentPeer names at +4. */
#define KEY_COMPONENT_FLAGS 0x68u
#define KEY_COMPONENT_HEAVYWEIGHT 1u
#define KEY_PEER_COMPONENT 0x4u
#define KEY_PEER_VTABLE_RVA 0x688e864u
#define KEY_DOWN_BITMAP_RVA 0x6b221a8u
/* One bit per 16-bit keysym: the handler indexes it with keysym>>3. */
#define KEY_DOWN_BITMAP_BYTES 0x2000u
#define KEY_PEER_RVA 0xb740bcu
#define KEY_PEER_LENGTH 0x170u
/* ComponentPeer::getPeerFor, the body b740bc tail-calls; guarded with it so the
 * whole called path is install-time verified, not only its entry. */
#define KEY_PEER_FOR_RVA 0xb74074u
#define KEY_PEER_FOR_LENGTH 0x48u
#define KEY_PRESS_RVA 0xbc18ecu
#define KEY_PRESS_LENGTH 0x378u
#define KEY_UP_DOWN_RVA 0xb74600u
#define KEY_UP_DOWN_LENGTH 0x24cu
#define KEY_MODIFIERS_CHANGED_RVA 0xb899b8u
#define KEY_MODIFIERS_CHANGED_LENGTH 0xa8u
/* Exact 3.9.1 program headers: RX segment [0,6875d04), RW segment
 * [6877010,6bd6078). module_ranges carries p_filesz only, so these three
 * writable addresses, which live past the writable segment's file image, are
 * qualified here the way wheel-capture.inc qualifies the focus slot. */
_Static_assert(KEY_STATICS_RVA+KEY_MODIFIERS_OFFSET>=0x6877010u&&KEY_STATICS_RVA+KEY_MODIFIERS_OFFSET+4u<=0x6bd6078u,"JUCE modifier word inside the writable segment");
_Static_assert(KEY_STATICS_RVA+KEY_FOCUS_OFFSET>=0x6877010u&&KEY_STATICS_RVA+KEY_FOCUS_OFFSET+4u<=0x6bd6078u,"focused component slot inside the writable segment");
_Static_assert(KEY_STATICS_RVA+KEY_DESKTOP_OFFSET>=0x6877010u&&KEY_STATICS_RVA+KEY_DESKTOP_OFFSET+4u<=0x6bd6078u,"Desktop singleton slot inside the writable segment");
_Static_assert(KEY_DOWN_BITMAP_RVA>=0x6877010u&&KEY_DOWN_BITMAP_RVA+KEY_DOWN_BITMAP_BYTES<=0x6bd6078u,"keysDown bitmap inside the writable segment");
_Static_assert(KEY_PEER_RVA+KEY_PEER_LENGTH<=0x6875d04u,"peer lookup inside the executable segment");
_Static_assert(KEY_PEER_FOR_RVA+KEY_PEER_FOR_LENGTH<=0x6875d04u,"peer table lookup inside the executable segment");
_Static_assert(KEY_PRESS_RVA+KEY_PRESS_LENGTH<=0x6875d04u,"handleKeyPress inside the executable segment");
_Static_assert(KEY_UP_DOWN_RVA+KEY_UP_DOWN_LENGTH<=0x6875d04u,"handleKeyUpOrDown inside the executable segment");
_Static_assert(KEY_MODIFIERS_CHANGED_RVA+KEY_MODIFIERS_CHANGED_LENGTH<=0x6875d04u,"handleModifierKeysChange inside the executable segment");
#ifdef COMMAND_COMPONENT
static void (*component_global_dispatch)(unsigned,uint32_t,uint32_t,unsigned);
/* The exact juce::KeyPress triple the production path builds, so the regression
 * reads what the native injection is given rather than a restated constant. */
static uint32_t component_global_key[3];
/* The four native calls of the key lane, substituted one for one so the
 * regression observes the real sequence, its arguments and the borrowed words
 * at each call rather than a restatement of them. */
static uint32_t (*component_key_peer)(uint32_t component);
static void (*component_key_modifiers_changed)(uint32_t peer);
static void (*component_key_up_down)(uint32_t peer,unsigned down);
static unsigned (*component_key_press)(uint32_t peer,const uint32_t *key);
#endif
/* Two stores into the application's own writable statics, both put back before
 * the operation completes. */
static void global_key_store(uint32_t a,uint32_t v){*(volatile uint32_t*)(uintptr_t)a=v;}
static unsigned char global_key_byte(uint32_t a){return *(const volatile unsigned char*)(uintptr_t)a;}
static void global_key_byte_store(uint32_t a,unsigned char v){*(volatile unsigned char*)(uintptr_t)a=v;}
static uint32_t global_key_peer(uint32_t component){
#ifdef COMMAND_COMPONENT
 return component_key_peer(component);
#else
 return ((uint32_t (*)(void*))(uintptr_t)(observer_image_bias+KEY_PEER_RVA))((void*)(uintptr_t)component);
#endif
}
/* The application's own first desktop component, read out of the juce::Desktop
 * the application already built. Nothing is constructed and nothing is called:
 * a null singleton, an empty list or a component that is not on the desktop
 * leaves the operation refused at C_IDENTITY. */
static uint32_t global_key_desktop_component(void){
 uint32_t desktop=word(observer_image_bias+KEY_STATICS_RVA+KEY_DESKTOP_OFFSET);
 if(!pointer(desktop)||desktop>UINT32_MAX-0x40)return 0;
 uint32_t list=word(desktop+KEY_DESKTOP_LIST),count=word(desktop+KEY_DESKTOP_COUNT);
 if(!count||count>0x40||!pointer(list)||list>UINT32_MAX-4)return 0;
 uint32_t top=word(list);
 if(!pointer(top)||top>UINT32_MAX-0x70||!(global_key_byte(top+KEY_COMPONENT_FLAGS)&KEY_COMPONENT_HEAVYWEIGHT))return 0;
 return top;
}
static void global_key_modifiers_changed(uint32_t peer){
#ifdef COMMAND_COMPONENT
 component_key_modifiers_changed(peer);
#else
 ((void (*)(void*))(uintptr_t)(observer_image_bias+KEY_MODIFIERS_CHANGED_RVA))((void*)(uintptr_t)peer);
#endif
}
static void global_key_up_down(uint32_t peer,unsigned down){
#ifdef COMMAND_COMPONENT
 component_key_up_down(peer,down);
#else
 ((void (*)(void*,unsigned))(uintptr_t)(observer_image_bias+KEY_UP_DOWN_RVA))((void*)(uintptr_t)peer,down);
#endif
}
static unsigned global_key_press(uint32_t peer,const uint32_t *key){
#ifdef COMMAND_COMPONENT
 return component_key_press(peer,key);
#else
 return ((unsigned (*)(void*,const uint32_t*))(uintptr_t)(observer_image_bias+KEY_PRESS_RVA))((void*)(uintptr_t)peer,key);
#endif
}
/* One synthetic press on an already qualified peer. *focus records whether the
 * application had a focused component at the moment handleKeyPress read that
 * slot: it is the condition JUCE's Tab traversal turns on, so it is carried
 * out to the settlement event as a device-readable diagnostic, never as an
 * acceptance. Returns the native handleKeyPress result unchanged. */
static unsigned global_key_native(uint32_t peer,unsigned op,const uint32_t *key,unsigned *focus){
 CommandKeySymbol sym=command_key_symbol(op);
 uint32_t mods_at=observer_image_bias+KEY_STATICS_RVA+KEY_MODIFIERS_OFFSET;
 uint32_t bit_at=observer_image_bias+KEY_DOWN_BITMAP_RVA+(sym.keysym>>3);
 unsigned char mask=(unsigned char)(1u<<(sym.keysym&7)),held=global_key_byte(bit_at);
 uint32_t old=word(mods_at);unsigned handled;
 /* The handler tells the peer only when the word actually changed (bc2258),
  * so a shift already held by a real keyboard adds no call and no edit. */
 uint32_t wanted=old|(key[1]&COMMAND_KEY_SHIFT);
 if(wanted!=old){global_key_store(mods_at,wanted);global_key_modifiers_changed(peer);}
 global_key_byte_store(bit_at,(unsigned char)(held|mask));
 global_key_up_down(peer,1);
 *focus=word(observer_image_bias+KEY_STATICS_RVA+KEY_FOCUS_OFFSET)!=0;
 handled=global_key_press(peer,key);
 global_key_up_down(peer,0);
 global_key_byte_store(bit_at,held);
 if(word(mods_at)!=old){global_key_store(mods_at,old);global_key_modifiers_changed(peer);}
 return handled;
}
/* The audio callback does not inspect UI-thread working fields. The retained
 * Editor queue holder protects its native queue lifetime; atomic source
 * identities cancel a callback after project/editor retirement. */
static int transport_active(const TransportJob *job){
 GeneralState *s=&mirror_state->general;
 return atomic_load_explicit(&observer_running,memory_order_seq_cst)&&
  atomic_load_explicit(&mirror_state->epoch,memory_order_acquire)==job->epoch&&
  atomic_load_explicit(&mirror_state->channel.project_owner,memory_order_acquire)==job->project_owner&&
  channel_owner_id(job->project)==job->project_owner&&
  atomic_load_explicit(&s->editor_root,memory_order_acquire)==job->root&&
  atomic_load_explicit(&s->editor,memory_order_acquire)==job->editor&&
  atomic_load_explicit(&s->editor_owner,memory_order_acquire)==job->editor_owner&&
  channel_owner_id(job->editor)==job->editor_owner;
}
static void transport_audio(void *opaque){
 TransportJob job=*(TransportJob*)opaque;
 if(!transport_active(&job))goto canceled;
 unsigned lane=trace_enter(0x130);if(lane==COMMAND_LANES)goto canceled;
 if(source_requests[job.at]!=job.seq||atomic_load_explicit(active_request+job.at,memory_order_seq_cst)!=job.seq||accepted[job.at].request.seq!=job.seq){trace_fail(C_TRACE_AMBIGUOUS);trace_leave(lane);goto canceled;}
 if(!transport_active(&job)){trace_leave(lane);goto canceled;}
 trace_leave(lane);
#ifdef COMMAND_COMPONENT
 component_global_dispatch(job.operation,job.responder,job.function,0);
#else
 ((void (*)(void*))(uintptr_t)job.function)((void*)(uintptr_t)job.responder);
#endif
 if(!transport_active(&job))goto canceled;
 lane=trace_enter(0x131);if(lane==COMMAND_LANES)goto canceled;
 if(source_requests[job.at]!=job.seq||atomic_load_explicit(active_request+job.at,memory_order_seq_cst)!=job.seq){trace_fail(C_TRACE_AMBIGUOUS);trace_leave(lane);goto canceled;}
 if(!transport_active(&job)){trace_leave(lane);goto canceled;}
 event(0,CE_GLOBAL_END,job.seq,(uint32_t)(uintptr_t)opaque,job.holder,job.responder,0,job.operation,1,0,job.epoch);
 atomic_store_explicit(active_request+job.at,0,memory_order_seq_cst);atomic_store_explicit(&command_state->slots[job.at].done,job.seq,memory_order_release);trace_leave(lane);
 goto finished;
canceled:
 /* Queue submission remains a fact. Closing or retiring the source drains
  * this callback without fabricating a native completion or a trace failure. */
 atomic_store_explicit(active_request+job.at,0,memory_order_seq_cst);
finished:
 atomic_fetch_sub_explicit(&transport_pending,1,memory_order_seq_cst);
}
static void global_service(Context *c,unsigned at,const CommandRequest *r){
 uint32_t target=0,function=0;unsigned why=global_resolve(c,r,&target,&function);if(why==COMMAND_WAIT_HEARTBEAT)return;
 if(!why&&command_transport(r->reserved))for(unsigned i=0;i<COMMAND_SLOTS;i++)if(i!=at){
  CommandSlot *older=command_state->slots+i;uint32_t seq=atomic_load_explicit(&older->published,memory_order_acquire);CommandRequest prior;
  if(!seq||seq>=r->seq||atomic_load_explicit(&older->reclaimed,memory_order_acquire)==seq)continue;
  if(!command_request_read(older,seq,&prior))return;
  if(command_transport(prior.reserved)&&(atomic_load_explicit(&older->processed,memory_order_acquire)!=seq||atomic_load_explicit(active_request+i,memory_order_seq_cst)==seq))return;
 }
 CommandSlot *slot=command_state->slots+at;accepted[at].request=*r;native_started[at]=0;atomic_store(&slot->rejected,0);accepted[at].program=target;accepted[at].property=r->reserved==CF_AUTOMATION?target+0xc:0;
 accepted[at].controller=r->reserved;accepted[at].native_bits=r->bits;accepted[at].synchronous=0;
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}
 if(why)goto reject;
 if(global_call.id){why=C_REENTRY;goto reject;}
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);
 if(command_toggle(r->reserved)){
  unsigned value;why=global_toggle_target(r,&target,&function,&accepted[at].property,&value);if(why)goto reject;
  accepted[at].native_bits=!value;
 }
 event(c,CE_GLOBAL_BEGIN,r->seq,command_toggle(r->reserved)?accepted[at].property:0,0,target,0,r->reserved,r->bits,command_toggle(r->reserved)?0:accepted[at].property,epoch);
 uint32_t check_target=0,check_function=0;why=global_resolve(c,r,&check_target,&check_function);
 if(why||target!=check_target||function!=check_function||!atomic_load(&observer_running)||atomic_load(&command_violation)||atomic_load(&command_state->trace_error)){if(!why)why=C_SOURCE_FAILURE;goto reject;}
 if(command_transport(r->reserved)){
  uint32_t holder=0,editor=0;why=transport_current(r,&check_target,&check_function,&holder,&editor);
  if(why||check_target!=target||check_function!=function){if(!why)why=C_SOURCE_CHANGED;goto reject;}
  TransportJob *job=transport_jobs+at;*job=(TransportJob){r->seq,at,r->epoch,root,project,r->project_owner,editor,r->global_owner,target,function,holder,r->reserved};
  atomic_fetch_add_explicit(&transport_pending,1,memory_order_seq_cst);native_started[at]=r->seq;
  if(!transport_queue_submit(observer_image_bias,holder,job,transport_audio)){native_started[at]=0;atomic_fetch_sub_explicit(&transport_pending,1,memory_order_seq_cst);why=C_NOT_READY;goto reject;}
  atomic_store_explicit(&slot->dispatched,r->seq,memory_order_release);atomic_store_explicit(&slot->returned,r->seq,memory_order_release);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store_explicit(&command_state->consumed,atomic_load(&command_state->consumed)+1,memory_order_release);return;
 }
 global_call.id=r->seq;global_call.owner=target;global_call.property=accepted[at].property;global_call.generation=atomic_load(&mirror_state->general.automation_generation);
 global_call.phase=command_toggle(r->reserved)?1:0;
 unsigned handled=1,key_focus=0;
 /* juce::KeyPress {keyCode, ModifierKeys, textCharacter}, built in both builds
  * from the one definition in command-state.h. */
 uint32_t key[3]={command_key_code(r->reserved),command_key_modifiers(r->reserved),command_key_symbol(r->reserved).text};
if(r->reserved==GLOBAL_TRACK_NEW){native_started[at]=r->seq;why=new_track_open_native(c,r,target);if(why)goto reject;}else if(r->reserved==GLOBAL_TRACK_TYPE){native_started[at]=r->seq;why=type_open_native(c,r,target);if(why)goto reject;}else if(command_key(r->reserved)){
 /* Qualify the application's own top-level peer before anything is started or
  * borrowed, and require it to name that same component back. */
 uint32_t top=global_key_desktop_component(),peer=top?global_key_peer(top):0;
 if(!typed(peer,KEY_PEER_VTABLE_RVA)||word(peer+KEY_PEER_COMPONENT)!=top){why=C_IDENTITY;goto reject;}
 native_started[at]=r->seq;
#ifdef COMMAND_COMPONENT
 memcpy(component_global_key,key,sizeof(key));component_global_dispatch(r->reserved,target,function,accepted[at].native_bits);
#endif
 handled=global_key_native(peer,r->reserved,key,&key_focus);
}else if(command_sequence_duplicate(r->reserved)){
 /* Resolve the whole duplicate before anything is built or submitted. Every
  * refusal here is a clean refusal: the only application code the plan enters
  * is FirstUnusedSequence, which reads the Project's own sequence array and
  * writes nothing but this lane's own out word. */
 SequenceDuplicatePlan plan;why=sequence_duplicate_plan(target,&plan);
 if(why)goto reject;
 /* The chosen slots, on the record before the submission, so the device can
  * read which sequence was copied where even when the copy lands elsewhere. */
 event(c,CE_GLOBAL_STATE,r->seq,0,plan.source,target,0,r->reserved,plan.destination,0,epoch);
 native_started[at]=r->seq;
#ifdef COMMAND_COMPONENT
 memcpy(component_global_key,key,sizeof(key));component_global_dispatch(r->reserved,target,function,accepted[at].native_bits);
#endif
 sequence_duplicate_submit(&plan);
}else {
#ifdef COMMAND_COMPONENT
 native_started[at]=r->seq;memcpy(component_global_key,key,sizeof(key));component_global_dispatch(r->reserved,target,function,accepted[at].native_bits);
#else
 if(r->reserved==CF_AUTOMATION||command_toggle(r->reserved)){native_started[at]=r->seq;((void (*)(void*,unsigned))(uintptr_t)function)((void*)(uintptr_t)target,accepted[at].native_bits);}
 else if(r->reserved==GLOBAL_SAVE||command_page(r->reserved)||command_history(r->reserved)){native_started[at]=r->seq;unsigned id=command_page(r->reserved)?command_page_id(r->reserved):r->reserved==GLOBAL_SAVE?0x2006:r->reserved==GLOBAL_UNDO?0x2018:0x2019;handled=((unsigned (*)(void*,unsigned))(uintptr_t)function)((void*)(uintptr_t)target,id);}
 else {native_started[at]=r->seq;((void (*)(void*))(uintptr_t)function)((void*)(uintptr_t)target);}
#endif
 }
 if(handled>1){why=C_SOURCE_FAILURE;goto reject;}
 if(r->epoch!=epoch||(!command_toggle(r->reserved)&&!command_transport(r->reserved)&&channel_owner_id(target)!=r->global_owner)||!atomic_load(&observer_running)||atomic_load(&command_violation)||atomic_load(&command_state->trace_error)||(r->reserved==CF_AUTOMATION&&(global_call.phase!=3||global_call.generation!=atomic_load(&mirror_state->general.automation_generation)))){why=C_SOURCE_CHANGED;goto reject;}
 if(command_toggle(r->reserved)){
  uint32_t post_target=0,post_function=0,post_property=0;unsigned value=0;
  /* Check the old current link before any post-call Sequence dereference. */
  if(r->reserved==GLOBAL_LOOP_TOGGLE&&(!ready||loading||depth||reset_pending||!root||!project||r->epoch!=epoch||r->project_owner!=channel_owner_id(project)||word(root+0x24c)!=project||word(project+0xe4)!=target-0x210)){why=C_SOURCE_CHANGED;goto reject;}
  why=global_resolve(c,r,&post_target,&post_function);
  if(!why)why=global_toggle_target(r,&post_target,&post_function,&post_property,&value);
  if(why||post_target!=target||post_function!=function||post_property!=accepted[at].property||global_call.phase!=2||value!=accepted[at].native_bits){if(!why)why=C_SOURCE_CHANGED;goto reject;}
  handled=value;
 }
 /* Key operations carry one diagnostic bit beside the native result: whether
  * the application had a focused component when the press was delivered. It
  * is readable on the device through command-client status, and it is never
  * part of acceptance: the native result is still the only completion. */
 event(c,CE_GLOBAL_END,r->seq,0,0,target,0,r->reserved,r->reserved==CF_AUTOMATION?global_call.mode:command_key(r->reserved)?(handled|(key_focus<<1)):handled,command_toggle(r->reserved)?0:accepted[at].property,epoch);
 memset(&global_call,0,sizeof(global_call));atomic_store_explicit(active_request+at,0,memory_order_seq_cst);
 atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);atomic_store(&slot->done,r->seq);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);return;
reject:memset(&global_call,0,sizeof(global_call));atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store(&slot->rejected,why);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);
}
