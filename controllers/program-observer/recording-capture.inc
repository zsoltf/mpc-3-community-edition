/* Native GUI Program parameter facade. MPC owns canonical Track selection,
 * automation admission, both queues, event timing, buffering and cancellation. */
static _Atomic uint32_t recording_payload[COMMAND_SLOTS],recording_work[COMMAND_SLOTS];
static _Atomic uint32_t recording_cp[COMMAND_SLOTS],recording_first_done[COMMAND_SLOTS],recording_second_done[COMMAND_SLOTS];
static _Atomic uint32_t recording_pad_owner[COMMAND_SLOTS],recording_pad_property[COMMAND_SLOTS];
static uint32_t recording_timed_event[COMMAND_SLOTS][14];
static uint32_t recording_factory[COMMAND_SLOTS],recording_track[COMMAND_SLOTS],recording_track_owner[COMMAND_SLOTS],recording_queue[COMMAND_SLOTS];
static __thread uint32_t recording_submission __attribute__((tls_model("initial-exec")));
typedef struct {uint32_t id,payload,sp,phase,producer,producer_sp;} RecordingCall;
static __thread RecordingCall recording_call __attribute__((tls_model("initial-exec")));
static __thread RecordingCall recording_second __attribute__((tls_model("initial-exec")));
static void recording_initialize(void){
 recording_submission=0;memset(&recording_call,0,sizeof(recording_call));memset(&recording_second,0,sizeof(recording_second));
 for(unsigned i=0;i<COMMAND_SLOTS;i++){atomic_store(recording_payload+i,0);atomic_store(recording_work+i,0);atomic_store(recording_cp+i,0);atomic_store(recording_first_done+i,0);atomic_store(recording_second_done+i,0);recording_factory[i]=recording_track[i]=recording_track_owner[i]=recording_queue[i]=0;}
}
static unsigned recording_prepare(unsigned at){
 uint32_t f=word(root+0x318),a=word(root+0x2e4);
 if(!typed(f,0x6899fa0)||f!=atomic_load(&master_factory)||f==atomic_load(&master_dead_factory)||!pointer(a)||a>UINT32_MAX-0x400||word(f+4)!=command_queues||word(f+0x10)!=command_queues||word(f+0x14)!=project||word(f+0x30)!=word(project+0xab0)||word(f+0x28)!=word(a+0x3bc))return C_NATIVE_MEMBERSHIP;
 atomic_store(recording_pad_owner+at,0);atomic_store(recording_pad_property+at,0);
 recording_factory[at]=f;recording_track[at]=recording_track_owner[at]=recording_queue[at]=0;
 atomic_store(recording_payload+at,0);atomic_store(recording_work+at,0);atomic_store(recording_cp+at,0);atomic_store(recording_first_done+at,0);atomic_store(recording_second_done+at,0);return 0;
}
static int recording_current(unsigned at){
 const CommandRequest *r=&accepted[at].request;
 if(!atomic_load_explicit(&mirror_state->ready,memory_order_acquire)||r->epoch!=atomic_load(&mirror_state->epoch)||atomic_load(&mirror_state->channel.project_owner)!=r->project_owner||channel_owner_id(accepted[at].program)!=r->program_owner)return 0;
 unsigned revision=atomic_load_explicit(&mirror_state->revision,memory_order_acquire),n=atomic_load(&mirror_state->count);
 if((revision&1)||n>MIRROR_TRACKS)return 0;
 unsigned target=0,canonical=!recording_track[at];
 for(unsigned i=0;i<n;i++){
  MirrorBinding *t=mirror_state->tracks+i;
  if(atomic_load(&t->serial)==r->serial&&atomic_load(&t->binding)==r->binding&&atomic_load(&t->program)==accepted[at].program&&atomic_load(&t->track_owner)==r->track_owner&&atomic_load(&t->program_owner)==r->program_owner)target=1;
  if(atomic_load(&t->track)==recording_track[at]&&atomic_load(&t->track_owner)==recording_track_owner[at]&&atomic_load(&t->program)==accepted[at].program&&atomic_load(&t->program_owner)==r->program_owner)canonical=1;
 }
 atomic_thread_fence(memory_order_acquire);
 return target&&canonical&&revision==atomic_load_explicit(&mirror_state->revision,memory_order_relaxed)&&r->epoch==atomic_load(&mirror_state->epoch)&&atomic_load(&mirror_state->ready);
}
static int recording_event(unsigned at,uint32_t ev){
 return pointer(ev)&&ev<=UINT32_MAX-0x38&&*(const unsigned char*)(uintptr_t)(ev+8)==2&&word(ev+0x10)==command_arg_kind(&accepted[at].request)&&word(ev+0x14)==accepted[at].native_bits&&word(ev+0x18)==accepted[at].controller;
}
static void recording_emit(Context *c,unsigned kind,unsigned at,uint32_t payload,uint32_t counter){
 event(c,kind,accepted[at].request.seq,payload,counter,accepted[at].program,command_arg_kind(&accepted[at].request),accepted[at].controller,accepted[at].native_bits,0,accepted[at].request.epoch);
}
static void recording_complete(unsigned at){
 if(!atomic_load_explicit(recording_first_done+at,memory_order_acquire))return;
 if(atomic_load_explicit(recording_work+at,memory_order_acquire)&&!atomic_load_explicit(recording_second_done+at,memory_order_acquire))return;
 atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&command_state->slots[at].done,accepted[at].request.seq,memory_order_release);
}
static void recording_lifetime(uint32_t object,int factory){
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(source_requests[i]&&command_recording(&accepted[i].request)&&accepted[i].request.reserved!=CF_MASTER&&(factory?recording_factory[i]==object:atomic_load(recording_cp+i)==object))command_fail(C_IDENTITY);
}
#ifdef COMMAND_COMPONENT
static void (*component_recording_dispatch)(uint32_t,uint32_t,unsigned,unsigned,float);
#endif
static void recording_dispatch(unsigned at,float value){
 recording_submission=accepted[at].request.seq;
#ifdef COMMAND_COMPONENT
 component_recording_dispatch(recording_factory[at],accepted[at].program,command_arg_kind(&accepted[at].request),accepted[at].controller,value);
#else
 ((void (*)(void*,void*,unsigned,unsigned,float))(uintptr_t)(observer_image_bias+0x13ed940))((void*)(uintptr_t)recording_factory[at],(void*)(uintptr_t)accepted[at].program,command_arg_kind(&accepted[at].request),accepted[at].controller,value);
#endif
 recording_submission=0;
}
/* Return 1 only for an exact candidate-owned queue family; generic native
 * publications from nested history/listener calls remain unrelated traffic. */
