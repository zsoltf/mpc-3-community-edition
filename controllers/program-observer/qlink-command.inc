/* Logical-slot input through the existing native asynchronous
 * facade and transaction owner. No downstream target pinning or DSP receipt. */
enum {QJ_ACTIVITY_RESULT=183,QJ_VALUE_RESULT,QJ_ACTIVITY_ENTER,QJ_ACTIVITY_DONE,QJ_VALUE_ENTER,QJ_VALUE_DONE,QJ_ACTIVITY_PREPARE,QJ_VALUE_PREPARE};
static _Atomic uint32_t qlink_payload[COMMAND_SLOTS][3],qlink_finished[COMMAND_SLOTS][3],qlink_sampled[COMMAND_SLOTS],qlink_changed[COMMAND_SLOTS],qlink_barrier[COMMAND_SLOTS][COMMAND_LANES];
static _Atomic uint32_t qlink_active;
static uint32_t qlink_source_frame[COMMAND_SLOTS][3];
static uint32_t qlink_result[COMMAND_SLOTS][3],qlink_fault[COMMAND_SLOTS];
static __thread uint32_t qlink_submission __attribute__((tls_model("initial-exec")));
static __thread unsigned qlink_part __attribute__((tls_model("initial-exec")));
typedef struct {uint32_t id,part,payload,sp;} QLinkCall;
static __thread QLinkCall qlink_calls[8] __attribute__((tls_model("initial-exec")));
static __thread unsigned qlink_depth __attribute__((tls_model("initial-exec")));
static void qlink_command_initialize(void){
 atomic_store(&qlink_active,0);memset(qlink_source_frame,0,sizeof(qlink_source_frame));qlink_submission=qlink_part=qlink_depth=0;memset(qlink_calls,0,sizeof(qlink_calls));memset(qlink_result,0,sizeof(qlink_result));memset(qlink_fault,0,sizeof(qlink_fault));
 for(unsigned i=0;i<COMMAND_SLOTS;i++){atomic_store(qlink_sampled+i,0);atomic_store(qlink_changed+i,0);for(unsigned lane=0;lane<COMMAND_LANES;lane++)atomic_store(&qlink_barrier[i][lane],0);for(unsigned j=0;j<3;j++){atomic_store(&qlink_payload[i][j],0);atomic_store(&qlink_finished[i][j],0);}}
}
static int qlink_receipted(unsigned at){return atomic_load_explicit(qlink_sampled+at,memory_order_acquire)==accepted[at].request.seq;}
static unsigned qlink_input_resolve(Context *c,const CommandRequest *r,QLinkNative *n,int admission){
 float value;memcpy(&value,&r->bits,4);
 if(!command_qlink(r->reserved)||!(value>=0&&value<=1)||r->qlink_index>=QLINK_SLOTS||!r->qlink_generation||r->field_incarnation!=r->qlink_generation||r->serial||r->binding||r->incarnation||r->track_owner||r->program_owner||r->pad_owner||r->global_owner)return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 unsigned now=atomic_load(&mirror_state->heartbeat);
 if(r->created>=command_tick_limit(WINDOW_SECONDS)||r->expires<r->created||r->expires-r->created>120000||r->expires<now)return C_EXPIRED;
 if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 QLinkDemand d={.enabled=1,.epoch=r->epoch};
 if(!qlink_resolve(c,&d,n)||atomic_load(&mirror_state->qlinks.invalid)||atomic_load(&mirror_state->qlinks.error))return C_NOT_READY;
 if(channel_owner_id(project)!=r->project_owner||n->root!=r->qlink_root||n->wrapper!=r->qlink_wrapper||n->provider!=r->qlink_provider||qlink_current.generation!=r->qlink_generation)return C_IDENTITY;
 if(admission&&*(const volatile unsigned char*)(uintptr_t)(n->sources+0x84*r->qlink_index+0x28))return C_BUSY;
 return C_OK;
}
static void qlink_input_service(Context *c,unsigned at,const CommandRequest *r){
 CommandSlot *slot=command_state->slots+at;QLinkNative n;unsigned why=qlink_input_resolve(c,r,&n,1);
 if(why==COMMAND_WAIT_HEARTBEAT)return;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(i!=at){CommandSlot *other=command_state->slots+i;unsigned seq=atomic_load(&other->published);if(seq!=atomic_load(&other->reclaimed)){CommandRequest request;if(!command_request_read(other,seq,&request))return;if(command_qlink(request.reserved)&&request.qlink_wrapper==r->qlink_wrapper&&request.qlink_index==r->qlink_index)return;}}
 accepted[at].request=*r;accepted[at].program=r->qlink_wrapper;accepted[at].track=accepted[at].property=0;accepted[at].controller=r->qlink_index;accepted[at].native_bits=r->bits;accepted[at].synchronous=0;native_started[at]=0;
 atomic_store(&slot->rejected,0);atomic_store(qlink_sampled+at,0);atomic_store(qlink_changed+at,0);for(unsigned lane=0;lane<COMMAND_LANES;lane++)atomic_store(&qlink_barrier[at][lane],0);qlink_fault[at]=0;
 for(unsigned j=0;j<3;j++){atomic_store(&qlink_payload[at][j],0);atomic_store(&qlink_finished[at][j],0);qlink_result[at][j]=0;qlink_source_frame[at][j]=0;}
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}
 if(why)goto rejected;
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);
 event(c,CE_DISPATCH,r->seq,0,r->qlink_wrapper,r->qlink_wrapper,0,r->qlink_index,r->bits,r->qlink_index,r->epoch);
 why=qlink_input_resolve(c,r,&n,1);if(why||qlink_submission||!atomic_load(&observer_running)||atomic_load(&command_state->error)||atomic_load(&command_state->trace_error)){if(!why)why=C_SOURCE_FAILURE;goto rejected;}
 native_started[at]=r->seq;atomic_store_explicit(&qlink_active,r->seq,memory_order_release);qlink_submission=r->seq;
 for(unsigned part=0;part<3;part++){
  qlink_part=part;
  /* A failed value publication still executes the cleanup submission. Three
   * jobs are ordered native submissions, not an atomic physical gesture. */
  if(part==1&&(qlink_result[at][0]!=2||qlink_fault[at]))continue;
  if(part==2&&qlink_result[at][0]==1&&!atomic_load(&qlink_payload[at][0]))break;
  if(part==1){for(unsigned lane=0;lane<COMMAND_LANES;lane++)atomic_store(&qlink_barrier[at][lane],atomic_load_explicit(&qlink_witness[lane][r->qlink_index].revision,memory_order_acquire));float value;memcpy(&value,&r->bits,4);((void (*)(void*,unsigned,float))(uintptr_t)(observer_image_bias+0x14de2d8))((void*)(uintptr_t)n.wrapper,r->qlink_index,value);}
  else ((void (*)(void*,unsigned,unsigned))(uintptr_t)(observer_image_bias+0x14dd984))((void*)(uintptr_t)n.wrapper,r->qlink_index,part==0);
 }
 qlink_submission=0;
 for(unsigned j=0;j<3;j++)if(qlink_result[at][j]!=2||!atomic_load_explicit(&qlink_payload[at][j],memory_order_acquire))why=C_TRACE_AMBIGUOUS;
 if(qlink_fault[at])why=qlink_fault[at];
 if(why){
  /* Preserve every native obligation, even if cleanup was only attempted.
   * No rejected/idle fiction after the first facade call. Recovery is explicit. */
  event(c,CE_RETURN,r->seq,0,n.wrapper,n.wrapper,0,r->qlink_index,r->bits,r->qlink_index,why);
  atomic_store(&slot->processed,r->seq);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);command_fail(why);return;
 }
 event(c,CE_RETURN,r->seq,0,n.wrapper,n.wrapper,0,r->qlink_index,r->bits,r->qlink_index,r->epoch);
 atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);return;
