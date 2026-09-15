/* Lease-bound Q-Link UI projection on the existing owner. Mode-origin raw
 * witnesses and presentation dirtiness never retain native callable objects. */
#include "native-name.h"
static QLinkCopy qlink_current;
static _Atomic uint32_t qlink_mode_dead,qlink_mode_hook;
static unsigned qlink_mode_key,qlink_mode_root,qlink_mode_generation,qlink_mode_revision,qlink_mode_last;
static unsigned qlink_cursor,qlink_next_tick,qlink_generation,qlink_reading,qlink_semantic_seen,qlink_page_seen;
enum {QS_DESCRIPTOR=191,QS_CONFIG,QS_TARGETS,QS_GROUPS,QS_MODE_REQUEST,QS_MODE_APPLY,QS_MODE_VALUE,QS_CACHE_ENTER,QS_CACHE_DONE,QJ_VALUE_FANOUT,QS_LABELS};
typedef struct {_Atomic uint32_t revision,generation,raw,tick;} QLinkWitness;
static QLinkWitness qlink_witness[COMMAND_LANES][QLINK_SLOTS];
static _Atomic uint32_t qlink_semantic[COMMAND_LANES],qlink_hook_provider,qlink_hook_queue,qlink_hook_generation,qlink_hook_labels;
static _Atomic uint32_t qlink_labels_dirty[COMMAND_LANES][QLINK_SLOTS];
static unsigned qlink_read_seen[QLINK_SLOTS],qlink_label_seen[QLINK_SLOTS];
typedef struct {uint32_t provider,index,sp,lr,generation,mode;} QLinkRefresh;
static __thread QLinkRefresh qlink_marker __attribute__((tls_model("initial-exec")));
static __thread QLinkRefresh qlink_refresh[8] __attribute__((tls_model("initial-exec")));
static __thread unsigned qlink_refresh_depth __attribute__((tls_model("initial-exec")));
static unsigned qlink_semantic_revision(void){unsigned total=0;for(unsigned i=0;i<COMMAND_LANES;i++)total+=atomic_load_explicit(qlink_semantic+i,memory_order_acquire);return total;}
static unsigned qlink_labels_revision(unsigned index){unsigned total=0;for(unsigned i=0;i<COMMAND_LANES;i++)total+=atomic_load_explicit(&qlink_labels_dirty[i][index],memory_order_acquire);return total;}
static unsigned qlink_refresh_revision(unsigned index){unsigned total=0;for(unsigned i=0;i<COMMAND_LANES;i++)total+=atomic_load_explicit(&qlink_witness[i][index].revision,memory_order_acquire);return total;}

