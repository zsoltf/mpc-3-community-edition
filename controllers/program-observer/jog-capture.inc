/* Native AsyncSequencer facade. All native reads stay in an admitted source
 * scope; the consumer only sees copied request/queue/completion evidence. */
static _Atomic uint32_t jog_current_x,jog_retired_x,jog_payload[COMMAND_SLOTS];
static uint32_t jog_enqueue_result[COMMAND_SLOTS];
static __thread uint32_t jog_submission __attribute__((tls_model("initial-exec")));
typedef struct {uint32_t id,payload,sp,operation,execution_epoch,timeline,owner,sequencer,outcome,restart_sp;} JogCall;
static __thread JogCall jog_calls[8] __attribute__((tls_model("initial-exec")));static __thread unsigned jog_depth __attribute__((tls_model("initial-exec")));
static void jog_initialize(void){
 atomic_store(&jog_current_x,0);atomic_store(&jog_retired_x,0);jog_submission=jog_depth=0;memset(jog_calls,0,sizeof(jog_calls));
 for(unsigned i=0;i<COMMAND_SLOTS;i++){atomic_store(jog_payload+i,0);jog_enqueue_result[i]=0;}
}
static const uint32_t jog_functions[3]={0x1bb9324,0x1bb949c,0x1bb9614};
static const uint32_t jog_invokers[3]={0x1bb86a0,0x1bb8588,0x1bb8470};
static const uint32_t jog_managers[3]={0x1bb779c,0x1bb77e8,0x1bb7834};
static unsigned jog_resolve(Context *c,const CommandRequest *r,uint32_t *receiver){
 uint32_t now=atomic_load_explicit(&mirror_state->heartbeat,memory_order_acquire);
 if(!command_jog(r->reserved)||!command_jog_delta(r->bits)||r->serial||r->binding||r->incarnation||r->track_owner||r->program_owner||r->field_incarnation||r->before_bits||r->before_revision||r->global_owner)return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 if(r->created>=command_tick_limit(WINDOW_SECONDS)||r->expires<r->created||r->expires-r->created>120000u||r->expires<now)return C_EXPIRED;
 if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 if(!active()||atomic_load(&enrollment_fault)||atomic_load(&command_state->trace_error))return C_SOURCE_FAILURE;
 if(!ready||loading||reset_pending||depth||!root||!project||!pool||!command_queues)return C_NOT_READY;
 if(r->epoch!=epoch||channel_owner_id(project)!=r->project_owner)return C_IDENTITY;
 for(unsigned i=0;i<count;i++)if(tracks[i].transition)return C_NOT_READY;
 if(c->r[0]!=command_queues+4||word(c->r[0]+0xf0))return C_NOT_READY;
 if(!typed(root,0x68d7bf8)||word(root+0x24c)!=project||!typed(project,0x6933c4c)||word(project+0xa94)!=pool||!typed(pool,0x68ad2c4))return C_NATIVE_MEMBERSHIP;
 uint32_t a=word(root+0x2e4);if(!pointer(a)||a>UINT32_MAX-0x400)return C_NATIVE_MEMBERSHIP;
 uint32_t x=word(a+0x3d8),s=word(a+0x3bc),t=word(a+0x3a4),u=word(a+0x3a0);
 if(!typed(x,0x689fdac)||x!=atomic_load_explicit(&jog_current_x,memory_order_acquire)||x==atomic_load_explicit(&jog_retired_x,memory_order_acquire)||!pointer(s)||s>UINT32_MAX-0x408||!typed(t,0x69091b0)||!pointer(u))return C_NATIVE_MEMBERSHIP;
 if(word(root+0xf8)!=command_queues||word(a+0xa8)!=command_queues||word(x+8)!=command_queues||word(x+0x40)!=s||word(s+0x400)!=t||word(s+0x3fc)!=u)return C_NATIVE_MEMBERSHIP;
 if(word(root+0x2e4)!=a||word(a+0x3d8)!=x||word(a+0x3bc)!=s)return C_NATIVE_MEMBERSHIP;
 *receiver=x;return C_OK;
}
#ifdef COMMAND_COMPONENT
static void (*component_jog_dispatch)(uint32_t,unsigned,int32_t);
#endif
static void jog_service(Context *c,unsigned at,const CommandRequest *r){
 CommandSlot *slot=command_state->slots+at;uint32_t x=0;unsigned why=jog_resolve(c,r,&x);
 if(why==COMMAND_WAIT_HEARTBEAT)return;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(i!=at){CommandSlot *q=command_state->slots+i;uint32_t seq=atomic_load(&q->published);CommandRequest old;if(seq&&seq<r->seq&&seq!=atomic_load(&q->done)&&command_request_read(q,seq,&old)&&command_jog(old.reserved))return;}
 accepted[at].request=*r;native_started[at]=0;atomic_store(&slot->rejected,0);accepted[at].program=x;accepted[at].track=accepted[at].property=0;accepted[at].controller=r->reserved;accepted[at].native_bits=r->bits;accepted[at].synchronous=0;
 atomic_store_explicit(jog_payload+at,0,memory_order_release);jog_enqueue_result[at]=0;
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}
 atomic_store(&slot->rejected,0);
 if(why)goto rejected;
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);
 event(c,CE_DISPATCH,r->seq,0,x+8,x,0,r->reserved,r->bits,0,r->epoch);
 uint32_t checked=0;why=jog_resolve(c,r,&checked);
 if(why||checked!=x||!atomic_load_explicit(&observer_running,memory_order_seq_cst)||atomic_load(&command_violation)||atomic_load(&command_state->error)||atomic_load(&command_state->trace_error)){if(!why)why=C_SOURCE_FAILURE;goto rejected;}
 if(jog_submission){why=C_REENTRY;goto rejected;}
 native_started[at]=r->seq;
 jog_submission=r->seq;
