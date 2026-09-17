/* Selected insert reads run synchronously on the existing UI command owner.
 * Cached native numbers are comparison keys only; every call uses a fresh slot. */
#include "native-name.h"
#include <time.h> /* the bounded in-drain settle wait below */
static EffectsCopy effects_current;
static EffectChooser effects_chooser;
static void effects_chooser_observe(Context*);
static _Atomic uint32_t effects_pending;
#include "effects-catalogue.h"
#include "module-ranges.h"
static int effects_image_address(uint32_t address,unsigned bytes,unsigned flags){
 if(address<observer_image_bias||bytes>UINT32_MAX-address)return 0;
 uint32_t rva=address-observer_image_bias;
 for(unsigned i=0;i<sizeof(module_ranges)/sizeof(module_ranges[0]);i++)if((module_ranges[i][2]&flags)==flags&&rva>=module_ranges[i][0]&&(uint64_t)rva+bytes<=module_ranges[i][1])return 1;
 return 0;
}
static uint32_t effects_order[EFFECT_SLOTS][EFFECT_MAX_PRESENTATION];
static unsigned effects_seeded[EFFECT_SLOTS];
/* Fixed firmware catalogue is selected only by a qualified native identity.
 * Native indices absent from authored pages remain reachable in appended pages. */
static void effects_layout_seed(unsigned slot,const EffectDescription *description){
 EffectSlot *item=effects_current.slots+slot;unsigned total=item->count;
 item->layout=0;item->presentation_count=total;
 for(unsigned i=0;i<total;i++)effects_order[slot][i]=i;
 if(!description||!description->valid)return;
 unsigned matched=UINT32_MAX;
 for(unsigned i=0;i<sizeof(effect_catalogue_identities)/sizeof(effect_catalogue_identities[0]);i++){
  const EffectCatalogueIdentity *id=effect_catalogue_identities+i;
  if(id->uid==description->uid&&id->instrument==description->instrument&&!strcmp(id->name,description->name)&&!strcmp(id->format,description->format)){matched=id->catalogue;break;}
 }
 for(unsigned entry=0;entry<sizeof(effect_catalogue)/sizeof(effect_catalogue[0]);entry++){
  const EffectCatalogue *cat=effect_catalogue+entry;if(entry!=matched)continue;
  if(cat->count>EFFECT_MAX_CATALOGUE_PAGES)return;
  unsigned char seen[EFFECT_MAX_PARAMETERS]={0};unsigned position=0;
  for(unsigned page=0;page<cat->count;page++)for(unsigned i=0;i<EFFECT_PAGE;i++){
   int index=effect_catalogue_pages[cat->first+page].indices[i];
   if(index>=0&&(unsigned)index>=total)return; /* Entire schema falls back. */
  }
  for(unsigned page=0;page<cat->count;page++)for(unsigned i=0;i<EFFECT_PAGE;i++){
   int index=effect_catalogue_pages[cat->first+page].indices[i];
   effects_order[slot][position++]=index<0?UINT32_MAX:(unsigned)index;
   if(index>=0)seen[index]=1; /* Retain native-authored duplicates above. */
  }
  for(unsigned i=0;i<total;i++)if(!seen[i])effects_order[slot][position++]=i;
  item->presentation_count=position;item->layout=entry+1;return;
 }
}
static void effects_page_context(unsigned slot,unsigned page){
 effects_current.page_count=effects_current.tab=effects_current.subtab=effects_current.section=effects_current.section_count=0;memset(effects_current.page_name,0,EFFECT_TEXT);
 if(slot>=EFFECT_SLOTS)return;
 const EffectSlot *item=effects_current.slots+slot;effects_current.page_count=(item->presentation_count+EFFECT_PAGE-1)/EFFECT_PAGE;
 const char *name="Parameters";unsigned section=page+1,sections=effects_current.page_count;
 if(item->layout&&item->layout<=sizeof(effect_catalogue)/sizeof(effect_catalogue[0])){
  const EffectCatalogue *cat=effect_catalogue+item->layout-1;
  if(page<cat->count){
   const EffectCataloguePage *p=effect_catalogue_pages+cat->first+page;name=p->name;effects_current.tab=p->tab;effects_current.subtab=p->subtab;section=sections=0;
   for(unsigned i=0;i<cat->count;i++){const EffectCataloguePage *q=effect_catalogue_pages+cat->first+i;if(q->tab==p->tab&&q->subtab==p->subtab){sections++;if(i<=page)section++;}}
  }else{name="Other";section=page-cat->count+1;sections=effects_current.page_count-cat->count;}
 }
 unsigned length=strlen(name);if(length>=EFFECT_TEXT){length=EFFECT_TEXT-1;while(length&&((unsigned char)name[length]&0xc0)==0x80)length--;}
 memcpy(effects_current.page_name,name,length);effects_current.section=section;effects_current.section_count=sections;
}
static unsigned effects_cursor,effects_next_tick,effects_reading,effects_generation;
/* These are transaction receipts, not retained callable processor handles. */
static _Atomic uint32_t effect_executed[COMMAND_SLOTS],effect_applied_key[COMMAND_SLOTS],effect_applied_ap[COMMAND_SLOTS],effect_sampled[COMMAND_SLOTS],effect_invalidated[COMMAND_SLOTS],effect_watch[COMMAND_SLOTS],effect_inserts[COMMAND_SLOTS],effect_wanted_key[COMMAND_SLOTS],effect_wanted_ap[COMMAND_SLOTS];
static void effects_native_body(unsigned at){if(accepted[at].request.reserved==EFFECT_ENABLE)atomic_store_explicit(effect_executed+at,accepted[at].request.seq,memory_order_release);}
static int effects_receipted(unsigned at){return atomic_load_explicit(effect_sampled+at,memory_order_acquire)==accepted[at].request.seq;}
static void effects_prepare_slot(unsigned at){atomic_store(&effects_pending,atomic_load(&effects_pending)&~(1u<<at));atomic_store(effect_watch+at,0);atomic_store(effect_executed+at,0);atomic_store(effect_applied_key+at,0);atomic_store(effect_applied_ap+at,0);atomic_store(effect_sampled+at,0);atomic_store(effect_invalidated+at,0);}
static void effects_initialize(void){memset(&effects_chooser,0,sizeof(effects_chooser));memset(&effects_current,0,sizeof(effects_current));effects_cursor=effects_next_tick=effects_reading=effects_generation=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)effects_prepare_slot(i);}
static void effects_publish(void){
 effects_current.chooser=effects_chooser;
 EffectsState *s=&mirror_state->effects;unsigned rev=atomic_load(&s->revision);if(rev>UINT32_MAX-2){atomic_store(&s->error,EF_INVALID);return;}
 uint32_t words[EFFECT_WORDS];memcpy(words,&effects_current,sizeof(words));atomic_store(&s->revision,rev+1);for(unsigned i=0;i<EFFECT_WORDS;i++)atomic_store_explicit(s->words+i,words[i],memory_order_relaxed);atomic_store_explicit(&s->revision,rev+2,memory_order_release);
}
static void effects_lifecycle(Context *c,unsigned site){
 EffectsState *s=&mirror_state->effects;
 if(!atomic_load_explicit(&observer_running,memory_order_seq_cst))return;
 unsigned wanted=atomic_load_explicit(&command_state->effects_interest.enabled,memory_order_acquire);
 wanted|=atomic_load_explicit(&effects_pending,memory_order_acquire);
 if(!wanted)return;
 if(site!=M_RESET&&site!=M_ROOT_DESTROY&&site!=PX_REPLACE&&site!=PX_UI_VECTOR&&site!=PX_RETIRE&&site!=PX_DEATH&&site!=PX_VST_REFRESH&&site!=PX_PARAMETER_TREE)return;
 unsigned relevant=site==M_RESET||site==M_ROOT_DESTROY;
 uint32_t inserts=atomic_load(&s->words[offsetof(EffectsCopy,inserts)/4]);
 if((site==PX_REPLACE&&c->r[0]==inserts)||(site==PX_UI_VECTOR&&c->r[0]==inserts+0x50))relevant=1;
 for(unsigned i=0;i<EFFECT_SLOTS&&!relevant;i++){
  unsigned at=(offsetof(EffectsCopy,slots)+i*sizeof(EffectSlot))/4;uint32_t e=atomic_load(s->words+at),ap=atomic_load(s->words+at+1);
  if(e&&((site==PX_RETIRE&&c->r[1]==e)||(site==PX_DEATH&&c->r[0]==e)))relevant=1;
  if(ap&&(site==PX_VST_REFRESH||site==PX_PARAMETER_TREE)&&c->r[0]==ap)relevant=2;
 }
 unsigned invalidate=0;
 for(unsigned i=0;i<COMMAND_SLOTS;i++){
  unsigned seq=atomic_load_explicit(effect_watch+i,memory_order_acquire);if(!seq)continue;
  unsigned e=atomic_load(effect_wanted_key+i),ap=atomic_load(effect_wanted_ap+i),owner=atomic_load(effect_inserts+i);
  if(site==M_RESET||site==M_ROOT_DESTROY||(site==PX_REPLACE&&c->r[0]==owner)||(site==PX_UI_VECTOR&&c->r[0]==owner+0x50)||(site==PX_RETIRE&&c->r[1]==e)||(site==PX_DEATH&&c->r[0]==e)||((site==PX_VST_REFRESH||site==PX_PARAMETER_TREE)&&c->r[0]==ap)){invalidate|=1u<<i;relevant=1;}
 }
 if(!relevant)return;
 unsigned lane=trace_enter(site);if(lane==COMMAND_LANES)return;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(invalidate&(1u<<i))atomic_store_explicit(effect_invalidated+i,atomic_load(effect_watch+i),memory_order_release);
 /* Foreign mutation cannot be synchronized by these observed UI boundaries.
  * Disable Effects for this process instead of guessing that a later read is safe. */
 if(token()!=command_owner)atomic_store(&s->error,EF_FOREIGN);
 else if(effects_reading)atomic_store(&s->error,EF_REENTRANT);
 else if(relevant==2)atomic_store(&s->error,EF_INVENTORY);
 atomic_store_explicit(&s->invalid,1,memory_order_release);trace_leave(lane);
}
typedef struct {Track *track;uint32_t inserts,keys[EFFECT_SLOTS],aps[EFFECT_SLOTS];} EffectNative;
static int effects_resolve(Context *c,const EffectsDemand *d,EffectNative *out,int require_selected){
 memset(out,0,sizeof(*out));
 if(!d->enabled||d->epoch!=epoch||token()!=command_owner||!ready||loading||reset_pending||depth||!active()||!root||!project||!pool||!command_queues||c->r[0]!=command_queues+4||word(c->r[0]+0xf0)||atomic_load(&command_state->new_project_intent)||atomic_load(&command_state->stop_requested))return 0;
 if(!typed(root,0x68d7bf8)||word(root+0x24c)!=project||!typed(project,0x6933c4c)||word(project+0xa94)!=pool||!typed(pool,0x68ad2c4))return 0;
 ChannelCell *selection=channel_cell(project+channel_offset(CF_SELECTION));if(!selection||!atomic_load(&selection->live))return 0;uint32_t selected=atomic_load(&selection->bits);
 Track *t=NULL;for(unsigned i=0;i<count;i++)if(tracks[i].serial==d->serial){if(t)return 0;t=tracks+i;}
 if(!t||(require_selected&&t->track!=selected)||t->transition||!t->track_owner||!t->program_owner||channel_owner_id(t->track)!=t->track_owner||channel_owner_id(t->program)!=t->program_owner||!vector(pool,0x34,t->track)||!typed(t->track,0x6935554)||word(t->track+0x648)!=t->program||word(t->track+0x64c)!=t->kind||!typed(word(project+0xa90),0x68ad07c)||!program_member(word(project+0xa90),t)||!command_mixable(t->program))return 0;
 uint32_t i=word(t->program+0x5e0);if(!typed(i,0x68ac370)||word(i+0x44)!=command_queues||word(t->program+0x40)!=command_queues)return 0;
 uint32_t begin=word(i+0x60),end=word(i+0x64),cap=word(i+0x68);if(end<begin||end-begin!=16||cap<end||cap-begin>16||!pointer(begin)||begin>UINT32_MAX-16)return 0;
 for(unsigned slot=0;slot<EFFECT_SLOTS;slot++){uint32_t e=word(begin+4*slot);if(e){if(!typed(e,0x692e180))return 0;uint32_t tuple=word(e+0x70);if(!pointer(tuple)||tuple>UINT32_MAX-12||word(tuple)!=t->program||word(tuple+4)!=0x101||word(tuple+8)!=slot)return 0;out->aps[slot]=word(e+0x68);}out->keys[slot]=e;}
 out->track=t;out->inserts=i;return 1;
}
static int effects_same(Context *c,const EffectsDemand *d,const EffectNative *before){
 EffectNative after;if(atomic_load(&mirror_state->effects.invalid)||atomic_load(&mirror_state->effects.error)||!effects_resolve(c,d,&after,0))return 0;
 return before->track==after.track&&before->inserts==after.inserts&&!memcmp(before->keys,after.keys,sizeof(before->keys))&&!memcmp(before->aps,after.aps,sizeof(before->aps));
}
#ifdef COMMAND_COMPONENT
static int (*component_effect_description)(uint32_t,uint32_t,EffectDescription*);
static int (*component_effect_string)(uint32_t,unsigned,unsigned,char*);
static int (*component_effect_count)(uint32_t);
static float (*component_effect_scalar)(uint32_t,unsigned,unsigned);
static int (*component_effect_automatable)(uint32_t,unsigned);
static int (*component_effect_integer)(uint32_t,unsigned,unsigned);
static float (*component_effect_enable)(uint32_t,unsigned);
#endif
/* Native PluginDescription follows the outer manufacturer's own retain/fill
 * protocol (23528f4). Strings are owned here only for this one UI invocation. */
