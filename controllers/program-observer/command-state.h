/* CMD31: bounded independent transactions, unique sequence-qualified ownership. MMV17
 * remains the sole musical-state owner. Prior CMD1 artifacts remain versioned. */
#ifndef MPC_COMMAND_STATE_H
#define MPC_COMMAND_STATE_H
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#include "channel-state.h"
#include "meter-state.h"
#include "pad-state.h"
#include "effects-state.h"
#include "qlink-state.h"
#include "io-state.h"
#define COMMAND_MAGIC 0x41444d43u
#define COMMAND_VERSION 31u
#define COMMAND_SLOTS 32u
#define COMMAND_LANES 8u
#define COMMAND_EVENTS 128u
#define COMMAND_DRAIN 46u
#define COMMAND_SEQUENCE_LAST (UINT32_MAX-1u)
/* C_NO_DESTINATION is appended, not inserted: it is the refusal the application
 * itself shows as "All Sequences are used!", and no existing code carries it.
 * A failed project, editor, navigator or sequence-array pin keeps the codes
 * that already mean exactly that, C_IDENTITY and C_NATIVE_MEMBERSHIP. */
enum {C_OK,C_FORMAT,C_EXPIRED,C_NOT_READY,C_IDENTITY,C_NOT_DRUM,C_NATIVE_MEMBERSHIP,
 C_PREPARED,C_SOURCE_CHANGED,C_BUSY,C_SOURCE_FAILURE,C_OWNER,C_REENTRY,C_TRACE_LOSS,C_TRACE_AMBIGUOUS,C_CAPACITY,C_CLOSED,C_NO_DESTINATION};
static inline const char *command_error_name(unsigned code){
 static const char *names[]={"ok","format","expired","not_ready","identity","unsupported_mixable","native_membership","prepared","source_changed","busy","source_failure","owner","reentry","trace_loss","trace_ambiguous","capacity","closed","no_destination"};
 return code<sizeof(names)/sizeof(*names)?names[code]:"unknown";
}
enum {CA_NONE,CA_ENROLL_INCREMENT,CA_ENROLL_DECREMENT,CA_LANE_GUARD,CA_CONSTRUCTOR_OWNER,CA_SOURCE_REENTRY};
static inline uint32_t command_admission_detail(unsigned guard,unsigned result,unsigned site){return (site<<16)|(guard<<8)|(result&255u);}
enum {JOG_BAR=CF_COUNT,JOG_BEAT,JOG_PULSE,GLOBAL_SAVE,GLOBAL_ZOOM_IN,GLOBAL_ZOOM_OUT,GLOBAL_ZOOM_UP,GLOBAL_ZOOM_DOWN,GLOBAL_KEY_ENTER,GLOBAL_KEY_CANCEL,GLOBAL_KEY_LEFT,GLOBAL_KEY_UP,GLOBAL_KEY_RIGHT,GLOBAL_KEY_DOWN,GLOBAL_RECORD_TOGGLE,GLOBAL_CLICK_TOGGLE,GLOBAL_LOOP_TOGGLE,GLOBAL_PAGE_MAIN,GLOBAL_PAGE_ARRANGE,GLOBAL_PAGE_CLIP,GLOBAL_PAGE_MIX,GLOBAL_PAGE_PAD_MIX,GLOBAL_PAGE_TRACK_EDIT,GLOBAL_PAGE_SAMPLE_EDIT,GLOBAL_PAGE_STEP,EFFECT_PARAMETER,EFFECT_ENABLE,QLINK_VALUE,QLINK_MODE,IO_PARAMETER,EFFECT_CHOOSER,GLOBAL_TRACK_TYPE,GLOBAL_TRACK_NEW,GLOBAL_PLAY,GLOBAL_STOP,GLOBAL_UNDO,GLOBAL_REDO,JOG_DATA,GLOBAL_KEY_TAB,GLOBAL_KEY_BACKTAB,JOG_PRESS,GLOBAL_SEQ_DUPLICATE};
static inline int command_chooser(unsigned op){return op==EFFECT_CHOOSER;}
static inline int command_io(unsigned op){return op==IO_PARAMETER;}
static inline int command_qlink_mode(unsigned op){return op==QLINK_MODE;}
static inline int command_mode_delta(uint32_t bits){int32_t value=(int32_t)bits;return value&&value>=-32&&value<=32;}
static inline int command_qlink(unsigned op){return op==QLINK_VALUE;}
static inline int command_effect(unsigned op){return op==EFFECT_PARAMETER||op==EFFECT_ENABLE;}
static inline int command_page(unsigned op){return op>=GLOBAL_PAGE_MAIN&&op<=GLOBAL_PAGE_STEP;}
static inline unsigned command_page_id(unsigned op){static const unsigned ids[]={0x20ea,0x20f0,0x20eb,0x20ee,0x20ed,0x20ec,0x20f4,0x20f8};return command_page(op)?ids[op-GLOBAL_PAGE_MAIN]:0;}
/* The six original keys keep their contiguous range; Tab and Shift+Tab are
 * appended operations and are named explicitly. */
