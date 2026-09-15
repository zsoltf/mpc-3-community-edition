/* Real observer/reader/command/consumer composition; native object allocations,
 * outer getter bodies, history/factory bodies and audio scheduling substituted. */
#define main command_suite_not_called
#include "command-component.c"
#undef main
static unsigned char insert_object[0xb0],effect_objects[5][0xc0],effect_aps[5][16];
static uint32_t effect_slots[4],effect_tuples[5][3],payload[32];
static uint32_t enable_ui[4],enable_audio[4],enable_echo[4];static unsigned enable_echo_pending,hold_enable_echo;
static float parameters[20];static unsigned ordinary=1,skip_setter,dispatches,reverb_fixture;
/* Full outer schema observed in the native r6 Reverb capture. The fixture is
 * independent of the implementation's compatibility selector/order table. */
static const char *const reverb_fixture_names[]={"Low Cut","Pre-Delay","ER/Tail Mix","Room Size","Time","Time High","Freq High","High Cut","Mix","Input Width","ER Type","ER Length","Reverb Delay","Density","Ambience","Time Low","Freq Low","Output Width"};
static int reverb_source(uint32_t e){return reverb_fixture&&e==effect_slots[0];}
/* Native descriptor fill is substituted; tuple is from shipped Reverb Init XPL.
 * The actual retained AP/String ABI remains a native acceptance boundary. */
