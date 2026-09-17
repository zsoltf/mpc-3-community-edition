/* USB mouse wheel -> the application's own data wheel, on MPC 3.9.1 Gen1.
 *
 * The mouse wheel IS the data wheel. One wheel notch is one data-wheel step on
 * the application's active focus controller, exactly like the hardware wheel.
 * What sits under the cursor plays no part: the receiver comes from the app's
 * own global active-controller slot, the same slot its hardware data-wheel
 * action performers read.
 *
 * The JUCE LinuxFbComponentPeer already decodes libinput scroll events. Its
 * event switch at bd2224 sends type 404 POINTER_SCROLL_WHEEL, which is what a
 * USB mouse wheel produces, to the handler at bd2008; types 405 and 406
 * (finger and continuous, touchpads) go to a separate handler at bd1e2c that a
 * mouse never reaches. Both are gated by the same cursor-state enable byte as
 * motion and buttons (M_POINTER_DEVICE sets it through the app's own setter),
 * build a JUCE MouseWheelDetails on the stack and deliver it to the JUCE
 * component under the cursor. MPC's touch controls do nothing useful with that
 * delivery, so the wheel is inert on this firmware. This hook does not skip or
 * rewrite that delivery; it continues normally.
 *
 * Site: bd20e0, inside the SCROLL_WHEEL handler, after both
 * libinput_event_pointer_get_scroll_value_v120 calls have stored deltaX at [r6]
 * and deltaY at [r6+4]. The handler scales each v120 result by a double literal
 * (+1/512 at bd21e0 for horizontal, -1/512 at bd21e8 for vertical); that
 * negative factor is the sign flip, so deltaY > 0 is wheel up. Horizontal
 * scroll ([r6], v120 axis 1) is read by the app and ignored here. Reaching this
 * site already proves the enable byte is non-zero and a pointer event is live.
 *
 * .ARM.exidx folds bd1e2c and bd2008 into one range [bd1e2c,bd2200), which is
 * what the install-time guard covers; the site itself is what selects the
 * wheel handler.
 *
 * Cost: the whole hook is the wheel event itself. No polling, no timer, no
 * allocation, no retained state beyond one sub-notch float, and no work outside
 * the event. See docs/mpc-app-map.md#mouse-pointer-gate.
 */
/* Folded scroll-handler range holding both scroll handlers, guarded so the
 * register this hook trusts and the control flow around the site are
 * install-time verified, not only the eight displaced bytes. */
#define WHEEL_HANDLER_RVA 0xbd1e2cu
#define WHEEL_HANDLER_LENGTH 0x3d4u
/* Active UIFocusController slot in .bss, read by the app's own accessor
 * 2b64174 and written by the TouchFocusController methods as controllers
 * become active. Exact 3.9.1 program headers: RX segment [0,6875d04), RW
 * segment [6877010,6bd6078). module_ranges carries p_filesz only, so this
 * slot, which lives past the writable segment's file image, is qualified
 * here the way pointer-capture.inc qualifies the peer's cursor slot. */
#define FOCUS_SLOT_RVA 0x6b6f798u
/* TouchFocusController::DataWheelUp / ::DataWheelDown, one folded exidx range
 * [35c2b04,35c2e24). UIFocusController is the primary base of all four
 * concrete focus-controller classes, so these are what vtable slots 6 and 7
 * resolve to on every legal receiver. */
/* TouchFocusController::ProcessDataWheelRotation(this, signed char delta,
 * bool coarse), vtable slot 8 at +0x20 in all four concrete focus-controller
 * classes, exidx [35c28f4,35c29dc). This is the entry the hardware panel
 * performer 331e9ac uses: it reads a coalesced signed tick count with
 * ldrsb r1,[r4,#8] and makes exactly ONE call per event.
 *
 * The sibling DataWheelUp/DataWheelDown at 35c2b04/35c2bdc are slots 6 and 7
 * and take no count, so N notches would need N calls. That is what the first
 * device build did, and it rubber-banded: TouchNumericEditorBase's own
 * ProcessDataWheelRotation (32db8c4) runs a time-based accelerator (3b65854,
 * float velocity multiplied on every call whose gap is under a threshold) and
 * truncates the result with sxtb at 32db98c, so N calls microseconds apart ramp
 * the velocity until the signed byte wraps. One call with a count does not. */
#define WHEEL_ROTATE_RVA 0x35c28f4u
#define WHEEL_ROTATE_LENGTH 0xe8u
/* juce::Component fields the hardware path's liveness walk b74904 reads: the
 * controller's HW-focused component at +0xe4, and the flags byte at +0x68
 * whose bit 1 that walk requires at every level before dispatching. */