static inline int command_key(unsigned op){return (op>=GLOBAL_KEY_ENTER&&op<=GLOBAL_KEY_DOWN)||op==GLOBAL_KEY_TAB||op==GLOBAL_KEY_BACKTAB;}
/* juce::KeyPress is {int keyCode; ModifierKeys mods; juce_wchar textCharacter}.
 * JUCE's Linux windowing derives every key code as XK_<name>&0xff, with its own
 * extended bit 0x10000000 for the cursor keys: Return 0xff0d->0x0d, Escape
 * 0xff1b->0x1b, Left/Up/Right/Down 0xff51..0xff54 -> 0x10000051..0x10000054,
 * and Tab 0xff09->0x09. ModifierKeys::shiftModifier is 1, which is the only
 * modifier any of these carry. Both fields are built here so the observer's
 * native injection and the regression read one definition. */
#define COMMAND_KEY_SHIFT 1u
static inline uint32_t command_key_code(unsigned op){
 static const uint32_t codes[]={0x0du,0x1bu,0x10000051u,0x10000052u,0x10000053u,0x10000054u};
 if(op==GLOBAL_KEY_TAB||op==GLOBAL_KEY_BACKTAB)return 0x09u;
 return (op>=GLOBAL_KEY_ENTER&&op<=GLOBAL_KEY_DOWN)?codes[op-GLOBAL_KEY_ENTER]:0u;
}
static inline uint32_t command_key_modifiers(unsigned op){return op==GLOBAL_KEY_BACKTAB?COMMAND_KEY_SHIFT:0u;}
/* The X keysym the real keyboard path decodes for each of these keys, and the
 * UTF-8 text character it derives from it. MPC 3.9.1's own libinput keyboard
 * handler (bc1cfc) indexes a keysDown bitmap by the keysym and passes the text
 * character as juce::KeyPress's third word, so a synthetic press needs both
 * alongside the code and modifier above. Return and Escape carry their control
 * character, Tab carries 9, and the cursor keys carry none. Shift+Tab is the X
 * server's own ISO_Left_Tab keysym fe20, which is the keysym the app sees and
 * the bit it sets; its code and text stay Tab's. One table, read by the
 * observer's injection and by the regression. */
typedef struct {uint32_t keysym,text;} CommandKeySymbol;
static inline CommandKeySymbol command_key_symbol(unsigned op){
 static const CommandKeySymbol keys[]={{0xff0du,0x0du},{0xff1bu,0x1bu},{0xff51u,0u},{0xff52u,0u},{0xff53u,0u},{0xff54u,0u}};
 static const CommandKeySymbol tab={0xff09u,0x09u},backtab={0xfe20u,0x09u},none={0u,0u};
 if(op==GLOBAL_KEY_TAB)return tab;
 if(op==GLOBAL_KEY_BACKTAB)return backtab;
 return (op>=GLOBAL_KEY_ENTER&&op<=GLOBAL_KEY_DOWN)?keys[op-GLOBAL_KEY_ENTER]:none;
}
static inline int command_toggle(unsigned op){return op>=GLOBAL_RECORD_TOGGLE&&op<=GLOBAL_LOOP_TOGGLE;}
static inline int command_transport(unsigned op){return op==GLOBAL_PLAY||op==GLOBAL_STOP;}
static inline int command_history(unsigned op){return op==GLOBAL_UNDO||op==GLOBAL_REDO;}
/* Duplicate Sequence: one press builds the application's own Copy Sequence
 * command from the current sequence into the first unused slot, named the way
 * the application names a new sequence, and submits it through the same
 * CommandManager the pencil-menu dialog's Confirm uses, so Undo and Redo cover
 * it. It resolves the editor exactly as the other editor-family global
 * operations do and takes no new hook site. See sequence-duplicate.inc. */
static inline int command_sequence_duplicate(unsigned op){return op==GLOBAL_SEQ_DUPLICATE;}
static inline int command_global(unsigned op){return op==GLOBAL_TRACK_NEW||op==GLOBAL_TRACK_TYPE||command_transport(op)||command_history(op)||command_key(op)||command_sequence_duplicate(op)||op==CF_AUTOMATION||(op>=GLOBAL_SAVE&&op<=GLOBAL_PAGE_STEP);}
enum {RC_ENTER=129,RC_DONE,RC_APPLY,RC_PRODUCER,RC_ENQUEUER,RC_ENQUEUE_RETURN,RC_SECOND_ENTER,RC_SECOND_RETURN,RC_SECOND_DONE,RC_CP_DEATH};
enum {MS_CREATE=99,MS_RESULT,MS_ENTER,MS_DONE,MS_FACTORY_DEATH,MS_FACTORY_DELETE,MS_SET_ENTER,MS_SET_DONE};
enum {PX_GET=165,PX_GET_DONE,PX_SET,PX_LISTENER,PX_NOTIFY,PX_DEATH,PX_FACADE,PX_FACADE_ALT,PX_SET_ALT,PX_REPLACE,PX_UI_VECTOR,PX_RETIRE,PX_WIDGET_CTOR,PX_WIDGET_DTOR,PX_VST_REFRESH,PX_PARAMETER_TREE,PX_UI_DRAIN,PX_INSERT_ENABLE};
enum {JN_PLAYING=159,JN_ADVANCE,JN_OPERATION,JN_RESTART_SET,JN_RESTART_DONE,JN_RESTART_EARLY};
enum {JG_PUBLISH=83,JG_BAR_ENTER,JG_BAR_DONE,JG_BAR_RESULT,JG_BEAT_ENTER,JG_BEAT_DONE,JG_BEAT_RESULT,JG_PULSE_ENTER,JG_PULSE_DONE,JG_PULSE_RESULT,JG_DESTROY,JG_DELETE};
static inline int command_jog(unsigned op){return op>=JOG_BAR&&op<=JOG_PULSE;}
/* The X-Touch jog wheel acting as the app's own data wheel. It is not a
 * transport jog: it never reaches the AsyncSequencer facade, so it shares no
 * predicate with command_jog and takes its own service lane. */
