#define _GNU_SOURCE
#include "mirror-state.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stddef.h>
/* Reusable consumer copies only observer-owned words. The topology and each
 * scalar are independently coherent; this is not an atomic cross-field view. */
typedef struct {uint32_t property,incarnation,owner_incarnation,revision,bits,updates,seed,available,error,length;char text[CHANNEL_TEXT+1];} CopiedField;
typedef struct {uint32_t incarnation,available,tick,token,revision,left,right,enabled,site;} CopiedMeter;
typedef struct {uint32_t serial,track,program,kind,incarnation,binding,vptr,revision,bits,updates,seed,available,track_owner,program_owner,meter_id,pad_owner,pad_index,pad_generation;CopiedField fields[CF_COUNT];CopiedMeter meter;} CopiedTrack;
typedef struct {uint32_t epoch,generation,revision,count,ready,heartbeat,error,alive,allocated,project_owner,selected_serial;CopiedField selection,master,playing,automation,loop,record_mode,click;uint32_t position_available,position_error,position_tick,position_token,bar,beat,clock;CopiedTrack tracks[MIRROR_TRACKS];CopiedMeter master_meter;uint32_t meter_error,meter_cells,meter_tokens,meter_closed,automation_detail_available,automation_mixed,automation_count,automation_detail_tick,automation_generation,editor_owner,zoom_owner;uint32_t send_programs[4],send_owners[4],sends_source_revision;EffectsCopy effects;uint32_t effects_available,effects_source_revision;QLinkCopy qlinks;uint32_t qlinks_available,qlinks_source_revision;IOCopy io;uint32_t io_available;uint32_t pad_count,pad_error;const PadState *pad_source;CopiedTrack pads[PAD_SLOTS];} CopiedMirror;
static void copy_meter(const MeterState *s,unsigned id,const CopiedMirror *snapshot,CopiedMeter *out){
 memset(out,0,sizeof(*out));out->incarnation=id;
 if(!id||id>METER_CELLS||atomic_load(&s->error)||!snapshot->ready)return;
 const MeterCell *m=s->cells+id-1;unsigned sub=atomic_load_explicit(&m->subscription,memory_order_acquire),chosen=0,ambiguous=0;
 if(!sub||!atomic_load(&m->live)||atomic_load(&m->epoch)!=snapshot->epoch)return;
 for(unsigned i=0;i<METER_LANES;i++){
  const MeterSample *p=m->lanes+i;unsigned rev=atomic_load_explicit(&p->revision,memory_order_acquire);if(!rev||(rev&1))continue;
  unsigned ps=atomic_load(&p->subscription),ep=atomic_load(&p->epoch),tick=atomic_load(&p->tick),tp=atomic_load(&p->token),l=atomic_load(&p->left),r=atomic_load(&p->right),enabled=atomic_load(&p->enabled),site=atomic_load(&p->site);atomic_thread_fence(memory_order_acquire);
  if(rev!=atomic_load_explicit(&p->revision,memory_order_relaxed)||ps!=sub||ep!=snapshot->epoch||tick>snapshot->heartbeat||snapshot->heartbeat-tick>=250)continue;
  float left,right;memcpy(&left,&l,4);memcpy(&right,&r,4);if(!isfinite(left)||!isfinite(right)||left<0||right<0||enabled>1)continue;
  if(!chosen||tick>out->tick){chosen=1;ambiguous=0;out->tick=tick;out->token=tp;out->left=l;out->right=r;out->revision=rev;out->enabled=enabled;out->site=site;}
  else if(tick==out->tick&&(l!=out->left||r!=out->right||enabled!=out->enabled))ambiguous=1;
 }
 out->available=chosen&&!ambiguous&&out->enabled&&atomic_load(&m->live)&&sub==atomic_load_explicit(&m->subscription,memory_order_acquire);
}
static inline int playable(uint32_t vptr){return vptr==0x6930c00||vptr==0x692fec4||vptr==0x6932338||vptr==0x6932a48||vptr==0x6930a14||vptr==0x693147c;}
static inline int midi_volume(uint32_t vptr){return vptr==0x6932338;}
static inline int volume_capable(uint32_t vptr);
static inline int mixable(uint32_t vptr){return vptr&&vptr!=0x6932338&&vptr!=0x6930a14;}
static inline int volume_capable(uint32_t vptr){return mixable(vptr)||midi_volume(vptr);}
static int copy_channel(const ChannelState *s,unsigned owner_id,unsigned field,CopiedField *out){
 memset(out,0,sizeof(*out));out->error=atomic_load_explicit(s->errors+field,memory_order_acquire);
 if(!owner_id||owner_id>CHANNEL_OWNERS||out->error)return 0;
 const ChannelOwner *o=s->owners+owner_id-1;
 if(!atomic_load_explicit(&o->live,memory_order_acquire)||atomic_load(&o->kind)!=channel_kind(field)||atomic_load(&o->incarnation)!=owner_id)return 0;
 unsigned property=atomic_load_explicit(&o->address,memory_order_relaxed)+channel_offset(field),h=((property>>4)*2654435761u)&(CHANNEL_HASH-1);const ChannelCell *c=NULL;
 for(unsigned i=0;i<MIRROR_PROBES;i++){unsigned n=atomic_load_explicit(s->field_directory+((h+i)&(CHANNEL_HASH-1)),memory_order_acquire);if(!n||n>CHANNEL_FIELDS)return 0;const ChannelCell *p=s->fields+n-1;if(atomic_load_explicit(&p->property,memory_order_relaxed)==property){c=p;break;}}
 if(!c||atomic_load(&c->owner_incarnation)!=owner_id||atomic_load(&c->field)!=field)return 0;
 for(unsigned retry=0;retry<4;retry++){
  unsigned rev=atomic_load_explicit(&c->revision,memory_order_acquire);if(rev&1)continue;
  out->property=property;out->incarnation=atomic_load_explicit(&c->incarnation,memory_order_relaxed);out->owner_incarnation=owner_id;out->bits=atomic_load_explicit(&c->bits,memory_order_relaxed);out->updates=atomic_load_explicit(&c->updates,memory_order_relaxed);out->seed=atomic_load_explicit(&c->seed,memory_order_relaxed);out->length=atomic_load_explicit(&c->length,memory_order_relaxed);
  if(out->length>CHANNEL_TEXT)return 0;
  if(field==CF_NAME)for(unsigned i=0;i<CHANNEL_TEXT/4;i++){uint32_t w=atomic_load_explicit(c->text+i,memory_order_relaxed);memcpy(out->text+i*4,&w,4);}
  out->text[out->length]=0;
  unsigned live=atomic_load_explicit(&c->live,memory_order_relaxed);atomic_thread_fence(memory_order_acquire);
  if(rev!=atomic_load_explicit(&c->revision,memory_order_relaxed))continue;
  out->revision=rev;out->available=live&&atomic_load_explicit(&o->live,memory_order_acquire)&&out->seed&&!atomic_load_explicit(s->errors+field,memory_order_acquire);
  if((field==CF_MIDI_VOLUME||field==CF_PAN||(field>=CF_SEND1&&field<=CF_MASTER))){float value;memcpy(&value,&out->bits,4);out->available&=isfinite(value)&&value>=0&&value<=1;}
  else if(field==CF_AUTOMATION)out->available&=out->bits<=2;
  else if(field==CF_RECORD_MODE)out->available&=out->bits<=4;
  else if(field==CF_MUTE||field==CF_SOLO||field==CF_ARM||field==CF_EFFECTIVE_MUTE||field==CF_SOLO_AUDIO||field==CF_PLAYING||field==CF_LOOP||field==CF_CLICK)out->available&=out->bits<=1;
  return out->available;
 }
 return 0;
}
static void copy_position(const MirrorState *s,CopiedMirror *out){
 out->position_error=atomic_load_explicit(&s->position_error,memory_order_acquire);if(out->position_error)return;
 unsigned chosen=0,ambiguous=0;
 for(unsigned i=0;i<MIRROR_POSITION_LANES;i++){
  const MirrorPosition *p=s->position+i;unsigned rev=atomic_load_explicit(&p->revision,memory_order_acquire);if(!rev||(rev&1))continue;
  unsigned ep=atomic_load(&p->epoch),tick=atomic_load(&p->tick),tp=atomic_load(&p->token),b=atomic_load(&p->bar),beat=atomic_load(&p->beat),clock=atomic_load(&p->clock);atomic_thread_fence(memory_order_acquire);
  if(rev!=atomic_load_explicit(&p->revision,memory_order_relaxed)||ep!=out->epoch||tick>out->heartbeat||out->heartbeat-tick>=1000)continue;
  if(!chosen||tick>out->position_tick){chosen=1;ambiguous=0;out->position_tick=tick;out->position_token=tp;out->bar=b;out->beat=beat;out->clock=clock;}
  else if(tick==out->position_tick&&(b!=out->bar||beat!=out->beat||clock!=out->clock))ambiguous=1;
 }
 out->position_available=out->ready&&chosen&&!ambiguous;
}
static void copy_general(const MirrorState *s,CopiedMirror *out){
 const GeneralState *g=&s->general;
 unsigned sends=atomic_load_explicit(&g->sends_revision,memory_order_acquire),mixer=atomic_load(&g->sends_mixer);
 out->sends_source_revision=sends;
 if(out->ready&&sends&&!(sends&1)&&atomic_load(&g->sends_epoch)==out->epoch&&mixer&&mixer<=CHANNEL_OWNERS&&mixer==atomic_load(&s->channel.mixer_owner)&&atomic_load(&s->channel.owners[mixer-1].live)){
  for(unsigned i=0;i<4;i++){unsigned p=atomic_load(&g->sends_programs[i]),owner=atomic_load(&g->sends_owners[i]);if(owner&&owner<=CHANNEL_OWNERS&&atomic_load(&s->channel.owners[owner-1].live)&&atomic_load(&s->channel.owners[owner-1].address)==p&&atomic_load(&s->channel.owners[owner-1].kind)==CO_PROGRAM){out->send_programs[i]=p;out->send_owners[i]=owner;}}
  atomic_thread_fence(memory_order_acquire);if(sends!=atomic_load(&g->sends_revision)||!atomic_load(&s->channel.owners[mixer-1].live)){memset(out->send_programs,0,sizeof(out->send_programs));memset(out->send_owners,0,sizeof(out->send_owners));}
 }
 unsigned editor=atomic_load(&g->editor_owner),zoom=atomic_load(&g->zoom_owner);
 if(editor&&editor<=CHANNEL_OWNERS&&atomic_load(&s->channel.owners[editor-1].live))out->editor_owner=editor;
 if(zoom&&zoom<=CHANNEL_OWNERS&&atomic_load(&s->channel.owners[zoom-1].live))out->zoom_owner=zoom;
 unsigned gen=atomic_load_explicit(&g->automation_generation,memory_order_acquire),chosen=0,ambiguous=0;
 out->automation_generation=gen;
 if(!out->automation.available||atomic_load(&g->automation_error))return;
 for(unsigned i=0;i<MIRROR_POSITION_LANES;i++){
  const AutomationSample *p=g->automation+i;unsigned rev=atomic_load_explicit(&p->revision,memory_order_acquire);if(!rev||(rev&1))continue;
  unsigned owner=atomic_load(&p->owner),ep=atomic_load(&p->epoch),tick=atomic_load(&p->tick),mode=atomic_load(&p->mode),mixed=atomic_load(&p->mixed),count=atomic_load(&p->count),pg=atomic_load(&p->generation);atomic_thread_fence(memory_order_acquire);
  if(rev!=atomic_load_explicit(&p->revision,memory_order_relaxed)||ep!=out->epoch||owner!=out->automation.owner_incarnation||pg!=gen||mode!=out->automation.bits||tick>out->heartbeat)continue;
  if(!chosen||tick>out->automation_detail_tick){chosen=1;ambiguous=0;out->automation_detail_tick=tick;out->automation_mixed=mixed;out->automation_count=count;}
  else if(tick==out->automation_detail_tick&&(mixed!=out->automation_mixed||count!=out->automation_count))ambiguous=1;
 }
 out->automation_detail_available=chosen&&!ambiguous&&gen==atomic_load(&g->automation_generation);
}
/* A pad view is a consumer projection, not a fabricated TrackPool member.
 * Retained pad requests can read their old parent after selection changes. */
