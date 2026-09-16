/* Finite experimental native dispatch over the existing MMV4 state owner. */
#ifdef COMMAND_COMPONENT
static void component_pause(unsigned);
#define COMMAND_PAUSE(n) component_pause(n)
#define COMMAND_WRITER_PAUSE(n) component_pause((n)+8)
#else
#define COMMAND_PAUSE(n) ((void)0)
#endif
#define observer_hook command_mirror_hook
#include "mirror-capture.c"
#undef observer_hook
#include "command-state.h"
#include "transport-queue.h"
CommandState *command_state;
_Atomic uint32_t command_in_hook,command_violation;
static uint32_t command_owner,command_queues;
static _Atomic uint32_t active_request[COMMAND_SLOTS];
static unsigned service_cursor;
/* UI-owned call boundary, read only after processed publishes its generation.
 * A post-call rejection is not a never-submitted request. */
static uint32_t native_started[COMMAND_SLOTS];
static struct {CommandRequest request;uint32_t program,track,property,native_bits,controller,synchronous;} accepted[COMMAND_SLOTS];
/* Permanent per-lane reservations: concurrent first callbacks must not contend
 * for one registry lock. Each reservation has its own exclusive granule. */
static AdmissionStorage lane_claims[COMMAND_LANES] __attribute__((aligned(2048)));
typedef struct {uint32_t request,capture,counter,program,sp,phase;} CommandInvocation;
static __thread uint32_t source_requests[COMMAND_SLOTS] __attribute__((tls_model("initial-exec")));
static __thread unsigned local_lane __attribute__((tls_model("initial-exec")));
typedef struct {uint32_t output,sequencer,sp,epoch,eligible;} PositionInvocation;
static __thread PositionInvocation positions[4] __attribute__((tls_model("initial-exec")));
static __thread unsigned position_depth __attribute__((tls_model("initial-exec")));
static __thread CommandInvocation invocation __attribute__((tls_model("initial-exec")));
static _Atomic uint32_t lane_in_hook[COMMAND_LANES];
/* Enrollment is rare (once per source thread), not a scalar writer lock.
 * SC count closes the interval before that thread has a lane. Normal source
 * callbacks then use only their own lane SC stores, with no exclusive loop. */
static AdmissionStorage enrollment_storage __attribute__((aligned(2048)));
#define enrolling enrollment_storage.busy
static _Atomic uint32_t enrollment_fault,enrollment_detail;
/* A failed decrement leaves conservative flight: bounded close reports unclosed.
 * A failed increment has no admitted output writer; only the consumer turns
 * this private sticky fault into a public error. No post-close error writes. */
static int enrollment_change(int delta,unsigned site){
 uint32_t result,value,left;
 __asm__ volatile("dmb ish\n mov %[left],#4\n1: ldrex %[value],[%[address]]\n add %[value],%[value],%[delta]\n strex %[result],%[value],[%[address]]\n cmp %[result],#0\n beq 2f\n subs %[left],%[left],#1\n bne 1b\n2: dmb ish"
 :[result]"=&r"(result),[value]"=&r"(value),[left]"=&r"(left)
 :[address]"r"(&enrolling),[delta]"r"(delta):"cc","memory");
 if(result){atomic_store_explicit(&enrollment_detail,command_admission_detail(delta>0?CA_ENROLL_INCREMENT:CA_ENROLL_DECREMENT,result,site),memory_order_seq_cst);atomic_store_explicit(&enrollment_fault,C_TRACE_LOSS,memory_order_seq_cst);atomic_store_explicit(&observer_running,0,memory_order_seq_cst);}
 return !result;
}
static void command_fail(unsigned why){atomic_store_explicit(&command_state->error,why,memory_order_release);atomic_store_explicit(&observer_running,0,memory_order_seq_cst);}
static void trace_fail(unsigned why){atomic_store_explicit(&command_state->trace_error,why,memory_order_release);}
/* Same qualified Program constructor owner already owns channel births. The
 * enclosing lane flight detects reentry before this function, and owns close. */
static int command_birth_owner(unsigned site){
 if(token()==command_owner&&token()==atomic_load_explicit(&mirror_state->channel.owner_token,memory_order_acquire))return 1;
 atomic_store_explicit(&command_state->admission_detail,command_admission_detail(CA_CONSTRUCTOR_OWNER,C_OWNER,site),memory_order_release);
 command_fail(C_OWNER);mirror_stop(MV_LIFETIME);return 0;
}
static void jog_initialize(void);
static void master_initialize(void);
static void meter_initialize(void);
static void global_initialize(void);
static int global_quiescent(void);
static void recording_initialize(void);
static void midi_initialize(void);
static void effects_initialize(void);
static void io_initialize(void);
static int io_receipted(unsigned at);
static void qlink_initialize(void);
static void qlink_command_initialize(void);
static void qlink_mode_initialize(void);
static int qlink_mode_receipted(unsigned);
static int qlink_receipted(unsigned);
static int effects_receipted(unsigned);
static void recording_lifetime(uint32_t,int);
void command_initialize(void){
 qlink_command_initialize();qlink_mode_initialize();qlink_initialize();effects_initialize();io_initialize();jog_initialize();master_initialize();meter_initialize();global_initialize();recording_initialize();midi_initialize();
 command_owner=token();command_queues=0;atomic_store(&command_in_hook,0);atomic_store(&command_violation,0);for(unsigned i=0;i<COMMAND_SLOTS;i++)atomic_store(active_request+i,0);for(unsigned i=0;i<COMMAND_LANES;i++)release(&lane_claims[i].busy);atomic_store(&lane_claims[0].busy,1);
 atomic_store(&enrolling,0);atomic_store(&enrollment_fault,0);atomic_store(&enrollment_detail,0);memset(source_requests,0,sizeof(source_requests));service_cursor=0;local_lane=1;position_depth=0;memset(&invocation,0,sizeof(invocation));memset(accepted,0,sizeof(accepted));memset(native_started,0,sizeof(native_started));
 channel_initialize();
 command_state->owner_token=command_owner;atomic_store(&command_state->lane_tokens[0],command_owner);
 for(unsigned i=0;i<COMMAND_LANES;i++)atomic_store(lane_in_hook+i,0);
}
unsigned command_enrollment_detail(void){return atomic_load_explicit(&enrollment_detail,memory_order_seq_cst);}
unsigned command_enrollment_error(void){return atomic_load_explicit(&enrollment_fault,memory_order_seq_cst);}
void command_close(void){atomic_store_explicit(&observer_running,0,memory_order_seq_cst);if(command_state)atomic_store_explicit(&command_state->external_stop,1,memory_order_seq_cst);}
int command_quiescent(void){
 if(atomic_load_explicit(&observer_running,memory_order_seq_cst)||atomic_load_explicit(&command_in_hook,memory_order_seq_cst))return 0;
 if(!command_writer_shut(command_state))return 0;
 if(!global_quiescent())return 0;
 if(atomic_load_explicit(&enrolling,memory_order_seq_cst))return 0;
 for(unsigned i=0;i<COMMAND_LANES;i++)if(atomic_load_explicit(lane_in_hook+i,memory_order_seq_cst))return 0;
 return 1;
}
/* A trace lane is assigned once per observed execution thread. Its events have
 * one writer, including source commits on that thread. Reservation loss affects
 * source availability in this finite command mode; it never manufactures an ack. */