static inline int command_data_wheel(unsigned op){return op==JOG_DATA;}
/* The MPC data wheel's own push, delivered through the same focus controller as
 * the steps: UIFocusController vtable slot 5 (35c2648), the controller-side
 * press entry, called as (controller, int buttonId). It is not a key press: it
 * never reaches the peer's keyboard router, so it shares no predicate with
 * command_key and takes its own service lane. See focus-press-capture.inc. */
static inline int command_focus_press(unsigned op){return op==JOG_PRESS;}
/* The button id the panel data wheel's push carries. Device-measured on the
 * owner's Live II (2026-09-15): one real wheel push reached 35c2648 with
 * buttonId = 11 on the active controller. It is not statically recoverable, so
 * this constant is the measurement and nothing else. Defined once, read by the
 * observer's format check, its one call and the bridge's publication. */
#define FOCUS_PRESS_BUTTON 11u
/* Both focus-controller operations reach the app through the same active
 * controller and the same one-call-per-request shape, so they share a single
 * flight. */
static inline int command_focus_controller(unsigned op){return command_data_wheel(op)||command_focus_press(op);}
/* The signed data-wheel count one ProcessDataWheelRotation call may carry. The
 * app's own accelerator scales whatever delta it is handed and truncates the
 * result with sxtb, so a fast spin must not be passed through verbatim. Shared
 * so the observer clamps to it and the bridge publishes at most this per
 * request, keeping the remainder instead of dropping it. See wheel-capture.inc. */