static int copy_pad(const CopiedMirror *s,const CopiedTrack *parent,unsigned index,CopiedTrack *out){
 memset(out,0,sizeof(*out));if(!s->ready||!s->pad_source||index>=PAD_SLOTS||parent->vptr!=0x6930c00)return 0;
 const PadState *state=s->pad_source;if(atomic_load_explicit(&state->error,memory_order_acquire))return 0;
 unsigned h=((parent->program>>4)*2654435761u)&(PAD_PARENT_HASH-1);const PadParent *p=NULL;
 for(unsigned i=0;i<PAD_PROBES;i++){unsigned n=atomic_load_explicit(state->parent_directory+((h+i)&(PAD_PARENT_HASH-1)),memory_order_acquire);if(!n||n>PAD_PARENTS)return 0;const PadParent *v=state->parents+n-1;if(atomic_load(&v->address)==parent->program){p=v;break;}}
 if(!p||!atomic_load(&p->live)||atomic_load(&p->owner)!=parent->program_owner)return 0;
 unsigned rev=atomic_load_explicit(&p->revision,memory_order_acquire);if(rev&1)return 0;
 unsigned id=atomic_load(&p->slots[index].owner),generation=atomic_load(&p->slots[index].generation);
 if(!generation||!id||id>PAD_OWNERS)return 0;
 const PadOwner *o=state->owners+id-1;
 if(!atomic_load_explicit(&o->live,memory_order_acquire)||!atomic_load(&o->complete)||atomic_load(&o->parent)!=parent->program||atomic_load(&o->parent_owner)!=parent->program_owner)return 0;
 /* Preserve the real Track/Program lineage separately from Instrument identity. */
 out->serial=parent->serial;out->track=parent->track;out->program=parent->program;out->kind=parent->kind;out->vptr=parent->vptr;out->binding=parent->binding;out->incarnation=parent->incarnation;out->track_owner=parent->track_owner;out->program_owner=parent->program_owner;
 out->pad_owner=id;out->pad_index=index;out->pad_generation=generation;
 static const unsigned fields[PF_COUNT]={CF_VOLUME,CF_PAN,CF_MUTE,CF_SOLO,CF_SOLO_AUDIO};
 for(unsigned f=0;f<PF_COUNT;f++){
  const PadScalar *v=o->fields+f;CopiedField *c=out->fields+fields[f];
  unsigned r=atomic_load_explicit(&v->revision,memory_order_acquire);if(!r||(r&1))continue;
  c->property=atomic_load(&o->address)+pad_offset(f);c->incarnation=id*PF_COUNT+f;c->owner_incarnation=id;c->revision=r;c->bits=atomic_load(&v->bits);c->updates=atomic_load(&v->updates);c->seed=atomic_load(&v->seed);unsigned live=atomic_load(&v->live);atomic_thread_fence(memory_order_acquire);
  c->available=live&&c->seed&&r==atomic_load_explicit(&v->revision,memory_order_relaxed);
  if(f<PF_MUTE){float value;memcpy(&value,&c->bits,4);c->available&=isfinite(value)&&value>=0&&value<=1;}else c->available&=c->bits<=1;
 }
 atomic_thread_fence(memory_order_acquire);
 if(rev!=atomic_load_explicit(&p->revision,memory_order_relaxed)||!atomic_load(&p->live)||!atomic_load(&o->live)||atomic_load(&state->error)){memset(out,0,sizeof(*out));return 0;}
 out->bits=out->fields[CF_VOLUME].bits;out->revision=out->fields[CF_VOLUME].revision;out->available=out->fields[CF_VOLUME].available;out->seed=out->fields[CF_VOLUME].seed;out->updates=out->fields[CF_VOLUME].updates;
 return 1;
}
/* Receipt-only read of an exact retained execution record. Retirement changes
 * availability for motors, but does not erase the copied committed value.
 * Never use this accessor to bind a current control or generate motor output. */