static unsigned trace_lane(unsigned site){
 if(local_lane)return local_lane-1;
 uint32_t tp=token();unsigned lost=0;
 for(unsigned i=0;i<COMMAND_LANES;i++)if(atomic_load_explicit(&command_state->lane_tokens[i],memory_order_acquire)==tp){local_lane=i+1;return i;}
 for(unsigned i=0;i<COMMAND_LANES;i++){
  unsigned rc=claim_result(&lane_claims[i].busy);
  if(rc){lost|=rc==3;continue;} /* occupied lane is ordinary concurrency */
  COMMAND_PAUSE(20); /* claimed, not yet published; enrolling still owns flight */
  atomic_store_explicit(&command_state->lane_tokens[i],tp,memory_order_release);local_lane=i+1;return i;
 }
 atomic_store_explicit(&command_state->admission_detail,command_admission_detail(CA_LANE_GUARD,lost?3:2,site),memory_order_release);
 trace_fail(lost?C_TRACE_LOSS:C_CAPACITY);return COMMAND_LANES;
}
static unsigned trace_enter(unsigned site){
 if(!atomic_load_explicit(&observer_running,memory_order_seq_cst))return COMMAND_LANES;
 unsigned first=!local_lane,lane;
 if(first){
  if(!enrollment_change(1,site))return COMMAND_LANES;
  COMMAND_PAUSE(1); /* before registration/token/error assignment */
  if(!atomic_load_explicit(&observer_running,memory_order_seq_cst)){
   enrollment_change(-1,site);return COMMAND_LANES;
  }
  lane=trace_lane(site);
  if(lane==COMMAND_LANES){
   /* A source callback cannot be silently lost from authoritative MMV4. */
   command_fail(C_TRACE_LOSS);mirror_stop(MV_PUBLICATION_LOSS);
   enrollment_change(-1,site);return lane;
  }
 }else lane=local_lane-1;
 if(atomic_load_explicit(lane_in_hook+lane,memory_order_seq_cst)){
  atomic_store_explicit(&command_state->admission_detail,command_admission_detail(CA_SOURCE_REENTRY,C_REENTRY,site),memory_order_release);trace_fail(C_REENTRY);command_fail(C_REENTRY);
  if(first)enrollment_change(-1,site);
  return COMMAND_LANES;
 }
 atomic_store_explicit(lane_in_hook+lane,1,memory_order_seq_cst);
 /* Handoff has no zero-flight interval visible to the closer. */
 if(first)enrollment_change(-1,site);
 if(!atomic_load_explicit(&observer_running,memory_order_seq_cst)){
  atomic_store_explicit(lane_in_hook+lane,0,memory_order_seq_cst);return COMMAND_LANES;
 }
 /* Meter callbacks publish peaks only; they cannot produce command receipts.
  * Keep the SC lane/closure handshake, but avoid 32 SC loads on every audio
  * block for an association table that this callback never consumes. */
 if(site<ME_STEREO||site>ME_TRACK)
  for(unsigned i=0;i<COMMAND_SLOTS;i++)source_requests[i]=atomic_load_explicit(active_request+i,memory_order_seq_cst);
 return lane;
}
static void trace_leave(unsigned lane){memset(source_requests,0,sizeof(source_requests));atomic_store_explicit(lane_in_hook+lane,0,memory_order_seq_cst);}
/* Called inside the complete source hook's lane interval, or by the sole UI
 * owner inside command_in_hook. No narrower event-only closing loophole. */
static void event(Context *c,unsigned kind,unsigned id,uint32_t capture,uint32_t counter,uint32_t program,uint32_t arg_kind,uint32_t controller,uint32_t bits,uint32_t property,uint32_t revision){
 unsigned lane=local_lane?local_lane-1:COMMAND_LANES,at=command_find(command_state,id);
 if(lane==COMMAND_LANES||at==COMMAND_SLOTS){trace_fail(C_TRACE_LOSS);return;}
 CommandLane *l=command_state->slots[at].lanes+lane;unsigned n=atomic_load_explicit(&l->published,memory_order_relaxed);
 if(n==COMMAND_EVENTS){trace_fail(C_CAPACITY);return;}
 CommandEvent e=(CommandEvent){kind,id,atomic_load_explicit(&mirror_state->heartbeat,memory_order_acquire),token(),c?source_sp(c):0,c?c->lr:0,capture,counter,program,arg_kind,controller,bits,id?((accepted[at].request.reserved||accepted[at].request.pad_owner)?accepted[at].request.field_incarnation:accepted[at].request.incarnation):0,property,revision,id?accepted[at].request.reserved:0};
 if(!id||id!=accepted[at].request.seq||atomic_load_explicit(&l->sequence,memory_order_acquire)!=id){trace_fail(C_TRACE_AMBIGUOUS);return;}
 atomic_store_explicit(&l->token,token(),memory_order_relaxed);
 command_event_store(l->events+n,&e);atomic_store_explicit(&l->published,n+1,memory_order_release);
}
static void effects_native_body(unsigned at);
static void queue_point(Context *c,unsigned site){
 if(site==M_QUEUE_ENTER){
  unsigned any=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)any|=source_requests[i];if(!any)return;
  uint32_t a=c->r[0];if(!pointer(a)||a>UINT32_MAX-20){trace_fail(C_TRACE_AMBIGUOUS);return;}
  uint32_t counter=word(a),program=word(a+4),kind=word(a+8),controller=word(a+12),bits=word(a+16);
  unsigned match=COMMAND_SLOTS;
  for(unsigned i=0;i<COMMAND_SLOTS;i++)if(source_requests[i]&&accepted[i].program==program&&accepted[i].controller==controller&&kind==command_arg_kind(&accepted[i].request)){
   if(accepted[i].request.seq!=source_requests[i]||accepted[i].native_bits!=bits||match!=COMMAND_SLOTS){trace_fail(C_TRACE_AMBIGUOUS);return;}match=i;
  }
  if(match==COMMAND_SLOTS)return;
  unsigned id=source_requests[match];
  if(invocation.request||counter!=program+0x40){trace_fail(C_TRACE_AMBIGUOUS);return;}
  invocation=(CommandInvocation){id,a,counter,program,source_sp(c),1};
  event(c,CE_QUEUE,id,a,counter,program,kind,controller,bits,0,0);
 }else if(invocation.request){
  unsigned id=invocation.request,at=command_find(command_state,id);
  if(at==COMMAND_SLOTS||source_requests[at]!=id){trace_fail(C_TRACE_AMBIGUOUS);return;}
  uint32_t p=accepted[at].program;
  if(site==M_COMMAND_BODY){
   if(c->r[0]!=p||c->lr!=observer_image_bias+0x25178d4||invocation.phase!=1||source_sp(c)+8!=invocation.sp||c->r[1]!=command_arg_kind(&accepted[at].request)||c->r[2]!=accepted[at].controller||(uint32_t)c->d[0]!=accepted[at].native_bits){trace_fail(C_TRACE_AMBIGUOUS);return;}
   invocation.phase=2;effects_native_body(at);event(c,CE_BODY,id,invocation.capture,invocation.counter,p,c->r[1],c->r[2],(uint32_t)c->d[0],0,0);
  }else{
   if(invocation.phase!=2||source_sp(c)+8!=invocation.sp||c->r[4]!=invocation.capture||c->r[3]!=invocation.counter){trace_fail(C_TRACE_AMBIGUOUS);return;}
   event(c,CE_DONE,id,invocation.capture,c->r[3],p,command_arg_kind(&accepted[at].request),accepted[at].controller,accepted[at].native_bits,0,0);
   memset(&invocation,0,sizeof(invocation));atomic_store_explicit(active_request+at,0,memory_order_seq_cst);
   atomic_store_explicit(&command_state->slots[at].done,id,memory_order_release);COMMAND_PAUSE(7);
  }
 }
}
static int vector(uint32_t object,unsigned offset,uint32_t target){
 uint32_t a=word(object+offset),b=word(object+offset+4),cap=word(object+offset+8);
 if(!pointer(a)||b<a||cap<b||(b-a)%4||(cap-a)%4||(cap-a)/4>MIRROR_TRACKS)return 0;
 unsigned found=0;for(uint32_t at=a;at<b;at+=4)found+=word(at)==target;
 return found==1&&a==word(object+offset)&&b==word(object+offset+4)&&cap==word(object+offset+8);
}
/* Project add excludes native bus kinds from ProgramPool. These Programs are
 * owned by the rooted Mixer, in distinct typed send/submix/output vectors.
 * A Track binding alone never supplies their Program lifetime. */
static int program_member(uint32_t pp,const Track *t){
 unsigned offset=0;uint32_t vptr=0;
 switch(t->kind){
 case 7:offset=0x78;vptr=0x6931f70;break;
 case 8:offset=0x84;vptr=0x6932150;break;
 case 9:offset=0x90;vptr=t->vptr==0x6931d90?0x6931d90:0x6931bb0;break;
 default:return vector(pp,0x1c,t->program);
 }
 uint32_t a=word(root+0x2e4);if(!pointer(a)||a>UINT32_MAX-0x400)return 0;
 uint32_t mixer=a+0x260;unsigned id=atomic_load(&mirror_state->channel.mixer_owner);
 if(t->vptr!=vptr||!id||channel_owner_id(mixer)!=id||!typed(mixer,0x6898b34))return 0;
 return vector(mixer,offset,t->program)&&typed(t->program,vptr)&&word(root+0x2e4)==a&&channel_owner_id(mixer)==id;
}
static int command_mixable(uint32_t p){
 static const uint32_t types[]={0x6930c00,0x692fec4,0x6932a48,0x693147c,0x69317a8,0x69319d0,0x6931bb0,0x6931d90,0x6931f70,0x6932150};
 for(unsigned i=0;i<sizeof(types)/sizeof(*types);i++)if(typed(p,types[i]))return word(word(p)+0x78)==observer_image_bias+0x1375c68;
 return 0;
}
static ChannelCell *request_cell(const CommandRequest *r,const Track *t){
 unsigned field=command_source_field(r->reserved),owner=channel_kind(field)==CO_PROJECT?project:channel_kind(field)==CO_TRACK?t->track:t->program;
 ChannelCell *c=channel_cell(owner+channel_offset(field));
 unsigned owner_id=channel_kind(field)==CO_PROJECT?r->project_owner:channel_kind(field)==CO_TRACK?r->track_owner:r->program_owner;
 if(!c||atomic_load(&c->field)!=field||atomic_load(&mirror_state->channel.errors[field])||!atomic_load_explicit(&c->live,memory_order_acquire)||atomic_load(&c->owner_incarnation)!=owner_id||atomic_load(&c->incarnation)!=r->field_incarnation||channel_owner_id(owner)!=owner_id)return NULL;
 return c;
}
#ifdef COMMAND_COMPONENT
static uint32_t (*component_midi_lookup)(uint32_t,uint32_t);
#endif
/* Native history addresses the first Track with this visible name. Resolve it
 * before invoking the factory, while the existing UI-owner admission is held. */