#define WHEEL_MAX_STEPS 4
#define JOG_DELTA_LIMIT 4096
/* Signed relative native units, not a desired position or Track identity. */
static inline int command_jog_delta(uint32_t bits){int32_t n=(int32_t)bits;return n&&n>=-JOG_DELTA_LIMIT&&n<=JOG_DELTA_LIMIT;}
enum {IO_INPUT_SET=218,IO_OUTPUT_SET,IO_FILTER_SET,IO_INPUT_DONE,IO_OUTPUT_DONE,IO_FILTER_DONE,IO_CONNECTOR_BIRTH,IO_CONNECTOR_DEATH,IO_CONNECTOR_DELETE,IO_BIND_ENTER,IO_BIND_DONE,IO_PORT_CATALOGUE,IO_INPUT_CATALOGUE_SEED,IO_OUTPUT_CATALOGUE_SEED,IO_CONNECTOR_READY};
enum {CE_DISPATCH=1,CE_RETURN,CE_QUEUE,CE_BODY,CE_COMMIT,CE_DONE,CE_OTHER_COMMIT,CE_OWNER_BEGIN,CE_OWNER_END,CE_JOG_ENQUEUE,CE_JOG_ENTER,CE_JOG_DONE,CE_MASTER_CREATE,CE_MASTER_QUEUE,CE_MASTER_ENTER,CE_MASTER_SET,CE_MASTER_COMMIT,CE_MASTER_VALUE,CE_MASTER_DONE,CE_GLOBAL_BEGIN,CE_GLOBAL_STATE,CE_GLOBAL_END,CE_RECORD_QUEUE,CE_RECORD_ENTER,CE_RECORD_APPLY,CE_RECORD_DONE,CE_RECORD_WORK,CE_RECORD_WORK_ENTER,CE_RECORD_HANDLED,CE_RECORD_WORK_DONE,CE_RECORD_COMMIT,CE_RECORD_TIME,CE_PAD_EXECUTE,CE_MIDI_QUEUE,CE_MIDI_ENTER,CE_MIDI_SET,CE_MIDI_COMMIT,CE_MIDI_DONE,CE_JOG_OUTCOME,CE_EFFECT_EXECUTE,CE_EFFECT_VALUE,CE_QL_QUEUE,CE_QL_RESULT,CE_QL_ENTER,CE_QL_DONE,CE_QL_VALUE,CE_QL_BARRIER,CE_QM_QUEUE,CE_QM_RESULT,CE_QM_ENTER,CE_QM_DONE,CE_QM_VALUE,CE_QM_ACCEPT,CE_IO_SET,CE_IO_COMMIT,CE_IO_DONE,CE_IO_VALUE,CE_IO_QUEUE,CE_IO_RESULT,CE_IO_ENTER,CE_IO_AUDIO_DONE,CE_CHOOSER_ACCEPT,CE_WHEEL_STEP};
typedef struct {
 uint32_t seq,pid,start_lo,start_hi,origin_sec,origin_nsec,epoch,serial,binding,incarnation,
 bits,created,expires,before_bits,before_revision,reserved; /* reserved is the version3 ChannelField operation */
 uint32_t track_owner,program_owner,field_incarnation,project_owner,global_owner;
 uint32_t pad_owner,pad_index,pad_generation;
 uint32_t effect_generation,effect_slot,effect_index,effect_key,effect_ap,effect_position;
 uint32_t qlink_root,qlink_wrapper,qlink_provider,qlink_generation,qlink_index;
 uint32_t io_generation,io_choice_generation,io_field,io_connector;
} CommandRequest;
typedef struct {
 uint32_t kind,request,tick,token,sp,lr,capture,counter,program,arg_kind,controller,bits,incarnation,property,revision,reserved;
} CommandEvent;
typedef struct {_Atomic uint32_t words[16];} CommandEventCell;
typedef struct {
 _Atomic uint32_t token,sequence,published;
 CommandEventCell events[COMMAND_EVENTS];
} CommandLane;
typedef struct {
 _Atomic uint32_t request_sequence,request_words[39];
 _Atomic uint32_t published,processed,rejected,dispatched,returned,done,sealed,settled,reclaimed;
 CommandLane lanes[COMMAND_LANES];
} CommandSlot;
#define PROCESSOR_OBSERVATIONS 512u
#define PROCESSOR_WORDS 13u
enum {PX_LOSS_WRITER=1,PX_LOSS_FRAME=2,PX_LOSS_SEQUENCE=4};
typedef struct {_Atomic uint32_t guard,sequence,words[PROCESSOR_WORDS];} ProcessorObservation;
typedef struct {
 uint32_t magic,version,bytes,pid,start_lo,start_hi,origin_sec,origin_nsec,owner_token,capacity,seconds;
 _Atomic uint32_t alive,error,trace_error,published,consumed,closed,reclaimed;
 _Atomic uint32_t external_stop,external_gate; /* gate:0 idle,1 writer,2 permanently shut */
 _Atomic uint32_t stop_sequence,stop_requested; /* explicit stop at reclaimed frontier */
 _Atomic uint32_t new_project_intent,new_project_epoch,new_project_tick; /* 0 none,1 accepted,2 consumed,3 other shutdown */
 _Atomic uint32_t startup; /* 0 header initialization,1 setup,2 started,3 failed */
 _Atomic uint32_t admission_detail; /* one coherent packed diagnostic, no paired thread claim */
 /* Navigation admission witness, not musical position or audio-ready state.
  * watch is bridge-owned; the source alone publishes the coherent witness. */
 _Atomic uint32_t navigation_watch,navigation_guard,navigation_revision,navigation_owner,navigation_epoch,navigation_tick,navigation_count,navigation_token,navigation_error;
 _Atomic uint32_t processor_generation,processor_until,processor_key,processor_count;
 _Atomic uint32_t processor_active[COMMAND_LANES],processor_open[COMMAND_LANES],processor_loss[COMMAND_LANES],processor_dropped[COMMAND_LANES];
 ProcessorObservation processor_events[PROCESSOR_OBSERVATIONS];
 EffectsInterest effects_interest;
 QLinkInterest qlink_interest;
 IOInterest io_interest;
 MeterInterest meter_interest;
 CommandSlot slots[COMMAND_SLOTS];
 _Atomic uint32_t lane_tokens[COMMAND_LANES];
} CommandState;
/* Diagnostic publication is lane-owned except for bounded ticket/slot claims.
 * A single explicit exclusive attempt never retries inside compiler lowering. */