static void qlink_initialize(void){atomic_store(&qlink_mode_dead,0);atomic_store(&qlink_mode_hook,0);qlink_mode_key=qlink_mode_root=qlink_mode_generation=qlink_mode_revision=0;qlink_mode_last=UINT32_MAX;memset(&qlink_current,0,sizeof(qlink_current));qlink_cursor=qlink_next_tick=qlink_generation=qlink_reading=qlink_semantic_seen=0;qlink_page_seen=3;memset(qlink_witness,0,sizeof(qlink_witness));memset(qlink_semantic,0,sizeof(qlink_semantic));memset(qlink_labels_dirty,0,sizeof(qlink_labels_dirty));memset(qlink_read_seen,0,sizeof(qlink_read_seen));memset(qlink_label_seen,0,sizeof(qlink_label_seen));atomic_store(&qlink_hook_labels,0);atomic_store(&qlink_hook_provider,0);atomic_store(&qlink_hook_queue,0);atomic_store(&qlink_hook_generation,0);qlink_refresh_depth=0;memset(&qlink_marker,0,sizeof(qlink_marker));}
static void qlink_publish(void){
 QLinkState *s=&mirror_state->qlinks;unsigned rev=atomic_load(&s->revision);if(rev>UINT32_MAX-2){atomic_store(&s->error,QL_INVALID);return;}
 uint32_t words[QLINK_WORDS];memcpy(words,&qlink_current,sizeof(words));atomic_store(&s->revision,rev+1);
 for(unsigned i=0;i<QLINK_WORDS;i++)atomic_store_explicit(s->words+i,words[i],memory_order_relaxed);
 atomic_store_explicit(&s->revision,rev+2,memory_order_release);
}
static void qlink_lifecycle(unsigned site){
 if(site!=M_RESET&&site!=M_ROOT_DESTROY&&site!=M_PROJECT_DESTROY&&site!=M_PROJECT_CLEAR&&site!=M_POOL_CLEAR&&site!=M_POOL_DESTROY)return;
 /* Revocation is independent of a lease, so retained data cannot survive a
  * project transition. Only atomics here; no foreign native dereference. */
 QLinkState *s=&mirror_state->qlinks;
 if(token()!=command_owner)atomic_store(&s->error,QL_FOREIGN);
 else if(qlink_reading)atomic_store(&s->error,QL_REENTRANT);
 atomic_store_explicit(&s->invalid,1,memory_order_release);
}
typedef struct {uint32_t root,controls,queue,wrapper,provider,labels,mode,sources,descriptors,cache,strings,signals[3];} QLinkNative;
static int qlink_span(uint32_t p,unsigned bytes){return pointer(p)&&p<=UINT32_MAX-bytes;}
static int qlink_resolve(Context *c,const QLinkDemand *d,QLinkNative *n){
 memset(n,0,sizeof(*n));
 if(!d->enabled||d->epoch!=epoch||token()!=command_owner||!ready||loading||reset_pending||depth||!active()||!root||!project||!command_queues||c->r[0]!=command_queues+4||word(c->r[0]+0xf0)||atomic_load(&command_state->new_project_intent)||atomic_load(&command_state->stop_requested))return 0;
 if(!typed(root,0x68d7bf8)||word(root+0x24c)!=project||!typed(project,0x6933c4c))return 0;
 n->root=root;n->controls=word(root+0x388);n->queue=word(root+0xf8);n->wrapper=word(root+0x540);
 if(!qlink_span(n->controls,0x3c)||!qlink_span(n->wrapper,12)||n->queue!=command_queues)return 0;
 n->provider=word(n->controls+0x30);n->labels=word(n->controls+0x38);n->mode=word(n->controls+0x2c);
 if(!qlink_span(n->provider,0x38)||!qlink_span(n->labels,0x20)||!typed(n->mode,0x68962d8)||!qlink_span(n->mode,0xc8)||word(n->wrapper)!=n->queue||word(n->wrapper+8)!=n->provider||word(n->provider+0x28)!=QLINK_SLOTS)return 0;
 if(word(n->labels+0x10)!=QLINK_SLOTS||word(n->labels+0x18)!=QLINK_SLOTS||word(n->labels+0x1c)!=QLINK_SLOTS)return 0;
 n->sources=word(n->provider+0x2c);n->descriptors=word(n->provider+0x30);n->cache=word(n->provider+0x34);n->strings=word(n->labels+0x14);
 if(!qlink_span(n->sources,QLINK_SLOTS*0x84)||!qlink_span(n->descriptors,QLINK_SLOTS*0x68)||!qlink_span(n->cache,QLINK_SLOTS*4)||!qlink_span(n->strings,QLINK_SLOTS*12))return 0;
 n->signals[0]=word(n->mode+0x68);n->signals[1]=word(n->mode+0x8c);n->signals[2]=word(n->mode+0xc4);return 1;
}
/* QLinkModeController is a different root-owned object from ACV::QLinks T.
 * Only this existing qualified UI drain resolves and samples its scalar state. */
