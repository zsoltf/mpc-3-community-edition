/* Final motor barrier over observer-owned memory, without copying a mixer.
 * A changed target is deferred to the next ordinary fresh snapshot. */
#ifndef MPC_MIRROR_FRESH_H
#define MPC_MIRROR_FRESH_H
static inline int mirror_snapshot_current(const MirrorState *s,const CopiedMirror *v,uint32_t now){
 return now!=UINT32_MAX&&v->alive&&!v->error&&now>=v->heartbeat&&now-v->heartbeat<1000&&
  atomic_load_explicit(&s->alive,memory_order_acquire)&&!atomic_load(&s->error)&&
  v->epoch==atomic_load(&s->epoch)&&v->ready==atomic_load(&s->ready)&&
  v->revision==atomic_load_explicit(&s->revision,memory_order_acquire);
}
static inline int mirror_selection_current(const MirrorState *s,const CopiedMirror *v){
 MIRROR_FIELD(f);copy_channel(&s->channel,atomic_load(&s->channel.project_owner),CF_SELECTION,&f);
 return f.available==v->selection.available&&f.revision==v->selection.revision&&
  f.incarnation==v->selection.incarnation&&f.bits==v->selection.bits;
}
static inline int mirror_motor_current(const MirrorState *s,const CopiedMirror *v,const MirrorBank *bank,unsigned strip,uint32_t now){
 if(strip>=MIRROR_BANK||!mirror_snapshot_current(s,v,now)||!mirror_selection_current(s,v))return 0;
 const MirrorFader *f=bank->faders+strip;
 if(bank->assignment==BA_SEND&&v->sends_source_revision!=atomic_load_explicit(&s->general.sends_revision,memory_order_acquire))return 0;
 if(bank->assignment==BA_EFFECT&&bank->flip)
  return v->effects_available&&v->effects_source_revision==atomic_load_explicit(&s->effects.revision,memory_order_acquire)&&!atomic_load(&s->effects.invalid)&&!atomic_load(&s->effects.error)&&mirror_snapshot_current(s,v,now);
 if(bank->assignment==BA_QLINK&&bank->flip)
  return v->qlinks_available&&v->qlinks_source_revision==atomic_load_explicit(&s->qlinks.revision,memory_order_acquire)&&!atomic_load(&s->qlinks.invalid)&&!atomic_load(&s->qlinks.error)&&mirror_snapshot_current(s,v,now);
 if(f->empty)return mirror_snapshot_current(s,v,now);
 const MotorIdentity *id=&f->identity;const CopiedTrack *t=NULL;
 for(unsigned i=0;i<v->count;i++)if(v->tracks[i].serial==id->serial){t=v->tracks+i;break;}
 if(!t||t->binding!=id->binding||t->program_owner!=id->program_owner||!channel_owner_slot(id->program_owner)||!atomic_load(&s->channel.owners[channel_owner_slot(id->program_owner)-1].live)||atomic_load(&s->channel.owners[channel_owner_slot(id->program_owner)-1].incarnation)!=id->program_owner)return 0;
 unsigned field=bank_field(bank,strip);MIRROR_FIELD(value);
 if(id->pad_owner){
  CopiedTrack pad;if(!copy_pad(v,t,id->pad_index,&pad)||pad.pad_owner!=id->pad_owner||pad.pad_generation!=id->pad_generation||field>=CF_COUNT)return 0;
  value=pad.fields[field];
 }else if(field==CF_VOLUME){
  if(!id->incarnation||id->incarnation>MIRROR_CELLS)return 0;
  const MirrorCell *c=s->cells+id->incarnation-1;unsigned revision=atomic_load_explicit(&c->revision,memory_order_acquire);
  if(revision!=f->revision||(revision&1)||!atomic_load(&c->live)||atomic_load(&c->incarnation)!=id->incarnation||atomic_load(&c->program)!=id->program||atomic_load(&c->bits)!=f->bits)return 0;
  atomic_thread_fence(memory_order_acquire);
  return revision==atomic_load_explicit(&c->revision,memory_order_relaxed)&&mirror_snapshot_current(s,v,now);
 }else{
  if(field>=CF_COUNT||!copy_channel(&s->channel,channel_kind(field)==CO_TRACK?id->track_owner:id->program_owner,field,&value))return 0;
 }
 return value.available&&value.bits==f->bits&&value.revision==f->revision&&
  (id->pad_owner||value.incarnation==f->field_incarnation)&&mirror_snapshot_current(s,v,now);
}
#endif