static int midi_preflight(const Track *t){
 if(!typed(t->program,0x6932338)||!typed(t->track,0x6935554))return 0;
 uint32_t resolved;
#ifdef COMMAND_COMPONENT
 resolved=component_midi_lookup?component_midi_lookup(pool,t->track+0x468):0;
#else
 resolved=((uint32_t (*)(void*,void*))(uintptr_t)(observer_image_bias+0x257eec4))((void*)(uintptr_t)pool,(void*)(uintptr_t)(t->track+0x468));
#endif
 return resolved==t->track;
}
static PadScalar *request_pad(const CommandRequest *r,const Track *t,int native){
 if(!r->pad_owner||r->pad_owner>PAD_OWNERS||r->pad_index>=PAD_SLOTS||!r->pad_generation||!pad_controller(r->reserved)||atomic_load(&mirror_state->pads.error))return NULL;
 PadParent *p=pad_parent(t->program,0);if(!p||!atomic_load(&p->live)||atomic_load(&p->owner)!=r->program_owner||(atomic_load(&p->revision)&1))return NULL;
 if(atomic_load(&p->slots[r->pad_index].generation)!=r->pad_generation||atomic_load(&p->slots[r->pad_index].owner)!=r->pad_owner)return NULL;
 PadOwner *o=mirror_state->pads.owners+r->pad_owner-1;
 if(!atomic_load_explicit(&o->live,memory_order_acquire)||!atomic_load(&o->complete)||atomic_load(&o->parent)!=t->program||atomic_load(&o->parent_owner)!=r->program_owner)return NULL;
 unsigned f=pad_field(command_source_field(r->reserved));
 if(f==PF_COUNT||r->field_incarnation!=r->pad_owner*PF_COUNT+f)return NULL;
 PadScalar *v=o->fields+f;if(!atomic_load(&v->live)||!atomic_load(&v->seed))return NULL;
 if(native){uint32_t child=atomic_load(&o->address),a=word(t->program+0x1fec),b=word(t->program+0x1ff0),cap=word(t->program+0x1ff4);
  if(!typed(t->program,0x6930c00)||!pointer(a)||b<a||cap<b||(b-a)%4||(cap-a)%4||(cap-a)/4>PAD_SLOTS||r->pad_index>=(b-a)/4||word(a+r->pad_index*4)!=child||!typed(child,0x692f650)||word(child+0x13b0)!=t->program||word(child+0x12dc)!=r->pad_index||*(const volatile unsigned char*)(uintptr_t)(child+0x13d8))return NULL;
 }
 return v;
}
/* Internal deferral, never a published rejection or acknowledgement. */
#define COMMAND_WAIT_HEARTBEAT UINT32_MAX
static unsigned resolve(Context *c,const CommandRequest *r,Track **out,uint32_t *function){
 uint32_t now=atomic_load_explicit(&mirror_state->heartbeat,memory_order_acquire);unsigned field=r->reserved;
 if(!command_supported(field)||r->global_owner||(!r->pad_owner&&(r->pad_index||r->pad_generation)))return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 if(command_float(field)){float desired;memcpy(&desired,&r->bits,4);if(!(desired>=0.0f&&desired<=1.0f))return C_FORMAT;}else if(r->bits>1||(field==CF_SELECTION&&!r->bits))return C_FORMAT;
 if(r->created>=command_tick_limit(WINDOW_SECONDS)||r->expires<r->created||r->expires-r->created>120000u||r->expires<now)return C_EXPIRED;
 /* The external client's same-origin clock can lead the consumer's last
  * heartbeat. Wait for its next sample; do not mistake sample age for expiry.
  * Every identity/lifetime check runs again before any eventual native call. */
 if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 if(!active()||atomic_load(&enrollment_fault)||atomic_load(&command_state->trace_error))return C_SOURCE_FAILURE;
 if(!ready||loading||reset_pending||depth||!root||!project||!pool)return C_NOT_READY;
 if(r->epoch!=epoch)return C_IDENTITY;
 for(unsigned i=0;i<count;i++)if(tracks[i].transition)return C_NOT_READY;
 if(!command_queues||c->r[0]!=command_queues+4||word(c->r[0]+0xf0)!=0)return C_NOT_READY;
 Track *t=NULL;for(unsigned i=0;i<count;i++)if(tracks[i].serial==r->serial){if(t)return C_IDENTITY;t=tracks+i;}
 if(!t||t->binding!=r->binding||t->incarnation!=r->incarnation||t->track_owner!=r->track_owner||t->program_owner!=r->program_owner||!r->track_owner||!r->program_owner||channel_owner_id(t->track)!=r->track_owner||channel_owner_id(t->program)!=r->program_owner||channel_owner_id(project)!=r->project_owner)return C_IDENTITY;
 uint32_t e=root;if(!typed(e,0x68d7bf8))return C_NATIVE_MEMBERSHIP;
 uint32_t pr=word(e+0x24c);if(pr!=project||!typed(pr,0x6933c4c))return C_NATIVE_MEMBERSHIP;
 uint32_t tp=word(pr+0xa94),pp=word(pr+0xa90);
 if(tp!=pool||!typed(tp,0x68ad2c4)||!typed(pp,0x68ad07c)||!vector(tp,0x34,t->track))return C_NATIVE_MEMBERSHIP;
 if(!typed(t->track,0x6935554)||word(t->track+0x648)!=t->program||word(t->track+0x64c)!=t->kind||!program_member(pp,t))return C_NATIVE_MEMBERSHIP;
 if(field!=CF_MIDI_VOLUME&&field!=CF_SELECTION&&field!=CF_ARM&&(!command_mixable(t->program)||word(word(t->program)+0x28)!=observer_image_bias+0x250ba74))return C_NOT_DRUM;
 if(*(const volatile unsigned char*)(uintptr_t)(t->program+0x1158))return C_PREPARED;
 if(field==CF_MIDI_VOLUME&&!midi_preflight(t))return C_NATIVE_MEMBERSHIP;
 uint32_t rev,bits;
 if(r->pad_owner){
  PadScalar *cell=request_pad(r,t,1);if(!cell)return C_IDENTITY;
  rev=atomic_load_explicit(&cell->revision,memory_order_acquire);bits=atomic_load(&cell->bits);atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load(&cell->revision))return C_SOURCE_CHANGED;
 }else if(field==CF_VOLUME){
  MirrorCell *cell=lookup(t->program+0x5b0);
  if(!cell||!atomic_load_explicit(&cell->live,memory_order_acquire)||atomic_load(&cell->incarnation)!=t->incarnation)return C_IDENTITY;
  rev=atomic_load_explicit(&cell->revision,memory_order_acquire);bits=atomic_load_explicit(&cell->bits,memory_order_relaxed);atomic_thread_fence(memory_order_acquire);
  if(rev!=atomic_load_explicit(&cell->revision,memory_order_relaxed))return C_SOURCE_CHANGED;
 }else{
  ChannelCell *cell=request_cell(r,t);if(!cell)return C_IDENTITY;
  rev=atomic_load_explicit(&cell->revision,memory_order_acquire);bits=atomic_load_explicit(&cell->bits,memory_order_relaxed);atomic_thread_fence(memory_order_acquire);
  if(rev!=atomic_load_explicit(&cell->revision,memory_order_relaxed))return C_SOURCE_CHANGED;
 }
 if((rev&1)||rev!=r->before_revision||bits!=r->before_bits||(field==CF_SELECTION?bits==t->track:bits==r->bits))return C_SOURCE_CHANGED;
 if(pr!=word(e+0x24c)||tp!=word(pr+0xa94)||pp!=word(pr+0xa90)||word(t->track+0x648)!=t->program)return C_NATIVE_MEMBERSHIP;
 /* Selection/arm invoke owner operations, never raw Property setters. */
 if(field==CF_ARM&&(!typed(t->track+0x46c,0x69355e0)||word(word(t->track)+0x28)!=observer_image_bias+0x26500f8||!word(t->track+0x48c)))return C_NATIVE_MEMBERSHIP;
 *out=t;*function=observer_image_bias+(field==CF_SELECTION?0x2549c90:field==CF_ARM?0x26500f8:field==CF_MIDI_VOLUME?0x13d5fd0:0x250ba74);return C_OK;
}
#ifdef COMMAND_COMPONENT
static void (*component_dispatch)(uint32_t,uint32_t,uint32_t,float);
#endif
static void native_dispatch(uint32_t program,uint32_t function,unsigned controller,float value){
#ifdef COMMAND_COMPONENT
 (void)function;component_dispatch(program,0x101,controller,value);
#else
 ((void (*)(void*,uint32_t,uint32_t,float))(uintptr_t)function)((void*)(uintptr_t)program,0x101,controller,value);
#endif
}
/* Called only by the existing consumer. Retired matching plus source/UI
 * admission closure protects immutable old metadata. No native pointer reads. */