#ifdef COMMAND_COMPONENT
 component_jog_dispatch(x,r->reserved,(int32_t)r->bits);
#else
 ((void (*)(void*,int32_t))(uintptr_t)(observer_image_bias+jog_functions[r->reserved-JOG_BAR]))((void*)(uintptr_t)x,(int32_t)r->bits);
#endif
 jog_submission=0;
 if(jog_enqueue_result[at]!=2||!atomic_load_explicit(jog_payload+at,memory_order_acquire)){why=jog_enqueue_result[at]==1?C_NOT_READY:C_TRACE_AMBIGUOUS;goto rejected;}
 event(c,CE_RETURN,r->seq,atomic_load(jog_payload+at),x+8,x,0,r->reserved,r->bits,0,r->epoch);
 atomic_store_explicit(&slot->dispatched,r->seq,memory_order_release);atomic_store_explicit(&slot->returned,r->seq,memory_order_release);
 atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store_explicit(&command_state->consumed,atomic_load(&command_state->consumed)+1,memory_order_release);return;
rejected:
 atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store(&slot->rejected,why);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store_explicit(&command_state->consumed,atomic_load(&command_state->consumed)+1,memory_order_release);
 if(atomic_load_explicit(jog_payload+at,memory_order_acquire))command_fail(why); /* accepted native work cannot be hidden as a harmless rejection */
}
static void jog_point(Context *c,unsigned site){
 if(site==JG_DESTROY||site==JG_DELETE){
  uint32_t x=c->r[0];if(x!=atomic_load_explicit(&jog_current_x,memory_order_acquire))return;
  atomic_store_explicit(&jog_retired_x,x,memory_order_release);
  for(unsigned i=0;i<COMMAND_SLOTS;i++)if(source_requests[i]&&command_jog(accepted[i].request.reserved))command_fail(C_IDENTITY);
  return;
 }
 if(site==JG_PUBLISH||(site>=JG_BAR_RESULT&&site<=JG_PULSE_RESULT&&(site-JG_BAR_RESULT)%3==0)){
  if(!jog_submission)return;
  unsigned at=command_find(command_state,jog_submission);
  /* The audio matcher retires at callback completion. The UI submission marker
   * independently owns result/return attribution until the facade returns;
   * command_in_hook prevents sealing/reuse throughout that interval. */
  if(at==COMMAND_SLOTS||accepted[at].request.seq!=jog_submission||(site==JG_PUBLISH&&source_requests[at]!=jog_submission)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  const CommandRequest *r=&accepted[at].request;uint32_t x=accepted[at].program;
  if(token()!=command_owner||!atomic_load(&command_in_hook)){trace_fail(C_OWNER);return;}
  if(site==JG_PUBLISH){
   uint32_t slot=c->r[10];unsigned unit=r->reserved-JOG_BAR;
   if(atomic_load(jog_payload+at)||!pointer(slot)||slot>UINT32_MAX-20||c->r[7]!=command_queues+0xfc||c->r[9]!=((x+8)|1u)||word(slot)!=observer_image_bias+jog_invokers[unit]||word(slot+4)!=observer_image_bias+jog_managers[unit]||word(slot+8)!=x+8||word(slot+12)!=x||word(slot+16)!=r->bits){trace_fail(C_TRACE_AMBIGUOUS);return;}
   atomic_store_explicit(jog_payload+at,slot+8,memory_order_release);
   event(c,CE_JOG_ENQUEUE,r->seq,slot+8,x+8,x,0,r->reserved,r->bits,0,r->epoch);
  }else{
   if(site!=JG_BAR_RESULT+3*(r->reserved-JOG_BAR)||jog_enqueue_result[at]||c->r[0]>1){trace_fail(C_TRACE_AMBIGUOUS);return;}
   jog_enqueue_result[at]=c->r[0]+1;
  }
  return;
 }
 unsigned entering=(site==JG_BAR_ENTER||site==JG_BEAT_ENTER||site==JG_PULSE_ENTER),unit=(site-JG_BAR_ENTER)/3;
 uint32_t payload=c->r[entering?0:4];unsigned at=COMMAND_SLOTS;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(source_requests[i]&&command_jog(accepted[i].request.reserved)&&atomic_load_explicit(jog_payload+i,memory_order_acquire)==payload){if(at!=COMMAND_SLOTS){trace_fail(C_TRACE_AMBIGUOUS);return;}at=i;}
 if(at==COMMAND_SLOTS)return; /* ordinary native navigation is not our request */
 const CommandRequest *r=&accepted[at].request;uint32_t x=accepted[at].program;
 if(r->reserved!=JOG_BAR+unit){trace_fail(C_TRACE_AMBIGUOUS);return;}
 if(entering){
  if(jog_depth==8||!pointer(payload)||word(payload)!=x+8||word(payload+4)!=x||word(payload+8)!=r->bits){trace_fail(C_TRACE_AMBIGUOUS);return;}
  JogCall *call=jog_calls+jog_depth++;*call=(JogCall){r->seq,payload,source_sp(c),r->reserved,atomic_load_explicit(&mirror_state->epoch,memory_order_acquire),0,0,0,0,0};
  call->sequencer=word(x+0x40);call->timeline=word(call->sequencer+0x400);call->owner=channel_owner_id(call->timeline);
  event(c,CE_JOG_ENTER,r->seq,payload,x+8,x,0,r->reserved,r->bits,0,call->execution_epoch);
 }else{
  JogCall *call=jog_depth?jog_calls+jog_depth-1:NULL;
  if(!call||call->id!=r->seq||call->payload!=payload||call->operation!=r->reserved||source_sp(c)+0x30!=call->sp){trace_fail(C_TRACE_AMBIGUOUS);return;}
  /* Native pending-counter decrement has completed. No read of X here. */
  event(c,CE_JOG_OUTCOME,r->seq,payload,atomic_load_explicit(&command_state->navigation_count,memory_order_acquire),x,call->outcome,r->reserved,r->bits,call->owner,call->execution_epoch);
  event(c,CE_JOG_DONE,r->seq,payload,x+8,x,0,r->reserved,r->bits,0,atomic_load_explicit(&mirror_state->epoch,memory_order_acquire));
  memset(call,0,sizeof(*call));jog_depth--;atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&command_state->slots[at].done,r->seq,memory_order_release);
 }
}

