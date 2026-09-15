/* Native mode selection on the existing UI/transaction owner. Selection
 * readback and the exact controller jobs are separate from provider/DSP state. */
enum {QM_PREPARE0=202,QM_RESULT0,QM_PREPARE1,QM_RESULT1,QM_ENTER0,QM_DONE0,QM_ENTER1,QM_DONE1,QM_DEATH};
static uint32_t mode_frames[COMMAND_SLOTS][2],mode_results[COMMAND_SLOTS][2],mode_fault[COMMAND_SLOTS],mode_expected[COMMAND_SLOTS],mode_accept_error[COMMAND_SLOTS];
static _Atomic uint32_t mode_payloads[COMMAND_SLOTS][2],mode_finished[COMMAND_SLOTS][2],mode_sampled[COMMAND_SLOTS],mode_active;
static __thread unsigned mode_submission __attribute__((tls_model("initial-exec")));
static __thread QLinkCall mode_calls[8] __attribute__((tls_model("initial-exec")));
static __thread unsigned mode_depth __attribute__((tls_model("initial-exec")));
static void qlink_mode_initialize(void){
 memset(mode_frames,0,sizeof(mode_frames));memset(mode_results,0,sizeof(mode_results));memset(mode_fault,0,sizeof(mode_fault));memset(mode_expected,0,sizeof(mode_expected));memset(mode_accept_error,0,sizeof(mode_accept_error));memset(mode_payloads,0,sizeof(mode_payloads));memset(mode_finished,0,sizeof(mode_finished));memset(mode_sampled,0,sizeof(mode_sampled));atomic_store(&mode_active,0);mode_submission=mode_depth=0;
}
static int qlink_mode_receipted(unsigned at){return atomic_load_explicit(mode_sampled+at,memory_order_acquire)==accepted[at].request.seq;}
static unsigned mode_request_resolve(Context *c,const CommandRequest *r,uint32_t *k,unsigned *id,unsigned *count){
 if(!command_qlink_mode(r->reserved)||!command_mode_delta(r->bits)||!r->qlink_root||!r->global_owner||!r->field_incarnation||r->serial||r->binding||r->incarnation||r->track_owner||r->program_owner||r->pad_owner||r->qlink_wrapper||r->qlink_provider||r->qlink_generation||r->qlink_index)return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 unsigned now=atomic_load(&mirror_state->heartbeat);if(r->expires<r->created||r->expires-r->created>120000||r->expires<now)return C_EXPIRED;if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 if(!qlink_mode_resolve(c,r->epoch,k,id,count)||root!=r->qlink_root||*k!=r->global_owner||qlink_mode_generation!=r->field_incarnation||channel_owner_id(project)!=r->project_owner)return C_IDENTITY;
 return C_OK;
}
static void mode_event(Context *c,unsigned kind,unsigned at,unsigned part,unsigned payload,unsigned bits,unsigned outcome){
 const CommandRequest *r=&accepted[at].request;event(c,kind,r->seq,payload,r->global_owner+8,r->global_owner,part,accepted[at].controller,bits,r->global_owner,outcome);
}
/* Same UI-owner invocation, immediately after setter return. A later mode
 * sample is observational and cannot supply this acceptance proof. */