static int effects_description_text(uint32_t value,char out[EFFECT_TEXT],int exact){
 unsigned char bytes[NAME_BYTES]={0};uint32_t length=0,status=0;
 if(!copy_name(value,bytes,&length,&status)||(exact&&(status==2||length>=EFFECT_TEXT)))return 0;
 if(length>=EFFECT_TEXT){length=EFFECT_TEXT-1;while(length&&(bytes[length]&0xc0)==0x80)length--;}
 memset(out,0,EFFECT_TEXT);memcpy(out,bytes,length);return 1;
}
static int effects_description(Context *c,const EffectsDemand *d,const EffectNative *n,unsigned slot,EffectDescription *out){
 uint32_t e=n->keys[slot],expected_ap=n->aps[slot];
 memset(out,0,sizeof(*out));
#ifdef COMMAND_COMPONENT
 if(component_effect_description)return component_effect_description(e,expected_ap,out);
#endif
 /* Model the native InsertEffect descriptor accessor, not generic parameter
  * children. The retained AP owns this call; no E is needed inside it. */
 if(!typed(e,0x692e180)||!pointer(expected_ap)||word(word(e)+0xcc)!=observer_image_bias+0x13f5de4)return 0;
 uint32_t pair[2]={0,0};
 ((void (*)(uint32_t*,void*,unsigned))(uintptr_t)word(word(e)+0xcc))(pair,(void*)(uintptr_t)e,0);
 int valid=0;
 if(effects_same(c,d,n)&&pair[0]==expected_ap&&pair[1]&&effects_image_address(word(pair[0]),0x124,4)&&effects_image_address(word(word(pair[0])+0x120)&~1u,2,5)){
  uint32_t description[17]={0};
  for(unsigned i=0;i<7;i++)((void (*)(uint32_t*))(uintptr_t)(observer_image_bias+0x925c70))(description+i);
  effects_current.reads++;
  ((void (*)(void*,uint32_t*))(uintptr_t)word(word(pair[0])+0x120))((void*)(uintptr_t)pair[0],description);
  valid=effects_same(c,d,n)&&effects_description_text(description[0],out->name,1)&&effects_description_text(description[2],out->format,1);
  effects_description_text(description[1],out->descriptive,0);effects_description_text(description[4],out->manufacturer,0);
  out->uid=description[12];out->instrument=((unsigned char*)description)[0x34];
  valid=valid&&out->instrument<=1;
  for(unsigned i=7;i;i--)((void (*)(uint32_t*))(uintptr_t)(observer_image_bias+0x911f58))(description+i-1);
 }
 if(pair[1])((void (*)(void*))(uintptr_t)(observer_image_bias+0xc96a68))((void*)(uintptr_t)pair[1]);
 out->valid=valid;return valid;
}
static int effects_string(uint32_t e,unsigned method,unsigned index,char out[EFFECT_TEXT]){
 uint32_t expected=method==0x10?0x234fd38:method==0x74?0x2351c50:0x2351e28;
 if(word(word(e)+method)!=observer_image_bias+expected)return 0;
 effects_current.reads++;
#ifdef COMMAND_COMPONENT
 if(component_effect_string)return component_effect_string(e,method,index,out);
#endif
 uint32_t native_string=0; /* Native JUCE one-word String, hidden return storage. */
 ((void (*)(uint32_t*,void*,unsigned))(uintptr_t)(observer_image_bias+expected))(&native_string,(void*)(uintptr_t)e,index);
 unsigned char bytes[NAME_BYTES]={0};uint32_t length=0,status=0;int valid=copy_name(native_string,bytes,&length,&status);
 ((void (*)(uint32_t*))(uintptr_t)(observer_image_bias+0x911f58))(&native_string);
 memset(out,0,EFFECT_TEXT);if(!valid)return 0;
 if(length>=EFFECT_TEXT){length=EFFECT_TEXT-1;while(length&&(bytes[length]&0xc0)==0x80)length--;}
 memcpy(out,bytes,length);return 1;
}
static int effects_integer(uint32_t e,unsigned method,unsigned index){
 effects_current.reads++;
#ifdef COMMAND_COMPONENT
 if(component_effect_integer)return component_effect_integer(e,method,index);
#endif
 return ((int (*)(void*,unsigned))(uintptr_t)word(word(e)+method))((void*)(uintptr_t)e,index);
}
static int effects_count(uint32_t e){if(word(word(e)+0x30)!=observer_image_bias+0x234ff08)return -1;effects_current.reads++;
#ifdef COMMAND_COMPONENT
 if(component_effect_count)return component_effect_count(e);
#endif
 return ((int (*)(void*))(uintptr_t)(observer_image_bias+0x234ff08))((void*)(uintptr_t)e);}