/* Source-only causal witness. Register-captured playing and the later native
 * increment must belong to the same function frame and retained Timeline.
 * A nested/missing frame loses admission evidence; it never invents progress. */
typedef struct {uint32_t timeline,sp,owner,epoch,playing;} NavigationFrame;
static __thread NavigationFrame navigation_frame __attribute__((tls_model("initial-exec")));
static void navigation_point(Context *c,unsigned site){
 if(site==JN_PLAYING){
  navigation_frame=(NavigationFrame){c->r[4],source_sp(c),channel_owner_id(c->r[4]),atomic_load(&mirror_state->epoch),c->r[12]};return;
 }
 if(site==JN_ADVANCE){
  NavigationFrame f=navigation_frame;memset(&navigation_frame,0,sizeof(navigation_frame));
  if(!f.owner||f.playing!=1||f.timeline!=c->r[4]||f.sp!=source_sp(c)||f.epoch!=atomic_load(&mirror_state->epoch)||channel_owner_id(f.timeline)!=f.owner)return;
  ChannelOwner *owner=channel_owner(f.timeline);if(!owner||atomic_load(&owner->kind)!=CO_TIMELINE)return;
  uint32_t zero=0;if(!atomic_compare_exchange_strong(&command_state->navigation_guard,&zero,token())){atomic_store(&command_state->navigation_error,C_BUSY);return;}
  uint32_t rev=atomic_load(&command_state->navigation_revision),n=atomic_load(&command_state->navigation_count);
  if(rev>UINT32_MAX-2||n==UINT32_MAX){atomic_store(&command_state->navigation_error,C_CAPACITY);atomic_store(&command_state->navigation_guard,0);return;}
  atomic_store(&command_state->navigation_revision,rev+1);atomic_thread_fence(memory_order_release);
  atomic_store(&command_state->navigation_owner,f.owner);atomic_store(&command_state->navigation_epoch,f.epoch);atomic_store(&command_state->navigation_tick,atomic_load(&mirror_state->heartbeat));atomic_store(&command_state->navigation_token,token());atomic_store(&command_state->navigation_count,n+1);
  atomic_store_explicit(&command_state->navigation_revision,rev+2,memory_order_release);atomic_store(&command_state->navigation_guard,0);return;
 }
 JogCall *call=jog_depth?jog_calls+jog_depth-1:NULL;if(!call)return;
 if(site==JN_OPERATION||site==JN_RESTART_SET){
  if(c->r[4]!=call->sequencer)return;
  if(call->restart_sp&&call->restart_sp!=source_sp(c)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  call->restart_sp=source_sp(c);call->outcome|=site==JN_OPERATION?1:2;
 }else if(site==JN_RESTART_DONE){
  if(call->restart_sp==source_sp(c))call->outcome|=4;
 }else if(site==JN_RESTART_EARLY&&c->r[4]==call->sequencer){call->outcome|=c->r[0]?16:8;}
}