static int qlink_mode_resolve(Context *c,unsigned wanted_epoch,uint32_t *controller,unsigned *id,unsigned *count){
 QLinkNative n;QLinkDemand d={.enabled=1,.epoch=wanted_epoch};if(!qlink_resolve(c,&d,&n))return 0;
 uint32_t owner=word(root+0x750);if(!qlink_span(owner,0xac))return 0;
 uint32_t k=word(owner+0xa8);if(!typed(k,0x6897870)||!qlink_span(k,0x548)||word(k+8)!=command_queues||(atomic_load_explicit(&qlink_mode_dead,memory_order_acquire)==k&&qlink_mode_root==root))return 0;
 uint32_t capability=word(k+0x524);if(!qlink_span(capability,0x2c))return 0;
 unsigned h=word(capability+0x28),current=word(k+0x5c),available=h==14||h==16?15:14;
 if(current>=available||word(root+0x750)!=owner||word(owner+0xa8)!=k||word(k+8)!=command_queues||word(k+0x524)!=capability||word(capability+0x28)!=h||word(k+0x5c)!=current)return 0;
 *controller=k;*id=current;*count=available;return 1;
}
static int qlink_mode_sample(Context *c,unsigned now){
 uint32_t k=0;unsigned id=0,count=0;unsigned old_valid=qlink_current.mode_valid;
 qlink_current.mode_valid=0;
 if(!qlink_mode_resolve(c,epoch,&k,&id,&count))return old_valid!=0;
 if(k!=qlink_mode_key||root!=qlink_mode_root){if(qlink_mode_generation==UINT32_MAX)return old_valid!=0;qlink_mode_key=k;qlink_mode_root=root;qlink_mode_generation++;qlink_mode_last=UINT32_MAX;}
 unsigned changed=!old_valid||qlink_current.mode_controller!=k||qlink_current.mode_generation!=qlink_mode_generation||qlink_current.mode_count!=count||qlink_mode_last!=id;
 if(qlink_mode_last!=id){if(qlink_mode_revision==UINT32_MAX)return old_valid!=0;qlink_mode_revision++;qlink_mode_last=id;}
 qlink_current.mode_controller=k;qlink_current.mode_generation=qlink_mode_generation;qlink_current.mode_id=id;qlink_current.mode_count=count;qlink_current.mode_revision=qlink_mode_revision;qlink_current.mode_tick=now;qlink_current.mode_valid=1;atomic_store_explicit(&qlink_mode_hook,k,memory_order_release);return changed;
}
static int qlink_string(uint32_t p,char out[QLINK_TEXT],uint32_t *status){
 unsigned char bytes[NAME_BYTES]={0};uint32_t length=0;memset(out,0,QLINK_TEXT);
 if(!copy_name(p,bytes,&length,status))return 0;
 if(length>=QLINK_TEXT){length=QLINK_TEXT-1;while(length&&(bytes[length]&0xc0)==0x80)length--;*status=2;}
 memcpy(out,bytes,length);return 1; /* Borrowed native String: no retain/release. */
}
/* Enrolled lanes reuse the observer's complete hook lifetime. An unsupported
 * lane/reentrant source disables this projection, never musical commands. */
