/* Optional register-only diagnostic; no object dereference or native calls.
 * Source hook lifetime still uses trace_enter/leave. Frames admitted before
 * expiry close afterward, including nested non-recorded calls. */
typedef struct {uint32_t key,sp,generation,record;} ProcessorGetFrame;
static __thread ProcessorGetFrame processor_gets[4] __attribute__((tls_model("initial-exec")));
static __thread unsigned processor_get_depth __attribute__((tls_model("initial-exec")));
static __thread unsigned processor_get_overflow __attribute__((tls_model("initial-exec")));
static __thread unsigned processor_get_lane __attribute__((tls_model("initial-exec")));
static int processor_observation_active(void){
 uint32_t until=atomic_load_explicit(&command_state->processor_until,memory_order_acquire);
 if(!until||atomic_load(&mirror_state->heartbeat)>=until)return 0;
 uint32_t me=token();for(unsigned i=0;i<COMMAND_LANES;i++)if(atomic_load(&command_state->lane_tokens[i])==me)return 1;return 0;
}
static int processor_observation_wanted(unsigned site){
 if(site==PX_GET_DONE)return processor_get_depth||processor_get_overflow;
 return (site==PX_GET&&(processor_get_depth||processor_get_overflow))||processor_observation_active();
}
static void processor_observe(Context *c,unsigned site,unsigned source_lane){
 if(source_lane>=COMMAND_LANES&&!processor_get_depth&&!processor_get_overflow)return;
 unsigned lane=source_lane<COMMAND_LANES?source_lane:processor_get_lane;
 uint32_t generation=source_lane<COMMAND_LANES?processor_enter(command_state,lane):0;
 int entered=generation!=0;
 uint32_t filter=entered?atomic_load(&command_state->processor_key):0,key=c->r[0],paired=0;
 unsigned record=entered&&generation&&!(generation&1)&&processor_observation_active(),closing=0;
 if(site==PX_LISTENER)key=c->r[0]>=0x50?c->r[0]-0x50:0;
 if(site==PX_NOTIFY)key=c->r[0]>=4?c->r[0]-4:0; /* observed E+4; raw r0 retained */
 /* Cross-object lifecycle correlation requires discover: keys remain opaque.
  * Retirement r0 is a closure, ctor r0 is W; original r0..r3 stay in record. */
 if(site==PX_RETIRE)key=c->r[1];
 if(site==PX_WIDGET_CTOR)key=c->r[2]; /* W,Q,E,index; W+70 receives entry r2 */
 if(site==PX_UI_VECTOR)key=c->r[0]>=0x50?c->r[0]-0x50:0;
 if(site==PX_GET){
  if(!record&&!processor_get_depth&&!processor_get_overflow)goto done;
  processor_get_lane=lane;
  if(processor_get_depth==4||processor_get_overflow){processor_get_overflow++;processor_lost(command_state,lane,PX_LOSS_FRAME);goto done;}
  processor_gets[processor_get_depth++]=(ProcessorGetFrame){key,source_sp(c),generation,record&&(!filter||key==filter)};
 }else if(site==PX_GET_DONE){
  closing=1;
  if(processor_get_overflow){processor_get_overflow--;record=0;goto done;}
  if(!processor_get_depth){closing=0;record=0;goto done;}
  ProcessorGetFrame f=processor_gets[--processor_get_depth];key=f.key;
  paired=f.sp==source_sp(c)+40&&f.generation==generation;
  record=entered&&f.record&&paired;
  if(f.record&&!record)processor_lost(command_state,lane,PX_LOSS_FRAME);
 }
 if(record&&(!filter||key==filter)){
  uint32_t words[PROCESSOR_WORDS]={generation,site,atomic_load(&mirror_state->heartbeat),token(),source_sp(c),c->lr,key,c->r[0],c->r[1],c->r[2],c->r[3],(uint32_t)c->d[0],paired};
  (void)processor_publish(command_state,lane,words);
 }
 done:
 /* The lane alone owns its open count. A new generation cannot pass an
  * open frame or an admitted writer; failed diagnostic admission still closes. */
 (void)closing;
 if(lane<COMMAND_LANES)atomic_store(command_state->processor_open+lane,processor_get_depth+processor_get_overflow);
 if(entered)processor_leave(command_state,lane);
 else if(source_lane<COMMAND_LANES)processor_lost(command_state,lane,PX_LOSS_WRITER);
}
