/* Existing Effects UI owner opens the stock chooser. Native SELECT retains
 * description mapping/history/loading; this operation certifies only opening. */
typedef struct {uint32_t tmi,bridge,list,context,pages;} EffectChooserNative;
static int effects_chooser_native(EffectChooserNative *out){
 memset(out,0,sizeof(*out));
 if(!root||root>UINT32_MAX-0x840||!typed(root,0x68d7bf8))return 0;
 uint32_t tmi=word(root+0x83c);if(!typed(tmi,0x699efd0)||tmi>UINT32_MAX-0x3500||word(tmi+0x124)!=root)return 0;
 uint32_t object=word(tmi+0x598);
 const unsigned types[]={0x69e6b98,0x69e7608,0x69e9314,0x69e96a8,0x69e97b8};
 const unsigned offsets[]={0xe8,0x9c,0xc8,0xc8};
 for(unsigned i=0;i<5;i++){if(!typed(object,types[i])||object>UINT32_MAX-0x100)return 0;if(i<4)object=word(object+offsets[i]);}
 uint32_t bridge=object,context=word(root+0x1fc),pages=word(root+0x114);
 if(!pointer(context)||context>UINT32_MAX-0x40||!pointer(pages)||word(tmi+0x128)!=pages||word(bridge+0x2c)!=context||word(bridge+0x48)!=pages)return 0;
 uint32_t node=word(tmi+0x34ec),sentinel=tmi+0x34e8,n=word(tmi+0x34f8),seen[256];unsigned used=0;
 if(!n||n>256)return 0;
 while(node&&node!=sentinel){
  if(used>=n||used>=256||!pointer(node)||node>UINT32_MAX-0x1c)return 0;
  for(unsigned i=0;i<used;i++)if(seen[i]==node)return 0;
  seen[used++]=node;uint32_t key=word(node+0x10);
  if(key==0x30fa){object=word(node+0x14);if(!pointer(word(node+0x18))||!typed(object,0x69ca244)||object>UINT32_MAX-0x25c)return 0;
   uint32_t list=object+0x18c;if(!typed(list,0x69ac190)||word(list+0xc4)!=pages||word(list+0xc8)!=context)return 0;
   if(word(root+0x83c)!=tmi||word(tmi+0x124)!=root||word(root+0x1fc)!=context||word(root+0x114)!=pages)return 0;
   *out=(EffectChooserNative){tmi,bridge,list,context,pages};return 1;
  }
  node=word(node+(key<0x30fa?0xc:8));
 }
 return 0;
}
static unsigned effects_chooser_open(const EffectChooserNative *n){
 return ((unsigned (*)(void*,unsigned))(uintptr_t)(observer_image_bias+0x157507c))((void*)(uintptr_t)n->pages,0x30a8)!=0;
}
static int effects_chooser_context(const EffectChooserNative *n,uint32_t program,unsigned slot){return word(n->context+0x34)==program&&word(n->context+0x38)==0x101&&word(n->context+0x3c)==slot;}
static unsigned effects_chooser_resolve(Context *c,const CommandRequest *r,EffectNative *out){
 unsigned now=atomic_load(&mirror_state->heartbeat);
 if(!command_chooser(r->reserved)||r->bits>1||r->effect_slot>=EFFECT_SLOTS||!r->effect_generation||r->field_incarnation!=r->effect_generation||r->effect_index||r->pad_owner||r->global_owner)return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 if(r->expires<r->created||r->expires-r->created>120000||r->expires<now)return C_EXPIRED;
 if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 EffectsDemand d;if(!effects_demand_read(&command_state->effects_interest,&d)||!d.enabled||d.until<=now||d.epoch!=r->epoch||d.serial!=r->serial||d.slot!=(r->bits?r->effect_slot:EFFECT_LIST)||d.page!=r->effect_position)return C_SOURCE_CHANGED;
 if(atomic_load(&mirror_state->effects.invalid)||atomic_load(&mirror_state->effects.error)||effects_current.status!=EF_READY||effects_current.generation!=r->effect_generation||!effects_resolve(c,&d,out,1))return C_SOURCE_CHANGED;
 Track *t=out->track;const EffectSlot *slot=effects_current.slots+r->effect_slot;
 if(t->binding!=r->binding||t->incarnation!=r->incarnation||t->track_owner!=r->track_owner||t->program_owner!=r->program_owner||channel_owner_id(project)!=r->project_owner||out->keys[r->effect_slot]!=r->effect_key||out->aps[r->effect_slot]!=r->effect_ap||*(const volatile unsigned char*)(uintptr_t)(t->program+0x1158))return C_IDENTITY;
 if(r->bits?(slot->status!=EF_READY||!r->effect_key||!r->effect_ap):(slot->status!=EF_EMPTY||r->effect_key||r->effect_ap))return C_SOURCE_CHANGED;
 return C_OK;
}
static void effects_chooser_event(Context *c,unsigned kind,unsigned at,const EffectChooserNative *n){
 const CommandRequest *r=&accepted[at].request;
 event(c,kind,r->seq,n->tmi,n->pages,accepted[at].program,0x101,r->effect_slot,r->bits,n->context,r->effect_generation);
}
static void effects_chooser_service(Context *c,unsigned at,const CommandRequest *r){
 CommandSlot *slot=command_state->slots+at;EffectNative native;EffectChooserNative before,after;
 unsigned why=effects_chooser_resolve(c,r,&native);if(why==COMMAND_WAIT_HEARTBEAT)return;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(i!=at){CommandSlot *other=command_state->slots+i;unsigned seq=atomic_load(&other->published);if(seq&&seq<r->seq&&seq!=atomic_load(&other->reclaimed)){CommandRequest old;if(!command_request_read(other,seq,&old)||command_conflict(&old,r))return;}}
 atomic_store(&slot->rejected,0);accepted[at].request=*r;accepted[at].program=why?0:native.track->program;native_started[at]=0;
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *lane=slot->lanes+i;atomic_store(&lane->sequence,0);atomic_store(&lane->published,0);atomic_store_explicit(&lane->sequence,r->seq,memory_order_release);}
 if(why)goto refused;
 if(!effects_chooser_native(&before)){why=C_NATIVE_MEMBERSHIP;goto refused;}
 /* Never rebind an already open chooser, including one for another track. */
 if(effects_chooser_open(&before)){why=C_BUSY;goto refused;}
 if(effects_chooser_resolve(c,r,&native)){why=C_SOURCE_CHANGED;goto refused;}
 atomic_store(active_request+at,r->seq);native_started[at]=r->seq;effects_chooser_event(c,CE_DISPATCH,at,&before);
 ((void (*)(void*,void*))(uintptr_t)(observer_image_bias+0x2b068f0))((void*)(uintptr_t)before.bridge,(void*)(uintptr_t)native.track->program);
 if(effects_chooser_resolve(c,r,&native)||!effects_chooser_native(&after)||memcmp(&before,&after,sizeof(before))||word(after.context+0x34)!=native.track->program||word(after.context+0x38)!=0x101||effects_chooser_open(&after)){why=C_SOURCE_CHANGED;goto failed;}
 ((void (*)(void*,unsigned))(uintptr_t)(observer_image_bias+0x340e5d4))((void*)(uintptr_t)after.list,r->effect_slot);
 if(effects_chooser_resolve(c,r,&native)||!effects_chooser_native(&after)||memcmp(&before,&after,sizeof(before))||!effects_chooser_open(&after)||!effects_chooser_context(&after,native.track->program,r->effect_slot)){why=C_SOURCE_CHANGED;goto failed;}
 effects_chooser=(EffectChooser){r->seq,EC_OPEN,r->epoch,r->serial,r->track_owner,r->program_owner,r->effect_slot,r->effect_generation,r->effect_key,r->effect_ap,atomic_load(&mirror_state->heartbeat)};
 effects_publish();effects_chooser_event(c,CE_CHOOSER_ACCEPT,at,&after);effects_chooser_event(c,CE_RETURN,at,&after);
 atomic_store(active_request+at,0);atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);atomic_store(&slot->done,r->seq);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);return;