static inline int processor_cas(_Atomic uint32_t *address,uint32_t *expected,uint32_t desired){
#ifdef PROCESSOR_CAS_TEST
 if(processor_test_cas(address,expected,desired))return 0;
#endif
#if defined(__arm__)
 uint32_t value,result;
 __asm__ volatile("dmb ish\n ldrex %[value],[%[address]]\n cmp %[value],%[expected]\n bne 1f\n strex %[result],%[desired],[%[address]]\n b 2f\n1: clrex\n mov %[result],#1\n2: dmb ish"
 :[value]"=&r"(value),[result]"=&r"(result):[address]"r"(address),[expected]"r"(*expected),[desired]"r"(desired):"cc","memory");
 *expected=value;return !result;
#else
 return atomic_compare_exchange_weak(address,expected,desired);
#endif
}
static inline uint32_t processor_enter(CommandState *s,unsigned lane){
 uint32_t generation=atomic_load(&s->processor_generation);if(lane>=COMMAND_LANES||!generation||(generation&1))return 0;
 atomic_store(s->processor_active+lane,generation);
 if(generation!=atomic_load(&s->processor_generation)){atomic_store(s->processor_active+lane,0);return 0;}
 return generation;
}
static inline void processor_leave(CommandState *s,unsigned lane){atomic_store(s->processor_active+lane,0);}
static inline void processor_lost(CommandState *s,unsigned lane,unsigned why){
 if(lane>=COMMAND_LANES)return;
 atomic_store(s->processor_loss+lane,atomic_load(s->processor_loss+lane)|why);
 uint32_t dropped=atomic_load(s->processor_dropped+lane);if(dropped<UINT32_MAX)atomic_store(s->processor_dropped+lane,dropped+1);
}
static inline unsigned processor_open_total(const CommandState *s){unsigned n=0;for(unsigned i=0;i<COMMAND_LANES;i++)n+=atomic_load(s->processor_open+i);return n;}
static inline void processor_end_window(CommandState *s){atomic_store(&s->processor_until,0);}
static inline int processor_begin(CommandState *s,uint32_t now,uint32_t key,uint32_t milliseconds){
 if(!milliseconds||now>UINT32_MAX-milliseconds||atomic_load(&s->processor_until)>now||atomic_load(&s->processor_count)>=UINT32_MAX-1)return 0;
 uint32_t generation=atomic_load(&s->processor_generation),expected=generation;
 if((generation&1)||generation>UINT32_MAX-2||!atomic_compare_exchange_strong(&s->processor_generation,&expected,generation+1))return 0;
 for(unsigned i=0;i<COMMAND_LANES;i++)if(atomic_load(s->processor_open+i)||atomic_load(s->processor_active+i)){atomic_store(&s->processor_generation,generation);return 0;}
 atomic_store(&s->processor_key,key);atomic_store(&s->processor_until,now+milliseconds);atomic_store(&s->processor_generation,generation+2);return 1;
}
static inline int processor_publish(CommandState *s,unsigned lane,const uint32_t words[PROCESSOR_WORDS]){
 uint32_t at=atomic_load(&s->processor_count);unsigned reserved=0;
 for(unsigned attempt=0;attempt<4;attempt++){
  if(at>=UINT32_MAX-1){processor_lost(s,lane,PX_LOSS_SEQUENCE);atomic_store(&s->processor_until,0);return 0;}
  if(processor_cas(&s->processor_count,&at,at+1)){reserved=1;break;}
 }
 if(!reserved){processor_lost(s,lane,PX_LOSS_WRITER);return 0;}
 uint32_t sequence=at+1;ProcessorObservation *out=s->processor_events+at%PROCESSOR_OBSERVATIONS;unsigned guard=0;
 unsigned claimed=0;
 /* Lost exclusives with an observed free cell get at most four attempts.
  * An observed owner is contention: drop immediately, never wait for it. */
 for(unsigned attempt=0;attempt<4;attempt++){
  if(processor_cas(&out->guard,&guard,1)){claimed=1;break;}
  if(guard)break;
 }
 if(!claimed){processor_lost(s,lane,PX_LOSS_WRITER);return 0;}
 /* A delayed producer cannot replace a newer ticket at the same cell. */
 if(atomic_load(&out->sequence)>=sequence){atomic_store(&out->guard,0);processor_lost(s,lane,PX_LOSS_WRITER);return 0;}
 atomic_store_explicit(&out->sequence,0,memory_order_relaxed);atomic_thread_fence(memory_order_release);
 for(unsigned i=0;i<PROCESSOR_WORDS;i++)atomic_store_explicit(out->words+i,words[i],memory_order_relaxed);
 atomic_store_explicit(&out->sequence,sequence,memory_order_release);atomic_store(&out->guard,0);return 1;
}
static inline int processor_read(const CommandState *s,uint32_t sequence,uint32_t words[PROCESSOR_WORDS]){
 if(!sequence)return 0;
 const ProcessorObservation *cell=s->processor_events+(sequence-1)%PROCESSOR_OBSERVATIONS;
 if(atomic_load_explicit(&cell->sequence,memory_order_acquire)!=sequence)return 0;
 for(unsigned i=0;i<PROCESSOR_WORDS;i++)words[i]=atomic_load_explicit(cell->words+i,memory_order_relaxed);
 atomic_thread_fence(memory_order_acquire);return atomic_load_explicit(&cell->sequence,memory_order_relaxed)==sequence;
}
_Static_assert(sizeof(CommandRequest)==156,"pointer-free command request");
_Static_assert(sizeof(CommandEvent)==64,"copied command source event");
/* Reused words are atomic even for independent diagnostic readers. Sequence0
 * marks a changing payload; the release fence prevents new words preceding it. */