#define WHEEL_FOCUSED_OFFSET 0xe4u
#define WHEEL_FLAGS_OFFSET 0x68u
#define WHEEL_VISIBLE_FLAG 2u
#define WHEEL_LIVENESS_RVA 0xb74904u
#define WHEEL_LIVENESS_LENGTH 0x54u
_Static_assert(WHEEL_HANDLER_RVA+WHEEL_HANDLER_LENGTH<=0x6875d04u,"host scroll handler inside the executable segment");
_Static_assert(WHEEL_ROTATE_RVA+WHEEL_ROTATE_LENGTH<=0x6875d04u,"data-wheel rotation entry inside the executable segment");
_Static_assert(FOCUS_SLOT_RVA>=0x6877010u&&FOCUS_SLOT_RVA+4<=0x6bd6078u,"focus controller slot inside the writable segment");
_Static_assert(WHEEL_LIVENESS_RVA+WHEEL_LIVENESS_LENGTH<=0x6875d04u,"liveness walk inside the executable segment");
/* One detent of a standard wheel. libinput's v120 API reports a detent as 120
 * units (that is the whole point of the v120 scale), and the handler scales it
 * by the 1/512 literal at bd21e8 before storing deltaY, so one detent is
 * 120/512 = 0.234375. Written as the derivation; command-check pins both the
 * 120 numerator and the 1/512 literal against the exact image. */
#define WHEEL_NOTCH (120.0f/512.0f)
/* The delivered count is clamped to WHEEL_MAX_STEPS, defined once in
 * command-state.h because the X-Touch lane publishes against the same bound.
 * For the mouse, whole notches past the bound are dropped, not carried, so the
 * wheel stops when the user stops; only the sub-notch part is kept. The X-Touch
 * lane keeps its remainder instead, in the bridge's own accumulator. */
/* Coarse is 0: no modifier source is wired to the wheel. The hardware path
 * reads coarse from its action object; nothing here supplies one. */
#define WHEEL_COARSE 0u
/* The only four vptrs a legal receiver can carry: TouchPageContainer,
 * OverlayContainer, TouchFocusController and OverlayFocusController. */
static const uint32_t wheel_focus_vtables[4]={0x699ee00u,0x69c3090u,0x69c3af4u,0x69c8f9cu};
/* Sub-notch remainder and whole notches not yet delivered. Written only under
 * the command_owner check below, so these are owner-thread values and need no
 * atomic. The split exists so the accumulation can stay per event while the
 * single native call happens once per libinput drain. */
static float wheel_remainder;
static int wheel_pending;
static float wheel_float(uint32_t bits){float v;memcpy(&v,&bits,sizeof v);return v;}
/* The observer has a read primitive but no write primitive; this is the only
 * place it stores into the app's memory. The target is the app's own stack
 * MouseWheelDetails, written by the handler two instructions earlier and just
 * proven a pointer(). */
static void wheel_store(uint32_t a,uint32_t v){*(volatile uint32_t*)(uintptr_t)a=v;}
#ifdef COMMAND_COMPONENT
static void (*component_wheel_call)(uint32_t function,uint32_t receiver,int delta,unsigned coarse);
#endif
/* Whole notches for this event, sign carrying the direction, 0 for none. */
static int wheel_accumulate(uint32_t delta_bits){
 float dy=wheel_float(delta_bits);
 /* Rejects NaN, infinity and an absurd delta without a floating-point trap.
  * The bound also keeps total/WHEEL_NOTCH far inside int range: the conversion
  * below is undefined out of range and saturates on VFP, and a saturated
  * quotient would leave a remainder of millions of notches that banks the
  * maximum on every later event forever. With this bound the quotient stays
  * under 2^23 and the remainder stays under one notch. */
 if(!(dy>-1.0e6f)||!(dy<1.0e6f))return 0;
 if(dy==0.0f)return 0; /* horizontal-only event: the app still delivers it */
 float total=wheel_remainder+dy;
 int steps=(int)(total/WHEEL_NOTCH); /* truncates toward zero */
 wheel_remainder=total-(float)steps*WHEEL_NOTCH;
 return steps;
}
/* Receiver resolution and the single native call, and nowhere else: the app's
 * own active-controller slot, re-read and re-typed on every flush, with nothing
 * retained across flushes. One call carrying the coalesced count, exactly as
 * the hardware panel performer 331e9ac does.
 *
 * The hardware performer also runs a liveness walk, b74904, on the controller
 * before dispatching, and skips the step when it returns 0. That walk runs on
 * the CONTROLLER cast to juce::Component: it climbs [comp+0xc] requiring bit 1
 * of the flags byte at [comp+0x68] at every level, then checks bit 0 at the
 * root and asks the peer a virtual question through its vtable +0x38.
 *
 * This hook does not reproduce that walk and does not claim to. It applies the
 * flag bit read out of that walk's own instructions to a DIFFERENT object: the
 * controller's focused component at receiver+0xe4 must exist and must have bit
 * 1 of its own [+0x68] set. Two loads, no native call. The test is therefore
 * only a suppressor: it can stop a step the hardware path would also have
 * stopped, and it never causes one. It does not cover the controller's own
 * chain, an ancestor whose flag is clear, or a peer reporting itself minimised,
 * so steps can still reach a surface the hardware path would have skipped.
 * ProcessDataWheelRotation itself remains the last filter, as it returns
 * without effect when its focused component is null, is not HWFocusable, or has
 * not overridden the default rotation slot. */