static int get_description(uint32_t e,uint32_t ap,EffectDescription *out){
 (void)ap;memset(out,0,sizeof(*out));if(!reverb_source(e))return 0;
 out->valid=1;out->uid=0x61465276;snprintf(out->name,EFFECT_TEXT,"AIR Reverb");snprintf(out->format,EFFECT_TEXT,"MPC");return 1;
}
static int get_count(uint32_t e){return reverb_source(e)?18:20;}
static int get_string(uint32_t e,unsigned method,unsigned index,char *out){if(method==0x10)snprintf(out,EFFECT_TEXT,"%s",reverb_source(e)?"AIR Reverb":"Fixture Delay");else if(method==0x74){if(reverb_source(e))snprintf(out,EFFECT_TEXT,"%s",reverb_fixture_names[index]);else snprintf(out,EFFECT_TEXT,"Parameter %u",index);}else snprintf(out,EFFECT_TEXT,"%.2f sec",parameters[index]);return 1;}
static float get_scalar(uint32_t e,unsigned method,unsigned index){(void)e;return method==0x48?0:method==0x50?1:method==0x58?0.5f:parameters[index];}
static int get_automatable(uint32_t e,unsigned index){(void)e;(void)index;return 1;}
static int get_integer(uint32_t e,unsigned method,unsigned index){(void)e;return method==0x68?0:index==1?2:index==2?0x7fffffff:0;}
static float get_enable(uint32_t program,unsigned slot){require(program==ptr(program_objects[0])&&slot<4,"host-enable getter uses current Program/slot");float v;memcpy(&v,enable_ui+slot,4);return v;}
static void tick(void){if(!hold_enable_echo){for(unsigned i=0;i<4;i++)if(enable_echo_pending&(1u<<i))enable_ui[i]=enable_echo[i];enable_echo_pending=0;}atomic_store(&fixture->heartbeat,atomic_load(&fixture->heartbeat)+20);Context c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);}
static void *apply_effect(void *unused){
 (void)unused;union {uint64_t align;unsigned char bytes[sizeof(Context)+0x18];} frames;Context *entry=(Context*)(frames.bytes+(ordinary?8:0x18)),*end=(Context*)frames.bytes;
 uint32_t p=ptr(program_objects[0]),code=ordinary?payload[3]:payload[6],bits=ordinary?payload[4]:payload[13],index=(code-0x6000)&0xfff,slot=code>=0x146&&code<=0x149?code-0x146:(code-0x6000)>>12;
 *entry=context();entry->r[0]=ptr(ordinary?payload:payload+2);call(entry,ordinary?M_QUEUE_ENTER:RC_ENTER);
 *end=context();end->r[0]=p;if(ordinary){end->r[1]=0x101;end->r[2]=code;end->d[0]=bits;end->lr=0x25178d4;call(end,M_COMMAND_BODY);}else{end->r[2]=ptr(payload+2)+0x18;end->lr=0x1c04b94;call(end,RC_APPLY);}
 if(code>=0x146&&code<=0x149){enable_audio[slot]=enable_echo[slot]=bits;enable_echo_pending|=1u<<slot;}
 else if(!skip_setter){Context setter=context();setter.r[0]=effect_slots[slot];setter.r[1]=index;setter.d[0]=bits;setter.lr=ordinary?0x25176d4:0x250b5b4;call(&setter,PX_SET);float v;memcpy(&v,&bits,4);parameters[index]=floorf(v*10)/10;}
 *end=context();end->r[4]=ptr(ordinary?payload:payload+2);end->r[3]=payload[0];call(end,ordinary?M_QUEUE_DONE:RC_DONE);return NULL;
}
static void facade(uint32_t f,uint32_t p,unsigned kind,unsigned code,float value){
 dispatches++;require(f==ptr(recording_fixture_factory)&&p==ptr(program_objects[0])&&kind==0x101&&((code>=0x6000&&code<0xa000)||(code>=0x146&&code<=0x149)),"facade receives selected Program/kind and parameter or native326+slot enable controller");unsigned bits;memcpy(&bits,&value,4);memset(payload,0,sizeof(payload));
 if(ordinary){payload[0]=p+0x40;payload[1]=p;payload[2]=kind;payload[3]=code;payload[4]=bits;}
 else{uint32_t n=ptr(payload+2);payload[0]=0x13cf830;payload[1]=0x13d18b0;put(n,f+4);put(n+8,f);put(n+0xc,kind);put(n+0x10,code);put(n+0x14,p);put(n+0x20,2);put(n+0x28,kind);put(n+0x2c,bits);put(n+0x30,code);put(n+0x50,ptr(track_objects[0]));Context c=context();c.r[7]=ptr(queue_object)+0xfc;c.r[9]=(f+4)|1u;c.r[10]=ptr(payload);call(&c,JG_PUBLISH);}
 pthread_t t;require(!pthread_create(&t,NULL,apply_effect,NULL)&&!pthread_join(t,NULL),"substituted audio job on separate source thread");
}
static void prepare(void){
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"bounded Effects component state");initial(1);command_initialize();observer_image_bias=0;observer_running=1;atomic_store(&command_state->alive,1);seed_fixture(0,0,0x3f000000,1);load_fixture();
 uint32_t p=ptr(program_objects[0]),i=ptr(insert_object);put(p+0x40,ptr(queue_object));put(p+0x5e0,i);put(i,0x68ac370);put(i+0x44,ptr(queue_object));put(i+0x60,ptr(effect_slots));put(i+0x64,ptr(effect_slots)+16);put(i+0x68,ptr(effect_slots)+16);
 for(unsigned slot=0;slot<5;slot++){uint32_t e=ptr(effect_objects[slot]);put(e,0x692e180);put(e+0x68,ptr(effect_aps[slot]));effect_tuples[slot][0]=p;effect_tuples[slot][1]=0x101;effect_tuples[slot][2]=slot<4?slot:0;put(e+0x70,ptr(effect_tuples[slot]));if(slot<4)effect_slots[slot]=e;}
 Context c=context();c.r[5]=ptr(project_object)+0x78;c.r[7]=ptr(track_objects[0]);call(&c,CH_SELECTION_COMMIT);
 component_effect_description=get_description;component_effect_enable=get_enable;for(unsigned i=0;i<4;i++)enable_ui[i]=enable_audio[i]=0x3f800000;component_effect_count=get_count;component_effect_string=get_string;component_effect_scalar=get_scalar;component_effect_automatable=get_automatable;component_effect_integer=get_integer;component_recording_dispatch=facade;for(unsigned j=0;j<20;j++)parameters[j]=0.5f;
}
static void watch(unsigned slot,unsigned page){EffectsInterest *d=&command_state->effects_interest;unsigned rev=atomic_load(&d->revision);atomic_store(&d->revision,rev+1);atomic_store(&d->enabled,1);atomic_store(&d->epoch,epoch);atomic_store(&d->serial,tracks[0].serial);atomic_store(&d->slot,slot);atomic_store(&d->page,page);atomic_store(&d->until,100000);atomic_store(&d->revision,rev+2);}
static void edit(int retire){
 CopiedMirror s;require(copy_mirror(fixture,&s)&&s.effects_available,"copied Effects page ready");MirrorBank bank;bank_init(&bank);bank.assignment=BA_EFFECT;bank_apply(&bank,&s,s.heartbeat);bank.effects_slot=s.effects.slot;bank.effects_page=s.effects.page;MirrorInput in={.commands=command_state};
 input_effect_edit(&in,&bank,&s,0,19,0,s.heartbeat);input_pump(&in,&bank,&s,s.heartbeat);unsigned seq=atomic_load(&command_state->published),at=command_find(command_state,seq);require(at<COMMAND_SLOTS&&!in.error,"production encoder handler publishes typed Effects request");
 tick();require(!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error)&&atomic_load(&command_state->slots[at].done)==seq,"ordinary/recording native job returns through existing DONE");command_retire();require(atomic_load(&command_state->slots[at].sealed)!=seq,"DONE without priority UI readback cannot seal");
 if(retire){Context c=context();c.r[0]=ptr(insert_object);c.r[1]=0;c.r[2]=ptr(effect_objects[4]);call(&c,PX_REPLACE);effect_slots[0]=ptr(effect_objects[4]);}
 /* Leaving the page does not strand an admitted completion. */
 if(retire)atomic_store(&command_state->effects_interest.enabled,0);
 tick();command_retire();require(atomic_load(&command_state->slots[at].sealed)==seq,"fresh readback or explicit retired outcome seals after DONE");
 atomic_store(&fixture->heartbeat,atomic_load(&fixture->heartbeat)+1);copy_mirror(fixture,&s);input_drain(&in,&bank,&s,s.heartbeat);require(!in.error&&in.settled==1,"actual consumer validates and settles effect execution/readback");require(in.flights[at].effect_value.revision==(retire?EF_INVALID:EF_READY),"replacement is unavailable completion, never replacement value success");command_retire();require(command_idle(command_state),"Effects transaction reclaims through original owner");
 if(!retire){uint32_t actual;memcpy(&actual,&parameters[s.effects.parameters[0].index],4);require(s.effects.parameters[0].bits==actual,"priority completion publishes actual value before next page scan (rapid reversal baseline)");
  input_effect_edit(&in,&bank,&s,0,-1,0,s.heartbeat);float wanted=parameters[s.effects.parameters[0].index]-1.0f/127.0f;uint32_t wanted_bits;memcpy(&wanted_bits,&wanted,4);require(in.effects[0].request.bits==wanted_bits,"acknowledged flight rebases reversal on actual quantized receipt, not previous request");
  in.effects[0].pending=0;input_drain(&in,&bank,&s,s.heartbeat);input_effect_edit(&in,&bank,&s,0,-1,0,s.heartbeat);require(in.effects[0].request.bits==wanted_bits,"reclaimed flight reversal keeps same actual baseline without scan wait");
  /* A new external native value remains authoritative after owner publication. */
  watch(s.effects.slot,s.effects.page);parameters[s.effects.parameters[0].index]=0.8f;for(unsigned j=0;j<8;j++)tick();copy_mirror(fixture,&s);in.effects[0].pending=0;input_effect_edit(&in,&bank,&s,0,-1,0,s.heartbeat);wanted=0.8f-1.0f/127.0f;memcpy(&wanted_bits,&wanted,4);require(in.effects[0].request.bits==wanted_bits,"new external source value replaces prior local intent baseline");
}
}
static void fast_reverse(void){
 CopiedMirror s;require(copy_mirror(fixture,&s)&&s.effects_available,"fast reverse copied page");MirrorBank bank;bank_init(&bank);bank.assignment=BA_EFFECT;bank_apply(&bank,&s,s.heartbeat);bank.effects_slot=s.effects.slot;bank.effects_page=s.effects.page;MirrorInput in={.commands=command_state};
 input_effect_edit(&in,&bank,&s,0,19,0,s.heartbeat);input_pump(&in,&bank,&s,s.heartbeat);unsigned first=atomic_load(&command_state->published);
 input_effect_edit(&in,&bank,&s,0,-1,0,s.heartbeat);input_pump(&in,&bank,&s,s.heartbeat);require(in.effects[0].pending&&atomic_load(&command_state->published)==first,"opposite turn remains unsent while first native command owns parameter");
 tick();tick();command_retire();atomic_fetch_add(&fixture->heartbeat,1);copy_mirror(fixture,&s);input_drain(&in,&bank,&s,s.heartbeat);command_retire();require(!in.error&&in.settled==1&&command_idle(command_state),"first quantized command closes before relative rebase");
 float actual=parameters[s.effects.parameters[0].index],wanted=actual-1.0f/127.0f;uint32_t expected;memcpy(&expected,&wanted,4);input_pump(&in,&bank,&s,s.heartbeat);CommandRequest next;unsigned seq=atomic_load(&command_state->published),at=command_find(command_state,seq);
 require(seq==first+1&&at<COMMAND_SLOTS&&command_request_read(command_state->slots+at,seq,&next)&&next.bits==expected,"pre-ack reversal next command is BELOW actual readback, not below prior requested value");
 tick();tick();command_retire();atomic_fetch_add(&fixture->heartbeat,1);copy_mirror(fixture,&s);input_drain(&in,&bank,&s,s.heartbeat);command_retire();require(!in.error&&in.settled==2&&command_idle(command_state),"rebased reversal follows real ordinary/recording closure and source settlement");
}
static void observed_reverb_mapping(void){
 /* Native observed page{0,1,2} and Mix50 receipt establish this branch's premise;
  * getters/facade/audio remain substituted here, not a native acceptance claim. */
 static const unsigned order[3][8]={{1,3,4,8,10,11,2,UINT32_MAX},{9,17,12,UINT32_MAX,14,13,UINT32_MAX,UINT32_MAX},{5,6,7,UINT32_MAX,15,16,0,UINT32_MAX}};
 reverb_fixture=1;parameters[8]=1;Context c=context();c.r[0]=ptr(insert_object);call(&c,PX_REPLACE);watch(EFFECT_LIST,0);for(unsigned j=0;j<24;j++)tick();CopiedMirror s;
 require(copy_mirror(fixture,&s)&&s.effects.status==EF_READY&&s.effects.slots[0].count==18&&s.effects.slots[0].presentation_count==24&&s.effects.slots[0].layout!=0,"shipped AIR Reverb identity selects observed padded24 presentation independently of native18 count");
 unsigned seen=0;for(unsigned page=0;page<3;page++){
  watch(0,page);for(unsigned j=0;j<8;j++)tick();require(copy_mirror(fixture,&s)&&s.effects.status==EF_READY,"mapped page reaches real copied reader");
  for(unsigned strip=0;strip<8;strip++){const EffectParameter *p=s.effects.parameters+strip;unsigned expected=order[page][strip];
   require(p->position==page*8+strip&&p->index==expected&&(expected==UINT32_MAX?p->status==EF_EMPTY:p->status==EF_READY),"observed presentation order/empty cells retain explicit native index");
   require(!p->default_valid,"finite outer default cannot qualify reset when generic AP storage may be absent");
   if(expected!=UINT32_MAX){require(!(seen&(1u<<expected))&&!strcmp(p->name,reverb_fixture_names[expected]),"mapped name belongs to native index with no duplicate");seen|=1u<<expected;}
  }
 }
 require(seen==((1u<<18)-1),"three observed pages preserve all18 native parameters exactly once");
 watch(0,0);for(unsigned j=0;j<8;j++)tick();copy_mirror(fixture,&s);MirrorBank bank;bank_init(&bank);bank.assignment=BA_EFFECT;bank_apply(&bank,&s,s.heartbeat);bank.effects_slot=0;bank.effects_page=0;MirrorInput in={.commands=command_state};ordinary=1;float other=parameters[3];
 input_effect_edit(&in,&bank,&s,3,0,2,s.heartbeat);require(!in.effects[3].pending,"Shift reset on unqualified Reverb default emits no request");
 input_effect_edit(&in,&bank,&s,0,0,1,s.heartbeat);require(in.effects[0].pending,"ordinary native two-state click remains admitted independently of default eligibility");in.effects[0].pending=0;
 input_effect_edit(&in,&bank,&s,3,-63,0,s.heartbeat);input_pump(&in,&bank,&s,s.heartbeat);unsigned seq=atomic_load(&command_state->published),at=command_find(command_state,seq);CommandRequest request;
 require(at<COMMAND_SLOTS&&command_request_read(command_state->slots+at,seq,&request)&&request.effect_index==8&&request.effect_position==3,"Mix moved from native page1 to presentationstrip3 publishes index8, never position3 as controller");
 tick();tick();command_retire();atomic_fetch_add(&fixture->heartbeat,1);copy_mirror(fixture,&s);input_drain(&in,&bank,&s,s.heartbeat);command_retire();
 require(!in.error&&in.settled==1&&command_idle(command_state)&&parameters[8]==0.5f&&parameters[3]==other,"mapped request changes Mix through actual consumer receipt/reclaim while nativeparameter3 stays unchanged");
 require(s.effects.parameters[3].index==8&&s.effects.parameters[3].position==3&&s.effects.parameters[3].bits==0x3f000000,"priority actual readback updates mapped Mix cell, not native-index modulo cell");
 reverb_fixture=0;c=context();c.r[0]=ptr(insert_object);call(&c,PX_REPLACE);watch(0,0);for(unsigned j=0;j<13;j++)tick();
}

