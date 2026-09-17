/* MIDI Track level uses the GUI Track-parameter factory, not Program gain or
 * the downstream CC cache. Transient receipts share the existing command owner. */
static _Atomic uint32_t midi_payload[COMMAND_SLOTS],midi_branch[COMMAND_SLOTS];
static __thread uint32_t midi_submission __attribute__((tls_model("initial-exec")));
static __thread struct {uint32_t id,payload,sp,branch,set,commit;} midi_call __attribute__((tls_model("initial-exec")));
static void midi_initialize(void){midi_submission=0;memset(&midi_call,0,sizeof(midi_call));for(unsigned i=0;i<COMMAND_SLOTS;i++){atomic_store(midi_payload+i,0);atomic_store(midi_branch+i,0);}}
static int midi_current(unsigned at){
 const CommandRequest *r=&accepted[at].request;ChannelCell *c=channel_cell(accepted[at].track+0x28c);
 return recording_current(at)&&r->reserved==CF_MIDI_VOLUME&&typed(accepted[at].track,0x6935554)&&typed(accepted[at].program,0x6932338)&&word(accepted[at].track+0x648)==accepted[at].program&&channel_owner_id(accepted[at].track)==r->track_owner&&c&&atomic_load(&c->live)&&atomic_load(&c->field)==CF_MIDI_VOLUME&&atomic_load(&c->incarnation)==r->field_incarnation&&atomic_load(&c->owner_incarnation)==r->track_owner&&!channel_fault(atomic_load(&mirror_state->channel.errors[CF_MIDI_VOLUME]));
}
/* Native240274c builds controller events with tag1 (ORR1 at2402778),
 * distinct from the Program parameter event family accepted by recording. */