static int transaction_drained(unsigned at){
 if(atomic_load_explicit(active_request+at,memory_order_seq_cst)||atomic_load_explicit(&enrolling,memory_order_seq_cst)||atomic_load_explicit(&command_in_hook,memory_order_seq_cst))return 0;
 for(unsigned i=0;i<COMMAND_LANES;i++)if(atomic_load_explicit(lane_in_hook+i,memory_order_seq_cst))return 0;
 return 1;
}
void command_retire(void){
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  CommandSlot *slot=command_state->slots+at;
  unsigned seq=atomic_load_explicit(&slot->published,memory_order_acquire);
  if(!seq||atomic_load_explicit(&slot->reclaimed,memory_order_acquire)==seq||atomic_load_explicit(&slot->processed,memory_order_acquire)!=seq)continue;
  if(!transaction_drained(at))continue;
  unsigned refused=atomic_load_explicit(&slot->rejected,memory_order_acquire)&&
   atomic_load_explicit(&slot->dispatched,memory_order_acquire)!=seq&&native_started[at]!=seq;
  if(!refused){
   if(!atomic_load_explicit(&observer_running,memory_order_seq_cst))continue;
   if(atomic_load_explicit(&slot->returned,memory_order_acquire)!=seq||atomic_load_explicit(&slot->done,memory_order_acquire)!=seq)continue;
   if(command_effect(accepted[at].request.reserved)&&!effects_receipted(at))continue;
   if(command_qlink(accepted[at].request.reserved)&&!qlink_receipted(at))continue;
   if(command_qlink_mode(accepted[at].request.reserved)&&!qlink_mode_receipted(at))continue;
   if(command_io(accepted[at].request.reserved)&&!io_receipted(at))continue;
   if(atomic_load_explicit(&slot->sealed,memory_order_acquire)!=seq)atomic_store_explicit(&slot->sealed,seq,memory_order_release);
   if(atomic_load_explicit(&slot->settled,memory_order_acquire)!=seq)continue;
  }
  /* Refusal releases storage only: no returned/done/sealed/settled fabrication. */
  atomic_store_explicit(&command_state->reclaimed,atomic_load_explicit(&command_state->reclaimed,memory_order_relaxed)+1,memory_order_release);
  /* Last old-generation write: clients may now independently reuse this slot. */
  atomic_store_explicit(&slot->reclaimed,seq,memory_order_release);
 }
}
#include "jog-capture.inc"
#include "processor-observation.inc"
#include "master-capture.inc"
#include "sequence-duplicate.inc"
#include "general-capture.inc"
#include "type-open.inc"
#include "new-track-open.inc"
#include "recording-capture.inc"
#include "midi-capture.inc"
#include "effects-capture.inc"
#include "effects-chooser.inc"
#include "qlink-capture.inc"
#include "qlink-command.inc"
#include "qlink-mode.inc"
#include "io-capture.inc"
#include "pointer-capture.inc"
#include "wheel-capture.inc"
#include "focus-press-capture.inc"
static void service_one(Context *c,unsigned at){
 CommandSlot *slot=command_state->slots+at;
 unsigned published=atomic_load_explicit(&slot->published,memory_order_acquire);
 if(!published||atomic_load_explicit(&slot->processed,memory_order_acquire)==published||atomic_load_explicit(&slot->reclaimed,memory_order_acquire)==published)return;
 CommandRequest r;Track *t=NULL;uint32_t function=0;
 if(!command_request_read(slot,published,&r)){command_fail(C_FORMAT);return;}
 if(atomic_load_explicit(&command_state->new_project_intent,memory_order_seq_cst))return;
 if(command_chooser(r.reserved)){effects_chooser_service(c,at,&r);return;}
 if(command_io(r.reserved)){io_input_service(c,at,&r);return;}
 if(command_qlink_mode(r.reserved)){qlink_mode_service(c,at,&r);return;}
 if(command_qlink(r.reserved)){qlink_input_service(c,at,&r);return;}
 if(command_data_wheel(r.reserved)){wheel_command_service(c,at,&r);return;}
 if(command_focus_press(r.reserved)){focus_press_service(c,at,&r);return;}
 if(command_jog(r.reserved)){jog_service(c,at,&r);return;}
 if(r.reserved==CF_MASTER){master_service(c,at,&r);return;}
 if(command_global(r.reserved)){global_service(c,at,&r);return;}
 unsigned why=command_effect(r.reserved)?effects_request(c,&r,&t,&function):resolve(c,&r,&t,&function);
 if(why==COMMAND_WAIT_HEARTBEAT)return;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(i!=at){
  CommandSlot *other=command_state->slots+i;unsigned seq=atomic_load_explicit(&other->published,memory_order_acquire);CommandRequest old;
  if(seq&&seq<published&&atomic_load_explicit(&other->reclaimed,memory_order_acquire)!=seq){
   if(!command_request_read(other,seq,&old))return; /* independent slot reused during scan */
   if(command_conflict(&old,&r))return;
  }
 }
 accepted[at].request=r;native_started[at]=0;effects_prepare_slot(at);
 for(unsigned i=0;i<COMMAND_LANES;i++){
  CommandLane *l=slot->lanes+i;atomic_store_explicit(&l->sequence,0,memory_order_relaxed);atomic_thread_fence(memory_order_release);
  atomic_store_explicit(&l->published,0,memory_order_relaxed);atomic_store_explicit(&l->sequence,published,memory_order_release);
 }
 atomic_store(&slot->rejected,0);
 if(why){atomic_store_explicit(&slot->rejected,why,memory_order_relaxed);atomic_store_explicit(&slot->processed,published,memory_order_release);atomic_store_explicit(&command_state->consumed,atomic_load_explicit(&command_state->consumed,memory_order_relaxed)+1,memory_order_release);return;}
 accepted[at].program=t->program;accepted[at].track=t->track;accepted[at].controller=command_request_controller(&r);accepted[at].native_bits=command_native_bits(r.reserved,r.bits);accepted[at].synchronous=r.reserved==CF_SELECTION||r.reserved==CF_ARM;
 accepted[at].property=command_effect(r.reserved)?0:r.pad_owner?atomic_load(&mirror_state->pads.owners[r.pad_owner-1].address)+pad_offset(pad_field(command_source_field(r.reserved))):r.reserved?atomic_load(&request_cell(&r,t)->property):t->program+0x5b0;
 atomic_store_explicit(active_request+at,published,memory_order_seq_cst);
 event(c,CE_DISPATCH,published,0,0,t->program,accepted[at].synchronous?0:command_arg_kind(&accepted[at].request),accepted[at].controller,accepted[at].native_bits,accepted[at].property,0);
 COMMAND_PAUSE(5); /* resolved dispatch attempt, before final call validation */
 unsigned stop=atomic_load_explicit(&command_violation,memory_order_seq_cst);
 if(!atomic_load_explicit(&observer_running,memory_order_seq_cst)||stop||atomic_load(&enrollment_fault)||atomic_load(&command_state->error)||atomic_load(&command_state->trace_error)||!active())why=stop?(stop&65535):C_SOURCE_FAILURE;
 else if(!typed(root,0x68d7bf8)||word(root+0x24c)!=project||!typed(project,0x6933c4c)||word(project+0xa94)!=pool||!typed(pool,0x68ad2c4)||!vector(pool,0x34,t->track)||!typed(t->track,0x6935554)||word(t->track+0x648)!=t->program||word(t->track+0x64c)!=t->kind||!typed(word(project+0xa90),0x68ad07c)||!program_member(word(project+0xa90),t))why=C_NATIVE_MEMBERSHIP;
 else if(r.reserved!=CF_MIDI_VOLUME&&!accepted[at].synchronous&&(!command_mixable(t->program)||word(word(t->program)+0x28)!=function))why=C_NOT_DRUM;
 else if(r.reserved==CF_ARM&&(!typed(t->track+0x46c,0x69355e0)||word(word(t->track)+0x28)!=function||!word(t->track+0x48c)))why=C_NATIVE_MEMBERSHIP;
 else if(channel_owner_id(t->track)!=r.track_owner||channel_owner_id(t->program)!=r.program_owner||channel_owner_id(project)!=r.project_owner||word(t->track+0x648)!=t->program||*(const volatile unsigned char*)(uintptr_t)(t->program+0x1158))why=C_NATIVE_MEMBERSHIP;
 if(!why&&atomic_load_explicit(&command_state->new_project_intent,memory_order_seq_cst))why=C_NOT_READY;
 if(!why&&command_effect(r.reserved))why=effects_request(c,&r,&t,&function);
 if(!why&&r.pad_owner&&!request_pad(&r,t,1))why=C_IDENTITY;
 if(!why&&r.reserved==CF_MIDI_VOLUME&&!midi_preflight(t))why=C_NATIVE_MEMBERSHIP;
 if(!why&&command_recording(&r))why=recording_prepare(at);
 if(why){
  atomic_store_explicit(active_request+at,0,memory_order_seq_cst);
  atomic_store_explicit(&slot->rejected,why,memory_order_relaxed);atomic_store_explicit(&slot->processed,published,memory_order_release);atomic_store_explicit(&command_state->consumed,atomic_load_explicit(&command_state->consumed,memory_order_relaxed)+1,memory_order_release);
  command_fail(why);return;
 }
 if(command_effect(r.reserved)){atomic_store(effect_inserts+at,effects_current.inserts);atomic_store(effect_wanted_key+at,r.effect_key);atomic_store(effect_wanted_ap+at,r.effect_ap);atomic_store_explicit(effect_watch+at,published,memory_order_release);atomic_store_explicit(&effects_pending,atomic_load(&effects_pending)|(1u<<at),memory_order_release);}
 native_started[at]=published;
 float value;memcpy(&value,&accepted[at].native_bits,4);
 if(accepted[at].synchronous){
  event(c,CE_OWNER_BEGIN,published,0,0,t->program,0,0,r.bits,0,0);
#ifndef COMMAND_COMPONENT
  if(r.reserved==CF_SELECTION)((void (*)(void*,void*))(uintptr_t)function)((void*)(uintptr_t)project,(void*)(uintptr_t)t->track);
  else ((void (*)(void*,unsigned))(uintptr_t)function)((void*)(uintptr_t)t->track,r.bits);
#else
  component_dispatch(r.reserved==CF_SELECTION?project:t->track,r.reserved,r.reserved==CF_SELECTION?t->track:r.bits,value);
#endif
  event(c,CE_OWNER_END,published,0,0,t->program,0,0,r.bits,0,0);
  atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&slot->done,published,memory_order_release);
 }else if(r.reserved==CF_MIDI_VOLUME)midi_dispatch(at,value);
 else if(command_recording(&r))recording_dispatch(at,value);
 else native_dispatch(t->program,function,accepted[at].controller,value);
 /* A dispatch attempt event is not a successful call. These flags require a
  * normal return; an escaping native exception remains an uncompleted crash. */
 atomic_store_explicit(&slot->dispatched,published,memory_order_release);
 event(c,CE_RETURN,published,0,0,accepted[at].program,accepted[at].synchronous?0:command_arg_kind(&accepted[at].request),accepted[at].controller,accepted[at].native_bits,0,0);
 atomic_store_explicit(&slot->returned,published,memory_order_release);atomic_store_explicit(&slot->processed,published,memory_order_release);atomic_store_explicit(&command_state->consumed,atomic_load_explicit(&command_state->consumed,memory_order_relaxed)+1,memory_order_release);
}
#include "meter-capture.inc"
unsigned command_meter_closing(void){
 atomic_store_explicit(&mirror_state->meters.closing,1,memory_order_seq_cst);
 if(atomic_load_explicit(&mirror_state->meters.closed,memory_order_acquire))return 1;
 /* Awaiting-project startup may have no rooted UI drain yet. SC closing before
  * SC owner-flight sampling prevents a later admitted owner from acquiring a
  * first token; an already admitted acquisition keeps this path waiting. */
 if(!atomic_load_explicit(&command_in_hook,memory_order_seq_cst)&&!atomic_load_explicit(&mirror_state->meters.tokens,memory_order_acquire))return 1;
 return 0;
}
/* Fixed send ordinals are Mixer membership, never Return Track ordering.
 * Only this existing qualified UI queue owner samples native topology. */