static inline void command_request_store(CommandSlot *s,const CommandRequest *r){
 atomic_store_explicit(&s->request_sequence,0,memory_order_relaxed);atomic_thread_fence(memory_order_release);
 uint32_t words[39];memcpy(words,r,sizeof(words));for(unsigned i=0;i<39;i++)atomic_store_explicit(s->request_words+i,words[i],memory_order_relaxed);
 atomic_store_explicit(&s->request_sequence,r->seq,memory_order_release);
}
static inline int command_request_read(const CommandSlot *s,uint32_t seq,CommandRequest *r){
 if(!seq||atomic_load_explicit(&s->request_sequence,memory_order_acquire)!=seq)return 0;
 uint32_t words[39];for(unsigned i=0;i<39;i++)words[i]=atomic_load_explicit(s->request_words+i,memory_order_relaxed);
 atomic_thread_fence(memory_order_acquire);memcpy(r,words,sizeof(words));return atomic_load_explicit(&s->request_sequence,memory_order_relaxed)==seq&&r->seq==seq;
}
static inline void command_event_store(CommandEventCell *cell,const CommandEvent *e){
 uint32_t words[16];memcpy(words,e,sizeof(words));for(unsigned i=0;i<16;i++)atomic_store_explicit(cell->words+i,words[i],memory_order_relaxed);
}
static inline int command_event_read(const CommandLane *lane,uint32_t seq,unsigned index,CommandEvent *e){
 if(index>=COMMAND_EVENTS||atomic_load_explicit(&lane->sequence,memory_order_acquire)!=seq||atomic_load_explicit(&lane->published,memory_order_acquire)<=index)return 0;
 uint32_t words[16];for(unsigned i=0;i<16;i++)words[i]=atomic_load_explicit(lane->events[index].words+i,memory_order_relaxed);
 atomic_thread_fence(memory_order_acquire);memcpy(e,words,sizeof(words));return atomic_load_explicit(&lane->sequence,memory_order_relaxed)==seq&&e->request==seq;
}
/* Counts are totals, not high-water completion claims. Every slot also carries
 * its exact published/reclaimed sequence; later work cannot hide an older flight. */
static inline unsigned command_pending(const CommandState *s){
 unsigned n=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)n+=atomic_load_explicit(&s->slots[i].published,memory_order_acquire)!=atomic_load_explicit(&s->slots[i].reclaimed,memory_order_acquire);return n;
}
static inline int command_idle(const CommandState *s){
 uint32_t n=atomic_load_explicit(&s->published,memory_order_acquire);
 return n==atomic_load_explicit(&s->consumed,memory_order_acquire)&&n==atomic_load_explicit(&s->reclaimed,memory_order_acquire)&&!command_pending(s);
}
static inline unsigned command_find(const CommandState *s,uint32_t seq){
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(atomic_load_explicit(&s->slots[i].published,memory_order_acquire)==seq)return i;
 return COMMAND_SLOTS;
}
static inline unsigned command_free(const CommandState *s){
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(atomic_load_explicit(&s->slots[i].published,memory_order_acquire)==atomic_load_explicit(&s->slots[i].reclaimed,memory_order_acquire))return i;
 return COMMAND_SLOTS;
}
static inline int command_conflict(const CommandRequest *a,const CommandRequest *b){
 if(command_chooser(a->reserved)||command_chooser(b->reserved)){if(command_chooser(a->reserved)&&command_chooser(b->reserved))return 1;return (command_effect(a->reserved)||command_effect(b->reserved))&&a->program_owner==b->program_owner;}
 if(command_io(a->reserved)&&command_io(b->reserved))return a->track_owner==b->track_owner&&a->io_field==b->io_field;
 if(command_qlink_mode(a->reserved)||command_qlink_mode(b->reserved)){if((command_qlink(a->reserved)||command_qlink_mode(a->reserved))&&(command_qlink(b->reserved)||command_qlink_mode(b->reserved)))return a->qlink_root==b->qlink_root;return 0;}
 if(command_qlink(a->reserved)&&command_qlink(b->reserved))return a->qlink_wrapper==b->qlink_wrapper&&a->qlink_index==b->qlink_index; /* Independent logical slots retain separate obligations. */
 if(command_jog(a->reserved)&&command_jog(b->reserved))return 0;
 /* Single-flight focus controller. One COMMAND_DRAIN can service several slots,
  * so only refusing a second published request keeps the app's accelerator
  * seeing one counted call per accepted request instead of N calls microseconds
  * apart. A press conflicts with another press and with a data-wheel step for
  * the same reason: both act on the one active controller, and the app's own
  * hardware panel never overlaps them either. */
 if(command_focus_controller(a->reserved)||command_focus_controller(b->reserved))return command_focus_controller(a->reserved)&&command_focus_controller(b->reserved);
 if(a->reserved!=b->reserved)return 0;
 if(command_effect(a->reserved))return a->program_owner==b->program_owner&&a->effect_slot==b->effect_slot&&a->effect_index==b->effect_index;
 if(a->pad_owner||b->pad_owner)return a->pad_owner&&b->pad_owner&&a->program_owner==b->program_owner&&a->pad_index==b->pad_index;
 if(command_global(a->reserved))return 1;
 if(a->reserved==CF_MASTER)return a->global_owner==b->global_owner;
 if(a->reserved==CF_SELECTION)return a->project_owner==b->project_owner;
 if(a->reserved==CF_ARM||a->reserved==CF_MIDI_VOLUME)return a->track_owner==b->track_owner;
 return a->program_owner==b->program_owner;
}
/* The cooperating-writer file lock permits one external writer. Admission and
 * permanent closure share one CAS word: a late writer cannot even change flight
 * metadata after gate2. Weak-CAS reservation loss is bounded and fails closed.
 * Only the consumer closes this gate; source hooks never wait or acquire it. */