static void mode_acceptance(Context *c,unsigned at){
 uint32_t k=0;unsigned id=0,count=0;
 unsigned why=mode_request_resolve(c,&accepted[at].request,&k,&id,&count);
 if(!why&&id!=accepted[at].controller)why=C_SOURCE_CHANGED;
 mode_accept_error[at]=why;
 mode_event(c,CE_QM_ACCEPT,at,mode_expected[at],k,id,why);
}
static void qlink_mode_service(Context *c,unsigned at,const CommandRequest *r){
 CommandSlot *slot=command_state->slots+at;uint32_t k=0;unsigned id=0,count=0,why=mode_request_resolve(c,r,&k,&id,&count);if(why==COMMAND_WAIT_HEARTBEAT)return;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(i!=at){CommandSlot *other=command_state->slots+i;unsigned seq=atomic_load(&other->published);if(seq&&seq<r->seq&&seq!=atomic_load(&other->reclaimed)){CommandRequest old;if(!command_request_read(other,seq,&old)||command_conflict(&old,r))return;}}
 accepted[at].request=*r;accepted[at].program=r->global_owner;accepted[at].property=accepted[at].track=0;accepted[at].synchronous=0;native_started[at]=0;mode_fault[at]=mode_accept_error[at]=0;atomic_store(mode_sampled+at,0);
 for(unsigned p=0;p<2;p++){mode_frames[at][p]=mode_results[at][p]=0;atomic_store(&mode_payloads[at][p],0);atomic_store(&mode_finished[at][p],0);}
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}atomic_store(&slot->rejected,0);
 if(why)goto refused;
 why=mode_request_resolve(c,r,&k,&id,&count);if(why||mode_submission||!atomic_load(&observer_running)||atomic_load(&command_state->trace_error)){if(!why)why=C_SOURCE_FAILURE;goto refused;}
 int next=((int)id+(int32_t)r->bits)%(int)count;if(next<0)next+=(int)count;
 accepted[at].controller=(unsigned)next;accepted[at].native_bits=(unsigned)next;mode_expected[at]=qlink_mode_jobs(id,(unsigned)next);
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);mode_event(c,CE_DISPATCH,at,mode_expected[at],0,(unsigned)next,r->epoch);
 native_started[at]=r->seq;atomic_store_explicit(&mode_active,r->seq,memory_order_release);mode_submission=r->seq;
 ((void (*)(void*,const int*))(uintptr_t)(observer_image_bias+0x11bcd54))((void*)(uintptr_t)k,&next);
 mode_submission=0;
 mode_acceptance(c,at);
 for(unsigned p=0;p<2;p++)if(mode_expected[at]&(1u<<p)){if(mode_results[at][p]!=2||!atomic_load_explicit(&mode_payloads[at][p],memory_order_acquire))why=C_TRACE_AMBIGUOUS;}else if(mode_results[at][p]||atomic_load(&mode_payloads[at][p]))why=C_TRACE_AMBIGUOUS;
 if(mode_fault[at])why=mode_fault[at];
 mode_event(c,CE_RETURN,at,mode_expected[at],0,(unsigned)next,r->epoch);
 if(why){atomic_store(&slot->processed,r->seq);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);command_fail(why);return;}
 if(!mode_accept_error[at]){atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);}
 if(!mode_expected[at]){atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&slot->done,r->seq,memory_order_release);}
 atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);return;