static inline int copy_pad_receipt(const CopiedMirror *s,unsigned id,uint32_t parent,unsigned parent_owner,unsigned field,CopiedField *out){
 memset(out,0,sizeof(*out));unsigned f=pad_field(field);
 if(!s->pad_source||!id||id>PAD_OWNERS||f==PF_COUNT||atomic_load(&s->pad_source->error))return 0;
 const PadOwner *o=s->pad_source->owners+id-1;
 if(atomic_load(&o->incarnation)!=id||atomic_load(&o->parent)!=parent||atomic_load(&o->parent_owner)!=parent_owner||!atomic_load(&o->complete))return 0;
 const PadScalar *v=o->fields+f;
 for(unsigned retry=0;retry<4;retry++){
  unsigned rev=atomic_load_explicit(&v->revision,memory_order_acquire);if(!rev||(rev&1))continue;
  out->property=atomic_load(&o->address)+pad_offset(f);out->incarnation=id*PF_COUNT+f;out->owner_incarnation=id;out->bits=atomic_load(&v->bits);out->seed=atomic_load(&v->seed);out->updates=atomic_load(&v->updates);atomic_thread_fence(memory_order_acquire);
  if(rev!=atomic_load_explicit(&v->revision,memory_order_relaxed))continue;
  out->revision=rev;out->available=out->seed!=0;return out->available;
 }return 0;
}
static int copy_mirror_view(const MirrorState *s,CopiedMirror *out,int pads){
 for(unsigned attempt=0;attempt<4;attempt++){
  /* Only count/pad_count rows belong to a snapshot. Clear metadata here and
   * each populated row below; clearing both 128-row capacities on every MIDI
   * event needlessly consumes memory bandwidth during audio playback. */
  memset(out,0,offsetof(CopiedMirror,tracks));
  memset(&out->master_meter,0,offsetof(CopiedMirror,pads)-offsetof(CopiedMirror,master_meter));
  uint32_t rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(rev&1)continue;
  out->epoch=atomic_load_explicit(&s->epoch,memory_order_relaxed);out->generation=atomic_load_explicit(&s->generation,memory_order_relaxed);out->ready=atomic_load_explicit(&s->ready,memory_order_relaxed);
  out->project_owner=atomic_load_explicit(&s->channel.project_owner,memory_order_relaxed);copy_channel(&s->channel,out->project_owner,CF_SELECTION,&out->selection);
  copy_channel(&s->channel,atomic_load(&s->channel.click_owner),CF_CLICK,&out->click);
  copy_channel(&s->channel,atomic_load(&s->channel.record_owner),CF_RECORD_MODE,&out->record_mode);copy_channel(&s->channel,atomic_load(&s->channel.loop_owner),CF_LOOP,&out->loop);copy_channel(&s->channel,atomic_load(&s->channel.automation_owner),CF_AUTOMATION,&out->automation);copy_channel(&s->channel,atomic_load(&s->channel.mixer_owner),CF_MASTER,&out->master);copy_channel(&s->channel,atomic_load(&s->channel.timeline_owner),CF_PLAYING,&out->playing);
  out->count=atomic_load_explicit(&s->count,memory_order_relaxed);if(out->count>MIRROR_TRACKS)return 0;
  for(unsigned i=0;i<out->count;i++){
   CopiedTrack *t=out->tracks+i;const MirrorBinding *b=s->tracks+i;
   memset(t,0,sizeof(*t));
   t->serial=atomic_load_explicit(&b->serial,memory_order_relaxed);t->track=atomic_load_explicit(&b->track,memory_order_relaxed);t->program=atomic_load_explicit(&b->program,memory_order_relaxed);t->kind=atomic_load_explicit(&b->kind,memory_order_relaxed);t->incarnation=atomic_load_explicit(&b->incarnation,memory_order_relaxed);t->binding=atomic_load_explicit(&b->binding,memory_order_relaxed);t->vptr=atomic_load_explicit(&b->vptr,memory_order_relaxed);
   t->track_owner=atomic_load_explicit(&b->track_owner,memory_order_relaxed);t->program_owner=atomic_load_explicit(&b->program_owner,memory_order_relaxed);
   t->meter_id=atomic_load_explicit(&b->meter,memory_order_relaxed);
   for(unsigned field=CF_PAN;field<CF_COUNT;field++)if(field!=CF_SELECTION&&(field<CF_MASTER||field==CF_MIDI_VOLUME))copy_channel(&s->channel,channel_kind(field)==CO_TRACK?t->track_owner:t->program_owner,field,t->fields+field);
   if(out->selection.available&&out->selection.bits==t->track)out->selected_serial=t->serial;
   if(!mixable(t->vptr)){t->fields[CF_PAN].available=0;for(unsigned f=CF_SEND1;f<=CF_SEND4;f++)t->fields[f].available=0;}
   if(midi_volume(t->vptr)){
    const CopiedField *v=t->fields+CF_MIDI_VOLUME;t->bits=v->bits;t->revision=v->revision;t->seed=v->seed;t->updates=v->updates;t->available=v->available;
    continue;
   }
   if(!t->incarnation)continue;
   if(t->incarnation>MIRROR_CELLS)return 0;
   const MirrorCell *c=s->cells+t->incarnation-1;
   for(unsigned retry=0;retry<4;retry++){
    uint32_t cr=atomic_load_explicit(&c->revision,memory_order_acquire);if(cr&1)continue;
    uint32_t live=atomic_load_explicit(&c->live,memory_order_relaxed),inc=atomic_load_explicit(&c->incarnation,memory_order_relaxed),program=atomic_load_explicit(&c->program,memory_order_relaxed);
    t->bits=atomic_load_explicit(&c->bits,memory_order_relaxed);t->updates=atomic_load_explicit(&c->updates,memory_order_relaxed);t->seed=atomic_load_explicit(&c->seed,memory_order_relaxed);
    atomic_thread_fence(memory_order_acquire);
    if(cr==atomic_load_explicit(&c->revision,memory_order_relaxed)){
     float value;memcpy(&value,&t->bits,4);
     t->revision=cr;t->available=live&&inc==t->incarnation&&program==t->program&&t->seed&&isfinite(value);break;
    }
   }
  }
  atomic_thread_fence(memory_order_acquire);
  if(rev!=atomic_load_explicit(&s->revision,memory_order_relaxed))continue;
  out->revision=rev;out->error=atomic_load_explicit(&s->error,memory_order_acquire);out->alive=atomic_load_explicit(&s->alive,memory_order_acquire);out->heartbeat=atomic_load_explicit(&s->heartbeat,memory_order_acquire);out->allocated=atomic_load_explicit(&s->allocated,memory_order_acquire);copy_position(s,out);copy_general(s,out);
  out->effects_source_revision=atomic_load_explicit(&s->effects.revision,memory_order_acquire);
  out->effects_available=effects_copy_read(&s->effects,&out->effects)&&out->ready&&out->effects.epoch==out->epoch&&out->effects.serial==out->selected_serial&&out->effects.tick<=out->heartbeat&&out->heartbeat-out->effects.tick<EFFECT_FRESH_MS;
  out->io_available=io_copy_read(&s->io,&out->io)&&out->ready&&out->io.epoch==out->epoch&&out->io.serial==out->selected_serial&&out->io.status==IO_READY&&out->io.tick<=out->heartbeat&&out->heartbeat-out->io.tick<IO_FRESH_MS;
  out->qlinks_source_revision=atomic_load_explicit(&s->qlinks.revision,memory_order_acquire);
  out->qlinks_available=qlink_copy_read(&s->qlinks,&out->qlinks)&&out->ready&&out->qlinks.status==QL_SAMPLED&&out->qlinks.epoch==out->epoch&&out->qlinks.tick<=out->heartbeat&&out->heartbeat-out->qlinks.tick<QLINK_FRESH_MS;
  out->meter_error=atomic_load(&s->meters.error);out->meter_cells=atomic_load(&s->meters.used);out->meter_tokens=atomic_load(&s->meters.tokens);out->meter_closed=atomic_load(&s->meters.closed);
  copy_meter(&s->meters,atomic_load(&s->meters.master),out,&out->master_meter);
  for(unsigned i=0;i<out->count;i++)copy_meter(&s->meters,out->tracks[i].meter_id,out,&out->tracks[i].meter);
  out->pad_source=&s->pads;out->pad_error=atomic_load(&s->pads.error);
  if(pads&&out->ready)for(unsigned i=0;i<out->count;i++)if(out->tracks[i].serial==out->selected_serial&&out->tracks[i].vptr==0x6930c00){out->pad_count=PAD_SLOTS;for(unsigned j=0;j<PAD_SLOTS;j++)copy_pad(out,out->tracks+i,j,out->pads+j);break;}
  return 1;
 }
 return 0;
}
static inline int copy_mirror(const MirrorState *s,CopiedMirror *out){return copy_mirror_view(s,out,1);}
static inline const CopiedField *copied_field(const CopiedMirror *s,const CopiedTrack *t,unsigned field,CopiedField *volume){
 if(t->pad_owner)return field<CF_COUNT?t->fields+field:NULL;
 if(field==CF_SELECTION)return &s->selection;
 if(field==CF_VOLUME&&midi_volume(t->vptr))return t->fields+CF_MIDI_VOLUME;
 if(field!=CF_VOLUME)return field<CF_COUNT?t->fields+field:NULL;
 *volume=(CopiedField){.property=t->program+0x5b0,.incarnation=t->incarnation,.owner_incarnation=t->program_owner,.revision=t->revision,.bits=t->bits,.updates=t->updates,.seed=t->seed,.available=t->available};return volume;
}
static inline void effects_json_text(const char *s){putchar('"');for(unsigned i=0;i<EFFECT_TEXT&&s[i];i++){unsigned char c=s[i];if(c=='"'||c=='\\')printf("\\%c",c);else if(c<32)printf("\\u%04x",c);else putchar(c);}putchar('"');}
static inline void effects_json(const CopiedMirror *s){
 const EffectsCopy *e=&s->effects;
 printf("{\"available\":%s,\"status\":%u,\"epoch\":%u,\"serial\":%u,\"generation\":%u,\"slot\":%u,\"page\":%u,\"tick\":%u,\"native_reads\":%u,\"slots\":[",s->effects_available?"true":"false",e->status,e->epoch,e->serial,e->generation,e->slot,e->page,e->tick,e->reads);
 for(unsigned i=0;i<EFFECT_SLOTS;i++){const EffectSlot *v=e->slots+i;printf("%s{\"slot\":%u,\"status\":%u,\"key\":%u,\"ap\":%u,\"count\":%u,\"presentation_count\":%u,\"layout\":%u,\"name\":",i?",":"",i,v->status,v->key,v->ap,v->count,v->presentation_count,v->layout);effects_json_text(v->name);printf(",\"description\":{\"valid\":%u,\"uid\":%u,\"instrument\":%u,\"name\":",v->description.valid,v->description.uid,v->description.instrument);effects_json_text(v->description.name);printf(",\"format\":");effects_json_text(v->description.format);printf(",\"manufacturer\":");effects_json_text(v->description.manufacturer);printf(",\"descriptive\":");effects_json_text(v->description.descriptive);printf("},\"enable_valid\":%u,\"enable_bits\":%u,\"enable_revision\":%u,\"enable_tick\":%u}",v->enable_valid,v->enable_bits,v->enable_revision,v->enable_tick);}
 const EffectChooser *q=&e->chooser;printf("],\"chooser\":{\"request\":%u,\"status\":%u,\"epoch\":%u,\"serial\":%u,\"slot\":%u,\"generation\":%u,\"key\":%u,\"ap\":%u,\"tick\":%u,\"quality\":\"native_ui_open_and_inventory_not_dsp_completion\"},\"page_context\":{\"pages\":%u,\"tab\":%u,\"subtab\":%u,\"section\":%u,\"sections\":%u,\"name\":",q->request,q->status,q->epoch,q->serial,q->slot,q->generation,q->key,q->ap,q->tick,e->page_count,e->tab,e->subtab,e->section,e->section_count);effects_json_text(e->page_name);printf("},\"parameters\":[");for(unsigned i=0;i<EFFECT_PAGE;i++){const EffectParameter *v=e->parameters+i;printf("%s{\"index\":%u,\"status\":%u,\"tick\":%u,\"revision\":%u,\"bits\":%u,\"automatable\":%u,\"steps\":%u,\"position\":%u,\"default_bits\":%u,\"default_valid\":%u,\"name\":",i?",":"",v->index,v->status,v->tick,v->revision,v->bits,v->automatable,v->steps,v->position,v->default_bits,v->default_valid);effects_json_text(v->name);printf(",\"text\":");effects_json_text(v->text);putchar('}');}printf("]}");
}
static inline void qlink_json_text(const char *s){putchar('"');for(unsigned i=0;i<QLINK_TEXT&&s[i];i++){unsigned char c=s[i];if(c=='"'||c=='\\')printf("\\%c",c);else if(c<32)printf("\\u%04x",c);else putchar(c);}putchar('"');}
static inline void qlink_json(const CopiedMirror *s){
 const QLinkCopy *q=&s->qlinks;
 printf("{\"mode_selection\":{\"valid\":%s,\"controller\":%u,\"generation\":%u,\"revision\":%u,\"id\":%u,\"eligible_count\":%u,\"tick\":%u,\"name\":",q->mode_valid?"true":"false",q->mode_controller,q->mode_generation,q->mode_revision,q->mode_id,q->mode_count,q->mode_tick);qlink_json_text(qlink_mode_name(q->mode_id));printf("},");
 printf("\"sample_available\":%s,\"quality\":\"native_ui_seed_and_completed_mode_cache_value_agreement\",\"assignment_confirmed\":false,\"semantic_generation_observed\":true,\"target_settled\":false,\"gesture_is_availability\":false,\"assignment_available\":null,\"cross_slot_atomic\":false,\"status\":%u,\"epoch\":%u,\"enrollment_generation\":%u,\"demand\":%u,\"tick\":%u,\"native_reads\":%u,\"root\":%u,\"controls\":%u,\"queue\":%u,\"wrapper\":%u,\"provider\":%u,\"labels\":%u,\"mode_owner\":%u,\"raw_signals\":[%u,%u,%u],\"slots\":[",s->qlinks_available?"true":"false",q->status,q->epoch,q->generation,q->demand,q->tick,q->reads,q->root,q->controls,q->queue,q->wrapper,q->provider,q->labels,q->mode,q->raw_signals[0],q->raw_signals[1],q->raw_signals[2]);
 for(unsigned i=0;i<QLINK_SLOTS;i++){
  const QLinkSlot *v=q->slots+i;unsigned fresh=s->qlinks_available&&v->sampled&&v->revision&&v->tick<=s->heartbeat&&s->heartbeat-v->tick<QLINK_FRESH_MS;
  printf("%s{\"index\":%u,\"fresh\":%s,\"tick\":%u,\"revision\":%u,\"bits\":%u,\"normalized_valid\":%s,\"normalized\":",i?",":"",i,fresh?"true":"false",v->tick,v->revision,v->bits,v->normalized_valid?"true":"false");
  float value;memcpy(&value,&v->bits,4);if(v->normalized_valid&&isfinite(value))printf("%.9g",value);else printf("null");
  printf(",\"refresh_revision\":%u,\"refresh_tick\":%u,\"refresh_raw_bits\":%u,\"mode_cache_value_agreement\":%s,\"matching_lane_revisions\":[",v->refresh_revision,v->refresh_tick,v->refresh_raw,v->refresh_kind?"true":"false");for(unsigned lane=0;lane<8;lane++)printf("%s%u",lane?",":"",v->refresh_matches[lane]);putchar(']');
  printf(",\"raw_kind\":%u,\"gesture_activity\":%u,\"raw_signals\":[%u,%u,%u],\"name_status\":%u,\"text_status\":%u,\"name\":",v->kind,v->gesture_activity,v->raw_signals[0],v->raw_signals[1],v->raw_signals[2],v->name_status,v->text_status);qlink_json_text(v->name);printf(",\"text\":");qlink_json_text(v->text);putchar('}');
 }printf("]}");
}
static inline void io_json(const CopiedMirror *s){
 const IOCopy *q=&s->io;
 printf("{\"available\":%s,\"status\":%u,\"epoch\":%u,\"serial\":%u,\"generation\":%u,\"connector\":%u,\"tick\":%u,\"equipment_routing_proved\":false,\"fields\":[",s->io_available?"true":"false",q->status,q->epoch,q->serial,q->generation,q->connector,q->tick);
 for(unsigned i=0;i<IO_FIELDS;i++){const IOField *f=q->fields+i;printf("%s{\"field\":%u,\"name\":",i?",":"",i);qlink_json_text(io_field_name(i));printf(",\"status\":%u,\"property\":%u,\"incarnation\":%u,\"revision\":%u,\"value\":%u,\"tick\":%u,\"choice_generation\":%u,\"choice_count\":%u,\"choice_index\":%u,\"text\":",f->status,f->property,f->incarnation,f->revision,f->value,f->tick,f->choice_generation,f->choice_count,f->choice_index);qlink_json_text(f->text);putchar('}');}printf("]}");
}
#ifndef MIRROR_READER_COMPONENT
static void json_text(const char *s,unsigned n){putchar('"');for(unsigned i=0;i<n;i++){unsigned char c=s[i];if(c=='"'||c=='\\')printf("\\%c",c);else if(c<32)printf("\\u%04x",c);else putchar(c);}putchar('"');}
static void json_field(const CopiedField *f,unsigned field){
 printf("{\"available\":%s,\"error\":%u,\"incarnation\":%u,\"owner_incarnation\":%u,\"revision\":%u,\"seed_kind\":%u,\"commits\":%u,\"bits\":%u,\"value\":",f->available?"true":"false",f->error,f->incarnation,f->owner_incarnation,f->revision,f->seed,f->updates,f->bits);
 if(!f->available)printf("null");else if(field==CF_NAME)json_text(f->text,f->length);else if((field==CF_MIDI_VOLUME||field==CF_VOLUME||field==CF_PAN||(field>=CF_SEND1&&field<=CF_MASTER))){float value;memcpy(&value,&f->bits,4);printf("%.9g",value);}else printf("%u",f->bits);
 if(field==CF_NAME)printf(",\"truncated\":%s",f->length==CHANNEL_TEXT?"true":"false");
 printf("}");
}
static void json_meter(const CopiedMeter *m){
 float left,right;memcpy(&left,&m->left,4);memcpy(&right,&m->right,4);
 printf("{\"available\":%s,\"incarnation\":%u,\"source_tick\":%u,\"thread_token\":%u,\"revision\":%u,\"enabled\":%u,\"source_site\":%u,\"left\":",m->available?"true":"false",m->incarnation,m->tick,m->token,m->revision,m->enabled,m->site);
 if(m->available)printf("%.9g,\"right\":%.9g}",left,right);else printf("null,\"right\":null}");
}
static uint32_t elapsed_origin(const MirrorState *s){struct timespec now;if(clock_gettime(CLOCK_MONOTONIC,&now))return UINT32_MAX;int64_t n=((int64_t)now.tv_sec-s->origin_sec)*1000+((int64_t)now.tv_nsec-s->origin_nsec)/1000000;return n<0||n>UINT32_MAX?UINT32_MAX:(uint32_t)n;}
int main(int argc,char **argv){
 int all=argc==3&&!strcmp(argv[2],"--all");
 if(argc!=2&&!all){fprintf(stderr,"Usage: mirror-read /native/stage/volume.state [--all]\n");return 2;}
 int fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0){perror("open");return 2;}
 struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=(off_t)sizeof(MirrorState)){fprintf(stderr,"wrong mirror file\n");close(fd);return 2;}
 const MirrorState *s=mmap(NULL,sizeof(*s),PROT_READ,MAP_SHARED,fd,0);close(fd);if(s==MAP_FAILED)return 2;
 if(s->magic!=MIRROR_MAGIC||s->version!=MIRROR_VERSION||s->bytes!=sizeof(*s)||s->capacity!=MIRROR_CELLS){fprintf(stderr,"unsupported mirror format\n");return 2;}
 CopiedMirror out;int stable=copy_mirror(s,&out);uint32_t now=elapsed_origin(s);
 int fresh=stable&&out.alive&&!out.error&&now!=UINT32_MAX&&now>=out.heartbeat&&now-out.heartbeat<=1000;
 unsigned bank[MIRROR_BANK],n=0;int available=fresh&&out.ready;
 if(stable)for(unsigned i=0;i<out.count&&n<MIRROR_BANK;i++)if(playable(out.tracks[i].vptr)){bank[n++]=i;available&=out.tracks[i].available;}
 if(!n)available=0;
 printf("{\"format\":\"MMV17\",\"producer_pid\":%u,\"stable\":%s,\"fresh\":%s,\"error\":%u,\"epoch\":%u,\"generation\":%u,\"topology_revision\":%u,\"master_count\":%u,\"cells_used\":%u,\"cells_capacity\":%u,\"bank_available\":%s,\"view\":\"%s\",\"tracks\":[",s->pid,stable?"true":"false",fresh?"true":"false",out.error,out.epoch,out.generation,out.revision,out.count,out.allocated,MIRROR_CELLS,available?"true":"false",all?"all":"bank");
 unsigned shown=all&&stable?out.count:n;
 for(unsigned k=0;k<shown;k++){
  unsigned index=all?k:bank[k];CopiedTrack *t=out.tracks+index;float value;memcpy(&value,&t->bits,4);
  printf("%s{\"fader\":%u,\"master_index\":%u,\"track_serial\":%u,\"track\":%u,\"program\":%u,\"program_incarnation\":%u,\"binding_revision\":%u,\"volume_revision\":%u,\"seed_kind\":%u,\"commits\":%u,\"available\":%s,\"bits\":%u,\"volume\":",k?",":"",k+1,index,t->serial,t->track,t->program,t->incarnation,t->binding,t->revision,t->seed,t->updates,t->available?"true":"false",t->bits);
  if(t->available&&isfinite(value))printf("%.9g",value);else printf("null");
  printf(",\"track_incarnation\":%u,\"program_owner_incarnation\":%u,\"mixable\":%s,\"volume_capable\":%s,\"selected\":%s,\"fields\":{",t->track_owner,t->program_owner,mixable(t->vptr)?"true":"false",volume_capable(t->vptr)?"true":"false",out.selection.available?(out.selected_serial==t->serial?"true":"false"):"null");
  static const char *names[CF_COUNT]={"volume","pan","mute","solo","arm","name","color","selection","effective_mute","solo_audio","send1","send2","send3","send4","global_master","playing","automation","loop","record_mode","click","midi_volume"};
  for(unsigned f=CF_PAN;f<CF_COUNT;f++){if(f==CF_SELECTION||(f>=CF_MASTER&&f!=CF_MIDI_VOLUME))continue;printf("%s\"%s\":",f==CF_PAN?"":",",names[f]);json_field(t->fields+f,f);}printf("},\"meter\":");json_meter(&t->meter);printf("}");
 }
 printf("],\"pad_error\":%u,\"pad_count\":%u,\"pads\":[",out.pad_error,out.pad_count);
 for(unsigned i=0;i<out.pad_count;i++){const CopiedTrack *p=out.pads+i;printf("%s{\"label\":\"%c%02u\",\"parent_track_serial\":%u,\"instrument_incarnation\":%u,\"membership_generation\":%u,\"sample_name_available\":false,\"color_available\":false,\"selection_available\":false,\"meter_available\":false,\"fields\":{",i?",":"",'A'+i/16,i%16+1,out.selected_serial,p->pad_owner,p->pad_generation);static const unsigned fields[]={CF_VOLUME,CF_PAN,CF_MUTE,CF_SOLO,CF_SOLO_AUDIO};static const char *labels[]={"volume","pan","mute","solo","solo_audio"};for(unsigned j=0;j<5;j++){printf("%s\"%s\":",j?",":"",labels[j]);json_field(p->fields+fields[j],fields[j]);}printf("}}");}
 printf("],\"effects\":");effects_json(&out);printf(",\"qlinks\":");qlink_json(&out);printf(",\"io\":");io_json(&out);printf(",\"meter_error\":%u,\"meter_cells\":%u,\"meter_capacity\":%u,\"meter_tokens\":%u,\"meter_closed\":%u,\"master_meter\":",out.meter_error,out.meter_cells,METER_CELLS,out.meter_tokens,out.meter_closed);json_meter(&out.master_meter);printf(",\"selection\":");json_field(&out.selection,CF_SELECTION);printf(",\"master\":");json_field(&out.master,CF_MASTER);printf(",\"playing\":");json_field(&out.playing,CF_PLAYING);printf(",\"automation\":");json_field(&out.automation,CF_AUTOMATION);printf(",\"automation_members\":{\"available\":%s,\"mixed\":%u,\"count\":%u,\"generation\":%u},\"save_owner\":%u,\"zoom_owner\":%u",out.automation_detail_available?"true":"false",out.automation_mixed,out.automation_count,out.automation_generation,out.editor_owner,out.zoom_owner);printf(",\"loop\":");json_field(&out.loop,CF_LOOP);printf(",\"click\":");json_field(&out.click,CF_CLICK);printf(",\"record_mode\":");json_field(&out.record_mode,CF_RECORD_MODE);printf(",\"position\":{\"available\":%s,\"error\":%u,\"source_tick\":%u,\"thread_token\":%u,\"native_bar\":%u,\"native_beat\":%u,\"native_clock\":%u,\"index_conversion\":false}}\n",out.position_available?"true":"false",out.position_error,out.position_tick,out.position_token,out.bar,out.beat,out.clock);munmap((void*)s,sizeof(*s));return available?0:1;
}
#endif
