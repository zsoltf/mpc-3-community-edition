/* Eight-fader output policy over the existing copied MMV4 snapshot owner. MIT. */
#ifndef MPC_MIRROR_MOTOR_CORE_H
#define MPC_MIRROR_MOTOR_CORE_H
#define MIRROR_READER_COMPONENT 1
#include "mirror-read.c"
/* Physical touch knowledge belongs to the current ALSA connection. Binding
 * barriers/targets belong to one bank identity and are discarded on remap. */
enum {MT_UNKNOWN,MT_DOWN,MT_UP};
enum {BA_PAN,BA_TRACK,BA_SEND,BA_EFFECT,BA_QLINK,BA_IO};
enum {BV_TRACK,BV_INSTRUMENT,BV_SUBMIX,BV_RETURN,BV_OUTPUT,BV_ALL,BV_MIDI,BV_INPUT,BV_AUDIO,BV_DRUM_PADS,BV_COUNT};
static inline int bank_role(unsigned view,uint32_t v){
 switch(view){case BV_TRACK:return playable(v);case BV_INSTRUMENT:return v==0x6930c00||v==0x693147c||v==0x6932a48;case BV_SUBMIX:return v==0x6932150;case BV_RETURN:return v==0x6931f70;case BV_OUTPUT:return v==0x6931bb0||v==0x6931d90;case BV_ALL:return 1;case BV_MIDI:return v==0x6932338||v==0x6930a14;case BV_INPUT:return v==0x69319d0;case BV_AUDIO:return v==0x692fec4;default:return 0;}
}
typedef struct {
 uint32_t epoch,serial,binding,incarnation,track,program,track_owner,program_owner,pad_owner,pad_index,pad_generation;
} MotorIdentity;
typedef struct {
 MotorIdentity identity;
 uint32_t revision,bits,field_incarnation,barrier,last_tick,effect[5],qlink_matches[8],qlink_kind,qlink_activity;
 int touch,last_sent,physical,available,empty,wait;
} MirrorFader;
typedef struct {
 MirrorFader faders[MIRROR_BANK];
 MotorIdentity selected, strips[MIRROR_BANK];
 uint32_t offset,epoch,heartbeat,topology_revision,playable_count,remaps;
 unsigned view,fader_field,assignment,flip,effects_slot,effects_page,effects_generation,effects_chooser_request,qlink_page;
 unsigned volume_fields[MIRROR_BANK];
 /* Row this strip's identity was bound from in the snapshot's track array, or
  * -1 when it has none (pad rows resolve directly). It is a hint: every reader
  * revalidates the row against the identity and falls back to the scan. */
 int strip_row[MIRROR_BANK];
 int ready,chooser;
} MirrorBank;
static inline unsigned bank_parameter(const MirrorBank *bank,unsigned strip){
 if(bank->assignment==BA_EFFECT||bank->assignment==BA_QLINK)return CF_COUNT;
 if(bank->assignment==BA_SEND)return strip<4?CF_SEND1+strip:CF_COUNT;
 return CF_PAN;
}
static inline unsigned bank_field(const MirrorBank *bank,unsigned strip){return (bank->assignment==BA_EFFECT||bank->assignment==BA_QLINK)&&bank->flip?CF_COUNT:bank->assignment==BA_SEND&&strip>=4?CF_COUNT:bank->flip&&bank->assignment==BA_SEND?bank_parameter(bank,strip):bank->fader_field==CF_VOLUME?bank->volume_fields[strip]:bank->fader_field;}
static inline unsigned bank_encoder_field(const MirrorBank *bank,unsigned strip){return bank->assignment==BA_SEND&&strip>=4?CF_COUNT:bank->flip?bank->volume_fields[strip]:bank_parameter(bank,strip);}
static inline const MotorIdentity *bank_encoder_identity(const MirrorBank *bank,unsigned strip){return bank->assignment==BA_SEND&&!bank->flip?&bank->selected:&bank->strips[strip];}
static inline void bank_init(MirrorBank *bank){memset(bank,0,sizeof(*bank));bank->assignment=BA_TRACK;bank->effects_slot=EFFECT_LIST;for(unsigned i=0;i<MIRROR_BANK;i++){bank->faders[i].last_sent=bank->faders[i].physical=-1;bank->strip_row[i]=-1;}}
static inline void bank_disconnect(MirrorBank *bank){uint32_t offset=bank->offset,epoch=bank->epoch,view=bank->view,field=bank->fader_field;bank_init(bank);bank->offset=offset;bank->epoch=epoch;bank->view=view;bank->fader_field=field;}
static inline void bank_drop(MirrorBank *bank,uint32_t now){
 for(unsigned i=0;i<MIRROR_BANK;i++){
  MirrorFader *f=bank->faders+i;memset(&f->identity,0,sizeof(f->identity));f->available=f->empty=0;f->last_sent=-1;f->wait=1;f->barrier=now;f->revision=f->bits=0;bank->strip_row[i]=-1;
 }
 bank->ready=bank->chooser=0;
}
static inline void bank_view(MirrorBank *bank,unsigned view,uint32_t now){if(view<BV_COUNT&&view!=bank->view){bank_drop(bank,now);bank->view=view;bank->offset=0;}}
static inline MotorIdentity bank_identity(const CopiedMirror *s,const CopiedTrack *t){return (MotorIdentity){s->epoch,t->serial,t->binding,t->incarnation,t->track,t->program,t->track_owner,t->program_owner,t->pad_owner,t->pad_index,t->pad_generation};}
/* Fixed native send destination membership, joined to retained copied Tracks.
 * An absent or ambiguous destination never falls back to an ordinary strip. */
