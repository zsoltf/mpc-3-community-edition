/* Existing UI owner owns native SharedEnableable tokens, never Track/Program
 * retains. Audio/UI source lanes own separate copied envelope records. */
static uint32_t meter_tokens[METER_CELLS],meter_subscription;
static uint32_t meter_interest_revision,meter_topology_revision,meter_interest_epoch;
static unsigned meter_interest_valid,meter_until,meter_seen_deaths;
static _Atomic unsigned meter_deaths;
static unsigned meter_hash(uint32_t n){return ((n>>4)*2654435761u)&(METER_HASH-1);}
static MeterCell *meter_find(uint32_t n){
 unsigned h=meter_hash(n);
 for(unsigned i=0;i<MIRROR_PROBES;i++){unsigned id=atomic_load_explicit(mirror_state->meters.directory+((h+i)&(METER_HASH-1)),memory_order_acquire);if(!id||id>METER_CELLS)return NULL;MeterCell *m=mirror_state->meters.cells+id-1;if(atomic_load_explicit(&m->address,memory_order_relaxed)==n)return m;}
 return NULL;
}
static void meter_error(unsigned why){atomic_store_explicit(&mirror_state->meters.error,why,memory_order_release);}
static void meter_initialize(void){memset(meter_tokens,0,sizeof(meter_tokens));meter_subscription=0;meter_interest_valid=0;meter_until=0;meter_seen_deaths=0;atomic_store(&meter_deaths,0);}
static void meter_release(unsigned i){
 uint32_t k=meter_tokens[i];meter_tokens[i]=0;
 atomic_store_explicit(&mirror_state->meters.cells[i].subscription,0,memory_order_release);
 if(!k)return;
#ifndef COMMAND_COMPONENT
 ((void (*)(void*))(uintptr_t)(observer_image_bias+0xe977bc))((void*)(uintptr_t)k);
 ((void (*)(void*))(uintptr_t)(observer_image_bias+0x1efc91c))((void*)(uintptr_t)k);
 ((void (*)(void*,unsigned))(uintptr_t)(observer_image_bias+0x4a66cc))((void*)(uintptr_t)k,0x10);
#endif
 atomic_store_explicit(&mirror_state->meters.tokens,atomic_load(&mirror_state->meters.tokens)-1,memory_order_release);
}
static void meter_release_all(void){
 meter_interest_valid=0;
 unsigned n=atomic_load_explicit(&mirror_state->meters.used,memory_order_acquire);
 for(unsigned i=0;i<n&&i<METER_CELLS;i++)meter_release(i);
 atomic_store_explicit(&mirror_state->meters.master,0,memory_order_release);
 for(unsigned i=0;i<count;i++)tracks[i].meter=0;
}
/* Death is register-key qualified before native vptr/listener mutation. The
 * native destructor cancels release callbacks, allowing the next UI drain to
 * dispose of a token even when destruction ran on another source thread. */