rejected:
 atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store(&slot->rejected,why);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);
}
static int qlink_job_find(uint32_t payload,unsigned *part){
 if(!atomic_load_explicit(&qlink_active,memory_order_acquire))return COMMAND_SLOTS;
 int found=COMMAND_SLOTS;
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  unsigned seq=atomic_load_explicit(active_request+at,memory_order_seq_cst);
  if(!seq||accepted[at].request.seq!=seq||!command_qlink(accepted[at].request.reserved))continue;
  for(unsigned j=0;j<3;j++)if(!atomic_load(&qlink_finished[at][j])&&atomic_load_explicit(&qlink_payload[at][j],memory_order_acquire)==payload){if(found!=COMMAND_SLOTS)return -1;found=(int)at;*part=j;}
 }
 return found;
}
static void qlink_job_point(Context *c,unsigned site){
 if(site==JG_PUBLISH||site==QJ_ACTIVITY_RESULT||site==QJ_VALUE_RESULT||site==QJ_ACTIVITY_PREPARE||site==QJ_VALUE_PREPARE){
  if(!qlink_submission)return;
  unsigned at=command_find(command_state,qlink_submission),part=qlink_part;
  if(at>=COMMAND_SLOTS||part>=3||accepted[at].request.seq!=qlink_submission){trace_fail(C_TRACE_AMBIGUOUS);return;}
  const CommandRequest *r=&accepted[at].request;uint32_t w=r->qlink_wrapper;
  if(token()!=command_owner||!atomic_load(&command_in_hook)){qlink_fault[at]=C_OWNER;return;}
  if(site==QJ_ACTIVITY_PREPARE||site==QJ_VALUE_PREPARE){
   if(site!=(part==1?QJ_VALUE_PREPARE:QJ_ACTIVITY_PREPARE)||qlink_source_frame[at][part]||c->r[4]!=w||c->r[6]!=r->qlink_index||c->r[5]!=(part==0?1:part==1?r->bits:0)||c->r[3]!=observer_image_bias+(part==1?0x14e3a50:0x14dfc04)||!qlink_span(source_sp(c),24)){qlink_fault[at]=C_TRACE_AMBIGUOUS;return;}
   qlink_source_frame[at][part]=source_sp(c);return;
  }
  if(site==JG_PUBLISH){
   uint32_t cell=c->r[10],input=c->r[8],expected=part==0?1:part==1?r->bits:0;
   /* Native pair/payload layout validated against the pinned factory. */
   uint32_t invoker=observer_image_bias+(part==1?0x14e3a50:0x14dfc04);
   uint32_t manager=observer_image_bias+(part==1?0x14dcd1c:0x14dcb54);
   if(atomic_load(&qlink_payload[at][part])||!qlink_span(cell,24)||!qlink_source_frame[at][part]||input!=qlink_source_frame[at][part]||c->r[7]!=command_queues+0xfc||c->r[9]!=(w|1u)||word(cell)!=invoker||word(cell+4)!=manager||word(cell+8)!=w||word(cell+12)!=w||word(cell+16)!=r->qlink_index||(part==1?word(cell+20):*(const volatile unsigned char*)(uintptr_t)(cell+20))!=expected){qlink_fault[at]=C_TRACE_AMBIGUOUS;return;}
   atomic_store_explicit(&qlink_payload[at][part],cell+8,memory_order_release);
   event(c,CE_QL_QUEUE,r->seq,cell+8,w,w,part,r->qlink_index,expected,r->qlink_index,r->epoch);
  }else{
   if(site!=(part==1?QJ_VALUE_RESULT:QJ_ACTIVITY_RESULT)||qlink_result[at][part]||source_sp(c)!=qlink_source_frame[at][part]||c->r[0]>1){qlink_fault[at]=C_TRACE_AMBIGUOUS;return;}
   qlink_result[at][part]=c->r[0]+1;
   event(c,CE_QL_RESULT,r->seq,atomic_load(&qlink_payload[at][part]),w,w,part,r->qlink_index,c->r[0],r->qlink_index,r->epoch);
  }return;
 }
 unsigned entering=site==QJ_ACTIVITY_ENTER||site==QJ_VALUE_ENTER,part=0;uint32_t payload=c->r[entering?0:4];int found=qlink_job_find(payload,&part);
 if(found==COMMAND_SLOTS)return;
 if(found<0){trace_fail(C_TRACE_AMBIGUOUS);return;}unsigned at=(unsigned)found;
 const CommandRequest *r=&accepted[at].request;unsigned value=part==0?1:part==1?r->bits:0;
 if(site==QJ_VALUE_FANOUT){if(part!=1||c->r[5]!=r->qlink_provider||c->r[7]!=r->qlink_index||c->r[6]!=r->qlink_provider+4){trace_fail(C_TRACE_AMBIGUOUS);return;}atomic_store_explicit(qlink_changed+at,1,memory_order_release);return;}
 if((part==1)!=(site==QJ_VALUE_ENTER||site==QJ_VALUE_DONE)){trace_fail(C_TRACE_AMBIGUOUS);return;}
 if(entering){
  if(qlink_depth==8||!qlink_span(payload,16)||word(payload)!=r->qlink_wrapper||word(payload+4)!=r->qlink_wrapper||word(payload+8)!=r->qlink_index||(part==1?word(payload+12):*(const volatile unsigned char*)(uintptr_t)(payload+12))!=value||(part&&!atomic_load_explicit(&qlink_finished[at][part-1],memory_order_acquire))){trace_fail(C_TRACE_AMBIGUOUS);return;}
  qlink_calls[qlink_depth++]=(QLinkCall){r->seq,part,payload,source_sp(c)};
  event(c,CE_QL_ENTER,r->seq,payload,r->qlink_wrapper,r->qlink_wrapper,part,r->qlink_index,value,r->qlink_index,r->epoch);
 }else{
  QLinkCall *call=qlink_depth?qlink_calls+qlink_depth-1:NULL;
  if(!call||call->id!=r->seq||call->part!=part||call->payload!=payload||call->sp!=source_sp(c)+(part==1?0xf0:8)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  event(c,CE_QL_DONE,r->seq,payload,r->qlink_wrapper,r->qlink_wrapper,part,r->qlink_index,value,r->qlink_index,r->epoch);
  qlink_depth--;atomic_store_explicit(&qlink_finished[at][part],r->seq,memory_order_release);
  if(part==2){atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&command_state->slots[at].done,r->seq,memory_order_release);}
 }
}
static int qlink_complete(Context *c){
 if(!atomic_load_explicit(&qlink_active,memory_order_acquire))return 0;
 unsigned completed=0,pending=0;
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  CommandSlot *slot=command_state->slots+at;unsigned seq=atomic_load_explicit(&slot->done,memory_order_acquire);
  if(!seq||accepted[at].request.seq!=seq||!command_qlink(accepted[at].request.reserved)||seq==atomic_load(&slot->reclaimed)||qlink_receipted(at))continue;
  const CommandRequest *r=&accepted[at].request;QLinkNative n;unsigned outcome=QL_INVALID,bits=0;
  if(!qlink_input_resolve(c,r,&n,0)){
   qlink_reading=1;float value=((float (*)(void*,unsigned))(uintptr_t)(observer_image_bias+0x14ddc94))((void*)(uintptr_t)n.wrapper,r->qlink_index);QLinkNative after;
   if(!qlink_input_resolve(c,r,&after,0)&&!memcmp(&n,&after,sizeof(n))&&value>=0&&value<=1){memcpy(&bits,&value,4);outcome=QL_SAMPLED;}
   qlink_reading=0;
  }
  for(unsigned lane=0;lane<COMMAND_LANES;lane++)event(c,CE_QL_BARRIER,seq,r->qlink_wrapper,r->qlink_provider,r->qlink_wrapper,lane,r->qlink_index,atomic_load(&qlink_barrier[at][lane]),0,r->epoch);
  event(c,CE_QL_VALUE,seq,r->qlink_wrapper,r->qlink_provider,r->qlink_wrapper,atomic_load_explicit(qlink_changed+at,memory_order_acquire),r->qlink_index,bits,0,outcome);
  atomic_store_explicit(qlink_sampled+at,seq,memory_order_release);completed++;if(completed==8)break;
 }
 for(unsigned at=0;at<COMMAND_SLOTS;at++)if(native_started[at]&&command_qlink(accepted[at].request.reserved)&&!qlink_receipted(at)&&atomic_load(&command_state->slots[at].published)!=atomic_load(&command_state->slots[at].reclaimed))pending=1;
 if(!pending)atomic_store_explicit(&qlink_active,0,memory_order_release);
 return completed;
}