failed:
 /* Native UI retains its own listeners/jobs. Failed open is never success. */
 atomic_store(active_request+at,0);atomic_store(&slot->processed,r->seq);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);command_fail(why);return;
refused:
 atomic_store(active_request+at,0);atomic_store(&slot->rejected,why);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);
}
static void effects_chooser_observe(Context *c){
 EffectChooser *q=&effects_chooser;if(q->status!=EC_OPEN&&q->status!=EC_LOADING)return;
 EffectsDemand d={.enabled=1,.epoch=q->epoch,.serial=q->serial,.slot=EFFECT_LIST};EffectNative n;EffectChooserNative ui;
 q->tick=atomic_load(&mirror_state->heartbeat);
 if(!effects_resolve(c,&d,&n,1)||n.track->track_owner!=q->track_owner||n.track->program_owner!=q->program_owner||!effects_chooser_native(&ui)){q->status=EC_UNAVAILABLE;return;}
 if(effects_chooser_open(&ui)){if(!effects_chooser_context(&ui,n.track->program,q->slot))q->status=EC_UNAVAILABLE;return;}
 if(n.keys[q->slot]==q->key&&n.aps[q->slot]==q->ap&&!atomic_load(&mirror_state->effects.invalid)&&effects_current.generation==q->generation){q->status=EC_CANCELLED;return;}
 q->status=EC_LOADING;
 const EffectSlot *slot=effects_current.slots+q->slot;
 if(effects_current.epoch==q->epoch&&effects_current.serial==q->serial&&effects_current.track_owner==q->track_owner&&effects_current.program_owner==q->program_owner&&effects_current.generation!=q->generation&&!atomic_load(&mirror_state->effects.invalid)&&!atomic_load(&mirror_state->effects.error)&&slot->key==n.keys[q->slot]&&slot->ap==n.aps[q->slot]&&(slot->status==EF_READY||slot->status==EF_EMPTY||slot->status==EF_UNSUPPORTED)){
  /* An inventory generation alone is not a selected-slot load witness: another
   * slot or a same-address refresh can advance it. Keep that outcome explicit. */
  q->status=slot->status==EF_READY&&(slot->key!=q->key||slot->ap!=q->ap)?EC_LOADED:EC_UNAVAILABLE;
  if(q->status==EC_LOADED){q->generation=effects_current.generation;q->key=slot->key;q->ap=slot->ap;}
 }
}