static void send_destinations(Context *c){
 if(!ready||loading||reset_pending||depth||!root||!project||!pool||!command_queues||c->r[0]!=command_queues+4||word(c->r[0]+0xf0))return;
 if(!typed(root,0x68d7bf8)||word(root+0x24c)!=project||!typed(project,0x6933c4c))return;
 uint32_t a=word(root+0x2e4);if(!pointer(a)||a>UINT32_MAX-0x400)return;
 uint32_t mixer=a+0x260,id=channel_owner_id(mixer),programs[4]={0},owners[4]={0};
 if(!id||id!=atomic_load(&mirror_state->channel.mixer_owner)||!typed(mixer,0x6898b34))return;
 uint32_t begin=word(mixer+0x78),end=word(mixer+0x7c),cap=word(mixer+0x80);int valid=pointer(begin)&&begin<=UINT32_MAX-16&&end==begin+16&&cap>=end&&(cap-begin)%4==0&&cap-begin<=MIRROR_TRACKS*4;
 for(unsigned i=0;valid&&i<4;i++){programs[i]=word(begin+4*i);owners[i]=channel_owner_id(programs[i]);if(!owners[i]||!typed(programs[i],0x6931f70))valid=0;for(unsigned j=0;j<i;j++)if(programs[i]==programs[j])valid=0;}
 if(word(root+0x2e4)!=a||channel_owner_id(mixer)!=id||word(mixer+0x78)!=begin||word(mixer+0x7c)!=end||word(mixer+0x80)!=cap)valid=0;
 if(!valid){memset(programs,0,sizeof(programs));memset(owners,0,sizeof(owners));}
 GeneralState *g=&mirror_state->general;unsigned changed=atomic_load(&g->sends_epoch)!=epoch||atomic_load(&g->sends_mixer)!=id;
 for(unsigned i=0;i<4;i++)changed|=atomic_load(&g->sends_programs[i])!=programs[i]||atomic_load(&g->sends_owners[i])!=owners[i];
 if(!changed)return;
 uint32_t rev=atomic_load(&g->sends_revision);if(rev>UINT32_MAX-2)return;
 atomic_store(&g->sends_revision,rev+1);atomic_thread_fence(memory_order_release);atomic_store(&g->sends_epoch,epoch);atomic_store(&g->sends_mixer,id);
 for(unsigned i=0;i<4;i++){atomic_store(&g->sends_programs[i],programs[i]);atomic_store(&g->sends_owners[i],owners[i]);}
 atomic_store_explicit(&g->sends_revision,rev+2,memory_order_release);
}