static void meter_source(Context *c,unsigned site,unsigned lane,MeterCell *m){
 if(site==ME_DEATH||site==ME_DELETE){atomic_store_explicit(&m->subscription,0,memory_order_release);atomic_store_explicit(&m->live,0,memory_order_release);atomic_fetch_add_explicit(&meter_deaths,1,memory_order_release);return;}
 unsigned sub=atomic_load_explicit(&m->subscription,memory_order_acquire),ep=atomic_load(&m->epoch);
 if(!sub||!atomic_load_explicit(&m->live,memory_order_acquire))return;
 uint32_t n=atomic_load_explicit(&m->address,memory_order_relaxed);
 /* The native producer owns N at this synchronous epilogue. No caller sample
  * array or delayed pointer read is retained. Inactive returns are explicit. */
 uint32_t left=word(n+0x1c),right=word(n+0x20),enabled=*(const volatile unsigned char*)(uintptr_t)(n+0x34);
 if(sub!=atomic_load_explicit(&m->subscription,memory_order_acquire)||!atomic_load(&m->live))return;
 MeterSample *s=m->lanes+lane;unsigned rev=atomic_load_explicit(&s->revision,memory_order_relaxed);
 if(rev>UINT32_MAX-2){meter_error(ME_CAPACITY);return;}
 atomic_store_explicit(&s->revision,rev+1,memory_order_relaxed);atomic_thread_fence(memory_order_release);
 atomic_store(&s->subscription,sub);atomic_store(&s->epoch,ep);atomic_store(&s->tick,atomic_load(&mirror_state->heartbeat));atomic_store(&s->token,token());atomic_store(&s->left,left);atomic_store(&s->right,right);atomic_store(&s->enabled,enabled);atomic_store(&s->site,site);
 atomic_store_explicit(&s->revision,rev+2,memory_order_release);(void)c;
}
static unsigned meter_bind(uint32_t n,unsigned owner_id,unsigned owner_kind,int demand){
 if(!owner_id||!typed(n,0x68ac3d4))return 0;
 MeterState *s=&mirror_state->meters;MeterCell *m=meter_find(n);unsigned id=0;
 if(m&&atomic_load(&m->live)&&atomic_load(&m->owner)==owner_id&&atomic_load(&m->owner_kind)==owner_kind)id=(unsigned)(m-s->cells)+1;
 else{
  unsigned used=atomic_load(&s->used),h=meter_hash(n),slot=METER_HASH;
  if(used==METER_CELLS){meter_error(ME_CAPACITY);return 0;}
  for(unsigned i=0;i<MIRROR_PROBES;i++){unsigned at=(h+i)&(METER_HASH-1),old=atomic_load(s->directory+at);if(!old||(m&&old==(unsigned)(m-s->cells)+1)){slot=at;break;}}
  if(slot==METER_HASH){meter_error(ME_CAPACITY);return 0;}
  if(m){atomic_store(&m->subscription,0);atomic_store(&m->live,0);}
  id=used+1;m=s->cells+used;atomic_store(&m->address,n);atomic_store(&m->owner,owner_id);atomic_store(&m->owner_kind,owner_kind);atomic_store(&m->live,1);
  atomic_store_explicit(&s->used,id,memory_order_release);atomic_store_explicit(s->directory+slot,id,memory_order_release);
 }
 if(atomic_load(&m->subscription)&&atomic_load(&m->epoch)==epoch)return id;
 meter_release(id-1);
 if(meter_subscription==UINT32_MAX){meter_error(ME_CAPACITY);return 0;}
 if(demand){
  uint32_t k=0;
#ifndef COMMAND_COMPONENT
  ((void (*)(uint32_t*,void*))(uintptr_t)(observer_image_bias+0x1efc4e4))(&k,(void*)(uintptr_t)n);
#else
  k=id; /* component substitutes native allocation/demand only */
#endif
  if(!k){meter_error(ME_IDENTITY);return 0;}
  meter_tokens[id-1]=k;atomic_store_explicit(&s->tokens,atomic_load(&s->tokens)+1,memory_order_release);
 }
 atomic_store(&m->epoch,epoch);atomic_store_explicit(&m->subscription,++meter_subscription,memory_order_release);return id;
}
/* Fresh membership resolution on the qualified outer UI drain. The native
 * demand token retains only its signal, not any owner object. */