static float effects_scalar(uint32_t e,unsigned method,unsigned index){effects_current.reads++;
#ifdef COMMAND_COMPONENT
 if(component_effect_scalar)return component_effect_scalar(e,method,index);
#endif
 return ((float (*)(void*,unsigned))(uintptr_t)word(word(e)+method))((void*)(uintptr_t)e,index);}
/* Program/UI host enable mirror. This is not plugin-internal bypass or a
 * claim that the audio processor reached an audible ramp boundary. */
static int effects_enable_read(Context *c,const EffectsDemand *d,const EffectNative *n,unsigned slot,uint32_t *bits){
 if(slot>=EFFECT_SLOTS||!n->keys[slot])return 0;
 effects_current.reads++;float value;
#ifdef COMMAND_COMPONENT
 if(!component_effect_enable)return 0;
 value=component_effect_enable(n->track->program,slot);
#else
 value=((float (*)(void*,unsigned,unsigned))(uintptr_t)(observer_image_bias+0x250b754))((void*)(uintptr_t)n->track->program,0x101,0x146+slot);
#endif
 if(!effects_same(c,d,n)||(value!=0.0f&&value!=1.0f))return 0;
 memcpy(bits,&value,4);return 1;
}
static void effects_enable_store(EffectSlot *slot,uint32_t bits,uint32_t now){
 if(!slot->enable_valid||slot->enable_bits!=bits){
  if(slot->enable_revision>UINT32_MAX-2){atomic_store(&mirror_state->effects.error,EF_INVALID);return;}
  slot->enable_revision+=2;
 }
 slot->enable_bits=bits;slot->enable_tick=now;slot->enable_valid=1;
}
static void effects_service(Context *c){
 EffectsDemand d;uint32_t now=atomic_load(&mirror_state->heartbeat);
 if(!effects_demand_read(&command_state->effects_interest,&d)||!d.enabled||d.until<=now){if(effects_current.status!=EF_OFF){memset(&effects_current,0,sizeof(effects_current));effects_publish();}return;}
 effects_chooser_observe(c);
 EffectsState *state=&mirror_state->effects;
 if(atomic_load(&state->error)){if(effects_current.status!=atomic_load(&state->error)){effects_current.status=atomic_load(&state->error);effects_publish();}return;}
 if(now<effects_next_tick)return;
 effects_next_tick=now>UINT32_MAX-20?UINT32_MAX:now+20;
 EffectNative native;if(!effects_resolve(c,&d,&native,1)){effects_current.status=EF_UNSUPPORTED;effects_current.tick=now;effects_current.epoch=d.epoch;effects_current.serial=d.serial;effects_publish();return;}
 int changed=atomic_load(&state->invalid)||effects_current.epoch!=d.epoch||effects_current.serial!=d.serial||effects_current.track_owner!=native.track->track_owner||effects_current.program_owner!=native.track->program_owner||effects_current.inserts!=native.inserts;
 for(unsigned i=0;i<EFFECT_SLOTS;i++)changed|=effects_current.slots[i].key!=native.keys[i]||effects_current.slots[i].ap!=native.aps[i];
 if(changed){unsigned gen=++effects_generation;if(!gen){atomic_store(&state->error,EF_INVALID);return;}memset(&effects_current,0,sizeof(effects_current));effects_current.generation=gen;effects_current.epoch=d.epoch;effects_current.serial=d.serial;effects_current.track_owner=native.track->track_owner;effects_current.program_owner=native.track->program_owner;effects_current.inserts=native.inserts;for(unsigned i=0;i<EFFECT_SLOTS;i++){effects_current.slots[i].key=native.keys[i];effects_current.slots[i].ap=native.aps[i];effects_current.slots[i].generation=gen;effects_current.slots[i].status=native.keys[i]?EF_LOADING:EF_EMPTY;}effects_cursor=0;memset(effects_seeded,0,sizeof(effects_seeded));atomic_store(&state->invalid,0);}
 if(changed||effects_current.slot!=d.slot||effects_current.page!=d.page){memset(effects_current.parameters,0,sizeof(effects_current.parameters));effects_current.slot=d.slot;effects_current.page=d.page;effects_cursor=0;}
 effects_current.demand=d.revision;effects_current.status=EF_LOADING;effects_current.tick=now;
 /* List metadata is cached until the actual slot/inventory generation changes.
  * One slot or one parameter per drain bounds calls to the visible projection. */
 effects_reading=1;
 for(unsigned slot=0;slot<EFFECT_SLOTS;slot++)if(effects_current.slots[slot].status==EF_LOADING){
  EffectSlot *item=effects_current.slots+slot;
  if(!effects_seeded[slot]){
   int n=effects_count(native.keys[slot]);
   if(n<0||n>(int)EFFECT_MAX_PARAMETERS||!effects_same(c,&d,&native)){effects_reading=0;atomic_store(&state->invalid,1);return;}
   item->count=(unsigned)n;
   if(!effects_string(native.keys[slot],0x10,0,item->name)||!effects_same(c,&d,&native)){effects_reading=0;atomic_store(&state->invalid,1);return;}
   effects_description(c,&d,&native,slot,&item->description);
   if(!effects_same(c,&d,&native)){effects_reading=0;atomic_store(&state->invalid,1);return;}
   effects_layout_seed(slot,&item->description);
   effects_seeded[slot]=1;item->status=EF_READY;
  }
  effects_reading=0;effects_publish();return;
 }
 effects_page_context(d.slot,d.page);
 if(d.slot==EFFECT_LIST){
  unsigned slot=effects_cursor++%EFFECT_SLOTS;EffectSlot *item=effects_current.slots+slot;uint32_t bits=0;
  if(item->status==EF_READY){if(effects_enable_read(c,&d,&native,slot,&bits))effects_enable_store(item,bits,now);else item->enable_valid=0;}
  effects_current.status=EF_READY;effects_reading=0;effects_publish();return;
 }
 if(d.slot>=EFFECT_SLOTS||effects_current.slots[d.slot].status!=EF_READY){effects_current.status=EF_EMPTY;effects_reading=0;effects_publish();return;}
 unsigned native_total=effects_current.slots[d.slot].count,total=effects_current.slots[d.slot].presentation_count;if(d.page>=(total+EFFECT_PAGE-1)/EFFECT_PAGE&&d.page){effects_current.status=EF_INVALID;effects_reading=0;effects_publish();return;}
 unsigned strip=effects_cursor++%EFFECT_PAGE,position=d.page*EFFECT_PAGE+strip,index=position<total?effects_order[d.slot][position]:UINT32_MAX;EffectParameter *p=effects_current.parameters+strip;
 if(index>=native_total){p->index=UINT32_MAX;p->position=position;p->status=EF_EMPTY;}else if(p->status!=EF_UNSUPPORTED){
  uint32_t e=native.keys[d.slot];int n=effects_count(e);
  if(n!=(int)native_total||!effects_same(c,&d,&native)){effects_reading=0;atomic_store(&state->error,EF_INVENTORY);return;}
  if(word(word(e)+0x40)!=observer_image_bias+0x2350278){effects_reading=0;atomic_store(&state->error,EF_UNSUPPORTED);return;}
  if(p->status!=EF_READY){p->index=index;p->position=position;if(!effects_string(e,0x74,index,p->name)||!effects_same(c,&d,&native)){effects_reading=0;atomic_store(&state->invalid,1);return;}
   const unsigned methods[3]={0x48,0x50,0x58},expected[3]={0x2350670,0x2350a90,0x2350eb8};uint32_t *dest[3]={&p->minimum,&p->maximum,&p->default_bits};
   for(unsigned k=0;k<3;k++){if(word(word(e)+methods[k])!=observer_image_bias+expected[k]){effects_reading=0;atomic_store(&state->error,EF_UNSUPPORTED);return;}float v=effects_scalar(e,methods[k],index);memcpy(dest[k],&v,4);if(!effects_same(c,&d,&native)){effects_reading=0;return;}}
   if(word(word(e)+0x90)!=observer_image_bias+0x2352000){effects_reading=0;atomic_store(&state->error,EF_UNSUPPORTED);return;}effects_current.reads++;p->automatable=
#ifdef COMMAND_COMPONENT
 component_effect_automatable?component_effect_automatable(e,index):
#endif
 ((int (*)(void*,unsigned))(uintptr_t)(observer_image_bias+0x2352000))((void*)(uintptr_t)e,index)!=0;if(!effects_same(c,&d,&native)){effects_reading=0;return;}
   /* Preserve the native number of states. A concrete two-state override
    * is independent of legacy isDiscrete automation interpolation policy. */
   if(word(word(e)+0x60)!=observer_image_bias+0x2351690){effects_reading=0;atomic_store(&state->error,EF_UNSUPPORTED);return;}
   int steps=effects_integer(e,0x60,index);if(!effects_same(c,&d,&native)){effects_reading=0;return;}
   p->steps=steps>0?(unsigned)steps:0;
   /* Native Reverb has outer parameters but an empty generic AP parameter
    * array. This wrapper then returns finite zero as a fallback, not a
    * qualified default. Retain raw bits without admitting a reset command. */
   p->default_valid=0;
   float min,max;memcpy(&min,&p->minimum,4);memcpy(&max,&p->maximum,4);if(min!=0||max!=1){p->status=EF_UNSUPPORTED;effects_reading=0;effects_publish();return;}
  }
  float value=effects_scalar(e,0x40,index);if(!effects_same(c,&d,&native)||!(value>=0&&value<=1)){effects_reading=0;atomic_store(&state->invalid,1);return;}
  uint32_t bits;memcpy(&bits,&value,4);
  if(p->status!=EF_READY||p->bits!=bits||now-p->text_tick>=500){if(!effects_string(e,0x78,index,p->text)||!effects_same(c,&d,&native)){effects_reading=0;atomic_store(&state->invalid,1);return;}p->text_tick=now;}
  if(p->status!=EF_READY||p->bits!=bits){if(p->revision>UINT32_MAX-2){effects_reading=0;atomic_store(&state->error,EF_INVALID);return;}p->revision+=2;}p->bits=bits;p->tick=now;p->status=EF_READY;
 }
 effects_current.status=EF_READY;effects_reading=0;effects_publish();
}