#ifndef COMMAND_WRITER_PAUSE
#define COMMAND_WRITER_PAUSE(n) ((void)0)
#endif
static inline int command_writer_enter(CommandState *s){
 COMMAND_WRITER_PAUSE(0);
 for(unsigned attempt=0;attempt<4;attempt++){
  uint32_t idle=0;
  if(atomic_compare_exchange_weak_explicit(&s->external_gate,&idle,1,memory_order_seq_cst,memory_order_seq_cst)){
   COMMAND_WRITER_PAUSE(4);
   if(!atomic_load_explicit(&s->external_stop,memory_order_seq_cst)&&atomic_load_explicit(&s->alive,memory_order_acquire)&&!atomic_load(&s->closed)&&!atomic_load(&s->error)&&!atomic_load(&s->trace_error)&&!atomic_load_explicit(&s->stop_requested,memory_order_acquire))return 1;
   atomic_store_explicit(&s->external_gate,0,memory_order_seq_cst);return 0;
  }
  if(idle)return 0;
 }
 return 0;
}
static inline void command_writer_leave(CommandState *s){atomic_store_explicit(&s->external_gate,0,memory_order_seq_cst);}
static inline int command_navigation_watch(CommandState *s,unsigned enabled){
 if(!command_writer_enter(s))return 0;
 atomic_store_explicit(&s->navigation_watch,enabled,memory_order_release);command_writer_leave(s);return 1;
}
static inline int command_writer_shut(CommandState *s){
 atomic_store_explicit(&s->external_stop,1,memory_order_seq_cst);
 uint32_t idle=atomic_load_explicit(&s->external_gate,memory_order_seq_cst);
 if(idle==2)return 1;
 if(idle)return 0;
 /* A failed reservation is retried by the bounded consumer close loop. */
 return atomic_compare_exchange_weak_explicit(&s->external_gate,&idle,2,memory_order_seq_cst,memory_order_seq_cst);
}
static inline int command_publish_request_at(CommandState *s,const CommandRequest *r,unsigned at){
 if(command_qlink_mode(r->reserved)){if(!r->qlink_root||!r->global_owner||!r->field_incarnation||!command_mode_delta(r->bits)||r->qlink_wrapper||r->qlink_provider||r->qlink_generation||r->qlink_index)return 0;}else if(command_qlink(r->reserved)){if(!r->qlink_root||!r->qlink_wrapper||!r->qlink_provider||!r->qlink_generation||r->qlink_index>=QLINK_SLOTS||r->pad_owner||r->global_owner)return 0;}else if(r->qlink_root||r->qlink_wrapper||r->qlink_provider||r->qlink_generation||r->qlink_index)return 0;
 if(command_io(r->reserved)){if(!r->io_generation||!r->io_choice_generation||r->io_field>=IO_FIELDS||!r->io_connector||!command_mode_delta(r->bits))return 0;}else if(r->io_generation||r->io_choice_generation||r->io_field||r->io_connector)return 0;
 if(command_chooser(r->reserved)){if(r->bits>1||!r->effect_generation||r->effect_slot>=EFFECT_SLOTS||r->effect_index||r->effect_position>=(EFFECT_MAX_PRESENTATION+7)/8||r->pad_owner||r->global_owner||(r->bits?(!r->effect_key||!r->effect_ap):(r->effect_key||r->effect_ap)))return 0;}else if(command_effect(r->reserved)){if(!r->effect_generation||r->effect_slot>=EFFECT_SLOTS||r->effect_index>=EFFECT_MAX_PARAMETERS||r->effect_position>=EFFECT_MAX_PRESENTATION||!r->effect_key||!r->effect_ap||r->pad_owner||r->pad_index||r->pad_generation||r->global_owner)return 0;}else if(r->effect_generation||r->effect_slot||r->effect_index||r->effect_key||r->effect_ap||r->effect_position)return 0;
 if(r->pad_owner?(!r->pad_generation||r->pad_owner>PAD_OWNERS||r->pad_index>=PAD_SLOTS||!pad_controller(r->reserved)||r->global_owner):(r->pad_index||r->pad_generation))return 0;
 if(atomic_load_explicit(&s->new_project_intent,memory_order_seq_cst)||!command_writer_enter(s))return 0;
 if(atomic_load_explicit(&s->new_project_intent,memory_order_seq_cst)){command_writer_leave(s);return 0;}
 if(at>=COMMAND_SLOTS||atomic_load_explicit(&s->slots[at].published,memory_order_acquire)!=atomic_load_explicit(&s->slots[at].reclaimed,memory_order_acquire)||r->seq!=atomic_load_explicit(&s->published,memory_order_acquire)+1||r->seq>COMMAND_SEQUENCE_LAST){command_writer_leave(s);return 0;}
 for(unsigned i=0;i<COMMAND_SLOTS;i++){
  CommandSlot *q=s->slots+i;unsigned seq=atomic_load_explicit(&q->published,memory_order_acquire);CommandRequest old;
  if(seq!=atomic_load_explicit(&q->reclaimed,memory_order_acquire)&&(!command_request_read(q,seq,&old)||command_conflict(&old,r))){command_writer_leave(s);return 0;}
 }
 COMMAND_WRITER_PAUSE(1);command_request_store(s->slots+at,r);
 COMMAND_WRITER_PAUSE(2);if(atomic_load_explicit(&s->new_project_intent,memory_order_seq_cst)){command_writer_leave(s);return 0;}
 atomic_store_explicit(&s->published,r->seq,memory_order_release);
 atomic_store_explicit(&s->slots[at].published,r->seq,memory_order_release);
 command_writer_leave(s);return 1;
}
static inline int command_publish_request(CommandState *s,const CommandRequest *r){return command_publish_request_at(s,r,command_free(s));}
static inline int command_publish_settlement(CommandState *s,uint32_t seq){
 if(!command_writer_enter(s))return 0;
 unsigned at=command_find(s,seq);
 if(at==COMMAND_SLOTS||atomic_load_explicit(&s->slots[at].sealed,memory_order_acquire)!=seq){command_writer_leave(s);return 0;}
 COMMAND_WRITER_PAUSE(3);atomic_store_explicit(&s->slots[at].settled,seq,memory_order_release);
 command_writer_leave(s);return 1;
}
/* Zero seconds is explicit manual operation, bounded by the elapsed clock and
 * retained storage capacities. It is not an unlimited-lifetime promise. */
