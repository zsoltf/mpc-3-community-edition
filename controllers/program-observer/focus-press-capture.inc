/* The MPC data wheel's push, as a command lane: JOG_PRESS.
 *
 * On this firmware the wheel's push is what "Enter" means for a focused item.
 * A synthetic keyboard Return does not reach it: the app delivers the push
 * through its own focus controller, not through the peer's keyboard router.
 * The owner's device confirmed both halves of that - injected Return does
 * nothing on a focused item, while Tab and Shift+Tab (the same key lane) work.
 *
 * The app's own global-action performer 331e8c4, in the folded group
 * [331e6f4,331eb84), takes its receiver from the same active-focus-controller
 * accessor 2b64174 the data-wheel steps use (bl at 331e8fc), loads an int from
 * its action object (ldr r1,[r4,#8] at 331e948) and calls UIFocusController
 * vtable slot 5 at +0x14 (ldr r3,[r3,#0x14] at 331e950, blx r3 at 331e954).
 * That slot is 35c2648: it dereferences the controller's HW-focused component
 * at +0xe4, dynamic-casts it to HWFocusable and tail-calls HWFocusable slot 7
 * (this, int), the press handler 163 focusable classes implement.
 *
 * The buttonId is not statically recoverable; exhaustive PIC, word and branch
 * scans found no construction site for the wheel-press action object. It was
 * read from one real press on the owner's device with a temporary diagnostic
 * hook at this entry (reverted in a1e1a7c): the panel wheel push arrives with
 * buttonId = 11 on the active controller. FOCUS_PRESS_BUTTON in command-state.h
 * is that measurement.
 *
 * This lane adds NO hook site. 35c2648 is called, not patched, exactly as
 * 35c28f4 is called by the data-wheel lane; the whole entry is install-time
 * guarded as one .ARM.exidx range, and model-prepare.py checks at recipe time
 * that all four legal controller vtables still carry it at +0x14.
 *
 * Shape: one synchronous UI-thread call per accepted request, no queue, no
 * callback, and the completion shape service_one already uses for CF_SELECTION,
 * the same one wheel_command_service uses. Single flight is shared with
 * JOG_DATA in command_conflict, because both act on the one active controller.
 *
 * A suppressed press (no active focus controller, nothing focused, not showing,
 * illegal receiver) is a completed request carrying a zero CE_WHEEL_STEP, not a
 * rejection, exactly as for a suppressed data-wheel step.
 */
/* UIFocusController vtable slot 5, the controller-side press entry: one
 * complete .ARM.exidx range [35c2648,35c26e8). */
#define PRESS_ENTRY_RVA 0x35c2648u
#define PRESS_ENTRY_LENGTH 0xa0u
_Static_assert(PRESS_ENTRY_RVA+PRESS_ENTRY_LENGTH<=0x6875d04u,"controller press entry inside the executable segment");
#ifdef COMMAND_COMPONENT
static void (*component_press_call)(uint32_t function,uint32_t receiver,int button);
#endif
/* Receiver resolution and the single native call. The receiver is the app's own
 * active-controller slot, re-read and re-typed here and retained nowhere, and
 * the focused-component test is the same two-load suppressor wheel_dispatch
 * applies: it can stop a press the hardware path would also have stopped and
 * never causes one. Returns 1 when the call was made, 0 when it was suppressed.
 *
 * The press entry itself remains the last filter: it returns without effect
 * when its focused component is null or is not HWFocusable. */
static int focus_press_dispatch(void){
 if(token()!=command_owner)return 0;
 uint32_t receiver=word(observer_image_bias+FOCUS_SLOT_RVA);
 if(!pointer(receiver))return 0;
 uint32_t vptr=word(receiver),legal=0;
 for(unsigned i=0;i<4;i++)if(vptr==observer_image_bias+wheel_focus_vtables[i])legal=1;
 if(!legal)return 0;
 uint32_t focused=word(receiver+WHEEL_FOCUSED_OFFSET);
 if(!pointer(focused)||focused>UINT32_MAX-(WHEEL_FLAGS_OFFSET+4u))return 0;
 if(!(word(focused+WHEEL_FLAGS_OFFSET)&WHEEL_VISIBLE_FLAG))return 0;
 uint32_t function=observer_image_bias+PRESS_ENTRY_RVA;
#ifdef COMMAND_COMPONENT
 if(component_press_call){component_press_call(function,receiver,(int)FOCUS_PRESS_BUTTON);return 1;}
#endif
 ((void (*)(uint32_t,int))(uintptr_t)function)(receiver,(int)FOCUS_PRESS_BUTTON);
 return 1;
}
static unsigned focus_press_resolve(Context *c,const CommandRequest *r){
 uint32_t now=atomic_load_explicit(&mirror_state->heartbeat,memory_order_acquire);
 if(!command_focus_press(r->reserved)||r->bits!=FOCUS_PRESS_BUTTON)return C_FORMAT;
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
static void focus_press_service(Context *c,unsigned at,const CommandRequest *r){
 CommandSlot *slot=command_state->slots+at;unsigned why=focus_press_resolve(c,r);
 if(why==COMMAND_WAIT_HEARTBEAT)return;
 accepted[at].request=*r;native_started[at]=0;accepted[at].program=accepted[at].track=accepted[at].property=0;
 accepted[at].controller=r->reserved;accepted[at].native_bits=r->bits;accepted[at].synchronous=1;
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}
 atomic_store(&slot->rejected,0);
 if(why)goto rejected;
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);
 event(c,CE_DISPATCH,r->seq,0,0,0,0,r->reserved,r->bits,0,r->epoch);
 why=focus_press_resolve(c,r);
 if(why||!atomic_load_explicit(&observer_running,memory_order_seq_cst)||atomic_load(&command_violation)||atomic_load(&command_state->error)||atomic_load(&command_state->trace_error)){if(!why)why=C_SOURCE_FAILURE;goto rejected;}
 native_started[at]=r->seq;
 /* The one call. CE_WHEEL_STEP carries 1 when it was made and 0 when the
  * receiver checks suppressed it, the same receipt the data-wheel lane files. */
 int pressed=focus_press_dispatch();
 event(c,CE_WHEEL_STEP,r->seq,0,0,0,0,r->reserved,(uint32_t)pressed,0,r->epoch);
 event(c,CE_RETURN,r->seq,0,0,0,0,r->reserved,r->bits,0,r->epoch);
 atomic_store_explicit(active_request+at,0,memory_order_seq_cst);
 atomic_store_explicit(&slot->dispatched,r->seq,memory_order_release);atomic_store_explicit(&slot->returned,r->seq,memory_order_release);
 atomic_store_explicit(&slot->done,r->seq,memory_order_release);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);
 atomic_store_explicit(&command_state->consumed,atomic_load(&command_state->consumed)+1,memory_order_release);return;
rejected:
 atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store(&slot->rejected,why);
 atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store_explicit(&command_state->consumed,atomic_load(&command_state->consumed)+1,memory_order_release);
}