static int midi_event_at(unsigned at,uint32_t ev){
 return pointer(ev)&&ev<=UINT32_MAX-0x38&&*(const unsigned char*)(uintptr_t)(ev+8)==1&&word(ev+0x10)==0x100&&word(ev+0x14)==accepted[at].native_bits&&word(ev+0x18)==7;
}
#ifdef COMMAND_COMPONENT
static void (*component_midi_dispatch)(uint32_t,uint32_t,unsigned,uint32_t,uint32_t,uint32_t,float);
#endif
static void midi_dispatch(unsigned at,float value){
 atomic_store(midi_payload+at,0);atomic_store(midi_branch+at,0);midi_submission=accepted[at].request.seq;
#ifdef COMMAND_COMPONENT
 component_midi_dispatch(recording_factory[at],accepted[at].track,7,0,0,0,value);
#else
 ((void (*)(void*,void*,unsigned,uint32_t,uint32_t,uint32_t,float))(uintptr_t)(observer_image_bias+0x13d5fd0))((void*)(uintptr_t)recording_factory[at],(void*)(uintptr_t)accepted[at].track,7,0,0,0,value);
#endif
 midi_submission=0;
}
static int midi_publication(Context *c){
 if(!midi_submission)return 0;
 uint32_t h=c->r[10];if(!pointer(h)||h>UINT32_MAX-8)return 0;
 uint32_t inv=word(h),mgr=word(h+4);
 return inv==observer_image_bias+0x265415c||mgr==observer_image_bias+0x2649de8||inv==observer_image_bias+0x13cfa14||mgr==observer_image_bias+0x13d1950;
}
static void midi_emit(Context *c,unsigned kind,unsigned at,uint32_t bits,uint32_t revision){
 unsigned branch=atomic_load(midi_branch+at);uint32_t key=branch==1?accepted[at].track+0x4d4:recording_factory[at]+4;
 event(c,kind,accepted[at].request.seq,atomic_load(midi_payload+at),key,accepted[at].track,0x100,7,bits,accepted[at].property,revision);
}
static void midi_point(Context *c,unsigned site){
 unsigned at=COMMAND_SLOTS;
 if(site==JG_PUBLISH){
  at=command_find(command_state,midi_submission);if(at==COMMAND_SLOTS||!midi_current(at)){trace_fail(C_IDENTITY);return;}
  uint32_t h=c->r[10],n=h+8,track=accepted[at].track,f=recording_factory[at];unsigned ordinary=word(h)==observer_image_bias+0x265415c;
  uint32_t key=ordinary?track+0x4d4:f+4,ev=n+(ordinary?8:0x10);
  if(source_requests[at]!=midi_submission||token()!=command_owner||!atomic_load(&command_in_hook)||atomic_load(midi_payload+at)||h>UINT32_MAX-0x58||word(h)!=observer_image_bias+(ordinary?0x265415c:0x13cfa14)||word(h+4)!=observer_image_bias+(ordinary?0x2649de8:0x13d1950)||c->r[7]!=command_queues+0xfc||c->r[9]!=(key|1u)||word(n)!=key||word(n+(ordinary?0x40:0x48))!=track||(!ordinary&&word(n+8)!=f)||!midi_event_at(at,ev)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  atomic_store(midi_branch+at,ordinary?1:2);atomic_store_explicit(midi_payload+at,n,memory_order_release);
  midi_emit(c,CE_MIDI_QUEUE,at,accepted[at].native_bits,ordinary?1:2);return;
 }
 if(site==MI_ENTER||site==MI_AUTO_ENTER){
  for(unsigned i=0;i<COMMAND_SLOTS;i++)if(source_requests[i]&&c->r[0]==atomic_load_explicit(midi_payload+i,memory_order_acquire)){if(at!=COMMAND_SLOTS){trace_fail(C_TRACE_AMBIGUOUS);return;}at=i;}
 }else at=command_find(command_state,midi_call.id);
 if(at==COMMAND_SLOTS)return;
 unsigned id=accepted[at].request.seq,branch=atomic_load(midi_branch+at);uint32_t n=atomic_load(midi_payload+at),track=accepted[at].track,key=branch==1?track+0x4d4:recording_factory[at]+4;
 if(source_requests[at]!=id||!midi_current(at)){trace_fail(C_IDENTITY);return;}
 if(site==MI_ENTER||site==MI_AUTO_ENTER){
  if(midi_call.id||branch!=(site==MI_ENTER?1u:2u)||word(n)!=key||word(n+(branch==1?0x40:0x48))!=track||!midi_event_at(at,n+(branch==1?8:0x10))){trace_fail(C_TRACE_AMBIGUOUS);return;}
  midi_call.id=id;midi_call.payload=n;midi_call.sp=source_sp(c);midi_call.branch=branch;midi_call.set=midi_call.commit=0;
  midi_emit(c,CE_MIDI_ENTER,at,accepted[at].native_bits,branch);
 }else if(site==MI_SET){
  /* Ignored as foreign traffic, on the same grounds as the effects lane: with
   * automation Write armed the application writes this Track level through its
   * own record path inside this window. Another receiver, another call site,
   * another value, or a repeat after this command's own call was receipted is
   * proof the call is not this command's. Ignoring emits no receipt; if this
   * command's own call never arrives it completes as a no-set command. */
  if(c->r[0]!=accepted[at].property||c->lr!=observer_image_bias+0x264d478||(uint32_t)c->d[0]!=accepted[at].native_bits||midi_call.set)return;
  /* No NULL test here is safe only while channel_birth and this trace both run
   * on the command-owner thread, so no reclaim can interleave inside one gesture
   * and a live key is never transiently unresolvable. Any future directory
   * compaction breaks that, so guarding this is a prerequisite of that work. */
  ChannelCell *v=channel_cell(c->r[0]);unsigned rev=atomic_load(&v->revision),bits=atomic_load(&v->bits);if(!rev||(rev&1)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  midi_call.set=1;midi_emit(c,CE_MIDI_SET,at,bits,rev);
 }else{
  if((site!=MI_DONE&&site!=MI_AUTO_DONE)||branch!=(site==MI_DONE?1u:2u)||c->r[4]!=n||source_sp(c)+(branch==1?8:0x18)!=midi_call.sp||c->r[3]!=key+4){trace_fail(C_TRACE_AMBIGUOUS);return;}
  midi_emit(c,CE_MIDI_DONE,at,accepted[at].native_bits,branch);memset(&midi_call,0,sizeof(midi_call));
  atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&command_state->slots[at].done,id,memory_order_release);
 }
}
static int midi_commit(Context *c,unsigned at){
 if(midi_call.id!=accepted[at].request.seq)return 0;
 /* Another value written to this property, or a repeat after this command's own
  * commit was receipted, is another writer's: consumed without a receipt. An
  * unreceipted set or a lost field identity remains a fault. */
 if(midi_call.commit||(uint32_t)c->d[8]!=accepted[at].native_bits)return 1;
 if(!midi_current(at)||!midi_call.set||c->r[5]!=accepted[at].property){trace_fail(C_TRACE_AMBIGUOUS);return 1;}
 ChannelCell *v=channel_cell(c->r[5]);unsigned rev=atomic_load(&v->revision);if(!rev||(rev&1)){trace_fail(C_TRACE_AMBIGUOUS);return 1;}
 midi_call.commit=1;midi_emit(c,CE_MIDI_COMMIT,at,(uint32_t)c->d[8],rev);return 1;
}