static void observed_insert_enable(void){
 /* Native r8 slot1/controller327 OFF closed after host readback at92ms.
  * These fixtures retain its actual source/body/UI-mirror distinction; they
  * do not execute the native getter, facade, audio engine or physical MCU. */
 for(unsigned path=0;path<3;path++){
  ordinary=path!=1;hold_enable_echo=1;watch(EFFECT_LIST,0);for(unsigned i=0;i<8;i++)tick();CopiedMirror s;require(copy_mirror(fixture,&s)&&s.effects.slots[1].enable_valid,"native-like host enable is published through retained list state");
  MirrorBank bank;bank_init(&bank);bank.assignment=BA_EFFECT;bank_apply(&bank,&s,s.heartbeat);MirrorInput in={.commands=command_state};unsigned before=dispatches;
  if(!path){
   input_effect_enable(&in,&bank,&s,1,s.heartbeat);input_effect_enable(&in,&bank,&s,1,s.heartbeat);input_pump(&in,&bank,&s,s.heartbeat);require(!input_pending(&in)&&dispatches==before&&command_idle(command_state),"two unsent toggles coalesce to unchanged actual state without native command");
   s.effects.slots[1].enable_tick=s.heartbeat-EFFECT_FRESH_MS;input_effect_enable(&in,&bank,&s,1,s.heartbeat);require(!in.effects[1].pending,"stale enable state cannot generate a toggle");copy_mirror(fixture,&s);
  }
  uint32_t old=enable_ui[1],wanted=old?0:0x3f800000;input_effect_enable(&in,&bank,&s,1,s.heartbeat);input_pump(&in,&bank,&s,s.heartbeat);unsigned seq=atomic_load(&command_state->published),at=command_find(command_state,seq);CommandRequest r;
  require(at<COMMAND_SLOTS&&command_request_read(command_state->slots+at,seq,&r)&&r.reserved==EFFECT_ENABLE&&r.effect_slot==1&&command_request_controller(&r)==327&&r.bits==wanted,"production list toggle publishes native327 exact inverse with instance generation");
  tick();tick();command_retire();require(atomic_load(&command_state->slots[at].done)==seq&&atomic_load(&command_state->slots[at].sealed)!=seq&&enable_audio[1]==wanted&&enable_ui[1]==old,"ordinary/recording DONE and apply-state change do not certify delayed host mirror");
  copy_mirror(fixture,&s);input_drain(&in,&bank,&s,s.heartbeat);require(!in.error&&!in.settled,"consumer waits without inventing matching UI source");
  uint32_t prior_key=effect_slots[1];
  if(path==2){Context c=context();c.r[0]=ptr(insert_object);c.r[1]=1;c.r[2]=ptr(effect_objects[4]);call(&c,PX_REPLACE);effect_tuples[4][2]=1;effect_slots[1]=ptr(effect_objects[4]);}
  hold_enable_echo=0;tick();command_retire();atomic_fetch_add(&fixture->heartbeat,1);copy_mirror(fixture,&s);input_drain(&in,&bank,&s,s.heartbeat);command_retire();
  require(!in.error&&in.settled==1&&command_idle(command_state),"enable drains through real consumer settlement/reclamation after ordinary or recording completion");
  require(in.flights[at].effect_value.revision==(path==2?EF_INVALID:EF_READY),"replacement completion cannot certify matching replacement host value");
  if(path!=2)require(in.flights[at].effect_seen==2&&in.flights[at].effect_value.bits==wanted&&s.effects.slots[1].enable_bits==wanted,"matching host enable receipt has no fabricated parameter setter event");
  else{Context c=context();c.r[0]=ptr(insert_object);call(&c,PX_REPLACE);effect_slots[1]=prior_key;effect_tuples[4][2]=0;}
 }
 /* An external writer between copied intent and fresh admission is refused
  * before the native facade; the owner reclaims it without false settlement. */
 watch(EFFECT_LIST,0);for(unsigned i=0;i<9;i++)tick();CopiedMirror s;copy_mirror(fixture,&s);MirrorBank bank;bank_init(&bank);bank.assignment=BA_EFFECT;bank_apply(&bank,&s,s.heartbeat);MirrorInput in={.commands=command_state};unsigned before=dispatches;
 input_effect_enable(&in,&bank,&s,1,s.heartbeat);input_pump(&in,&bank,&s,s.heartbeat);unsigned seq=atomic_load(&command_state->published),at=command_find(command_state,seq);enable_ui[1]=enable_ui[1]?0:0x3f800000;tick();command_retire();copy_mirror(fixture,&s);input_drain(&in,&bank,&s,s.heartbeat);
 require(at<COMMAND_SLOTS&&!in.error&&in.refused==1&&!in.settled&&dispatches==before&&atomic_load(&command_state->slots[at].rejected)==C_SOURCE_CHANGED&&command_idle(command_state),"fresh native pre-value rejects stale inverse before dispatch, distinct from settlement");
 puts("PASS observed327 host-enable route: ordinary/recording DONE waits for actual UI mirror, replacement outcome, stale admission and unsent no-op (native bodies/storage/audio scheduling substituted)");
}