/* Exact wrapper reads stay on the qualified UI owner. The facade remains the
 * native history/automation owner; this does not construct effect commands. */
static unsigned effects_request(Context *c,const CommandRequest *r,Track **out,uint32_t *function){
 uint32_t now=atomic_load(&mirror_state->heartbeat);float desired;memcpy(&desired,&r->bits,4);
 if(!command_effect(r->reserved)||r->field_incarnation!=r->effect_generation||!r->effect_generation||r->effect_slot>=EFFECT_SLOTS||r->effect_index>=EFFECT_MAX_PARAMETERS||r->effect_position>=EFFECT_MAX_PRESENTATION||!r->effect_key||!r->effect_ap||r->pad_owner||r->pad_index||r->pad_generation||r->global_owner||!(desired>=0&&desired<=1))return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 if(r->expires<r->created||r->expires-r->created>120000||r->expires<now)return C_EXPIRED;
 if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 unsigned enable=r->reserved==EFFECT_ENABLE;
 if(enable&&(r->effect_index||r->effect_position||(r->bits!=0&&r->bits!=0x3f800000u)))return C_FORMAT;
 EffectsDemand d;if(!effects_demand_read(&command_state->effects_interest,&d)||!d.enabled||d.until<=now||d.epoch!=r->epoch||d.serial!=r->serial||d.slot!=(enable?EFFECT_LIST:r->effect_slot)||(!enable&&d.page!=r->effect_position/EFFECT_PAGE))return C_SOURCE_CHANGED;
 if(atomic_load(&mirror_state->effects.invalid)||atomic_load(&mirror_state->effects.error)||effects_current.status!=EF_READY||effects_current.generation!=r->effect_generation||effects_current.slot!=d.slot||effects_current.page!=d.page)return C_SOURCE_CHANGED;
 EffectNative n;if(!effects_resolve(c,&d,&n,1))return C_NATIVE_MEMBERSHIP;
 Track *t=n.track;
 if(t->binding!=r->binding||t->incarnation!=r->incarnation||t->track_owner!=r->track_owner||t->program_owner!=r->program_owner||channel_owner_id(project)!=r->project_owner||n.keys[r->effect_slot]!=r->effect_key||n.aps[r->effect_slot]!=r->effect_ap||(!enable&&effects_current.slots[r->effect_slot].count<=r->effect_index)||*(const volatile unsigned char*)(uintptr_t)(t->program+0x1158))return C_IDENTITY;
 if(enable){
  EffectSlot *slot=effects_current.slots+r->effect_slot;uint32_t actual=0;
  if(slot->status!=EF_READY||!slot->enable_valid||slot->enable_tick>now||now-slot->enable_tick>=EFFECT_FRESH_MS||slot->enable_bits!=r->before_bits||slot->enable_revision!=r->before_revision||slot->enable_bits==r->bits)return C_SOURCE_CHANGED;
  effects_reading=1;int valid=effects_enable_read(c,&d,&n,r->effect_slot,&actual);effects_reading=0;
  if(!valid)return C_NATIVE_MEMBERSHIP;
  if(actual!=r->before_bits)return C_SOURCE_CHANGED;
  *out=t;*function=observer_image_bias+0x250ba74;return 0;
 }
 EffectParameter *p=effects_current.parameters+r->effect_position%EFFECT_PAGE;
 if(p->status!=EF_READY||p->index!=r->effect_index||p->position!=r->effect_position||p->tick>now||now-p->tick>=EFFECT_FRESH_MS||p->bits!=r->before_bits||p->revision!=r->before_revision||p->bits==r->bits)return C_SOURCE_CHANGED;
 *out=t;*function=observer_image_bias+0x250ba74;return 0;
}
static void effects_execute(Context *c){
 unsigned id=recording_call.id?recording_call.id:invocation.request,at=command_find(command_state,id);
 if(at==COMMAND_SLOTS||accepted[at].request.reserved!=EFFECT_PARAMETER)return;
 const CommandRequest *r=&accepted[at].request;
 /* This runs for EVERY native parameter setter call made while a command is in
  * flight on this thread, so it also sees writers that are not this command.
  * With automation Write armed, the application's own record path writes the
  * parameter through the same setter inside this window; treating that as a
  * failure latched a session-wide sticky C_TRACE_AMBIGUOUS and killed the whole
  * surface. Relevance and correctness are therefore separated: only a call this
  * lane cannot tell apart from its own still fails the trace. */
 if(source_requests[at]!=id||(recording_call.id&&invocation.request)){trace_fail(C_TRACE_AMBIGUOUS);return;}
 /* Ignored as foreign traffic. Each of these is proof that the call is NOT this
  * command's own setter call, not merely that it is unexpected:
  *  lr - this command's call can only enter from the one pinned call site of
  *   its live context, the ordinary queue body or the record callback;
  *  r[1] - this command names exactly one parameter index;
  *  d[0] - the dispatched value is fixed at admission and re-proved at the
  *   matching body, so another value is another write;
  *  r[0] type - this command's receiver is an effect parameter holder;
  *  effect_executed - this command's own call is receipted exactly once, so a
  *   later call is another writer's, or a repeat that cannot change the receipt
  *   already published for the first.
  * Ignoring cannot fabricate a receipt, only leave this command's own call free
  * to arrive. If it never does, effect_executed stays 0, effects_complete skips
  * the slot and this one request stays pending for its own client timeout. */
 if(c->lr!=observer_image_bias+(recording_call.id?0x250b5b4:0x25176d4)||c->r[1]!=r->effect_index||(uint32_t)c->d[0]!=r->bits||!typed(c->r[0],0x692e180)||atomic_load(effect_executed+at))return;
 /* Receiver is alive at its own setter entry. No later call uses this key. */
 uint32_t e=c->r[0],ap=word(e+0x68),tuple=word(e+0x70);
 /* A receiver whose identity tuple cannot be read is indistinguishable from
  * this command's own receiver and still fails. A readable tuple naming another
  * Program, kind or insert slot is another insert's parameter: foreign. */
 if(!pointer(tuple)){trace_fail(C_TRACE_AMBIGUOUS);return;}
 if(word(tuple)!=accepted[at].program||word(tuple+4)!=0x101||word(tuple+8)!=r->effect_slot)return;
 /* Every relevance test is now spent, so what remains is about this command's
  * own call. The window is proven only by the matching body: a call that got
  * this far and arrives outside it cannot be told apart from this command's
  * own, and this fault still precedes every receipt. A missing AP is likewise
  * this command's receiver in a state that cannot carry the receipt. */
 if((recording_call.id?recording_call.phase!=2:invocation.phase!=2)||!ap){trace_fail(C_TRACE_AMBIGUOUS);return;}
 atomic_store(effect_applied_key+at,e);atomic_store(effect_applied_ap+at,ap);
 event(c,CE_EFFECT_EXECUTE,id,e,ap,accepted[at].program,0x101,accepted[at].controller,r->bits,r->effect_index,r->effect_generation);
 atomic_store_explicit(effect_executed+at,id,memory_order_release);
}
/* An effect command's settled value is written by the audio thread a few
 * milliseconds after the dispatch returns, while effects_complete runs at the
 * TOP of a drain, before the slot scan. So the drain that dispatches always
 * misses its own value and the sample waits a whole 50 ms timer tick for a
 * value that has existed for about 45 ms of it. effects_settle waits here,
 * after the scan, for the audio-side completion of what THIS drain dispatched,
 * and only then reads back again.
 *
 * Bounded by construction on every path: a fixed count of fixed sleeps, so an
 * audio thread that is slow, stalled or not running at all costs the bound and
 * nothing more. nanosleep can only return early.
 *
 * The bound is 16 ms. The device samples of return-to-done were 0, 0 and 10 ms
 * (the last Write-armed), and the audio period is 2.9 ms. 12 ms would be 1.2x
 * the largest of three samples, which is not headroom; 16 ms is 1.6x it and
 * about five and a half periods, enough for the enqueue, a body, the optional
 * second Write-armed job and scheduling jitter. It is also under a third of
 * the 50 ms drain period, and JUCE resets the timer countdown before invoking
 * the callback, so even a full timeout cannot push the next drain out. The
 * wait exits the moment done arrives, so the bound only governs the case where
 * the gain has already been lost; raising it costs nothing in the common case
 * and lowering it only trades success rate for a shorter worst-case stall.
 *
 * On timeout the readback is NOT run, so the timeout path is exactly today's
 * behaviour: the sample happens at the next drain. */