static int recording_publication(Context *c){
 if(!recording_submission&&!recording_call.id)return 0;
 uint32_t h=c->r[10];if(!pointer(h)||h>UINT32_MAX-8)return 0;
 uint32_t inv=word(h),mgr=word(h+4);
 return (recording_submission&&(inv==observer_image_bias+0x13cf830||mgr==observer_image_bias+0x13d18b0))||(recording_call.id&&recording_call.producer&&(inv==observer_image_bias+0x1bbce38||mgr==observer_image_bias+0x1bbf2e8));
}
static void recording_point(Context *c,unsigned site){
 if(site==RC_CP_DEATH){recording_lifetime(c->r[0],0);return;}
 unsigned at=COMMAND_SLOTS;
 if(site==JG_PUBLISH){
  uint32_t h=c->r[10],n=h+8;unsigned second=word(h)==observer_image_bias+0x1bbce38||word(h+4)==observer_image_bias+0x1bbf2e8;
  at=command_find(command_state,second?recording_call.id:recording_submission);
  if(at==COMMAND_SLOTS||source_requests[at]!=accepted[at].request.seq||!recording_current(at)||h>UINT32_MAX-(second?0x80:0x60)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  uint32_t f=recording_factory[at];
  if(!second){
   uint32_t track=word(n+0x50);
   if(token()!=command_owner||!atomic_load(&command_in_hook)||atomic_load(recording_payload+at)||c->r[7]!=command_queues+0xfc||c->r[9]!=((f+4)|1u)||word(h)!=observer_image_bias+0x13cf830||word(h+4)!=observer_image_bias+0x13d18b0||word(n)!=f+4||word(n+8)!=f||word(n+0xc)!=command_arg_kind(&accepted[at].request)||word(n+0x10)!=accepted[at].controller||word(n+0x14)!=accepted[at].program||!recording_event(at,n+0x18)||!typed(track,0x6935554)||word(track+0x648)!=accepted[at].program||!channel_owner_id(track)){trace_fail(C_TRACE_AMBIGUOUS);return;}
   recording_track[at]=track;recording_track_owner[at]=channel_owner_id(track);
   if(!recording_current(at)){trace_fail(C_IDENTITY);return;}
   recording_emit(c,CE_RECORD_QUEUE,at,n,f+4);atomic_store_explicit(recording_payload+at,n,memory_order_release);
  }else{
   uint32_t cp=recording_call.producer;
   if(!cp||atomic_load(recording_work+at)||!recording_queue[at]||c->r[7]!=recording_queue[at]||c->r[9]!=((cp+0x18)|1u)||word(h)!=observer_image_bias+0x1bbce38||word(h+4)!=observer_image_bias+0x1bbf2e8||word(n)!=cp+0x18||word(n+8)!=cp||word(n+0xc)!=recording_track[at]||!recording_event(at,n+0x18)){trace_fail(C_TRACE_AMBIGUOUS);return;}
   for(unsigned k=0;k<14;k++)recording_timed_event[at][k]=word(n+0x18+k*4);
   event(c,CE_RECORD_TIME,accepted[at].request.seq,recording_timed_event[at][0],recording_timed_event[at][1],accepted[at].program,command_arg_kind(&accepted[at].request),accepted[at].controller,accepted[at].native_bits,word(n+0x10),accepted[at].request.epoch);
   recording_emit(c,CE_RECORD_WORK,at,n,cp+0x18);atomic_store_explicit(recording_work+at,n,memory_order_release);
  }return;
 }
 if(site==RC_ENTER||site==RC_SECOND_ENTER){
  uint32_t n=c->r[0];for(unsigned i=0;i<COMMAND_SLOTS;i++)if(source_requests[i]&&n==atomic_load_explicit((site==RC_ENTER?recording_payload:recording_work)+i,memory_order_acquire)){if(at!=COMMAND_SLOTS){trace_fail(C_TRACE_AMBIGUOUS);return;}at=i;}
 }else at=command_find(command_state,site==RC_SECOND_RETURN||site==RC_SECOND_DONE?recording_second.id:recording_call.id);
 if(at==COMMAND_SLOTS)return;
 unsigned id=accepted[at].request.seq;uint32_t n=atomic_load(recording_payload+at),w=atomic_load(recording_work+at),f=recording_factory[at],cp=atomic_load(recording_cp+at);
 if(source_requests[at]!=id||!recording_current(at)){trace_fail(C_IDENTITY);return;}
 if(site==RC_ENTER){
  if(recording_call.id||word(n)!=f+4||word(n+8)!=f||word(n+0x14)!=accepted[at].program||word(n+0x50)!=recording_track[at]||!recording_event(at,n+0x18)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  recording_call=(RecordingCall){id,n,source_sp(c),1,0,0};recording_emit(c,CE_RECORD_ENTER,at,n,f+4);
 }else if(site==RC_APPLY){
  if(recording_call.phase!=1||c->lr!=observer_image_bias+0x1c04b94||c->r[0]!=accepted[at].program||!recording_event(at,c->r[2])){trace_fail(C_TRACE_AMBIGUOUS);return;}
  recording_call.phase=2;effects_native_body(at);recording_emit(c,CE_RECORD_APPLY,at,n,f+4);
 }else if(site==RC_PRODUCER){
  if(recording_call.phase!=1||recording_call.producer||c->lr!=observer_image_bias+0x1c04b60||!recording_event(at,c->r[2])||!typed(c->r[0],0x689fe14)||word(c->r[0]+0x80)!=recording_track[at]||word(c->r[0]+0x18)!=command_queues){trace_fail(C_TRACE_AMBIGUOUS);return;}
  recording_call.producer=c->r[0];recording_call.producer_sp=source_sp(c);atomic_store_explicit(recording_cp+at,c->r[0],memory_order_release);
 }else if(site==RC_ENQUEUER){
  uint32_t q=c->r[0];
  if(!cp||c->r[4]!=cp||c->r[6]!=cp+0x1c||c->r[8]!=cp+0x18||!pointer(q)||!pointer(word(q))||word(word(q)+8)!=observer_image_bias+0x28c4078){trace_fail(C_TRACE_AMBIGUOUS);return;}
  recording_queue[at]=q+4;
 }else if(site==RC_ENQUEUE_RETURN){
  if(!cp||c->r[0]>1||(c->r[0]&&!atomic_load(recording_work+at))||(!c->r[0]&&atomic_load(recording_work+at))){trace_fail(C_TRACE_AMBIGUOUS);return;}
 }else if(site==RC_DONE){
  if(c->r[4]!=n||source_sp(c)+0x18!=recording_call.sp){trace_fail(C_TRACE_AMBIGUOUS);return;}
  recording_emit(c,CE_RECORD_DONE,at,n,f+4);memset(&recording_call,0,sizeof(recording_call));atomic_store_explicit(recording_first_done+at,1,memory_order_release);recording_complete(at);
 }else if(site==RC_SECOND_ENTER){
  if(recording_second.id||word(w)!=cp+0x18||word(w+8)!=cp||word(w+0xc)!=recording_track[at]||!recording_event(at,w+0x18)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  for(unsigned k=0;k<14;k++)if(word(w+0x18+k*4)!=recording_timed_event[at][k]){trace_fail(C_TRACE_AMBIGUOUS);return;}
  recording_second=(RecordingCall){id,w,source_sp(c),1,0,0};recording_emit(c,CE_RECORD_WORK_ENTER,at,w,cp+0x18);
 }else if(site==RC_SECOND_RETURN){
  if(c->r[4]!=w||recording_second.phase!=1||source_sp(c)+0x30!=recording_second.sp){trace_fail(C_TRACE_AMBIGUOUS);return;}
  recording_second.phase=2;recording_emit(c,CE_RECORD_HANDLED,at,w,cp+0x18);
 }else if(site==RC_SECOND_DONE){
  if(c->r[4]!=w||recording_second.phase!=2||source_sp(c)+0x30!=recording_second.sp){trace_fail(C_TRACE_AMBIGUOUS);return;}
  recording_emit(c,CE_RECORD_WORK_DONE,at,w,cp+0x18);memset(&recording_second,0,sizeof(recording_second));atomic_store_explicit(recording_second_done+at,1,memory_order_release);recording_complete(at);
 }
}
/* The native GUI operation addresses a slot. Capture the receiver selected by
 * MPC at execution, keeping submission identity distinct from this observation. */
static void recording_pad_execute(Context *c){
 /* The same GUI facade can choose history -> Program queue (ordinary Read)
  * or the recording callback. Both resolve the native pad slot at execution. */
 unsigned request=recording_call.id?recording_call.id:invocation.request;
 unsigned at=command_find(command_state,request);
 if(at==COMMAND_SLOTS||!accepted[at].request.pad_owner)return;
 if((recording_call.id&&recording_call.phase!=2)||(!recording_call.id&&invocation.phase!=2)){trace_fail(C_TRACE_AMBIGUOUS);return;}
 if(recording_call.id&&invocation.request){trace_fail(C_TRACE_AMBIGUOUS);return;}
 const CommandRequest *r=&accepted[at].request;PadOwner *p=pad_owner(c->r[0]);PadParent *parent=pad_parent(accepted[at].program,0);
 unsigned f=pad_field(command_source_field(r->reserved));
 if(!recording_current(at)||atomic_load(&mirror_state->pads.error)||!p||!typed(c->r[0],0x692f650)||!parent||f==PF_COUNT||!atomic_load(&p->live)||!atomic_load(&p->complete)||atomic_load(&parent->revision)&1||atomic_load(&p->parent_owner)!=r->program_owner||atomic_load(&p->parent)!=accepted[at].program||atomic_load(&parent->slots[r->pad_index].owner)!=atomic_load(&p->incarnation)||c->r[1]!=accepted[at].controller||(uint32_t)c->d[0]!=accepted[at].native_bits||atomic_load(recording_pad_owner+at)){trace_fail(C_TRACE_AMBIGUOUS);return;}
 PadScalar *v=p->fields+f;unsigned rev=atomic_load_explicit(&v->revision,memory_order_acquire),bits=atomic_load(&v->bits);
 if(!atomic_load(&v->live)||!atomic_load(&v->seed)||(rev&1)){trace_fail(C_TRACE_AMBIGUOUS);return;}
 unsigned id=atomic_load(&p->incarnation),property=c->r[0]+pad_offset(f);
 atomic_store(recording_pad_property+at,property);atomic_store_explicit(recording_pad_owner+at,id,memory_order_release);
 event(c,CE_PAD_EXECUTE,r->seq,c->r[0],atomic_load(&parent->slots[r->pad_index].generation),accepted[at].program,r->pad_index,accepted[at].controller,id,bits,rev);
}
static int recording_commit(Context *c,unsigned at,unsigned site){
 if(recording_call.id!=accepted[at].request.seq||recording_call.phase!=2)return 0;
 if(!recording_current(at)){trace_fail(C_IDENTITY);return 1;}
 const CommandRequest *r=&accepted[at].request;uint32_t rev=0;
 if(r->pad_owner){unsigned f;PadOwner *p=pad_property(c->r[5],&f);if(p&&f<PF_COUNT&&!atomic_load(&mirror_state->pads.error)&&atomic_load(&p->live)&&atomic_load(&p->incarnation)==atomic_load(recording_pad_owner+at))rev=atomic_load(&p->fields[f].revision);}
 else if(!r->reserved){MirrorCell *cell=lookup(c->r[5]);if(cell&&atomic_load(&cell->live)&&atomic_load(&cell->incarnation)==r->incarnation)rev=atomic_load(&cell->revision);}
 else{ChannelCell *cell=channel_cell(c->r[5]);if(cell&&atomic_load(&cell->live)&&atomic_load(&cell->incarnation)==r->field_incarnation)rev=atomic_load(&cell->revision);}
 if(!rev)trace_fail(C_TRACE_AMBIGUOUS);
 else event(c,CE_RECORD_COMMIT,r->seq,recording_call.payload,recording_factory[at]+4,accepted[at].program,command_arg_kind(&accepted[at].request),accepted[at].controller,site==M_FLOAT?(uint32_t)c->d[8]:command_native_bits(r->reserved,c->r[7]&255),c->r[5],rev);
 return 1;
}