static void qlink_source(Context *c,unsigned site,unsigned lane){
 unsigned provider=atomic_load_explicit(&qlink_hook_provider,memory_order_acquire),queue=atomic_load(&qlink_hook_queue),generation=atomic_load(&qlink_hook_generation);
 if(site==QS_LABELS){unsigned index=word(source_sp(c)+4);if(c->r[5]==atomic_load(&qlink_hook_labels)&&index<QLINK_SLOTS){unsigned rev=atomic_load(&qlink_labels_dirty[lane][index]);if(rev>=UINT32_MAX/COMMAND_LANES-1)atomic_store(&mirror_state->qlinks.error,QL_INVALID);else atomic_store_explicit(&qlink_labels_dirty[lane][index],rev+1,memory_order_release);}return;}
 if(site==QS_MODE_VALUE){
  memset(&qlink_marker,0,sizeof(qlink_marker));
  if(provider&&qlink_span(c->r[0],0x54)&&word(c->r[0]+0x50)==provider&&word(c->r[0]+0x40)==queue)qlink_marker=(QLinkRefresh){provider,c->r[1],source_sp(c),c->lr,generation,1};
  return;
 }
 if(site==QS_CACHE_ENTER){
  QLinkRefresh marker=qlink_marker;memset(&qlink_marker,0,sizeof(qlink_marker));
  if(!provider||c->r[0]!=provider||c->r[1]>=QLINK_SLOTS)return;
  if(qlink_refresh_depth==8){atomic_store(&mirror_state->qlinks.error,QL_REENTRANT);return;}
  unsigned mode=marker.provider==provider&&marker.index==c->r[1]&&marker.sp==source_sp(c)&&marker.lr==c->lr&&marker.generation==generation;
  qlink_refresh[qlink_refresh_depth++]=(QLinkRefresh){provider,c->r[1],source_sp(c),c->lr,generation,mode};return;
 }
 if(site==QS_CACHE_DONE){
  if(!qlink_refresh_depth)return;
  QLinkRefresh *f=qlink_refresh+qlink_refresh_depth-1;unsigned sp=source_sp(c);
  if(f->sp!=sp+0xe0)return; /* Unadmitted nested provider invocation. */
  QLinkRefresh frame=*f;qlink_refresh_depth--;
  if(word(sp+0xdc)!=frame.lr||word(sp+0xc)!=frame.index){atomic_store(&mirror_state->qlinks.error,QL_INVALID);return;}
  if(!frame.mode||frame.provider!=provider||frame.generation!=generation||atomic_load(&mirror_state->qlinks.invalid))return;
  QLinkWitness *w=&qlink_witness[lane][frame.index];unsigned rev=atomic_load(&w->revision);
  if(rev>=UINT32_MAX/COMMAND_LANES-2){atomic_store(&mirror_state->qlinks.error,QL_INVALID);return;}
  atomic_store(&w->revision,rev+1);atomic_store(&w->generation,generation);atomic_store(&w->raw,word(sp+8));atomic_store(&w->tick,atomic_load(&mirror_state->heartbeat));atomic_store_explicit(&w->revision,rev+2,memory_order_release);return;
 }
 unsigned match=0;
 if(site==QS_DESCRIPTOR)match=provider&&c->r[4]==provider&&c->r[5]<QLINK_SLOTS;
 else if(site==QS_CONFIG)match=provider&&c->r[0]==provider;
 else if(site==QS_TARGETS)match=queue&&qlink_span(c->r[2],0xfc)&&word(c->r[2]+0xf8)==queue;
 else if(site==QS_GROUPS)match=queue&&qlink_span(c->r[2],0x74)&&word(c->r[2]+0x70)==queue;
 else if(site==QS_MODE_REQUEST)match=provider&&qlink_span(c->r[0],0x54)&&word(c->r[0]+0x40)==queue&&word(c->r[0]+0x50)==provider;
 else if(site==QS_MODE_APPLY){unsigned m=qlink_span(c->r[0],12)?word(c->r[0]+4):0;match=provider&&qlink_span(m,0x54)&&word(m+0x40)==queue&&word(m+0x50)==provider;}
 if(match){unsigned rev=atomic_load(qlink_semantic+lane);if(rev>=UINT32_MAX/COMMAND_LANES-1){atomic_store(&mirror_state->qlinks.error,QL_INVALID);return;}atomic_store_explicit(qlink_semantic+lane,rev+1,memory_order_release);atomic_store_explicit(&mirror_state->qlinks.invalid,1,memory_order_release);}
}
/* Find a stable completed mode-origin raw publication equal to the current
 * native cache. Equality is current value agreement, not unique DSP causation. */