#define EFFECT_SETTLE_STEPS 64u
#define EFFECT_SETTLE_STEP_NS 250000L
/* Component seam: counts the sleeps the wait actually performs, so a fixture
 * can tell a wait that succeeded early from one that ran to the bound, and
 * from one the quiet guard skipped, without measuring wall time. */
#ifndef EFFECT_SETTLE_SLEEP
#define EFFECT_SETTLE_SLEEP() ((void)0)
#endif
static void effects_complete(Context *c);
static int effects_settle_quiet(void){
 return !atomic_load(&mirror_state->meters.closing)&&!atomic_load(&command_state->stop_requested)&&
  !atomic_load_explicit(&command_state->new_project_intent,memory_order_seq_cst)&&
  !atomic_load(&command_state->trace_error)&&atomic_load_explicit(&observer_running,memory_order_seq_cst);
}
/* Exactly the two conditions effects_complete needs from the audio side, and
 * nothing else: it re-checks everything itself, so this is only a gate. */
static int effects_settled(unsigned mask){
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  if(!(mask&(1u<<at)))continue;
  unsigned seq=accepted[at].request.seq;
  if(!seq||atomic_load_explicit(&command_state->slots[at].done,memory_order_acquire)!=seq)return 0;
  if(accepted[at].request.reserved==EFFECT_PARAMETER&&atomic_load_explicit(effect_executed+at,memory_order_acquire)!=seq)return 0;
 }
 return 1;
}
/* A recording flight whose producer has already published its second job
 * cannot reach slot->done until that job completes, because recording_complete
 * holds done back for exactly that (recording-capture.inc:43-47). On the
 * device that job landed 31 ms after the dispatch - the application's recorder
 * latency, not ours, and far outside any bound this wait should accept. So
 * waiting on such a slot is a stall that cannot succeed: drop it from the mask
 * instead, and if nothing winnable is left, do not sleep at all.
 *
 * This changes nothing about when a sample may be taken or what it requires.
 * done, done_tick and every receipt are untouched, and a dropped slot is
 * sampled at the next drain exactly as it is today - the consumer requires the
 * value to be at or after done_tick, and done_tick is the LATER of the two
 * recording completions (mirror-input-core.h:652, :909), so an earlier sample
 * on this path would be a trace-ambiguous fault rather than an optimisation.
 *
 * It is a race in principle: a producer that has not published its second job
 * yet is not visible here, and that slot still waits and times out. The device
 * capture publishes it at +0 ms, before this wait runs, so this removes the
 * common cost rather than every cost. Re-checked on every iteration, so a job
 * published during the wait ends it too. */