static void service(Context *c){
 meter_service(c);
 send_destinations(c);
 effects_complete(c);
 effects_service(c);
 io_complete(c);io_service(c);
 qlink_mode_complete(c);qlink_complete(c);qlink_service(c); /* Bounded receipts never starve mode-cache feedback. */
 if(atomic_load(&mirror_state->meters.closing)||atomic_load(&command_state->stop_requested)||atomic_load_explicit(&command_state->new_project_intent,memory_order_seq_cst))return;
 unsigned start=service_cursor;service_cursor=(service_cursor+1)%COMMAND_SLOTS;
 for(unsigned n=0;n<COMMAND_SLOTS;n++){
  if(!atomic_load_explicit(&observer_running,memory_order_seq_cst)||atomic_load(&command_state->trace_error))return;
  unsigned at=(start+n)%COMMAND_SLOTS;CommandRequest r;unsigned seq=atomic_load(&command_state->slots[at].published);
  if(seq&&command_request_read(command_state->slots+at,seq,&r)&&!command_jog(r.reserved)&&!command_global(r.reserved))service_one(c,at);
 }
 /* Native GUI semantics: dispatch relative packets in publication order,
  * retaining one native jog until its actual callback DONE. */
 for(unsigned n=0;n<COMMAND_SLOTS;n++){
  unsigned chosen=COMMAND_SLOTS,first=UINT32_MAX;
  for(unsigned at=0;at<COMMAND_SLOTS;at++){CommandSlot *slot=command_state->slots+at;unsigned seq=atomic_load(&slot->published);CommandRequest r;
   if(seq&&seq<first&&atomic_load(&slot->processed)!=seq&&command_request_read(slot,seq,&r)&&(command_jog(r.reserved)||command_global(r.reserved))){first=seq;chosen=at;}
  }
  if(chosen==COMMAND_SLOTS||!atomic_load(&observer_running)||atomic_load(&command_state->trace_error))return;
  service_one(c,chosen);if(atomic_load(&command_state->slots[chosen].processed)!=first)return;
 }
}
/* Pure observer-key classification: no native reads, allocation, mutation or
 * failure side effects. Uncertainty retains the original admitted path. */