static void meter_service(Context *c){
 MeterState *s=&mirror_state->meters;
 if(!command_queues||c->r[0]!=command_queues+4||word(c->r[0]+0xf0))return;
 if(atomic_load(&s->closing)||atomic_load(&command_state->stop_requested)){
  meter_release_all();atomic_store_explicit(&s->closed,1,memory_order_release);return;
 }
 if(atomic_load(&s->error)){meter_release_all();return;}
 MeterDemand demand;unsigned now=atomic_load_explicit(&mirror_state->heartbeat,memory_order_acquire);
 if(!meter_demand_read(&command_state->meter_interest,&demand)){if(meter_interest_valid&&now>=meter_until)meter_release_all();return;}
 if(!demand.enabled||demand.epoch!=epoch||demand.until<=now){
  if(meter_interest_valid||atomic_load(&s->tokens)||atomic_load(&s->master))meter_release_all();
  return;
 }
 if(!ready||depth||loading||reset_pending||!root||!project)return;
 for(unsigned i=0;i<count;i++)if(tracks[i].transition)return;
 unsigned deaths=atomic_load_explicit(&meter_deaths,memory_order_acquire);
 unsigned topology=atomic_load_explicit(&mirror_state->revision,memory_order_acquire);
 if(meter_interest_valid&&meter_interest_revision==demand.revision&&meter_topology_revision==topology&&meter_interest_epoch==epoch&&meter_seen_deaths==deaths)return;
 if(word(root+0x24c)!=project||word(project+0xa94)!=pool||!typed(pool,0x68ad2c4)){meter_error(ME_IDENTITY);return;}
 uint32_t pp=word(project+0xa90);if(!typed(pp,0x68ad07c)){meter_error(ME_IDENTITY);return;}
 uint32_t begin=word(pp+0x1c),end=word(pp+0x20),cap=word(pp+0x24),tb=word(pool+0x34),te=word(pool+0x38);
 if((begin&&!pointer(begin))||end<begin||cap<end||(end-begin)%4||(end-begin)/4>MIRROR_CELLS||te<tb||(te-tb)/4!=count||(count&&!pointer(tb))){meter_error(ME_IDENTITY);return;}
 /* Retire the previous bank before requesting the next. Each service pass
  * ends with at most eight channel demands; a departing cell whose owner is
  * shared with a newly demanded strip is released by the wanted sweep below. */
 unsigned existing=atomic_load(&s->used);
 for(unsigned i=0;i<existing;i++)if(meter_tokens[i]){
  MeterCell *cell=s->cells+i;unsigned keep=0,owner_id=atomic_load(&cell->owner),kind=atomic_load(&cell->owner_kind);
  for(unsigned j=0;j<demand.count;j++)if(owner_id==(kind==CO_TRACK?demand.track_owner[j]:demand.program_owner[j]))keep=1;
  if(!keep||!atomic_load_explicit(&cell->live,memory_order_acquire))meter_release(i);
 }
 unsigned wanted[MIRROR_TRACKS+1],nw=0,changed=0;
 uint32_t a=word(root+0x2e4);unsigned master=0;
 if(pointer(a)&&a<UINT32_MAX-0x400&&typed(a+0x120,0x68ac3bc)&&typed(a+0x260,0x6898b34))master=meter_bind(a+0x128,channel_owner_id(a+0x260),CO_MIXER,0);
 atomic_store_explicit(&s->master,master,memory_order_release);if(master)wanted[nw++]=master;
 for(unsigned i=0;i<count;i++){
  Track *t=tracks+i;unsigned id=0;
  if(word(tb+i*4)!=t->track||!typed(t->track,0x6935554)||word(t->track+0x648)!=t->program||word(t->track+0x64c)!=t->kind){meter_error(ME_IDENTITY);return;}
  unsigned interested=0;
  for(unsigned j=0;j<demand.count;j++)if(demand.serial[j]==t->serial&&demand.track_owner[j]==t->track_owner&&demand.program_owner[j]==t->program_owner)interested=1;
  if(interested&&t->program&&t->program_owner&&channel_owner_id(t->program)==t->program_owner){
   if(!program_member(pp,t)||word(t->program)!=observer_image_bias+t->vptr){meter_error(ME_IDENTITY);return;}
   /* Exact Channel Mixer selection: type4 uses Track point1, others Program
    * point5. Do not substitute an invented pre/post-fader name. */
   id=meter_bind(t->kind==4?t->track+0x324:t->program+0x2f4,t->kind==4?t->track_owner:t->program_owner,t->kind==4?CO_TRACK:CO_PROGRAM,1);
  }
  if(t->meter!=id){t->meter=id;changed=1;}if(id)wanted[nw++]=id;
 }
 unsigned used=atomic_load(&s->used);
 for(unsigned i=0;i<used;i++){unsigned j;for(j=0;j<nw;j++)if(wanted[j]==i+1)break;if(j==nw)meter_release(i);}
 if(changed)export_topology();
 meter_interest_revision=demand.revision;meter_topology_revision=atomic_load_explicit(&mirror_state->revision,memory_order_acquire);meter_interest_epoch=epoch;meter_until=demand.until;meter_seen_deaths=deaths;meter_interest_valid=1;
}
static int meter_topology_boundary(unsigned site){
 return site==M_RESET||site==M_CREATE_ENTER||site==M_ROOT_DESTROY||site==M_PROJECT_DESTROY||site==M_PROJECT_CLEAR||site==M_POOL_CLEAR||site==M_POOL_DESTROY||site==M_TRACK_DESTROY||site==M_REMOVE_ENTER||site==M_REPLACE_SIMPLE_PRE||site==M_REPLACE_FULL_PRE;
}