/* Installed at four points in the peer's libinput wrappers, one body. Two
 * (a0a0d0, a0d9f0) are the blocks each drain loop falls into right after it has
 * drained get_event to NULL: those deliver. The other two (a09c90, a0d5e4) are
 * the third test of the post-poll() fd-flag dispatch chain near the loop head,
 * run once per wake before the drain, where the accumulator is empty and the
 * body costs one load and a return. They cannot split a burst, because the
 * drain that produces one always follows them. */
/* The clamp, the receiver resolution and the single call, shared by the mouse
 * flush above and the X-Touch data-wheel command lane below. Returns the count
 * actually delivered: 0 when the step was suppressed, which is a completed
 * no-op and not a failure. */
static int wheel_dispatch(int steps){
 if(token()!=command_owner)return 0;
 if(steps>WHEEL_MAX_STEPS)steps=WHEEL_MAX_STEPS;
 if(steps<-WHEEL_MAX_STEPS)steps=-WHEEL_MAX_STEPS;
 if(!steps)return 0;
 uint32_t receiver=word(observer_image_bias+FOCUS_SLOT_RVA);
 if(!pointer(receiver))return 0;
 uint32_t vptr=word(receiver),legal=0;
 for(unsigned i=0;i<4;i++)if(vptr==observer_image_bias+wheel_focus_vtables[i])legal=1;
 if(!legal)return 0;
 /* Nothing focused, or focused on something the app itself would not show. */
 uint32_t focused=word(receiver+WHEEL_FOCUSED_OFFSET);
 if(!pointer(focused)||focused>UINT32_MAX-(WHEEL_FLAGS_OFFSET+4u))return 0;
 if(!(word(focused+WHEEL_FLAGS_OFFSET)&WHEEL_VISIBLE_FLAG))return 0;
 uint32_t function=observer_image_bias+WHEEL_ROTATE_RVA;
#ifdef COMMAND_COMPONENT
 if(component_wheel_call){component_wheel_call(function,receiver,steps,WHEEL_COARSE);return steps;}
#endif
 /* steps is already inside signed char range, so the int argument and the
  * callee's signed char parameter agree. */
 ((void (*)(uint32_t,int,unsigned))(uintptr_t)function)(receiver,steps,WHEEL_COARSE);
 return steps;
}
static void wheel_flush(void){
 if(token()!=command_owner)return;
 int steps=wheel_pending;
 wheel_pending=0;
 if(!steps)return;
 (void)wheel_dispatch(steps);
}
/* Per-event work, at bd20e0: read the delta the peer already computed and bank
 * whole notches. No native call here, so a burst of events inside one drain
 * costs a few loads and one float add each. The counter saturates well above
 * the delivered clamp so a long spin cannot overflow it. */
#define WHEEL_MAX_PENDING 8
static void wheel_data(Context *c){
 /* The thread of the hardware data-wheel action path is inferred, not proven,
  * so the hook enforces its own owner check rather than trusting the site. */
 if(token()!=command_owner)return;
 if(!pointer(c->r[6])||c->r[6]>UINT32_MAX-8)return;
 int pending=wheel_pending+wheel_accumulate(word(c->r[6]+4)); /* deltaY only */
 if(pending>WHEEL_MAX_PENDING)pending=WHEEL_MAX_PENDING;
 if(pending<-WHEEL_MAX_PENDING)pending=-WHEEL_MAX_PENDING;
 wheel_pending=pending;
 /* The mouse wheel is the data wheel and nothing else. The peer's own JUCE
  * delivery at bd21c8 still runs, but with both deltas zeroed it is a no-op
  * for Viewport and ListBox scrolling, so a list does not move its selection
  * by one step and scroll its view at the same time. Nothing between this site
  * and that delivery reads either float again: r6 is touched only by the
  * str r6,[sp] at bd21c0 that passes the struct on, and no instruction in
  * between reads [sp,#40] or [sp,#44]. The flags half-word at [r6+8] and the
  * rest of the struct are left alone. */
 wheel_store(c->r[6],0);
 wheel_store(c->r[6]+4,0);
}