static unsigned effects_settle_winnable(unsigned mask){
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  if(!(mask&(1u<<at)))continue;
  if(atomic_load_explicit(recording_work+at,memory_order_acquire)&&!atomic_load_explicit(recording_second_done+at,memory_order_acquire))mask&=~(1u<<at);
 }
 return mask;
}
static void effects_settle(Context *c,unsigned mask){
 if(!mask)return;
 for(unsigned step=0;step<=EFFECT_SETTLE_STEPS;step++){
  if(!effects_settle_quiet())return;
  mask=effects_settle_winnable(mask);if(!mask)return;
  if(effects_settled(mask)){effects_complete(c);return;}
  if(step<EFFECT_SETTLE_STEPS){EFFECT_SETTLE_SLEEP();struct timespec t={0,EFFECT_SETTLE_STEP_NS};nanosleep(&t,NULL);}
 }
}
static void effects_complete(Context *c){
 if(!atomic_load_explicit(&effects_pending,memory_order_acquire)||token()!=command_owner||!active()||!command_queues||c->r[0]!=command_queues+4||word(c->r[0]+0xf0))return;
 /* Completion precedes the ordinary page scan, including after leaving Effects.
  * Missing execution remains unsealed; never manufacture a setter receipt. */
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  CommandSlot *slot=command_state->slots+at;unsigned seq=atomic_load_explicit(&slot->done,memory_order_acquire);
  if(!seq||seq!=atomic_load(&slot->published)||seq==atomic_load(&slot->reclaimed)||accepted[at].request.seq!=seq||!command_effect(accepted[at].request.reserved)||effects_receipted(at)||(accepted[at].request.reserved==EFFECT_PARAMETER&&atomic_load_explicit(effect_executed+at,memory_order_acquire)!=seq))continue;
  const CommandRequest *r=&accepted[at].request;unsigned enable=r->reserved==EFFECT_ENABLE;uint32_t e=enable?r->effect_key:atomic_load(effect_applied_key+at),ap=enable?r->effect_ap:atomic_load(effect_applied_ap+at);unsigned outcome=EF_READY,bits=0;
  EffectsDemand d={.enabled=1,.epoch=r->epoch,.serial=r->serial,.slot=r->effect_slot,.page=r->effect_position/EFFECT_PAGE};EffectNative n;
  if((enable&&atomic_load_explicit(effect_executed+at,memory_order_acquire)!=seq)||atomic_load(&mirror_state->effects.error)||atomic_load(effect_invalidated+at)==seq||r->epoch!=epoch||e!=r->effect_key||ap!=r->effect_ap||!effects_resolve(c,&d,&n,0)||n.track->track_owner!=r->track_owner||n.track->program_owner!=r->program_owner||n.keys[r->effect_slot]!=e||n.aps[r->effect_slot]!=ap)outcome=EF_INVALID;
  else if(enable){
   effects_reading=1;int valid=effects_enable_read(c,&d,&n,r->effect_slot,&bits);effects_reading=0;
   if(!valid)outcome=EF_INVALID;
   else{
    uint32_t now=atomic_load(&mirror_state->heartbeat);
    if(effects_current.generation==r->effect_generation&&effects_current.epoch==r->epoch&&effects_current.serial==r->serial){effects_enable_store(effects_current.slots+r->effect_slot,bits,now);effects_current.tick=now;effects_publish();}
    /* DONE precedes the separate UI mirror. Poll only at existing owner
     * drains until matching actual host state or the request deadline.
     * Another native writer can produce the same value; no causal DSP claim. */
    if(bits!=r->bits){if(now<r->expires)continue;outcome=EF_INVALID;}
   }
  }
  else{
   effects_reading=1;int count=effects_count(n.keys[r->effect_slot]);
   if(count<=0||r->effect_index>=(unsigned)count||!effects_same(c,&d,&n)||word(word(e)+0x40)!=observer_image_bias+0x2350278)outcome=EF_INVALID;
   else {float v=effects_scalar(n.keys[r->effect_slot],0x40,r->effect_index);if(!effects_same(c,&d,&n)||!(v>=0&&v<=1))outcome=EF_INVALID;else{
    memcpy(&bits,&v,4);
    /* Publish this actual priority sample through the same page owner before
     * a receipt can release the next encoder gesture. Otherwise the client
     * can rebase onto a pre-command page value for an entire scan rotation. */
    if(effects_current.generation==r->effect_generation&&effects_current.epoch==r->epoch&&effects_current.serial==r->serial&&effects_current.slot==r->effect_slot&&effects_current.page==r->effect_position/EFFECT_PAGE){
     EffectParameter *p=effects_current.parameters+r->effect_position%EFFECT_PAGE;char text[EFFECT_TEXT];
     if(p->status==EF_READY&&p->index==r->effect_index&&p->position==r->effect_position){
      if(!effects_string(e,0x78,r->effect_index,text)||!effects_same(c,&d,&n))outcome=EF_INVALID;
      else if(p->bits!=bits&&p->revision>UINT32_MAX-2){atomic_store(&mirror_state->effects.error,EF_INVALID);outcome=EF_INVALID;}
      else{if(p->bits!=bits)p->revision+=2;p->bits=bits;p->tick=p->text_tick=atomic_load(&mirror_state->heartbeat);memcpy(p->text,text,sizeof(text));effects_current.tick=p->tick;effects_publish();}
     }
    }
   }}
   effects_reading=0;
  }
  event(c,CE_EFFECT_VALUE,seq,e,ap,accepted[at].program,0x101,accepted[at].controller,bits,r->effect_index,outcome);
  atomic_store_explicit(effect_sampled+at,seq,memory_order_release);atomic_store_explicit(effect_watch+at,0,memory_order_release);atomic_store(&effects_pending,atomic_load(&effects_pending)&~(1u<<at));
 }
}
