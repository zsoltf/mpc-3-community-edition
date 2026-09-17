/* Real Toolbar CommandFactory -> native history -> retained SetMixerCommand.
 * C/B and payload are copied identities; the adapter never retains native state. */
static _Atomic uint32_t master_factory,master_dead_factory,master_payload[COMMAND_SLOTS];
static uint32_t master_c[COMMAND_SLOTS],master_b[COMMAND_SLOTS],master_enqueue[COMMAND_SLOTS];
static __thread uint32_t master_submission __attribute__((tls_model("initial-exec")));
typedef struct {uint32_t id,payload,sp,set_sp,phase,value,revision;} MasterCall;
static __thread MasterCall master_call __attribute__((tls_model("initial-exec")));
static void master_initialize(void){atomic_store(&master_factory,0);atomic_store(&master_dead_factory,0);master_submission=0;memset(&master_call,0,sizeof(master_call));for(unsigned i=0;i<COMMAND_SLOTS;i++){atomic_store(master_payload+i,0);master_c[i]=master_b[i]=master_enqueue[i]=0;}}
static int master_copy(unsigned at,uint32_t *bits,uint32_t *revision){
 ChannelCell *cell=channel_cell(accepted[at].property);const CommandRequest *r=&accepted[at].request;
 if(!cell||atomic_load(&cell->field)!=CF_MASTER||atomic_load(&cell->incarnation)!=r->field_incarnation||atomic_load(&cell->owner_incarnation)!=r->global_owner||!atomic_load(&cell->live)||!channel_owner_id(accepted[at].program)||channel_fault(atomic_load(&mirror_state->channel.errors[CF_MASTER])))return 0;
 for(unsigned n=0;n<4;n++){unsigned rev=atomic_load_explicit(&cell->revision,memory_order_acquire);if(rev&1)continue;unsigned value=atomic_load_explicit(&cell->bits,memory_order_relaxed);atomic_thread_fence(memory_order_acquire);if(rev==atomic_load_explicit(&cell->revision,memory_order_relaxed)){*bits=value;*revision=rev;return rev!=0;}}
 return 0;
}
static unsigned master_resolve(Context *c,const CommandRequest *r,uint32_t *factory,uint32_t *mixer){
 float value;memcpy(&value,&r->bits,4);unsigned now=atomic_load(&mirror_state->heartbeat);
 if(r->reserved!=CF_MASTER||!(value>=0&&value<=1)||r->serial||r->binding||r->incarnation||r->track_owner||r->program_owner||!r->global_owner||!r->field_incarnation)return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 if(r->created>=command_tick_limit(WINDOW_SECONDS)||r->expires<r->created||r->expires-r->created>120000||r->expires<now)return C_EXPIRED;
 if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 if(!active()||atomic_load(&enrollment_fault)||atomic_load(&command_state->trace_error))return C_SOURCE_FAILURE;
 if(!ready||loading||reset_pending||depth||!root||!project||!pool||!command_queues)return C_NOT_READY;
 if(r->epoch!=epoch||channel_owner_id(project)!=r->project_owner)return C_IDENTITY;
 for(unsigned i=0;i<count;i++)if(tracks[i].transition)return C_NOT_READY;
 if(c->r[0]!=command_queues+4||word(c->r[0]+0xf0))return C_NOT_READY;
 if(!typed(root,0x68d7bf8)||word(root+0x24c)!=project||!typed(project,0x6933c4c)||word(project+0xa94)!=pool||!typed(pool,0x68ad2c4))return C_IDENTITY;
 uint32_t a=word(root+0x2e4),f=word(root+0x318);if(!pointer(a)||a>UINT32_MAX-0x400)return C_NATIVE_MEMBERSHIP;uint32_t m=a+0x260;
 if(!typed(m,0x6898b34)||!typed(f,0x6899fa0)||f!=atomic_load(&master_factory)||f==atomic_load(&master_dead_factory)||word(f+4)!=command_queues||word(f+0x10)!=command_queues||word(f+0x14)!=project||word(f+0x30)!=word(project+0xab0)||word(f+0x40)!=m||word(root+0xf8)!=command_queues||word(a+0xa8)!=command_queues)return C_NATIVE_MEMBERSHIP;
 ChannelCell *cell=channel_cell(m+0xa8);if(channel_owner_id(m)!=r->global_owner||atomic_load(&mirror_state->channel.mixer_owner)!=r->global_owner||!cell||!atomic_load(&cell->live)||atomic_load(&cell->incarnation)!=r->field_incarnation||atomic_load(&cell->owner_incarnation)!=r->global_owner||channel_fault(atomic_load(&mirror_state->channel.errors[CF_MASTER])))return C_IDENTITY;
 if(word(root+0x318)!=f||word(root+0x2e4)!=a)return C_NATIVE_MEMBERSHIP;
 *factory=f;*mixer=m;return 0;
}
#ifdef COMMAND_COMPONENT
static void (*component_master_dispatch)(uint32_t,unsigned,float);
#endif
static void master_service(Context *c,unsigned at,const CommandRequest *r){
 uint32_t f=0,m=0;unsigned why=master_resolve(c,r,&f,&m);if(why==COMMAND_WAIT_HEARTBEAT)return;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(i!=at){CommandSlot *q=command_state->slots+i;CommandRequest old;unsigned seq=atomic_load(&q->published);if(seq&&seq<r->seq&&seq!=atomic_load(&q->reclaimed)&&command_request_read(q,seq,&old)&&old.reserved==CF_MASTER)return;}
 CommandSlot *slot=command_state->slots+at;accepted[at].request=*r;native_started[at]=0;atomic_store(&slot->rejected,0);accepted[at].program=m;accepted[at].property=m?m+0xa8:0;accepted[at].track=0;accepted[at].controller=CF_MASTER;accepted[at].native_bits=r->bits;accepted[at].synchronous=0;
 atomic_store(master_payload+at,0);master_c[at]=master_b[at]=master_enqueue[at]=0;
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}atomic_store(&slot->rejected,0);
 if(why)goto rejected;
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);event(c,CE_DISPATCH,r->seq,0,0,m,0,CF_MASTER,r->bits,0,r->epoch);
 uint32_t checked_f=0,checked_m=0;why=master_resolve(c,r,&checked_f,&checked_m);
 if(why||f!=checked_f||m!=checked_m||!atomic_load(&observer_running)||atomic_load(&command_violation)||atomic_load(&command_state->error)||atomic_load(&command_state->trace_error)){if(!why)why=C_SOURCE_FAILURE;goto rejected;}
 if(master_submission||jog_submission){why=C_REENTRY;goto rejected;}native_started[at]=r->seq;master_submission=r->seq;float value;memcpy(&value,&r->bits,4);