static inline const CopiedTrack *bank_return(const CopiedMirror *s,unsigned slot){
 if(slot>=4||!s->send_programs[slot]||!s->send_owners[slot])return NULL;
 const CopiedTrack *found=NULL;
 for(unsigned i=0;i<s->count;i++){
  const CopiedTrack *t=s->tracks+i;
  if(t->vptr!=0x6931f70||t->program!=s->send_programs[slot]||t->program_owner!=s->send_owners[slot])continue;
  if(found)return NULL;
  found=t;
 }
 return found;
}
static inline void bank_apply(MirrorBank *bank,const CopiedMirror *snapshot,uint32_t now){
 if(bank->epoch!=snapshot->epoch){bank_drop(bank,now);bank->offset=0;bank->epoch=snapshot->epoch;}
 bank->heartbeat=snapshot->heartbeat;bank->topology_revision=snapshot->revision;
 if(!snapshot->ready||!snapshot->alive||snapshot->error){
  if(snapshot->alive&&!snapshot->error&&!snapshot->ready&&!snapshot->epoch&&!snapshot->count){
   if(!bank->chooser){bank_drop(bank,now);bank->chooser=1;}
  }else bank_drop(bank,now);
  return;
 }
 bank->chooser=0;
 unsigned indices[MIRROR_TRACKS],n=0;
 const CopiedTrack *rows=bank->view==BV_DRUM_PADS?snapshot->pads:snapshot->tracks;
 if(bank->view==BV_DRUM_PADS){n=snapshot->pad_count;for(unsigned i=0;i<n;i++)indices[i]=i;}
 else for(unsigned i=0;i<snapshot->count;i++)if(bank_role(bank->view,snapshot->tracks[i].vptr))indices[n++]=i;
 bank->playable_count=n;
 MotorIdentity selected={0};
 if(snapshot->selection.available)for(unsigned i=0;i<snapshot->count;i++){
  const CopiedTrack *t=snapshot->tracks+i;if(t->track!=snapshot->selection.bits)continue;
  /* Return controls remain independently bound, but a return is never the
   * source of this selected-send view. Keep other qualified mixable sources. */
  if(bank->assignment==BA_SEND&&(t->vptr==0x6931f70||!mixable(t->vptr)))break;
  selected=(MotorIdentity){snapshot->epoch,t->serial,t->binding,t->incarnation,t->track,t->program,t->track_owner,t->program_owner,0,0,0};break;
 }
 /* Follow an actual MPC selection change, not every refresh: explicit Bank
  * navigation remains free to show another part of the collection. */
 if(selected.epoch!=bank->selected.epoch||selected.track!=bank->selected.track){
  if(bank->assignment==BA_EFFECT){bank->effects_slot=EFFECT_LIST;bank->effects_page=0;bank->effects_generation=0;bank->flip=0;}
  if(bank->view==BV_DRUM_PADS){bank->offset=0;bank_drop(bank,now);}
  else if(bank->assignment!=BA_SEND)for(unsigned i=0;i<n;i++)if(snapshot->tracks[indices[i]].serial==selected.serial){
   unsigned before=bank->offset;
   if(i<before||i>=before+MIRROR_BANK)bank->offset=(i/MIRROR_BANK)*MIRROR_BANK;
   if(before!=bank->offset)bank_drop(bank,now);
   break;
  }
 }
 /* Binding replacement refreshes parameter targets without becoming a new
  * musical selection or undoing explicit Bank navigation. */
 bank->selected=selected;
 if(bank->assignment==BA_EFFECT&&snapshot->effects_available&&snapshot->effects.serial==selected.serial){
  if(bank->effects_generation&&bank->effects_generation!=snapshot->effects.generation){bank->effects_slot=EFFECT_LIST;bank->effects_page=0;bank->flip=0;}
  bank->effects_generation=snapshot->effects.generation;
  const EffectChooser *q=&snapshot->effects.chooser;
  if(q->request&&q->request!=bank->effects_chooser_request&&q->status==EC_LOADED&&q->epoch==selected.epoch&&q->serial==selected.serial&&q->track_owner==selected.track_owner&&q->program_owner==selected.program_owner&&q->slot<EFFECT_SLOTS&&q->generation==snapshot->effects.generation&&q->key==snapshot->effects.slots[q->slot].key&&q->ap==snapshot->effects.slots[q->slot].ap&&snapshot->effects.slots[q->slot].status==EF_READY){bank->effects_chooser_request=q->request;bank->effects_slot=q->slot;bank->effects_page=0;bank->flip=0;}

 }
 if(bank->offset>=n)bank->offset=n?((n-1)/8)*8:0;
 bank->ready=1; /* a valid zero-track inventory also establishes empty slots */
 for(unsigned i=0;i<MIRROR_BANK;i++){
  MirrorFader *f=bank->faders+i;MotorIdentity identity={0};const CopiedTrack *track=NULL;
  if(bank->assignment==BA_SEND)track=bank_return(snapshot,i);
  else if(bank->offset+i<n)track=rows+indices[bank->offset+i];
  if(track)identity=bank_identity(snapshot,track);
  /* Record the track row this identity came from. A pad row keeps -1: a pad
   * identity already resolves by index, and a pad whose own owner is absent
   * must keep falling back to the parent scan it resolved by before. */
  bank->strip_row[i]=track&&(bank->assignment==BA_SEND||bank->view!=BV_DRUM_PADS)?(int)(track-snapshot->tracks):-1;
  bank->strips[i]=identity;bank->volume_fields[i]=track&&midi_volume(track->vptr)?CF_MIDI_VOLUME:CF_VOLUME;unsigned field=bank_field(bank,i);int known_empty=bank->assignment==BA_SEND?i>=4:!track;
  if(bank->view==BV_DRUM_PADS&&track&&!track->pad_owner){track=NULL;identity=(MotorIdentity){0};}
  if(bank->flip&&bank->assignment==BA_SEND){
   track=NULL;identity=(MotorIdentity){0};known_empty=field==CF_COUNT;
   if(field!=CF_COUNT&&selected.serial)for(unsigned j=0;j<snapshot->count;j++)if(snapshot->tracks[j].serial==selected.serial){track=snapshot->tracks+j;identity=selected;break;}
  }
  uint32_t effect[5]={0};const EffectParameter *parameter=NULL;const QLinkSlot *qlink=NULL;
  if(bank->assignment==BA_EFFECT&&bank->flip){
   const EffectsCopy *e=&snapshot->effects;identity=selected;track=NULL;known_empty=0;
   if(snapshot->effects_available&&e->status==EF_READY&&e->serial==selected.serial&&e->slot==bank->effects_slot&&e->page==bank->effects_page&&e->slot<EFFECT_SLOTS){
    const EffectParameter *p=e->parameters+i;known_empty=p->status==EF_EMPTY;
    if(p->status==EF_READY&&p->tick<=now&&now-p->tick<EFFECT_FRESH_MS){parameter=p;effect[0]=e->generation;effect[1]=e->slot;effect[2]=p->index;effect[3]=e->slots[e->slot].key;effect[4]=e->slots[e->slot].ap;}
   }
  }
  if(bank->assignment==BA_QLINK&&bank->flip){
   /* Logical Q-Link identity never borrows a selected Track incarnation. */
   identity=(MotorIdentity){0};track=NULL;known_empty=0;
   const QLinkCopy *q=&snapshot->qlinks;
   if(snapshot->qlinks_available&&q->epoch==snapshot->epoch&&q->status==QL_SAMPLED){
    effect[0]=q->generation;effect[1]=bank->qlink_page*8+i;effect[2]=q->wrapper;effect[3]=q->provider;effect[4]=q->root;
    const QLinkSlot *p=q->slots+effect[1];if(p->sampled&&p->normalized_valid&&p->tick<=now&&now-p->tick<QLINK_FRESH_MS)qlink=p;
   }
  }
  if(memcmp(&identity,&f->identity,sizeof(identity))||memcmp(effect,f->effect,sizeof(effect))||f->empty!=known_empty){
   f->identity=identity;memcpy(f->effect,effect,sizeof(effect));f->last_sent=-1;f->wait=1;f->barrier=now;f->revision=f->bits=0;bank->remaps++;
  }
  f->empty=known_empty;f->available=track&&track->available;
  if(track){float value;memcpy(&value,&track->bits,4);if(!track->available||!isfinite(value)||value<0||value>1||!volume_capable(track->vptr))f->available=0;}
  f->qlink_kind=f->qlink_activity=0;memset(f->qlink_matches,0,sizeof(f->qlink_matches));
  if(qlink){memcpy(f->qlink_matches,qlink->refresh_matches,sizeof(f->qlink_matches));f->qlink_kind=qlink->refresh_kind;f->qlink_activity=qlink->gesture_activity;f->bits=qlink->bits;f->revision=qlink->revision;f->field_incarnation=effect[0];f->available=1;}
  if(parameter){f->bits=parameter->bits;f->revision=parameter->revision;f->field_incarnation=effect[0];f->available=1;}
  if(track&&field<CF_COUNT){f->revision=track->revision;f->bits=track->bits;if(field){const CopiedField *v=track->fields+field;f->revision=v->revision;f->bits=v->bits;f->field_incarnation=v->incarnation;f->available=v->available&&(field==CF_MIDI_VOLUME?midi_volume(track->vptr):mixable(track->vptr));}}
 }
}
static inline void bank_touch(MirrorBank *bank,unsigned channel,int down,uint32_t now){
 if(channel>=MIRROR_BANK)return;
 MirrorFader *f=bank->faders+channel;
 if(down){f->touch=MT_DOWN;f->wait=1;f->barrier=now;}
 else{f->touch=MT_UP;f->wait=1;f->barrier=now;}
}
static inline void bank_navigate(MirrorBank *bank,int direction,uint32_t now){
 uint32_t before=bank->offset;
 if(direction<0)bank->offset=before>=8?before-8:0;
 else if(before+8<bank->playable_count)bank->offset=before+8;
 if(before!=bank->offset)bank_drop(bank,now);
}
static inline void bank_channel(MirrorBank *bank,int direction,uint32_t now){
 uint32_t before=bank->offset;if(direction<0&&before)bank->offset--;else if(direction>0&&before+1<bank->playable_count)bank->offset++;if(before!=bank->offset)bank_drop(bank,now);
}
static inline int bank_target(const MirrorBank *bank,unsigned channel,uint32_t now){
 const MirrorFader *f=bank->faders+channel;
 if((!bank->ready&&!bank->chooser)||now<bank->heartbeat||now-bank->heartbeat>=1000)return -1;
 if(bank->chooser||f->empty)return 0;
 if(!f->available||(!(bank->assignment==BA_QLINK&&bank->flip&&f->effect[0])&&(!f->identity.serial||!f->identity.incarnation)))return -1;
 float value;memcpy(&value,&f->bits,4);if(!isfinite(value)||value<0||value>1)return -1;return (int)(value*16383.0f+0.5f);
}
static inline int bank_due(MirrorBank *bank,unsigned channel,uint32_t now){
 MirrorFader *f=bank->faders+channel;
 /* Normal MCU startup assumes no hold until a touch-down is observed. This
  * permits automatic sync; it is not proof of the cap's state on connection. */
 if((!bank->ready&&!bank->chooser)||f->touch==MT_DOWN||now<bank->heartbeat||now-bank->heartbeat>=1000)return -1;
 if(!bank->chooser&&!f->empty&&(!f->available||(!(bank->assignment==BA_QLINK&&bank->flip&&f->effect[0])&&(!f->identity.serial||!f->identity.incarnation))))return -1;
 if(f->wait){if(bank->heartbeat<=f->barrier)return -1;f->wait=0;}
 int position=0;
 if(!bank->chooser&&!f->empty){float value;memcpy(&value,&f->bits,4);if(!isfinite(value)||value<0||value>1)return -1;position=(int)(value*16383.0f+0.5f);}
 return position!=f->last_sent&&now>=f->last_tick&&now-f->last_tick>=10?position:-1;
}
#endif