/* X-Touch jog wheel acting as the data wheel: a JOG_DATA command lane.
 *
 * This is deliberately NOT the jog lane. jog_resolve qualifies the native
 * AsyncSequencer transport receiver, jog_service retains a native jog until its
 * audio-thread DONE, and the bridge's jog admission gates on playing state and
 * navigation progress. None of that applies to a focus-controller data-wheel
 * step, which is a synchronous UI-thread call with no queue, no callback and no
 * musical position. So this lane has its own predicate (command_data_wheel),
 * its own format/identity check, and the synchronous completion shape
 * service_one already uses for CF_SELECTION.
 *
 * Exactly one ProcessDataWheelRotation call per accepted request. One
 * COMMAND_DRAIN can service several slots, so single flight is enforced at
 * publication by command_conflict, which refuses a second data-wheel request
 * while one is alive.
 *
 * A suppressed step (no active focus controller, nothing focused, not showing,
 * illegal receiver) is a completed request carrying a zero CE_WHEEL_STEP, not a
 * rejection: the hardware panel path drops those steps silently too. */
static unsigned wheel_command_resolve(Context *c,const CommandRequest *r){
 uint32_t now=atomic_load_explicit(&mirror_state->heartbeat,memory_order_acquire);
 if(!command_data_wheel(r->reserved)||!command_jog_delta(r->bits))return C_FORMAT;
 /* Every field of another family must be zero: this request names no Track,
  * Program, Project, pad, effect, Q-Link or I/O target at all. */
 if(r->serial||r->binding||r->incarnation||r->before_bits||r->before_revision||
    r->track_owner||r->program_owner||r->field_incarnation||r->project_owner||r->global_owner||
    r->pad_owner||r->pad_index||r->pad_generation||
    r->effect_generation||r->effect_slot||r->effect_index||r->effect_key||r->effect_ap||r->effect_position||
    r->qlink_root||r->qlink_wrapper||r->qlink_provider||r->qlink_generation||r->qlink_index||
    r->io_generation||r->io_choice_generation||r->io_field||r->io_connector)return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 if(r->created>=command_tick_limit(WINDOW_SECONDS)||r->expires<r->created||r->expires-r->created>120000u||r->expires<now)return C_EXPIRED;
 if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 if(!active()||atomic_load(&enrollment_fault)||atomic_load(&command_state->trace_error))return C_SOURCE_FAILURE;
 if(!ready||loading||reset_pending||depth||!command_queues)return C_NOT_READY;
 /* The native call happens only inside the accepted outermost UI drain. */
 if(c->r[0]!=command_queues+4||word(c->r[0]+0xf0))return C_NOT_READY;
 if(r->epoch!=epoch)return C_IDENTITY;
 return C_OK;
}
static void wheel_command_service(Context *c,unsigned at,const CommandRequest *r){
 CommandSlot *slot=command_state->slots+at;unsigned why=wheel_command_resolve(c,r);
 if(why==COMMAND_WAIT_HEARTBEAT)return;
 accepted[at].request=*r;native_started[at]=0;accepted[at].program=accepted[at].track=accepted[at].property=0;
 accepted[at].controller=r->reserved;accepted[at].native_bits=r->bits;accepted[at].synchronous=1;
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}
 atomic_store(&slot->rejected,0);
 if(why)goto rejected;
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);
 event(c,CE_DISPATCH,r->seq,0,0,0,0,r->reserved,r->bits,0,r->epoch);
 why=wheel_command_resolve(c,r);
 if(why||!atomic_load_explicit(&observer_running,memory_order_seq_cst)||atomic_load(&command_violation)||atomic_load(&command_state->error)||atomic_load(&command_state->trace_error)){if(!why)why=C_SOURCE_FAILURE;goto rejected;}
 native_started[at]=r->seq;
 /* The one call. wheel_dispatch clamps, resolves the app's own active focus
  * controller and returns the count it actually passed. */
 int steps=wheel_dispatch((int32_t)r->bits);
 event(c,CE_WHEEL_STEP,r->seq,0,0,0,0,r->reserved,(uint32_t)(int32_t)steps,0,r->epoch);
 event(c,CE_RETURN,r->seq,0,0,0,0,r->reserved,r->bits,0,r->epoch);
 atomic_store_explicit(active_request+at,0,memory_order_seq_cst);
 atomic_store_explicit(&slot->dispatched,r->seq,memory_order_release);atomic_store_explicit(&slot->returned,r->seq,memory_order_release);
 atomic_store_explicit(&slot->done,r->seq,memory_order_release);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);
 atomic_store_explicit(&command_state->consumed,atomic_load(&command_state->consumed)+1,memory_order_release);return;
rejected:
 atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store(&slot->rejected,why);
 atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store_explicit(&command_state->consumed,atomic_load(&command_state->consumed)+1,memory_order_release);
}