#ifdef COMMAND_COMPONENT
 component_master_dispatch(f,0,value);
#else
 ((void (*)(void*,unsigned,float))(uintptr_t)(observer_image_bias+0x13d63a8))((void*)(uintptr_t)f,0,value);
#endif
 master_submission=0;
 if(master_enqueue[at]!=2||!atomic_load(master_payload+at)){why=master_enqueue[at]==1?C_NOT_READY:C_TRACE_AMBIGUOUS;goto rejected;}
 event(c,CE_RETURN,r->seq,atomic_load(master_payload+at),master_c[at]+0x34,m,0,CF_MASTER,r->bits,0,r->epoch);
 atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);return;
rejected:atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store(&slot->rejected,why);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);if(atomic_load(master_payload+at))command_fail(why);
}
static void master_point(Context *c,unsigned site){
 if(site==MS_FACTORY_DEATH||site==MS_FACTORY_DELETE||site==CH_MIXER_DEATH){
  if(site!=CH_MIXER_DEATH)recording_lifetime(c->r[0],1);
  if(site!=CH_MIXER_DEATH)atomic_store_explicit(&master_dead_factory,c->r[0],memory_order_release);
  for(unsigned i=0;i<COMMAND_SLOTS;i++)if(source_requests[i]&&accepted[i].request.reserved==CF_MASTER&&(site!=CH_MIXER_DEATH||accepted[i].program==c->r[0]))command_fail(C_IDENTITY);
  return;
 }
 if(site==MS_CREATE||site==JG_PUBLISH||site==MS_RESULT){
  if(!master_submission)return;
  unsigned at=command_find(command_state,master_submission);if(at==COMMAND_SLOTS||accepted[at].request.seq!=master_submission||token()!=command_owner||!atomic_load(&command_in_hook)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  if(site==JG_PUBLISH){
   /* Command history can notify other listeners during this factory call.
    * This generic seam sees their queues too. Read only the source-owned
    * header until either our tag or callable identifies relevant traffic;
    * never interpret an unrelated callable's variable-sized payload as C/B.
    * Matching any key/header part still requires the complete exact proof
    * below, including duplicate rejection. UI flight survives early DONE. */
   uint32_t slot=c->r[10],cmd=master_c[at];
   if(!pointer(slot)||slot>UINT32_MAX-8){trace_fail(C_TRACE_AMBIGUOUS);return;}
   if((!cmd||c->r[9]!=((cmd+0x34)|1u))&&word(slot)!=observer_image_bias+0x13bfd1c&&word(slot+4)!=observer_image_bias+0x13bfdf0)return;
  }
  if(site!=MS_RESULT&&source_requests[at]!=master_submission){trace_fail(C_TRACE_AMBIGUOUS);return;}
  const CommandRequest *r=&accepted[at].request;uint32_t m=accepted[at].program;
  if(site==MS_CREATE){uint32_t b=c->r[4],cmd=c->r[6];if(master_c[at]||c->r[5]!=atomic_load(&master_factory)||!pointer(b)||b>UINT32_MAX-0x60||cmd!=b+0x10||!typed(cmd,0x68eeaa8)||word(cmd+0x34)!=command_queues||word(cmd+0x3c)!=m||word(cmd+0x40)||word(cmd+0x44)!=r->bits){trace_fail(C_TRACE_AMBIGUOUS);return;}master_c[at]=cmd;master_b[at]=b;event(c,CE_MASTER_CREATE,r->seq,cmd,b,m,0,CF_MASTER,r->bits,0,r->epoch);}
  else if(site==MS_RESULT){if(master_enqueue[at]||c->r[0]>1){trace_fail(C_TRACE_AMBIGUOUS);return;}master_enqueue[at]=c->r[0]+1;}
  else{uint32_t cmd=master_c[at],b=master_b[at],slot=c->r[10];if(!cmd||atomic_load(master_payload+at)||!pointer(slot)||slot>UINT32_MAX-20||c->r[7]!=command_queues+0xfc||c->r[9]!=((cmd+0x34)|1u)||word(slot)!=observer_image_bias+0x13bfd1c||word(slot+4)!=observer_image_bias+0x13bfdf0||word(slot+8)!=cmd+0x34||word(slot+12)!=cmd||word(slot+16)!=b||word(cmd+0x2c)!=cmd||word(cmd+0x30)!=b){trace_fail(C_TRACE_AMBIGUOUS);return;}atomic_store_explicit(master_payload+at,slot+8,memory_order_release);event(c,CE_MASTER_QUEUE,r->seq,slot+8,cmd+0x34,m,0,CF_MASTER,r->bits,0,r->epoch);}
  return;
 }
 unsigned at=COMMAND_SLOTS;if(site==MS_ENTER||site==MS_DONE){uint32_t key=c->r[site==MS_ENTER?0:4];for(unsigned i=0;i<COMMAND_SLOTS;i++)if(source_requests[i]&&accepted[i].request.reserved==CF_MASTER&&atomic_load_explicit(master_payload+i,memory_order_acquire)==key){if(at!=COMMAND_SLOTS){trace_fail(C_TRACE_AMBIGUOUS);return;}at=i;}}
 else if(master_call.id)at=command_find(command_state,master_call.id);
 if(at==COMMAND_SLOTS)return;
 const CommandRequest *r=&accepted[at].request;uint32_t m=accepted[at].program,cmd=master_c[at],payload=atomic_load(master_payload+at);
 if(source_requests[at]!=r->seq){trace_fail(C_TRACE_AMBIGUOUS);return;}
 if(site==MS_ENTER){if(master_call.id||word(payload)!=cmd+0x34||word(payload+4)!=cmd||word(payload+8)!=master_b[at]){trace_fail(C_TRACE_AMBIGUOUS);return;}master_call=(MasterCall){r->seq,payload,source_sp(c),0,1,0,0};event(c,CE_MASTER_ENTER,r->seq,payload,cmd+0x34,m,0,CF_MASTER,r->bits,0,atomic_load(&mirror_state->epoch));}
 /* Unlike the other lanes, nothing before this point filters the receiver: the
  * hook admits every master setter call made while this command's native call
  * is on the thread. A call on another Mixer object is proof it is not this
  * command's own, so it is ignored, relevance before correctness. The index
  * argument stays a fault: its meaning for a foreign writer is not established
  * here, so a call this lane cannot attribute must still fail. */
 else if(site==MS_SET_ENTER){if(c->r[0]!=m)return;if(master_call.phase!=1||c->r[1]){trace_fail(C_TRACE_AMBIGUOUS);return;}master_call.phase=2;master_call.set_sp=source_sp(c);event(c,CE_MASTER_SET,r->seq,payload,cmd+0x34,m,0,CF_MASTER,(uint32_t)c->d[0],m+0xa8,0);}
 else if(site==MS_SET_DONE){if(c->r[4]!=m)return;if(master_call.phase!=2||source_sp(c)!=master_call.set_sp||!master_copy(at,&master_call.value,&master_call.revision)){trace_fail(C_TRACE_AMBIGUOUS);return;}master_call.phase=3;event(c,CE_MASTER_VALUE,r->seq,payload,cmd+0x34,m,0,CF_MASTER,master_call.value,m+0xa8,master_call.revision);}
 else if(site==MS_DONE){if(master_call.phase!=3||master_call.payload!=payload||source_sp(c)+8!=master_call.sp){trace_fail(C_TRACE_AMBIGUOUS);return;}event(c,CE_MASTER_DONE,r->seq,payload,cmd+0x34,m,0,CF_MASTER,master_call.value,0,atomic_load(&mirror_state->epoch));memset(&master_call,0,sizeof(master_call));atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&command_state->slots[at].done,r->seq,memory_order_release);}
}
static void master_commit(Context *c,unsigned at){
 if(master_call.id!=accepted[at].request.seq||master_call.phase!=2)return;
 uint32_t bits,revision;
 if(!master_copy(at,&bits,&revision)){trace_fail(C_TRACE_AMBIGUOUS);return;}
 event(c,CE_MASTER_COMMIT,master_call.id,master_call.payload,master_c[at]+0x34,accepted[at].program,0,CF_MASTER,bits,accepted[at].property,revision);
}