static inline int command_duration_valid(uint32_t seconds){return !seconds||(seconds>=60&&seconds<=900);}
static inline uint32_t command_tick_limit(uint32_t seconds){return seconds?seconds*1000u:UINT32_MAX-1u;}
static inline int command_request_stop(CommandState *s){
 if(!command_writer_enter(s))return 0;
 uint32_t n=atomic_load_explicit(&s->published,memory_order_acquire);
 int ready=command_idle(s);
 if(ready){COMMAND_WRITER_PAUSE(5);atomic_store_explicit(&s->stop_sequence,n,memory_order_relaxed);atomic_store_explicit(&s->stop_requested,1,memory_order_release);}
 command_writer_leave(s);return ready;
}
static inline int command_float(unsigned field){return command_qlink(field)||command_effect(field)||field==CF_MIDI_VOLUME||field==CF_MASTER||field==CF_VOLUME||field==CF_PAN||(field>=CF_SEND1&&field<=CF_SEND4);}
static inline unsigned command_controller(unsigned field){return field==CF_VOLUME||field==CF_MIDI_VOLUME?7:field==CF_PAN?10:field==CF_MUTE?0x100:field==CF_SOLO?0x101:field>=CF_SEND1&&field<=CF_SEND4?0x5b+field-CF_SEND1:0;}
static inline unsigned command_arg_kind(const CommandRequest *r){return r->reserved==CF_MIDI_VOLUME?0x100:r->pad_owner?r->pad_index:0x101;}
static inline unsigned command_request_controller(const CommandRequest *r){return r->reserved==EFFECT_ENABLE?0x146+r->effect_slot:r->reserved==EFFECT_PARAMETER?0x6000+(r->effect_slot<<12)+r->effect_index:r->pad_owner?pad_controller(r->reserved):command_controller(r->reserved);}
static inline int command_recording(const CommandRequest *r){return !command_qlink(r->reserved)&&(r->pad_owner||command_float(r->reserved)||(command_io(r->reserved)&&(r->io_field==IO_AUDIO_IN||r->io_field==IO_AUDIO_OUT)));}
static inline unsigned command_source_field(unsigned field){return field==CF_SOLO?CF_SOLO_AUDIO:field;}
static inline int command_supported(unsigned field){return command_chooser(field)||command_io(field)||command_qlink_mode(field)||command_qlink(field)||command_effect(field)||field==CF_MIDI_VOLUME||field==CF_VOLUME||field==CF_PAN||field==CF_MUTE||field==CF_SOLO||field==CF_ARM||field==CF_SELECTION||(field>=CF_SEND1&&field<=CF_SEND4);}
static inline int command_operation(unsigned op){return command_supported(op)||command_jog(op)||command_focus_controller(op)||op==CF_MASTER||command_global(op);}
static inline uint32_t command_native_bits(unsigned field,uint32_t bits){if(field!=CF_MUTE&&field!=CF_SOLO)return bits;float f=bits?1.0f:0.0f;uint32_t b;memcpy(&b,&f,4);return b;}
extern CommandState *command_state;
extern _Atomic uint32_t command_in_hook,command_violation;
void command_initialize(void);
void command_retire(void);
void command_close(void);
unsigned command_meter_closing(void);
unsigned command_enrollment_error(void);
unsigned command_enrollment_detail(void);
int command_quiescent(void);
#endif