static int qlink_witness_read(unsigned index,unsigned generation,unsigned raw,unsigned *revision,unsigned *tick,unsigned matches[COMMAND_LANES]){
 unsigned total=0,found=0,last=0;
 for(unsigned lane=0;lane<COMMAND_LANES;lane++){
  const QLinkWitness *w=&qlink_witness[lane][index];unsigned rev=atomic_load_explicit(&w->revision,memory_order_acquire);if(rev&1)return 0;
  unsigned gen=atomic_load(&w->generation),bits=atomic_load(&w->raw),t=atomic_load(&w->tick);atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load(&w->revision))return 0;
  total+=rev;matches[lane]=0;if(rev&&gen==generation&&bits==raw){matches[lane]=rev;found=1;if(t>last)last=t;}
 }
 if(found){*revision=total;*tick=last;}return found;
}
static int qlink_sample(Context *c,const QLinkDemand *d,const QLinkNative *n,unsigned i,unsigned now){
 QLinkState *s=&mirror_state->qlinks;QLinkSlot slot={.index=i};uint32_t record=n->strings+12*i,name=word(record+4),text=word(record+8),raw=word(n->cache+4*i),sem=qlink_semantic_revision();
 qlink_reading=1;slot.kind=word(record);slot.gesture_activity=*(const volatile unsigned char*)(uintptr_t)(n->sources+0x84*i+0x28);
 int strings=qlink_string(name,slot.name,&slot.name_status)&&qlink_string(text,slot.text,&slot.text_status);
 unsigned witness=qlink_witness_read(i,qlink_current.generation,raw,&slot.refresh_revision,&slot.refresh_tick,slot.refresh_matches);
 float value=((float (*)(void*,unsigned))(uintptr_t)(observer_image_bias+0x14ddc94))((void*)(uintptr_t)n->wrapper,i);qlink_current.reads++;
 QLinkNative after;int same=!atomic_load(&s->invalid)&&!atomic_load(&s->error)&&sem==qlink_semantic_revision()&&qlink_resolve(c,d,&after)&&!memcmp(n,&after,sizeof(*n));
 qlink_reading=0;if(!same){atomic_store(&s->invalid,1);return 0;}
 if(word(record+4)!=name||word(record+8)!=text||word(record)!=slot.kind||word(n->cache+4*i)!=raw)return 2; /* Transient value/text race; retry without remapping. */
 QLinkSlot *old=qlink_current.slots+i;
 if(strings){slot.sampled=1;slot.tick=now;slot.normalized_valid=value>=0&&value<=1;memcpy(&slot.bits,&value,4);memcpy(slot.raw_signals,n->signals,sizeof(n->signals));}
 if(witness){slot.refresh_raw=raw;slot.refresh_kind=1;}else{slot.refresh_revision=old->refresh_revision;slot.refresh_tick=old->refresh_tick;slot.refresh_raw=old->refresh_raw;slot.refresh_kind=0;}
 if(old->revision>UINT32_MAX-2){atomic_store(&s->error,QL_INVALID);return 0;}slot.revision=old->revision+2;*old=slot;return 1;
}
static void qlink_service(Context *c){
 QLinkDemand d;uint32_t now=atomic_load(&mirror_state->heartbeat);QLinkState *s=&mirror_state->qlinks;
 if(!qlink_demand_read(&command_state->qlink_interest,&d)||!d.enabled||d.until<=now){
  atomic_store(&qlink_hook_provider,0);atomic_store(&qlink_hook_queue,0);if(qlink_current.status!=QL_OFF){memset(&qlink_current,0,sizeof(qlink_current));qlink_publish();}return;
 }
 if(atomic_load(&s->error))return;
 if(now<qlink_next_tick)return;
 qlink_next_tick=now>UINT32_MAX-20?UINT32_MAX:now+20;
 QLinkNative n;if(!qlink_resolve(c,&d,&n)){
  if(qlink_current.status!=QL_UNAVAILABLE){memset(&qlink_current,0,sizeof(qlink_current));qlink_current.status=QL_UNAVAILABLE;qlink_current.epoch=d.epoch;qlink_current.tick=now;qlink_publish();}return;
 }
 /* This generation qualifies enrollment/envelopes, NOT complete semantic
  * assignment identity. Exact mode/assignment mutation coverage is pending. */
 unsigned sem=qlink_semantic_revision();unsigned changed=sem!=qlink_semantic_seen||atomic_load(&s->invalid)||qlink_current.status==QL_OFF||qlink_current.status==QL_UNAVAILABLE||qlink_current.epoch!=d.epoch||qlink_current.root!=n.root||qlink_current.controls!=n.controls||qlink_current.queue!=n.queue||qlink_current.wrapper!=n.wrapper||qlink_current.provider!=n.provider||qlink_current.labels!=n.labels||qlink_current.mode!=n.mode||qlink_current.sources!=n.sources||qlink_current.descriptors!=n.descriptors||qlink_current.cache!=n.cache||qlink_current.strings!=n.strings;
 if(changed){if(qlink_generation==UINT32_MAX){atomic_store(&s->error,QL_INVALID);return;}memset(&qlink_current,0,sizeof(qlink_current));qlink_current.generation=++qlink_generation;qlink_current.epoch=d.epoch;qlink_current.root=n.root;qlink_current.controls=n.controls;qlink_current.queue=n.queue;qlink_current.wrapper=n.wrapper;qlink_current.provider=n.provider;qlink_current.labels=n.labels;qlink_current.mode=n.mode;qlink_current.sources=n.sources;qlink_current.descriptors=n.descriptors;qlink_current.cache=n.cache;qlink_current.strings=n.strings;qlink_cursor=0;qlink_semantic_seen=sem;atomic_store(&qlink_hook_generation,qlink_current.generation);atomic_store(&qlink_hook_queue,n.queue);atomic_store(&qlink_hook_labels,n.labels);atomic_store_explicit(&qlink_hook_provider,n.provider,memory_order_release);atomic_store(&s->invalid,0);if(sem!=qlink_semantic_revision()){atomic_store(&s->invalid,1);return;}}
 unsigned page=d.page<=2?d.page:2,first=page<2?page*8:0,count=page<2?8:QLINK_SLOTS;
 unsigned mode_changed=qlink_mode_sample(c,now);
 unsigned entry=changed||qlink_page_seen!=page;qlink_page_seen=page;unsigned sampled=0;
 for(unsigned step=0;step<count&&sampled<8;step++){
  unsigned i=first+(qlink_cursor+step)%count;QLinkSlot *old=qlink_current.slots+i;
  unsigned refresh=qlink_refresh_revision(i),labels=qlink_labels_revision(i);
  /* Native work is bounded to entry, actual dirty values and a low-rate
   * presentation resample. Offscreen slots are untouched in the MCU page. */
  if(!entry&&old->sampled&&refresh==qlink_read_seen[i]&&labels==qlink_label_seen[i]&&old->tick<=now&&now-old->tick<500)continue;
  if(!entry&&old->sampled&&old->normalized_valid&&labels==qlink_label_seen[i]&&refresh!=qlink_read_seen[i]){
   unsigned raw=word(n.cache+4*i),revision=0,tick=0,matches[COMMAND_LANES]={0},before=qlink_semantic_revision();
   if(old->refresh_kind&&raw==old->refresh_raw&&qlink_witness_read(i,qlink_current.generation,raw,&revision,&tick,matches)&&word(n.cache+4*i)==raw&&!atomic_load(&s->invalid)&&before==qlink_semantic_revision()){
    /* Equal descriptor-domain value under the same mapping preserves its
     * already-normalized native sample. Advance witnesses without reformatting. */
    old->refresh_revision=revision;old->refresh_tick=tick;memcpy(old->refresh_matches,matches,sizeof(matches));old->tick=now;old->gesture_activity=*(const volatile unsigned char*)(uintptr_t)(n.sources+0x84*i+0x28);qlink_read_seen[i]=refresh;sampled++;continue;
   }
  }
  int result=qlink_sample(c,&d,&n,i,now);if(!result)return;if(result==2)continue;qlink_read_seen[i]=refresh;qlink_label_seen[i]=labels;sampled++;
 }
 qlink_cursor=(qlink_cursor+8)%count;
 if(!sampled&&!mode_changed)return;
 qlink_current.status=QL_SAMPLED;qlink_current.demand=d.revision;qlink_current.tick=now;memcpy(qlink_current.raw_signals,n.signals,sizeof(n.signals));qlink_publish();
}