static uint32_t chooser_context_fixture,chooser_pages_fixture;static unsigned chooser_popup,chooser_calls;
static unsigned chooser_is_open(void *pages,unsigned page){require(ptr(pages)==chooser_pages_fixture&&page==0x30a8,"native popup query ABI");return chooser_popup;}
static void chooser_bind(void *bridge,void *program){(void)bridge;chooser_calls++;put(chooser_context_fixture+0x34,ptr(program));put(chooser_context_fixture+0x38,0x101);}
static void chooser_launch(void *list,unsigned slot){(void)list;chooser_calls++;put(chooser_context_fixture+0x3c,slot);chooser_popup=1;}
static void chooser_checks(void){
 unsigned char *objects=calloc(1,0x5000);require(objects!=NULL,"rooted chooser fixture storage");
 uint32_t tmi=ptr(objects),main=tmi+0x3600,content=tmi+0x3700,clip=tmi+0x3800,panel=tmi+0x3900,bridge=tmi+0x3a00,overlay=tmi+0x3b00,node=tmi+0x3e00;
 chooser_context_fixture=tmi+0x4000;chooser_pages_fixture=tmi+0x4100;
 put(root+0x83c,tmi);put(tmi,0x699efd0);put(tmi+0x124,root);put(tmi+0x128,chooser_pages_fixture);put(root+0x114,chooser_pages_fixture);put(root+0x1fc,chooser_context_fixture);
 put(tmi+0x598,main);put(main,0x69e6b98);put(main+0xe8,content);put(content,0x69e7608);put(content+0x9c,clip);put(clip,0x69e9314);put(clip+0xc8,panel);put(panel,0x69e96a8);put(panel+0xc8,bridge);put(bridge,0x69e97b8);put(bridge+0x2c,chooser_context_fixture);put(bridge+0x48,chooser_pages_fixture);
 put(tmi+0x34ec,node);put(tmi+0x34f8,1);put(node+0x10,0x30fa);put(node+0x14,overlay);put(node+0x18,node+0x20);put(overlay,0x69ca244);put(overlay+0x18c,0x69ac190);put(overlay+0x250,chooser_pages_fixture);put(overlay+0x254,chooser_context_fixture);
 const unsigned native[]={0x157507c,0x2b068f0,0x340e5d4};void *functions[]={chooser_is_open,chooser_bind,chooser_launch};
 for(unsigned i=0;i<3;i++){uint32_t page=native[i]&~4095u;require(mmap((void*)(uintptr_t)page,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED,"native chooser call substitution");uint32_t *code=(uint32_t*)(uintptr_t)native[i];jump(&code,ptr(functions[i]));__builtin___clear_cache((char*)(uintptr_t)native[i],(char*)code);}
 for(unsigned replace=0;replace<2;replace++){
  unsigned slot=replace?0:2;Context change=context();change.r[0]=ptr(insert_object);change.r[1]=slot;change.r[2]=replace?effect_slots[slot]:0;if(!replace){call(&change,PX_REPLACE);effect_slots[slot]=0;}
  watch(replace?slot:EFFECT_LIST,0);for(unsigned j=0;j<13;j++)tick();CopiedMirror copy;require(copy_mirror(fixture,&copy)&&copy.effects.status==EF_READY,"chooser list/parameter source ready");MirrorBank bank;bank_init(&bank);bank.assignment=BA_EFFECT;bank_apply(&bank,&copy,copy.heartbeat);bank.effects_slot=replace?slot:EFFECT_LIST;MirrorInput in={.commands=command_state};
  unsigned oldkey=effect_slots[slot],oldgen=copy.effects.generation;input_effect_chooser(&in,&bank,&copy,slot,replace,copy.heartbeat);input_pump(&in,&bank,&copy,copy.heartbeat);unsigned seq=atomic_load(&command_state->published),at=command_find(command_state,seq);require(at<COMMAND_SLOTS&&in.submitted==1,"chooser input publishes typed same-slot add/replace request");
  tick();command_retire();require(chooser_popup&&chooser_calls==2*(replace+1)&&word(chooser_context_fixture+0x34)==ptr(program_objects[0])&&word(chooser_context_fixture+0x3c)==slot&&effect_slots[slot]==oldkey&&atomic_load(&command_state->slots[at].sealed)==seq,"actual chooser service observes popup/context, preserves old insert and seals only open acceptance");
  tick();copy_mirror(fixture,&copy);input_drain(&in,&bank,&copy,copy.heartbeat);command_retire();require(!in.error&&in.settled==1&&command_idle(command_state),"actual chooser receipt consumer reclaims without waiting for human selection");
  /* Open chooser blocks a new request before binder/context mutation. */
  CommandRequest busy=accepted[at].request;unsigned before=chooser_calls;unsigned busy_at=submit_fixture(busy);command_retire();require(atomic_load(&command_state->slots[busy_at].rejected)==C_BUSY&&chooser_calls==before,"already-open native popup never retargeted");
  chooser_popup=0;tick();copy_mirror(fixture,&copy);require(copy.effects.chooser.status==EC_CANCELLED&&effect_slots[slot]==oldkey&&copy.effects.generation==oldgen,"Cancel preserves occupied/empty slot and generation");
 }
 /* Fresh replacement inventory drives auto-entry; the load itself is native
  * UI-owned and is represented here by its real lifecycle source hook. */
 watch(0,0);for(unsigned j=0;j<9;j++)tick();CopiedMirror copy;copy_mirror(fixture,&copy);MirrorBank bank;bank_init(&bank);bank.assignment=BA_EFFECT;bank_apply(&bank,&copy,copy.heartbeat);bank.effects_slot=0;MirrorInput in={.commands=command_state};input_effect_chooser(&in,&bank,&copy,0,1,copy.heartbeat);input_pump(&in,&bank,&copy,copy.heartbeat);tick();command_retire();tick();copy_mirror(fixture,&copy);input_drain(&in,&bank,&copy,copy.heartbeat);command_retire();
 Context change=context();change.r[0]=ptr(insert_object);change.r[1]=0;change.r[2]=ptr(effect_objects[4]);call(&change,PX_REPLACE);effect_slots[0]=ptr(effect_objects[4]);chooser_popup=0;
 for(unsigned j=0;j<14;j++){tick();}copy_mirror(fixture,&copy);require(copy.effects.chooser.status==EC_LOADED&&copy.effects.chooser.key==effect_slots[0]&&copy.effects.chooser.ap==ptr(effect_aps[4]),"fresh UI replacement inventory distinct from open receipt");bank_apply(&bank,&copy,copy.heartbeat);require(bank.effects_slot==0&&bank.effects_page==0,"fresh loaded slot opens its parameter page");
 effects_chooser.status=EC_NONE;put(root+0x83c,0);free(objects);
 for(unsigned i=0;i<3;i++)munmap((void*)(uintptr_t)(native[i]&~4095u),4096);
 puts("PASS chooser actual rooted resolution/input/three-event acceptance/consumer, busy popup, cancel and fresh inventory entry; native binder/popup and replacement object storage substituted, DSP completion unobserved");
}

int main(void){
 alarm(30);require(mmap((void*)0x692e000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED,"effect vtable fixture");require(mmap((void*)0x6930000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED,"program vtable fixture");put(0x6930c28,0x250ba74);put(0x6930c78,0x1375c68);
 const unsigned offset[]={0x10,0x30,0x40,0x48,0x50,0x58,0x60,0x68,0x74,0x78,0x90},methods[]={0x234fd38,0x234ff08,0x2350278,0x2350670,0x2350a90,0x2350eb8,0x2351690,0x23512b0,0x2351c50,0x2351e28,0x2352000};for(unsigned j=0;j<11;j++)put(0x692e180+offset[j],methods[j]);prepare();
 tick();require(!effects_current.reads,"no interest means zero native getter calls");watch(EFFECT_LIST,0);for(unsigned j=0;j<5;j++)tick();CopiedMirror s;require(copy_mirror(fixture,&s)&&s.effects_available&&s.effects.status==EF_READY&&!strcmp(s.effects.slots[0].name,"Fixture Delay")&&s.effects.slots[3].count==20,"actual retained reader exposes four inserts");
 unsigned reads=effects_current.reads;tick();require(effects_current.reads==reads+1,"unchanged list caches metadata and samples only one host-enable mirror per drain");watch(0,0);for(unsigned j=0;j<8;j++)tick();require(copy_mirror(fixture,&s)&&s.effects.parameters[7].status==EF_READY,"bounded visible eight parameters reach copied reader");require(s.effects.parameters[1].steps==2&&s.effects.parameters[2].steps==0x7fffffff&&!s.effects.parameters[0].steps,"native count survives independent legacy discretefalse; defaultlarge and unknown counts preserved");edit(0);fast_reverse();
 watch(0,1);for(unsigned j=0;j<13;j++)tick();require(copy_mirror(fixture,&s)&&s.effects.parameters[0].index==8,"page selects outer index8 instead of inner AP index guesses");ordinary=0;edit(0);fast_reverse();observed_reverb_mapping();observed_insert_enable();chooser_checks();
 watch(0,0);for(unsigned j=0;j<13;j++)tick();ordinary=1;edit(1);
 watch(0,0);for(unsigned j=0;j<13;j++)tick();unsigned before=dispatches;MirrorBank bank;bank_init(&bank);bank.assignment=BA_EFFECT;copy_mirror(fixture,&s);bank_apply(&bank,&s,s.heartbeat);bank.effects_slot=0;MirrorInput in={.commands=command_state};input_effect_edit(&in,&bank,&s,0,4,0,s.heartbeat);Context c=context();c.r[0]=ptr(insert_object);call(&c,PX_REPLACE);tick();copy_mirror(fixture,&s);input_pump(&in,&bank,&s,s.heartbeat);require(!input_pending(&in)&&dispatches==before,"invalidated unsent generation never dispatches");
 /* A native callback returning without the actual setter cannot be certified. */
 for(unsigned j=0;j<13;j++){tick();}copy_mirror(fixture,&s);bank_apply(&bank,&s,s.heartbeat);bank.effects_slot=0;bank.effects_page=0;watch(0,0);for(unsigned j=0;j<8;j++)tick();copy_mirror(fixture,&s);input_effect_edit(&in,&bank,&s,0,9,0,s.heartbeat);input_pump(&in,&bank,&s,s.heartbeat);unsigned missing=atomic_load(&command_state->published),missing_at=command_find(command_state,missing);require(missing_at<COMMAND_SLOTS,"missing setter test published");skip_setter=1;tick();tick();command_retire();require(atomic_load(&command_state->slots[missing_at].done)==missing&&atomic_load(&command_state->slots[missing_at].sealed)!=missing&&!atomic_load(effect_executed+missing_at),"DONE alone and existing matching state cannot fabricate effect setter/readback completion");
 puts("PASS production Effects owner reads, copied list/page, ordinary and recording command receipts, quantized actual readback, mode-exit drain, replacement outcome and stale-unsent rejection; native getter/facade/audio bodies and object storage substituted");return 0;
}
