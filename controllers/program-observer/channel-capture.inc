/* Called only within the command mode's complete source-flight envelope.
 * Native values are copied at constructor/commit seams, never polled later. */
void channel_initialize(void){atomic_store_explicit(&mirror_state->channel.owner_token,token(),memory_order_release);}
static void channel_error(unsigned field,unsigned why){if(field<CHANNEL_FIELD_KINDS)atomic_store_explicit(&mirror_state->channel.errors[field],why,memory_order_release);}
static void channel_owner_error(unsigned kind,unsigned why){
 for(unsigned f=CI_MIDI_MONITOR;f<CHANNEL_FIELD_KINDS;f++)if(channel_kind(f)==kind)channel_error(f,why);
 for(unsigned f=CF_PAN;f<CF_COUNT;f++)if((kind==CO_PROGRAM&&(f==CF_PAN||f==CF_MUTE||f==CF_SOLO||f==CF_SOLO_AUDIO||f==CF_EFFECTIVE_MUTE||(f>=CF_SEND1&&f<=CF_SEND4)))||(kind==CO_TRACK&&(f==CF_ARM||f==CF_NAME||f==CF_COLOR||f==CF_MIDI_VOLUME))||(kind==CO_PROJECT&&f==CF_SELECTION)||(kind==CO_MIXER&&f==CF_MASTER)||(kind==CO_TIMELINE&&f==CF_PLAYING)||(kind==CO_AUTOMATION&&f==CF_AUTOMATION)||(kind==CO_LOOP&&f==CF_LOOP)||(kind==CO_RECORDING&&f==CF_RECORD_MODE)||(kind==CO_CLICK&&f==CF_CLICK))channel_error(f,why);
}
static unsigned channel_hash(uint32_t p){return ((p>>4)*2654435761u)&(CHANNEL_HASH-1);}
static ChannelOwner *channel_owner(uint32_t address){
 ChannelState *s=&mirror_state->channel;unsigned h=channel_hash(address);
 for(unsigned i=0;i<MIRROR_PROBES;i++){unsigned n=atomic_load_explicit(s->owner_directory+((h+i)&(CHANNEL_HASH-1)),memory_order_acquire);if(!n||n>CHANNEL_OWNERS)return NULL;ChannelOwner *o=s->owners+n-1;if(atomic_load_explicit(&o->address,memory_order_relaxed)==address)return o;}
 return NULL;
}
static unsigned channel_owner_id(uint32_t address){ChannelOwner *o=channel_owner(address);return o&&atomic_load_explicit(&o->live,memory_order_acquire)?atomic_load_explicit(&o->incarnation,memory_order_relaxed):0;}
static ChannelCell *channel_cell(uint32_t property){
 ChannelState *s=&mirror_state->channel;unsigned h=channel_hash(property);
 for(unsigned i=0;i<MIRROR_PROBES;i++){unsigned n=atomic_load_explicit(s->field_directory+((h+i)&(CHANNEL_HASH-1)),memory_order_acquire);if(!n||n>CHANNEL_FIELDS)return NULL;ChannelCell *c=s->fields+n-1;if(atomic_load_explicit(&c->property,memory_order_relaxed)==property)return c;}
 return NULL;
}
static int channel_on_owner(unsigned kind){
 if(token()==atomic_load_explicit(&mirror_state->channel.owner_token,memory_order_acquire))return 1;
 channel_owner_error(kind,CH_OWNER);return 0;
}
static void channel_owner_birth(uint32_t address,unsigned kind){
 if(!pointer(address)||!channel_on_owner(kind))return;
 ChannelState *s=&mirror_state->channel;ChannelOwner *old=channel_owner(address);
 unsigned n=atomic_load_explicit(&s->owners_used,memory_order_relaxed),h=channel_hash(address),slot=CHANNEL_HASH;
 if(n==CHANNEL_OWNERS){channel_owner_error(kind,CH_CAPACITY);return;}
 for(unsigned i=0;i<MIRROR_PROBES;i++){unsigned at=(h+i)&(CHANNEL_HASH-1),id=atomic_load_explicit(s->owner_directory+at,memory_order_relaxed);if(!id||(old&&id==atomic_load(&old->incarnation))){slot=at;break;}}
 if(slot==CHANNEL_HASH){channel_owner_error(kind,CH_CAPACITY);return;}
 /* A new native constructor is an explicit new lifetime, including reuse after
  * a failed construction. Old copied cells keep their old owner incarnation. */
 if(old)atomic_store_explicit(&old->live,0,memory_order_release);
 ChannelOwner *o=s->owners+n;atomic_store(&o->address,address);atomic_store(&o->kind,kind);atomic_store(&o->incarnation,n+1);atomic_store(&o->live,1);
 atomic_store_explicit(&s->owners_used,n+1,memory_order_release);atomic_store_explicit(s->owner_directory+slot,n+1,memory_order_release);
}
static void channel_owner_death(uint32_t address){ChannelOwner *o=channel_owner(address);if(o)atomic_store_explicit(&o->live,0,memory_order_release);}
static void channel_write(ChannelCell *c,uint32_t bits,const char *text,unsigned length,int death){
 unsigned field=atomic_load_explicit(&c->field,memory_order_relaxed);
 if(!atomic_load_explicit(&c->live,memory_order_acquire))return;
 if((field==CI_MIDI_IN_PORT||field==CI_MIDI_MONITOR||field==CI_SEND_TO||field==CI_AUDIO_OUT||field==CI_AUDIO_IN)&&token()!=atomic_load_explicit(&mirror_state->channel.owner_token,memory_order_acquire)){channel_error(field,CH_OWNER);return;}
 if((field==CF_ARM||field==CF_SOLO)&&token()!=atomic_load_explicit(&mirror_state->channel.owner_token,memory_order_acquire)){channel_error(field,CH_OWNER);return;}
 unsigned rev=atomic_load_explicit(&c->revision,memory_order_relaxed);
 if((rev&1)||rev>UINT32_MAX-2){channel_error(field,CH_WRAP);return;}
 atomic_store_explicit(&c->revision,rev+1,memory_order_relaxed);atomic_thread_fence(memory_order_release);
 if(death)atomic_store_explicit(&c->live,0,memory_order_relaxed);
 else{
  atomic_store_explicit(&c->bits,bits,memory_order_relaxed);atomic_store_explicit(&c->updates,atomic_load_explicit(&c->updates,memory_order_relaxed)+1,memory_order_relaxed);
  if(field==CF_NAME||field==CI_AUDIO_OUT||field==CI_AUDIO_OUT_AUDIO){for(unsigned i=0;i<CHANNEL_TEXT/4;i++){uint32_t w=0;if(i*4<length){unsigned n=length-i*4;if(n>4)n=4;memcpy(&w,text+i*4,n);}atomic_store_explicit(c->text+i,w,memory_order_relaxed);}atomic_store_explicit(&c->length,length,memory_order_relaxed);}
 }
 atomic_store_explicit(&c->revision,rev+2,memory_order_release);
}
static void channel_birth(uint32_t address,uint32_t property,unsigned field,uint32_t bits,const char *text,unsigned length,unsigned seed_kind){
 ChannelState *s=&mirror_state->channel;ChannelOwner *o=channel_owner(address);
 if(!o||!atomic_load_explicit(&o->live,memory_order_acquire)||!channel_on_owner(atomic_load(&o->kind)))return;
 unsigned n=atomic_load_explicit(&s->fields_used,memory_order_relaxed),h=channel_hash(property),slot=CHANNEL_HASH;ChannelCell *old=channel_cell(property);
 if(n==CHANNEL_FIELDS){channel_error(field,CH_CAPACITY);return;}
 if(old&&atomic_load(&old->live)&&atomic_load(&old->owner_incarnation)==atomic_load(&o->incarnation)){channel_error(field,CH_LIFETIME);return;}
 for(unsigned i=0;i<MIRROR_PROBES;i++){unsigned at=(h+i)&(CHANNEL_HASH-1),id=atomic_load_explicit(s->field_directory+at,memory_order_relaxed);if(!id||(old&&id==atomic_load(&old->incarnation))){slot=at;break;}}
 if(slot==CHANNEL_HASH){channel_error(field,CH_CAPACITY);return;}
 ChannelCell *c=s->fields+n;
 atomic_store(&c->property,property);atomic_store(&c->owner,address);atomic_store(&c->owner_incarnation,atomic_load(&o->incarnation));atomic_store(&c->incarnation,n+1);atomic_store(&c->field,field);atomic_store(&c->live,1);atomic_store(&c->seed,seed_kind);
 channel_write(c,bits,text,length,0);atomic_store(&c->updates,0);
 atomic_store_explicit(&s->fields_used,n+1,memory_order_release);atomic_store_explicit(s->field_directory+slot,n+1,memory_order_release);
}
static int channel_name(uint32_t property,char text[CHANNEL_TEXT],unsigned *length){
 uint32_t p=word(property+0x28);if(p<4096||p>UINT32_MAX-CHANNEL_TEXT){channel_error(CF_NAME,CH_TEXT);return 0;}
 for(unsigned i=0;i<CHANNEL_TEXT;i++){char ch=*(const volatile char*)(uintptr_t)(p+i);if(!ch){*length=i;return 1;}text[i]=ch;}
 /* Truncation is explicit copied text, never an unbounded native scan. */
 *length=CHANNEL_TEXT;return 1;
}
static uint32_t channel_program_frame(Context *c,int copy,unsigned extra){
 uint32_t sp=source_sp(c)+extra,p;
 if(extra&&word(source_sp(c)+0x8c)!=observer_image_bias+(copy?0x236faf4:0x236f090))return 0;
 if(!copy){if(word(sp+0xb4)!=observer_image_bias+0x237034c||word(sp+0x114)!=observer_image_bias+0x23705f8||word(sp+0x13c)!=observer_image_bias+0x2514c08)return 0;p=word(sp+0x12c);}
 else{if(word(sp+0x8c)!=observer_image_bias+0x2370710||word(sp+0xf4)!=observer_image_bias+0x2370a7c||word(sp+0x10c)!=observer_image_bias+0x2514334)return 0;p=word(sp+0xf8);}
 return pointer(p)&&p+(extra?0x48:0x3c)==c->r[4]?p:0;
}
static int channel_source(unsigned site){return site>=CH_PROGRAM_BIRTH||site==M_MUTE||site==M_BOOL||site==M_COLOR||site==M_NAME;}
typedef struct {uint32_t owner,property,sp,words[6],mask;} IODescriptorCapture;
static __thread IODescriptorCapture io_descriptor_birth __attribute__((tls_model("initial-exec")));
static __thread IODescriptorCapture io_descriptor_ui __attribute__((tls_model("initial-exec")));
static __thread IODescriptorCapture io_descriptor_audio __attribute__((tls_model("initial-exec")));
static void channel_output_descriptor(Context *c,unsigned site){
 unsigned birth=site<IO_OUTPUT_COMMIT0,copy=birth&&((site-IO_OUTPUT_BIRTH0)/3&1),audio=birth?site>=IO_OUTPUT_APPLY_BIRTH0:site>=IO_OUTPUT_APPLY_COMMIT0;
 unsigned part=birth?(site-IO_OUTPUT_BIRTH0)%3:audio?site-IO_OUTPUT_APPLY_COMMIT0:site-IO_OUTPUT_COMMIT0;
 uint32_t p=birth?channel_program_frame(c,copy,0):c->r[5]-(audio?0x370:0x348),property=p+(audio?0x370:0x348);
 if(!p)return;
 IODescriptorCapture *d=birth?&io_descriptor_birth:audio?&io_descriptor_audio:&io_descriptor_ui;
 ChannelCell *cell=birth?NULL:channel_cell(property);unsigned field=audio?CI_AUDIO_OUT_AUDIO:CI_AUDIO_OUT;
 if(!birth&&(!cell||atomic_load(&cell->field)!=field||!atomic_load(&cell->live)))return;
 if(!part){d->owner=p;d->property=property;d->sp=source_sp(c);d->mask=0;}
 if(d->owner!=p||d->property!=property||d->sp!=source_sp(c)||d->mask!=((1u<<part)-1)){channel_error(field,CH_LIFETIME);return;}
 unsigned reg=birth?(audio?6:2):audio||part==2?0:2;d->words[part*2]=c->r[reg];d->words[part*2+1]=c->r[reg+1];d->mask|=1u<<part;
 if(part==2){if(birth)channel_birth(p,property,field,0,(const char*)d->words,24,copy?2:1);else channel_write(cell,0,(const char*)d->words,24,0);memset(d,0,sizeof(*d));}
}
static void channel_hook(Context *c,unsigned site){
 if(site>=IO_OUTPUT_BIRTH0&&site<=IO_OUTPUT_APPLY_COMMIT2){channel_output_descriptor(c,site);return;}
 if(site==IO_AUDIO_IN_COPY){if(typed(c->r[5],0x692fec4))channel_birth(c->r[5],c->r[5]+0x2784,CI_AUDIO_IN,c->r[3],NULL,0,2);return;}

 if(site>=IO_AUDIO_MONITOR_BIRTH&&site<=IO_AUDIO_APPLY_COPY){
  unsigned copy=site==IO_AUDIO_MONITOR_COPY||site==IO_AUDIO_APPLY_COPY,monitor=site==IO_AUDIO_MONITOR_BIRTH||site==IO_AUDIO_MONITOR_COPY,ui=site==IO_AUDIO_IN_BIRTH;
  uint32_t p=c->r[copy?5:4],value=site==IO_AUDIO_MONITOR_BIRTH?(uint32_t)(c->d[16]>>32):c->r[3];
  if(!typed(p,0x692fec4))return;
  channel_birth(p,p+(monitor?0x26c8:ui?0x2784:0x2798),monitor?CI_AUDIO_MONITOR:ui?CI_AUDIO_IN:CI_AUDIO_IN_AUDIO,value,NULL,0,copy?2:1);return;
 }
 if(site==IO_AUDIO_UI_COMMIT||site==IO_AUDIO_MONITOR_COMMIT||site==IO_AUDIO_APPLY_COMMIT){
  unsigned tag=site==IO_AUDIO_UI_COMMIT?CI_AUDIO_IN:site==IO_AUDIO_MONITOR_COMMIT?CI_AUDIO_MONITOR:CI_AUDIO_IN_AUDIO;
  ChannelCell *cell=channel_cell(c->r[5]);if(cell&&atomic_load(&cell->field)==tag)channel_write(cell,c->r[7],NULL,0,0);return;
 }

 if(site==IO_MONITOR_BIRTH||site==IO_SEND_BIRTH){
  unsigned send=site==IO_SEND_BIRTH,offset=send?0x41c:0x3fc,tag=send?CI_SEND_TO:CI_MIDI_MONITOR;uint32_t t=c->r[4]-offset;
  if(word(source_sp(c)+44)!=observer_image_bias+(send?0x26523e0:0x26523c8))return;
  channel_birth(t,t+offset+4,tag,(uint32_t)c->d[16],NULL,0,1);channel_birth(t,t+offset+0x18,tag+1,(uint32_t)(c->d[16]>>32),NULL,0,1);return;
 }
 if(site==IO_MONITOR_COMMIT||site==IO_SEND_COMMIT||site==IO_MONITOR_AUDIO||site==IO_SEND_AUDIO){
  unsigned audio=site==IO_MONITOR_AUDIO||site==IO_SEND_AUDIO,send=site==IO_SEND_COMMIT||site==IO_SEND_AUDIO;
  uint32_t property=audio?c->r[2]+0x18:c->r[0];ChannelCell *cell=channel_cell(property);if(cell&&atomic_load(&cell->field)==(send?CI_SEND_TO:CI_MIDI_MONITOR)+audio)channel_write(cell,c->r[audio?1:7],NULL,0,0);return;
 }
 if(site>=IO_IN_PORT_BIRTH&&site<=IO_OUT_CHANNEL_BIRTH){
  static const unsigned fields[]={CI_MIDI_IN_PORT,CI_MIDI_IN_CHANNEL,CI_MIDI_OUT_PORT,CI_MIDI_OUT_CHANNEL};
  static const unsigned offsets[]={0x364,0x378,0x3a4,0x3d0};unsigned index=site-IO_IN_PORT_BIRTH;
  channel_birth(c->r[4],c->r[4]+offsets[index],fields[index],index==2?(uint32_t)(c->d[16]>>32):c->r[3],NULL,0,1);return;
 }
 if(site>=IO_IN_PORT_COMMIT&&site<=IO_CHANNEL_COMMIT){
  uint32_t property=c->r[site==IO_IN_PORT_COMMIT?0:5];ChannelCell *cell=channel_cell(property);if(!cell)return;
  unsigned field=atomic_load(&cell->field);if((site==IO_IN_PORT_COMMIT&&field==CI_MIDI_IN_PORT)||(site==IO_OUT_PORT_COMMIT&&field==CI_MIDI_OUT_PORT)||(site==IO_CHANNEL_COMMIT&&(field==CI_MIDI_IN_CHANNEL||field==CI_MIDI_OUT_CHANNEL)))channel_write(cell,c->r[site==IO_IN_PORT_COMMIT?1:site==IO_OUT_PORT_COMMIT?7:3],NULL,0,0);
  return;
 }

 if(site==MI_VOLUME_BIRTH||site==MI_VOLUME_COPY){
  uint32_t track=c->r[4]-0x114;ChannelOwner *o=channel_owner(track);
  if(!o||!atomic_load(&o->live)||atomic_load(&o->kind)!=CO_TRACK||c->r[8]!=track+0x28c)return;
  if(site==MI_VOLUME_BIRTH){
   if(word(source_sp(c))!=track||word(source_sp(c)+0x1c)!=observer_image_bias+0x26522b4)return;
   channel_birth(track,track+0x28c,CF_MIDI_VOLUME,c->r[2],NULL,0,1);
  }else{
   /* No additional Track copy caller has been qualified. Only an already
    * enrolled destination/source Track can establish this member-copy seed. */
   ChannelOwner *src=channel_owner(c->r[5]-0x114);
   if(src&&atomic_load(&src->live)&&atomic_load(&src->kind)==CO_TRACK)channel_birth(track,track+0x28c,CF_MIDI_VOLUME,c->r[3],NULL,0,2);
  }return;
 }

 if(site==GL_CLICK_OWNER){channel_owner_birth(c->r[0],CO_CLICK);return;}
 if(site==GL_CLICK_BIRTH){channel_birth(c->r[4],c->r[4]+0xc,CF_CLICK,c->r[3]&255,NULL,0,1);return;}
 if(site==GL_CLICK_DEATH||site==GL_CLICK_DELETE){channel_owner_death(c->r[0]);return;}
 if(site==CH_RECORD_OWNER){channel_owner_birth(c->r[0],CO_RECORDING);return;}
 if(site==CH_RECORD_BIRTH){channel_birth(c->r[4],c->r[4]+0x500,CF_RECORD_MODE,c->r[3],NULL,0,1);return;}
 if(site==CH_RECORD_DEATH){channel_owner_death(c->r[0]);return;}
 if(site==CH_LOOP_OWNER){channel_owner_birth(c->r[0],CO_LOOP);return;}
 if(site==CH_LOOP_BIRTH){channel_birth(c->r[4],c->r[4]+0x68,CF_LOOP,c->r[3]&255,NULL,0,1);return;}
 if(site==CH_AUTOMATION_OWNER){channel_owner_birth(c->r[0],CO_AUTOMATION);return;}
 if(site==CH_AUTOMATION_BIRTH){channel_birth(c->r[4],c->r[4]+0xc,CF_AUTOMATION,c->r[2],NULL,0,1);return;}
 if(site==CH_MIXER_BIRTH||site==CH_TIMELINE_BIRTH){channel_owner_birth(c->r[0],site==CH_MIXER_BIRTH?CO_MIXER:CO_TIMELINE);return;}
 if(site==CH_MIXER_DEATH||site==CH_TIMELINE_DEATH){channel_owner_death(c->r[0]);return;}
 if(site==CH_MASTER_BIRTH){channel_birth(c->r[4],c->r[4]+0xa8,CF_MASTER,c->r[5],NULL,0,1);return;}
 if(site==CH_PLAYING_BIRTH){channel_birth(c->r[4],c->r[4]+0xec,CF_PLAYING,c->r[3]&255,NULL,0,1);return;}

 if(site==CH_PROGRAM_BIRTH||site==CH_PROGRAM_COPY){channel_owner_birth(c->r[0],CO_PROGRAM);return;}
 if(site==CH_TRACK_BIRTH){channel_owner_birth(c->r[0],CO_TRACK);return;}
 if(site==CH_PROJECT_BIRTH){channel_owner_birth(c->r[0],CO_PROJECT);return;}
 if(site==CH_PROGRAM_DEATH||site==M_TRACK_DESTROY||site==M_PROJECT_DESTROY){channel_owner_death(c->r[0]);return;}
 if(site==CH_PAN_BIRTH||site==CH_PAN_COPY){
  unsigned copy=site==CH_PAN_COPY;uint32_t p=channel_program_frame(c,copy,0);if(p)channel_birth(p,p+0x3d0,CF_PAN,copy?c->r[7]:(uint32_t)c->d[16],NULL,0,copy?2:1);return;
 }
 if(site==CH_SEND_BIRTH||site==CH_SEND_COPY){
  unsigned copy=site==CH_SEND_COPY;uint32_t p=channel_program_frame(c,copy,0),property=copy?c->r[5]-0x38:c->r[8];
  if(p&&property>=p+0x428&&property<=p+0x4d0&&(property-p-0x428)%0x38==0)channel_birth(p,property,CF_SEND1+(property-p-0x428)/0x38,copy?c->r[3]:c->r[7],NULL,0,copy?2:1);
  return;
 }
 if(site==CH_NAME_BIRTH){char text[CHANNEL_TEXT];unsigned length;if(channel_name(c->r[4]+0x440,text,&length))channel_birth(c->r[4],c->r[4]+0x440,CF_NAME,0,text,length,1);return;}
 if(site==CH_COLOR_BIRTH){channel_birth(c->r[4],c->r[4]+0x5f0,CF_COLOR,c->r[3],NULL,0,1);return;}
 if(site==CH_ARM_BIRTH){if(word(source_sp(c)+0x6c)==observer_image_bias+0x2652458)channel_birth(c->r[4]-0x46c,c->r[4]+8,CF_ARM,c->r[3]&255,NULL,0,1);return;}
 if(site==CH_SELECTION_BIRTH){channel_birth(c->r[4],c->r[4]+0x78,CF_SELECTION,(uint32_t)c->d[16],NULL,0,1);return;}
 if(site>=CH_MUTE_BIRTH&&site<=CH_EFFECTIVE_COPY){
  unsigned offset=site-CH_MUTE_BIRTH,copy=offset&1;uint32_t p=channel_program_frame(c,copy,0x90);if(!p)return;
  unsigned field=offset<2?CF_MUTE:offset<4?CF_SOLO:offset<6?CF_SOLO_AUDIO:CF_EFFECTIVE_MUTE;
  unsigned property=field==CF_MUTE?0x50:field==CF_SOLO?0x88:field==CF_SOLO_AUDIO?0x98:0x184;
  unsigned value=field==CF_MUTE?(copy?c->r[2]:c->r[3]):field==CF_EFFECTIVE_MUTE?(copy?c->r[3]:c->r[10]):copy?c->r[6]:c->r[3];
  channel_birth(p,p+property,field,value&255,NULL,0,copy?2:1);return;
 }
 unsigned property=site==MIRROR_PROPERTY_DESTROY?c->r[0]:c->r[5];ChannelCell *cell=channel_cell(property);if(!cell||!atomic_load_explicit(&cell->live,memory_order_acquire))return;
 unsigned field=atomic_load(&cell->field);
 if(site==MIRROR_PROPERTY_DESTROY){channel_write(cell,0,NULL,0,1);return;}
 if(site==M_FLOAT&&(field==CF_PAN||(field>=CF_SEND1&&field<=CF_SEND4)||field==CF_MASTER||field==CF_MIDI_VOLUME))channel_write(cell,(uint32_t)c->d[8],NULL,0,0);
 else if((site==M_MUTE||site==CH_BOOL_COMMIT)&&(field==CF_MUTE||field==CF_SOLO_AUDIO||field==CF_EFFECTIVE_MUTE||field==CF_PLAYING||field==CF_LOOP||field==CF_CLICK))channel_write(cell,c->r[7]&255,NULL,0,0);
 else if(site==M_BOOL&&(field==CF_SOLO||field==CF_ARM))channel_write(cell,c->r[7]&255,NULL,0,0);
 else if(site==CH_RECORD_COMMIT&&field==CF_RECORD_MODE)channel_write(cell,c->r[7],NULL,0,0);
 else if(site==CH_ENUM_COMMIT&&field==CF_AUTOMATION)channel_write(cell,c->r[7],NULL,0,0);
 else if(site==M_COLOR&&field==CF_COLOR)channel_write(cell,c->r[7],NULL,0,0);
 else if(site==CH_SELECTION_COMMIT&&field==CF_SELECTION)channel_write(cell,c->r[7],NULL,0,0);
 else if(site==M_NAME&&field==CF_NAME){char text[CHANNEL_TEXT];unsigned length;if(channel_name(property,text,&length))channel_write(cell,0,text,length,0);}
}