static int known_volume_key(uint32_t property){
 unsigned h=hash(property);
 for(unsigned i=0;i<MIRROR_PROBES;i++){
  uint32_t n=atomic_load_explicit(mirror_state->directory+((h+i)&(MIRROR_HASH-1)),memory_order_acquire);
  if(!n)return 0;
  if(n>MIRROR_CELLS)return -1;
  MirrorCell *cell=mirror_state->cells+n-1;
  if(atomic_load_explicit(&cell->property,memory_order_relaxed)==property){
   /* Identity presence is sufficient: retirement is handled only after the
    * normal source admission, never treated as an absent key here. */
   return 1;
  }
 }
 return -1;
}
static int known_channel_key(uint32_t property){
 unsigned h=channel_hash(property);
 for(unsigned i=0;i<MIRROR_PROBES;i++){
  uint32_t n=atomic_load_explicit(mirror_state->channel.field_directory+((h+i)&(CHANNEL_HASH-1)),memory_order_acquire);
  if(!n)return 0;
  if(n>CHANNEL_FIELDS)return -1;
  ChannelCell *cell=mirror_state->channel.fields+n-1;
  if(atomic_load_explicit(&cell->property,memory_order_relaxed)==property){
   /* Identity presence is sufficient: retirement is handled only after the
    * normal source admission, never treated as an absent key here. */
   return 1;
  }
 }
 return -1;
}
static int unrelated_source_key(Context *c,unsigned site){
 uint32_t io_property=0;
 if(site==IO_MONITOR_COMMIT||site==IO_SEND_COMMIT||site==IO_IN_PORT_COMMIT)io_property=c->r[0];
 else if(site==IO_MONITOR_AUDIO||site==IO_SEND_AUDIO)io_property=c->r[2]+0x18;
 else if(site==IO_AUDIO_UI_COMMIT||site==IO_AUDIO_MONITOR_COMMIT||site==IO_AUDIO_APPLY_COMMIT||(site>=IO_OUTPUT_COMMIT0&&site<=IO_OUTPUT_APPLY_COMMIT2)||site==IO_OUT_PORT_COMMIT||site==IO_CHANNEL_COMMIT)io_property=c->r[5];
 if(io_property&&!known_channel_key(io_property))return 1;
 if(site!=M_FLOAT&&site!=MIRROR_PROPERTY_DESTROY&&site!=M_MUTE&&site!=M_BOOL&&site!=M_COLOR&&site!=M_NAME&&site!=CH_BOOL_COMMIT&&site!=CH_SELECTION_COMMIT&&site!=CH_ENUM_COMMIT&&site!=CH_RECORD_COMMIT)return 0;
 uint32_t property=c->r[site==MIRROR_PROPERTY_DESTROY?0:5];if(!pointer(property))return 0;
 if(global_call.id&&global_call.property==property)return 0; /* current UI-owned configured intent, including non-enrolled Sequence Property */
 /* Counts precede the owner-flight check: a completed intervening birth is
  * detected by its new count; an in-progress birth retains normal admission. */
 uint32_t pads=atomic_load_explicit(&mirror_state->pads.used,memory_order_acquire);
 uint32_t volumes=atomic_load_explicit(&mirror_state->allocated,memory_order_acquire),fields=atomic_load_explicit(&mirror_state->channel.fields_used,memory_order_acquire);
 if(volumes>MIRROR_CELLS||fields>CHANNEL_FIELDS||atomic_load_explicit(lane_in_hook,memory_order_seq_cst))return 0;
 unsigned pad_f;
 if(known_volume_key(property)!=0||known_channel_key(property)!=0||pad_property(property,&pad_f))return 0;
 return !atomic_load_explicit(lane_in_hook,memory_order_seq_cst)&&pads==atomic_load_explicit(&mirror_state->pads.used,memory_order_acquire)&&volumes==atomic_load_explicit(&mirror_state->allocated,memory_order_acquire)&&fields==atomic_load_explicit(&mirror_state->channel.fields_used,memory_order_acquire);
}
static void position_point(Context *c,unsigned site,unsigned lane){
 if(site==CH_POSITION_ENTER){
  if(position_depth==4){atomic_store_explicit(&mirror_state->position_error,CH_CAPACITY,memory_order_release);return;}
  unsigned ep=atomic_load_explicit(&mirror_state->epoch,memory_order_acquire);
  unsigned eligible=atomic_load_explicit(&mirror_state->ready,memory_order_acquire)&&c->r[1]==atomic_load_explicit(&mirror_state->sequencer,memory_order_acquire);
  positions[position_depth++]=(PositionInvocation){c->r[0],c->r[1],source_sp(c),ep,eligible};return;
 }
 if(!position_depth){atomic_store_explicit(&mirror_state->position_error,CH_LIFETIME,memory_order_release);return;}
 PositionInvocation f=positions[--position_depth];
 if(f.output!=c->r[4]||f.sequencer!=c->r[5]||f.sp!=source_sp(c)+24){atomic_store_explicit(&mirror_state->position_error,CH_LIFETIME,memory_order_release);return;}
 if(!f.eligible||!atomic_load_explicit(&mirror_state->ready,memory_order_acquire)||f.epoch!=atomic_load_explicit(&mirror_state->epoch,memory_order_acquire)||f.sequencer!=atomic_load_explicit(&mirror_state->sequencer,memory_order_acquire))return;
 /* The native getter has completed all signature/arrangement adjustments.
  * Copy only its currently owned output, and publish on this source's lane. */
 uint32_t b=word(c->r[4]),beat=word(c->r[4]+4),clock=word(c->r[4]+8);
 MirrorPosition *out=mirror_state->position+lane;unsigned rev=atomic_load(&out->revision);if(rev>UINT32_MAX-2){atomic_store_explicit(&mirror_state->position_error,CH_CAPACITY,memory_order_release);return;}
 atomic_store_explicit(&out->revision,rev+1,memory_order_relaxed);atomic_thread_fence(memory_order_release);
 atomic_store(&out->epoch,f.epoch);atomic_store(&out->tick,atomic_load(&mirror_state->heartbeat));atomic_store(&out->token,token());atomic_store(&out->bar,b);atomic_store(&out->beat,beat);atomic_store(&out->clock,clock);
 atomic_store_explicit(&out->revision,rev+2,memory_order_release);
}
void observer_hook(Context *c,unsigned kind){
 if(!c||kind<MODEL_HOOK_BASE||!command_state)return;
 unsigned site=kind-MODEL_HOOK_BASE;
 /* Product hook, not a diagnostic: it enrolls no lane and publishes no row. */
 if(site==M_POINTER_DEVICE){pointer_device_enable(c);return;}
 if(site==M_WHEEL_DATA){wheel_data(c);return;}
 if(site>=M_WHEEL_FLUSH&&site<=M_WHEEL_FLUSH_LAST){wheel_flush();return;}
 effects_lifecycle(c,site);
 qlink_lifecycle(site);
 if(site>=QS_DESCRIPTOR&&site<=QS_LABELS&&site!=QJ_VALUE_FANOUT){
  unsigned interested=atomic_load_explicit(&qlink_hook_provider,memory_order_acquire)&&atomic_load(&command_state->qlink_interest.enabled)&&atomic_load(&command_state->qlink_interest.until)>atomic_load(&mirror_state->heartbeat);
  if(!interested&&!(site==QS_CACHE_DONE&&qlink_refresh_depth))return;
  if(!local_lane||atomic_load_explicit(lane_in_hook+local_lane-1,memory_order_seq_cst)){atomic_store(&mirror_state->qlinks.error,QL_FOREIGN);return;}
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;qlink_source(c,site,lane);trace_leave(lane);return;
 }

 if(site==PX_SET&&(invocation.request||recording_call.id)){unsigned lane=trace_enter(site);if(lane!=COMMAND_LANES){effects_execute(c);trace_leave(lane);}}
 if((site>=PX_GET&&site<=PX_PARAMETER_TREE)||site==PX_INSERT_ENABLE||site==COMMAND_DRAIN){
  unsigned diagnostic_site=site==COMMAND_DRAIN?PX_UI_DRAIN:site;
  if(processor_observation_wanted(diagnostic_site)){
  /* Passive diagnostics never enroll a lane or turn reentry into musical
   * command failure. Existing enrolled-hook lifetime still closes normally. */
  unsigned lane=local_lane?local_lane-1:COMMAND_LANES;
  if(lane<COMMAND_LANES&&!atomic_load_explicit(lane_in_hook+lane,memory_order_seq_cst))lane=trace_enter(site);else lane=COMMAND_LANES;
  processor_observe(c,diagnostic_site,lane);if(lane!=COMMAND_LANES)trace_leave(lane);
  }
  if(site!=COMMAND_DRAIN)return; /* Existing drain still services musical work. */
 }
 if(site==158){
  /* Safe entry hook: literal loads are relocated, no native call displaced.
   * Positive intent exists only after the accepted NewProject confirmation. */
  if(!atomic_load_explicit(&observer_running,memory_order_seq_cst)||c->r[0]!=word(observer_image_bias+0x6b26814)||!pointer(c->r[0]))return;
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;
  unsigned intent=3;
  if(c->lr==observer_image_bias+0x25703f4&&!c->r[1]&&token()==command_owner&&!atomic_load(&command_state->new_project_intent)&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error)&&!atomic_load(&command_state->stop_requested)){
   atomic_store(&command_state->new_project_epoch,atomic_load(&mirror_state->epoch));atomic_store(&command_state->new_project_tick,atomic_load(&mirror_state->heartbeat));intent=1;
  }
  atomic_store_explicit(&command_state->new_project_intent,intent,memory_order_seq_cst);trace_leave(lane);return;
 }
 if(site>=IO_PC_BIRTH&&site<=IO_AC_BIRTH){unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;io_program_point(c,site);trace_leave(lane);return;}
 if(site>=IO_DEST_BIRTH&&site<=IO_DEST_READY){unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;io_destination_point(c,site);trace_leave(lane);return;}
 if(site>=IO_CONNECTOR_BIRTH&&site<=IO_CONNECTOR_READY){unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;io_connector_point(c,site);trace_leave(lane);return;}
 if(site>=IO_INPUT_SET&&site<=IO_FILTER_DONE){if(!io_submission)return;unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;io_setter_point(c,site);trace_leave(lane);return;}
 if(site==IO_MONITOR_PREPARE||site==IO_MONITOR_RESULT||site==IO_MONITOR_ENTER||site==IO_MONITOR_DONE||site==IO_SEND_PREPARE||site==IO_SEND_RESULT||site==IO_SEND_ENTER||site==IO_SEND_DONE||(site==JG_PUBLISH&&io_submission)){
  unsigned preparing=site==IO_MONITOR_PREPARE||site==IO_MONITOR_RESULT||site==IO_SEND_PREPARE||site==IO_SEND_RESULT||site==JG_PUBLISH;
  if(preparing){if(!io_submission)return;}else{int found=io_job_find(c->r[0]);if(found==COMMAND_SLOTS)return;if(found<0){trace_fail(C_TRACE_AMBIGUOUS);return;}}
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;io_job_point(c,site);trace_leave(lane);return;
 }
 if(site==QM_DEATH){
  if(c->r[0]==atomic_load_explicit(&qlink_mode_hook,memory_order_acquire)){atomic_store_explicit(&qlink_mode_dead,c->r[0],memory_order_release);atomic_store_explicit(&mirror_state->qlinks.invalid,1,memory_order_release);}return;
 }
 if((site>=QM_PREPARE0&&site<=QM_DONE1)||(site==JG_PUBLISH&&mode_submission)){
  if(site<=QM_RESULT1||site==JG_PUBLISH){if(!mode_submission)return;}
  else{unsigned part=0;int found=mode_job_find(c->r[site==QM_ENTER0||site==QM_ENTER1?0:site==QM_DONE0?4:5],&part);if(found==COMMAND_SLOTS)return;if(found<0){trace_fail(C_TRACE_AMBIGUOUS);return;}}
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;mode_job_point(c,site);trace_leave(lane);return;
 }
 if((site>=QJ_ACTIVITY_RESULT&&site<=QJ_VALUE_PREPARE)||site==QJ_VALUE_FANOUT||(site==JG_PUBLISH&&qlink_submission)){
  if(site==JG_PUBLISH||site==QJ_ACTIVITY_RESULT||site==QJ_VALUE_RESULT||site==QJ_ACTIVITY_PREPARE||site==QJ_VALUE_PREPARE){if(!qlink_submission)return;}
  else{unsigned part=0;int found=qlink_job_find(c->r[site==QJ_ACTIVITY_ENTER||site==QJ_VALUE_ENTER?0:4],&part);if(found==COMMAND_SLOTS)return;if(found<0){trace_fail(C_TRACE_AMBIGUOUS);return;}}
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;qlink_job_point(c,site);trace_leave(lane);return;
 }
 if((site>=MI_ENTER&&site<=MI_SET)||(site==JG_PUBLISH&&midi_publication(c))){
  unsigned relevant=site==JG_PUBLISH;
  if(site==MI_SET){unsigned at=command_find(command_state,midi_call.id);relevant=midi_call.id&&at<COMMAND_SLOTS&&c->r[0]==accepted[at].property;}
  else if(site==MI_ENTER||site==MI_AUTO_ENTER){for(unsigned i=0;i<COMMAND_SLOTS;i++)if(atomic_load(active_request+i)&&atomic_load_explicit(midi_payload+i,memory_order_acquire)==c->r[0])relevant=1;}
  else if(site!=JG_PUBLISH)relevant=midi_call.id!=0;
  if(!relevant)return;
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;midi_point(c,site);trace_leave(lane);return;
 }
 if(site==PD_EXECUTE){if(!recording_call.id&&!invocation.request)return;unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;recording_pad_execute(c);trace_leave(lane);return;}
 if((site>=RC_ENTER&&site<=RC_CP_DEATH)||(site==JG_PUBLISH&&recording_publication(c))){
  unsigned relevant=site==JG_PUBLISH;
  if(site==RC_CP_DEATH){for(unsigned i=0;i<COMMAND_SLOTS;i++)if(atomic_load(active_request+i)&&atomic_load(recording_cp+i)==c->r[0])relevant=1;}
  else if(site==RC_ENTER||site==RC_SECOND_ENTER){for(unsigned i=0;i<COMMAND_SLOTS;i++)if(atomic_load(active_request+i)&&atomic_load_explicit((site==RC_ENTER?recording_payload:recording_work)+i,memory_order_acquire)==c->r[0])relevant=1;}
  else if(site!=JG_PUBLISH)relevant=site==RC_SECOND_RETURN||site==RC_SECOND_DONE?recording_second.id:recording_call.id;
  if(!relevant)return;
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;recording_point(c,site);trace_leave(lane);return;
 }
 if(site>=GL_AUTO_FANOUT&&site<=GL_EDITOR_DEATH){
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;global_point(c,site,lane);trace_leave(lane);return;
 }
 if(site>=ME_STEREO&&site<=ME_TRACK){
  uint32_t address=site==ME_STEREO||site==ME_MONO?c->r[7]:site==ME_DECAY?c->r[4]:site==ME_TRACK?c->r[4]+0x324:c->r[0];
  MeterCell *m=meter_find(address);if(!m)return;
  /* Latch this incarnation before lane admission. A later destruction/reuse
   * cannot redirect a delayed callback into a new meter cell. */
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;meter_source(c,site,lane,m);trace_leave(lane);return;
 }
 if((site>=MS_CREATE&&site<=MS_SET_DONE)||(site==JG_PUBLISH&&master_submission)){
  if(site==MS_CREATE||site==MS_RESULT||site==JG_PUBLISH){if(!master_submission)return;}
  else if(site==MS_FACTORY_DEATH||site==MS_FACTORY_DELETE){if(c->r[0]!=atomic_load(&master_factory))return;}
  else if(site==MS_SET_ENTER||site==MS_SET_DONE){if(!master_call.id)return;}
  else{unsigned found=0;uint32_t key=c->r[site==MS_ENTER?0:4];for(unsigned i=0;i<COMMAND_SLOTS;i++)if(atomic_load(active_request+i)&&atomic_load_explicit(master_payload+i,memory_order_acquire)==key){found=1;break;}if(!found)return;}
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;master_point(c,site);trace_leave(lane);return;
 }
 if(site>=JN_PLAYING&&site<=JN_RESTART_EARLY){
  if(site==JN_PLAYING||site==JN_ADVANCE){if(!atomic_load_explicit(&command_state->navigation_watch,memory_order_acquire))return;}
  else if(!jog_depth)return;
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;navigation_point(c,site);trace_leave(lane);return;
 }
 if(site>=JG_PUBLISH&&site<=JG_DELETE){
  /* Ordinary native queue traffic must not enroll unrelated source threads.
   * Only copied observer keys/TLS are read before complete source admission. */
  if(site==JG_PUBLISH||site==JG_BAR_RESULT||site==JG_BEAT_RESULT||site==JG_PULSE_RESULT){if(!jog_submission)return;}
  else if(site==JG_DESTROY||site==JG_DELETE){if(c->r[0]!=atomic_load_explicit(&jog_current_x,memory_order_acquire))return;}
  else{unsigned found=0,entering=site==JG_BAR_ENTER||site==JG_BEAT_ENTER||site==JG_PULSE_ENTER;uint32_t key=c->r[entering?0:4];
   for(unsigned i=0;i<COMMAND_SLOTS;i++)if(atomic_load_explicit(active_request+i,memory_order_seq_cst)&&atomic_load_explicit(jog_payload+i,memory_order_acquire)==key){found=1;break;}
   if(!found)return;
  }
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;jog_point(c,site);trace_leave(lane);return;
 }
 if(site==CH_POSITION_ENTER||site==CH_POSITION_EXIT){if(atomic_load_explicit(&mirror_state->position_error,memory_order_acquire))return;unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;position_point(c,site,lane);trace_leave(lane);return;}
 if(site==292||site==293){unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;type_do_point(c,site);trace_leave(lane);return;}
 if((site==M_CREATE_ENTER||site==M_CREATE_READY)&&!create_source(c,site))return;
 if(site!=M_CREATE_ENTER&&site!=M_CREATE_READY&&(site==MIRROR_SEED_NORMAL||site==MIRROR_SEED_COPY||site==M_FLOAT||site==MIRROR_PROPERTY_DESTROY||channel_source(site))){
  if(unrelated_source_key(c,site))return;
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;
  if(site==CH_MIXER_DEATH)master_point(c,site);
  command_mirror_hook(c,kind);
  if(site==IO_SEND_COMMIT||site==M_NAME||site==CH_TRACK_BIRTH||site==29)io_graph_changed(); /* Destination eligibility includes graph, names and lifetimes. */
  /* authoritative birth/commit/death remains MMV4 */
  for(unsigned at=0;at<COMMAND_SLOTS;at++)if(source_requests[at]){
   unsigned id=source_requests[at];
   if(command_io(accepted[at].request.reserved)){if((site>=IO_IN_PORT_COMMIT&&site<=IO_CHANNEL_COMMIT)||site==IO_AUDIO_MONITOR_COMMIT)io_source_commit(c,site,at);continue;}
   if(command_toggle(accepted[at].request.reserved)){global_toggle_commit(c,site,at);continue;}
   if(accepted[at].request.reserved==CF_MIDI_VOLUME&&site==M_FLOAT&&c->r[5]==accepted[at].property&&midi_commit(c,at))continue;
   if(accepted[at].request.reserved==CF_MASTER){if(site==M_FLOAT&&c->r[5]==accepted[at].property)master_commit(c,at);continue;}
   if((site==M_FLOAT||site==M_MUTE||site==M_BOOL||site==CH_BOOL_COMMIT)&&c->r[5]==(accepted[at].request.pad_owner?atomic_load(recording_pad_property+at):accepted[at].property)&&recording_commit(c,at,site))continue;
   if(id==accepted[at].request.seq&&c->r[5]==(accepted[at].request.pad_owner&&atomic_load(recording_pad_property+at)?atomic_load(recording_pad_property+at):accepted[at].property)&&(site==M_FLOAT||site==M_MUTE||site==M_BOOL||site==CH_BOOL_COMMIT||site==CH_SELECTION_COMMIT)){
    const CommandRequest *r=&accepted[at].request;unsigned linked=(invocation.request==id&&invocation.phase==2)||(accepted[at].synchronous&&token()==command_owner&&atomic_load(&command_in_hook));
    uint32_t revision=0,bits=site==M_FLOAT?(uint32_t)c->d[8]:c->r[7];
    if(r->pad_owner){unsigned pf;PadOwner *po=pad_property(c->r[5],&pf);if(po&&atomic_load(&po->live)&&atomic_load(&po->incarnation)==(atomic_load(recording_pad_owner+at)?atomic_load(recording_pad_owner+at):r->pad_owner))revision=atomic_load(&po->fields[pf].revision);}
    else if(!r->reserved){MirrorCell *cell=lookup(c->r[5]);if(cell&&atomic_load(&cell->live)&&atomic_load(&cell->incarnation)==r->incarnation)revision=atomic_load(&cell->revision);}
    else{ChannelCell *cell=channel_cell(c->r[5]);if(cell&&atomic_load(&cell->live)&&atomic_load(&cell->incarnation)==r->field_incarnation)revision=atomic_load(&cell->revision);}
    if(r->reserved==CF_SELECTION)bits=bits==accepted[at].track?1:0;
    if(!revision)trace_fail(C_TRACE_AMBIGUOUS);
    else event(c,linked?CE_COMMIT:CE_OTHER_COMMIT,id,linked&&!accepted[at].synchronous?invocation.capture:0,linked&&!accepted[at].synchronous?invocation.counter:0,accepted[at].program,accepted[at].synchronous?0:command_arg_kind(&accepted[at].request),accepted[at].controller,bits,c->r[5],revision);
   }
  }trace_leave(lane);return;
 }
 if(site==M_QUEUE_ENTER||site==M_COMMAND_BODY||site==M_QUEUE_DONE){
  if(!atomic_load_explicit(&observer_running,memory_order_acquire))return;
  unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;queue_point(c,site);trace_leave(lane);return;
 }
 if(token()!=command_owner){
  if(site!=COMMAND_DRAIN){atomic_store_explicit(&command_violation,(site<<16)|C_OWNER,memory_order_seq_cst);if(atomic_load_explicit(&observer_running,memory_order_seq_cst))atomic_store_explicit(&observer_running,0,memory_order_seq_cst);}return;
 }
 if(!atomic_load_explicit(&observer_running,memory_order_seq_cst))return;
 if(atomic_load_explicit(&command_in_hook,memory_order_seq_cst)){atomic_store_explicit(&command_violation,(site<<16)|C_REENTRY,memory_order_seq_cst);command_fail(C_REENTRY);return;}
 atomic_store_explicit(&command_in_hook,1,memory_order_seq_cst);
 if(!atomic_load_explicit(&observer_running,memory_order_seq_cst))goto done;
 if(site==COMMAND_DRAIN)service(c);
 else{
  if(meter_topology_boundary(site))meter_release_all();
  if(site==M_ROOT_DESTROY)global_root(site);
  command_mirror_hook(c,kind);
  if(!active()){command_fail(C_SOURCE_FAILURE);goto done;}
  if(site==M_RESET||site==M_CREATE_ENTER)global_root(M_RESET);
  if(site==M_RESET||site==M_CREATE_ENTER){uint32_t q=word(c->r[0]+0xf8);if(q>UINT32_MAX-4||!typed(q,0x68aeaf0)||!typed(q+4,0x68aeadc)){command_fail(C_NATIVE_MEMBERSHIP);goto done;}command_queues=q;uint32_t a=word(root+0x2e4);if(pointer(a)&&a<UINT32_MAX-0x400){uint32_t x=word(a+0x3d8);if(typed(x,0x689fdac))atomic_store_explicit(&jog_current_x,x,memory_order_release);uint32_t f=word(root+0x318);if(typed(f,0x6899fa0))atomic_store_explicit(&master_factory,f,memory_order_release);}}
  if(!root)command_queues=0;
 }
done:atomic_store_explicit(&command_in_hook,0,memory_order_seq_cst);
}