refused:
 atomic_store(active_request+at,0);atomic_store(&slot->rejected,why);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);
}
static int mode_job_find(uint32_t payload,unsigned *part){
 if(!atomic_load_explicit(&mode_active,memory_order_acquire))return COMMAND_SLOTS;
 int found=COMMAND_SLOTS;
 for(unsigned at=0;at<COMMAND_SLOTS;at++){unsigned seq=atomic_load_explicit(active_request+at,memory_order_seq_cst);if(!seq||accepted[at].request.seq!=seq||!command_qlink_mode(accepted[at].request.reserved))continue;
  for(unsigned p=0;p<2;p++)if(!atomic_load(&mode_finished[at][p])&&atomic_load_explicit(&mode_payloads[at][p],memory_order_acquire)==payload){if(found!=COMMAND_SLOTS)return -1;found=(int)at;*part=p;}}
 return found;
}
static void mode_job_point(Context *c,unsigned site){
 if(site<=QM_RESULT1||site==JG_PUBLISH){
  if(!mode_submission)return;
  unsigned at=command_find(command_state,mode_submission);if(at>=COMMAND_SLOTS){trace_fail(C_TRACE_AMBIGUOUS);return;}
  const CommandRequest *r=&accepted[at].request;unsigned k=r->global_owner,part=site==QM_PREPARE1||site==QM_RESULT1;
  if(token()!=command_owner||!atomic_load(&command_in_hook)){mode_fault[at]=C_OWNER;return;}
  if(site==QM_PREPARE0||site==QM_PREPARE1){if(c->r[4]!=k)return;if(!(mode_expected[at]&(1u<<part))||mode_frames[at][part]||c->r[7]!=k+8||c->r[11]!=source_sp(c)+0x10){mode_fault[at]=C_TRACE_AMBIGUOUS;return;}mode_frames[at][part]=source_sp(c)+0x10;return;}
  if(site==JG_PUBLISH){
   if(c->r[8]==mode_frames[at][0]&&mode_frames[at][0]&&!mode_results[at][0])part=0;else if(c->r[8]==mode_frames[at][1]&&mode_frames[at][1]&&!mode_results[at][1])part=1;else return;
   unsigned cell=c->r[10];if(!qlink_span(cell,16)||atomic_load(&mode_payloads[at][part])||c->r[7]!=command_queues+0xfc||c->r[9]!=((k+8)|1u)||word(cell)!=observer_image_bias+(part?0x11bcc70:0x11c2104)||word(cell+4)!=observer_image_bias+(part?0x11b8ea0:0x11b8e70)||word(cell+8)!=k+8||word(cell+12)!=k){mode_fault[at]=C_TRACE_AMBIGUOUS;return;}
   atomic_store_explicit(&mode_payloads[at][part],cell+8,memory_order_release);mode_event(c,CE_QM_QUEUE,at,part,cell+8,accepted[at].controller,r->epoch);return;
  }
  if(c->r[4]!=k)return;
  if(mode_results[at][part]||!mode_frames[at][part]||source_sp(c)+0x10!=mode_frames[at][part]||c->r[0]>1){mode_fault[at]=C_TRACE_AMBIGUOUS;return;}
  mode_results[at][part]=c->r[0]+1;mode_event(c,CE_QM_RESULT,at,part,atomic_load(&mode_payloads[at][part]),c->r[0],r->epoch);return;
 }
 unsigned entering=site==QM_ENTER0||site==QM_ENTER1,part=0,payload=c->r[entering?0:site==QM_DONE0?4:5];int found=mode_job_find(payload,&part);if(found==COMMAND_SLOTS)return;if(found<0){trace_fail(C_TRACE_AMBIGUOUS);return;}unsigned at=(unsigned)found;const CommandRequest *r=&accepted[at].request;
 if(part!=(unsigned)(site==QM_ENTER1||site==QM_DONE1)){trace_fail(C_TRACE_AMBIGUOUS);return;}
 if(entering){if(mode_depth==8||!qlink_span(payload,8)||word(payload)!=r->global_owner+8||word(payload+4)!=r->global_owner||(part==1&&(mode_expected[at]&1)&&atomic_load(&mode_finished[at][0])!=r->seq)){trace_fail(C_TRACE_AMBIGUOUS);return;}mode_calls[mode_depth++]=(QLinkCall){r->seq,part,payload,source_sp(c)};mode_event(c,CE_QM_ENTER,at,part,payload,accepted[at].controller,r->epoch);}
 else{QLinkCall *f=mode_depth?mode_calls+mode_depth-1:NULL;if(!f||f->id!=r->seq||f->part!=part||f->payload!=payload||f->sp!=source_sp(c)+(part?16:32)){trace_fail(C_TRACE_AMBIGUOUS);return;}mode_event(c,CE_QM_DONE,at,part,payload,accepted[at].controller,r->epoch);mode_depth--;atomic_store_explicit(&mode_finished[at][part],r->seq,memory_order_release);
  if(part==1){atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&command_state->slots[at].done,r->seq,memory_order_release);}}
}
static void qlink_mode_complete(Context *c){
 if(!atomic_load_explicit(&mode_active,memory_order_acquire))return;
 unsigned pending=0;
 for(unsigned at=0;at<COMMAND_SLOTS;at++){const CommandRequest *r=&accepted[at].request;if(!command_qlink_mode(r->reserved)||!native_started[at]||qlink_mode_receipted(at))continue;
  if(atomic_load_explicit(&command_state->slots[at].done,memory_order_acquire)!=r->seq){pending=1;continue;}
  unsigned id=0,count=0,outcome=QL_INVALID;uint32_t k=0;if(!mode_request_resolve(c,r,&k,&id,&count))outcome=QL_SAMPLED;
  mode_event(c,CE_QM_VALUE,at,mode_expected[at],k,id,outcome);atomic_store_explicit(mode_sampled+at,r->seq,memory_order_release);
  /* Keep exact native obligations alive after failed acceptance. Only now,
   * after their DONE receipts, close admission without sealing/settling. */
  if(mode_accept_error[at])command_fail(mode_accept_error[at]);
 }
 if(!pending)atomic_store(&mode_active,0);
}
