/* Continuous coalesced input over bounded CMD31; MMV17 remains the musical-state owner. */
#ifndef MPC_MIRROR_INPUT_CORE_H
#define MPC_MIRROR_INPUT_CORE_H
#include "command-state.h"
#ifndef INPUT_STOPPED
#define INPUT_STOPPED() 0
#endif
#ifndef INPUT_LOG
#define INPUT_LOG(...) printf(__VA_ARGS__)
#endif
typedef struct {
 MotorIdentity identity;
 uint32_t bits,expires,field_incarnation,effect[5];
 unsigned held,changed,pending,reported,scope; /* 0 fader, 1 selected subview, 2 navigation, 3 channel strip */
} InputGesture;
#define INPUT_EDGES 64u
typedef struct {InputGesture desire;unsigned strip,field;} InputEdge;
typedef struct {
 MotorIdentity target;
 uint32_t cursor[COMMAND_LANES],flight,strip,bits,expires,before_revision;
 uint32_t phase,lane,token,capture,counter,commit_revision,commit_tick,done_tick;
 uint32_t field,field_incarnation,property,acknowledged;
 CommandEvent jog_events[3],jog_outcome,master_events[7];unsigned jog_seen,jog_outcome_seen,jog_playing,master_seen;
 CommandEvent global_events[3];unsigned global_seen;
 CommandEvent recording_events[10];unsigned recording_seen;
 CommandEvent pad_execution;unsigned pad_seen;
 CommandEvent midi_events[5];unsigned midi_seen;
 uint32_t global_owner,global_epoch;
 CommandRequest qlink_request;CommandEvent qlink_events[3][4],qlink_value,mode_accept;unsigned qlink_seen[3],qlink_value_seen,mode_accept_seen,qlink_barriers[COMMAND_LANES],qlink_barriers_seen;
 CommandRequest io_request;CommandEvent io_events[10];unsigned io_seen;
 CommandRequest effect_request;CommandEvent effect_execution,effect_value;unsigned effect_seen;
} InputFlight;
typedef struct {MotorIdentity identity;CommandRequest request;unsigned pending,relative;float shift,lower,upper;} InputEffect;
typedef struct {CommandRequest request;unsigned pending,relative,wait_tick,barriers[COMMAND_LANES];float shift,lower,upper;} InputQLink;
typedef struct {uint32_t epoch,owner,incarnation,bits,expires;unsigned held,valid,reported,pending;int physical;} InputMaster;
#define INPUT_JOG_EVENTS 64u
typedef struct {uint32_t operation,bits,epoch,owner,field,expires;} InputGlobalEvent;
typedef struct {int32_t delta;uint32_t operation,epoch,expires,origin;} InputJogEvent;
typedef struct {
 CommandState *commands;
 InputMaster master;
 InputEffect effects[EFFECT_PAGE];
 InputQLink qlinks[QLINK_SLOTS];
 CommandRequest mode_request;int mode_delta;
 CommandRequest io_requests[IO_FIELDS];int io_delta[IO_FIELDS];
 InputGesture gestures[MIRROR_BANK],desires[MIRROR_BANK][CF_COUNT];
 InputFlight flights[COMMAND_SLOTS];
 InputEdge edges[INPUT_EDGES];
 unsigned edge_head,edge_count;
 InputJogEvent jog_events[INPUT_JOG_EVENTS];unsigned jog_head,jog_count,jog_shift,jog_rewind,jog_forward,jog_repeat_tick,jog_epoch;
 uint32_t stop_down,play_down,stop_play_wait,stop_home,stop_until,stop_position_tick;
 uint32_t jog_wait,jog_wait_owner,jog_wait_epoch,jog_wait_count;
 InputGlobalEvent global_events[INPUT_JOG_EVENTS];unsigned global_head,global_count;
 unsigned error,submitted,settled,refused,next_strip;
} MirrorInput;
static inline void input_meter_interest(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,uint32_t now,int connected){
 CommandState *c=in->commands;if(!c)return;MeterInterest *d=&c->meter_interest;
 uint32_t serial[METER_BANK]={0},track[METER_BANK]={0},program[METER_BANK]={0};unsigned n=0;
 unsigned enabled=connected&&bank->ready&&s->ready&&bank->view!=BV_DRUM_PADS;
 if(enabled)for(unsigned i=0;i<MIRROR_BANK;i++){
  const MotorIdentity *id=bank->strips+i;if(!id->serial||id->pad_owner)continue;
  serial[n]=id->serial;track[n]=id->track_owner;program[n]=id->program_owner;n++;
 }
 unsigned changed=atomic_load(&d->enabled)!=enabled||atomic_load(&d->epoch)!=s->epoch||atomic_load(&d->count)!=n;
 for(unsigned i=0;i<n;i++)changed|=atomic_load(d->serial+i)!=serial[i]||atomic_load(d->track_owner+i)!=track[i]||atomic_load(d->program_owner+i)!=program[i];
 unsigned until=atomic_load(&d->until);if(!changed&&(!enabled||(until>now&&until-now>500)))return;
 if(!command_writer_enter(c))return;
 unsigned rev=atomic_load(&d->revision);if(rev>UINT32_MAX-2){atomic_store(&d->until,0);command_writer_leave(c);return;}
 atomic_store_explicit(&d->revision,rev+1,memory_order_release);
 atomic_store(&d->enabled,enabled);atomic_store(&d->epoch,s->epoch);atomic_store(&d->count,n);
 for(unsigned i=0;i<n;i++){atomic_store(d->serial+i,serial[i]);atomic_store(d->track_owner+i,track[i]);atomic_store(d->program_owner+i,program[i]);}
 atomic_store(&d->until,enabled&&now<=UINT32_MAX-1000?now+1000:0);
 atomic_store_explicit(&d->revision,rev+2,memory_order_release);command_writer_leave(c);
}
static inline void input_effects_interest(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,uint32_t now,int connected){
 CommandState *c=in->commands;if(!c)return;EffectsInterest *d=&c->effects_interest;
 unsigned enabled=connected&&bank->assignment==BA_EFFECT&&bank->ready&&s->selection.available,serial=enabled?bank->selected.serial:0;
 unsigned changed=atomic_load(&d->enabled)!=enabled||atomic_load(&d->epoch)!=s->epoch||atomic_load(&d->serial)!=serial||atomic_load(&d->slot)!=bank->effects_slot||atomic_load(&d->page)!=bank->effects_page;
 unsigned until=atomic_load(&d->until);if(!changed&&(!enabled||(until>now&&until-now>500)))return;
 if(!command_writer_enter(c))return;
 unsigned rev=atomic_load(&d->revision);if(changed&&rev>UINT32_MAX-2){atomic_store(&d->until,0);command_writer_leave(c);return;}if(changed&&rev<=UINT32_MAX-2){atomic_store(&d->revision,rev+1);atomic_store(&d->enabled,enabled);atomic_store(&d->epoch,s->epoch);atomic_store(&d->serial,serial);atomic_store(&d->slot,bank->effects_slot);atomic_store(&d->page,bank->effects_page);}
 atomic_store(&d->until,enabled&&now<=UINT32_MAX-1000?now+1000:0);if(changed&&rev<=UINT32_MAX-2)atomic_store_explicit(&d->revision,rev+2,memory_order_release);command_writer_leave(c);
}
static inline void input_io_interest(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,uint32_t now,int connected){
 CommandState *c=in->commands;if(!c)return;IOInterest *d=&c->io_interest;
 unsigned enabled=connected&&bank->assignment==BA_IO&&bank->ready&&s->selection.available,serial=enabled?bank->selected.serial:0;
 unsigned changed=atomic_load(&d->enabled)!=enabled||atomic_load(&d->epoch)!=s->epoch||atomic_load(&d->serial)!=serial,until=atomic_load(&d->until);
 if(!changed&&(!enabled||(until>now&&until-now>500)))return;
 if(!command_writer_enter(c))return;
 unsigned rev=atomic_load(&d->revision);if(changed&&rev>UINT32_MAX-2){atomic_store(&d->until,0);command_writer_leave(c);return;}
 if(changed){atomic_store(&d->revision,rev+1);atomic_store(&d->enabled,enabled);atomic_store(&d->epoch,s->epoch);atomic_store(&d->serial,serial);}
 atomic_store(&d->until,enabled&&now<=UINT32_MAX-1000?now+1000:0);if(changed)atomic_store_explicit(&d->revision,rev+2,memory_order_release);command_writer_leave(c);
}
static inline void input_qlink_interest(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,uint32_t now,int connected){
 CommandState *c=in->commands;if(!c)return;QLinkInterest *d=&c->qlink_interest;
 unsigned enabled=connected&&bank->assignment==BA_QLINK&&bank->ready;
 unsigned changed=atomic_load(&d->enabled)!=enabled||atomic_load(&d->epoch)!=s->epoch||atomic_load(&d->page)!=bank->qlink_page;
 unsigned until=atomic_load(&d->until);if(!changed&&(!enabled||(until>now&&until-now>500)))return;
 if(!command_writer_enter(c))return;
 unsigned rev=atomic_load(&d->revision);if(changed&&rev>UINT32_MAX-2){atomic_store(&d->until,0);command_writer_leave(c);return;}
 if(changed){atomic_store(&d->revision,rev+1);atomic_store(&d->enabled,enabled);atomic_store(&d->epoch,s->epoch);atomic_store(&d->page,bank->qlink_page);}
 atomic_store(&d->until,enabled&&now<=UINT32_MAX-1000?now+1000:0);if(changed)atomic_store_explicit(&d->revision,rev+2,memory_order_release);command_writer_leave(c);
}
static inline unsigned input_pending(const MirrorInput *in){unsigned n=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)n+=in->flights[i].flight!=0;return n;}
static inline InputFlight *input_existing(MirrorInput *in,const MotorIdentity *id,unsigned field){
 for(unsigned i=0;i<COMMAND_SLOTS;i++){
  InputFlight *t=in->flights+i;if(!t->flight||t->field!=field)continue;
  if(t->target.pad_owner||id->pad_owner){if(t->target.pad_owner&&id->pad_owner&&t->target.program_owner==id->program_owner&&t->target.pad_index==id->pad_index)return t;continue;}
  if(field==CF_SELECTION||((field==CF_ARM||field==CF_MIDI_VOLUME)?t->target.track_owner==id->track_owner:t->target.program_owner==id->program_owner))return t;
 }
 return NULL;
}
static inline const CopiedTrack *input_track(const CopiedMirror *s,const MotorIdentity *id){
 if(!s->ready||s->epoch!=id->epoch)return NULL;
 if(id->pad_owner){if(id->pad_index>=s->pad_count)return NULL;const CopiedTrack *p=s->pads+id->pad_index;if(p->pad_owner==id->pad_owner&&p->pad_generation==id->pad_generation&&p->serial==id->serial&&p->program_owner==id->program_owner&&p->binding==id->binding)return p;return NULL;}
 for(unsigned i=0;i<s->count;i++){
  const CopiedTrack *t=s->tracks+i;
  if(t->serial==id->serial&&t->binding==id->binding&&t->incarnation==id->incarnation&&t->track==id->track&&t->program==id->program&&t->track_owner==id->track_owner&&t->program_owner==id->program_owner)return t;
 }
 return NULL;
}
static inline const CopiedTrack *input_target(const CopiedMirror *s,const MotorIdentity *id,CopiedTrack *pad){
 const CopiedTrack *t=input_track(s,id);if(t||!id->pad_owner)return t;
 MotorIdentity parent=*id;parent.pad_owner=parent.pad_index=parent.pad_generation=0;
 t=input_track(s,&parent);
 return t&&copy_pad(s,t,id->pad_index,pad)&&pad->pad_owner==id->pad_owner&&pad->pad_generation==id->pad_generation?pad:NULL;
}
static inline int input_health(MirrorInput *in){
 CommandState *s=in->commands;
 if(!s||!atomic_load_explicit(&s->alive,memory_order_acquire)||atomic_load(&s->closed)||atomic_load(&s->error)||atomic_load(&s->trace_error)){in->error=C_SOURCE_FAILURE;return 0;}
 return !in->error;
}
static inline void input_master_invalidate(MirrorInput *in){in->master.pending=in->master.valid=in->master.reported=0;}
static inline void input_master_discard(MirrorInput *in){memset(&in->master,0,sizeof(in->master));}
static inline void input_master_sync(MirrorInput *in,const CopiedMirror *s){
 InputMaster *m=&in->master;const CopiedField *f=&s->master;
 if(!s->ready||!s->alive||s->error||!f->available){input_master_invalidate(in);return;}
 if(m->epoch!=s->epoch||m->owner!=f->owner_incarnation||m->incarnation!=f->incarnation){input_master_invalidate(in);m->epoch=s->epoch;m->owner=f->owner_incarnation;m->incarnation=f->incarnation;}
 if(!m->held)m->valid=1;
}
static inline void input_master_touch(MirrorInput *in,const CopiedMirror *s,int down){
 input_master_sync(in,s);if(down){if(!in->master.held)in->master.reported=0;in->master.held=1;}else{in->master.held=0;input_master_sync(in,s);}
}
static inline void input_master_pitch(MirrorInput *in,const CopiedMirror *s,int position,uint32_t now){
 if(position<0||position>16383||now>UINT32_MAX-120000)return;
 input_master_sync(in,s);InputMaster *m=&in->master;
 if(!m->valid||(m->reported&&m->physical==position))return;
 /* Touch can report a stale physical position before any travel, including
  * one different from the latest source/motor target. It is a baseline only. */
 if(m->held&&!m->reported){m->reported=1;m->physical=position;return;}
 m->reported=1;m->physical=position;float value=position/16383.0f;memcpy(&m->bits,&value,4);m->expires=now+120000;m->pending=1;
}
static inline int input_master_blocked(const MirrorInput *in){
 if(in->master.held||in->master.pending)return 1;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in->flights[i].flight&&in->flights[i].field==CF_MASTER&&!in->flights[i].acknowledged)return 1;
 return 0;
}
static inline void input_jog_discard(MirrorInput *in){in->jog_head=in->jog_count=0;in->global_head=in->global_count=0;in->jog_rewind=in->jog_forward=in->jog_shift=0;in->jog_repeat_tick=0;in->jog_wait=0;in->stop_down=in->play_down=in->stop_play_wait=in->stop_home=0;}
static inline void input_discard(MirrorInput *in){memset(in->io_delta,0,sizeof(in->io_delta));in->mode_delta=0;for(unsigned i=0;i<QLINK_SLOTS;i++)in->qlinks[i].pending=0;memset(in->effects,0,sizeof(in->effects));memset(in->gestures,0,sizeof(in->gestures));memset(in->desires,0,sizeof(in->desires));memset(in->edges,0,sizeof(in->edges));in->edge_head=in->edge_count=0;}
/* A strip-bank move changes fader/button bindings, not the selected Track
 * subview or a Channel navigation target. Assignment/view/disconnect still use
 * the full discard above. Published transactions are never discarded. */
static inline void input_bank_discard(MirrorInput *in){
 for(unsigned i=0;i<MIRROR_BANK;i++)if(in->gestures[i].scope!=1)memset(in->gestures+i,0,sizeof(in->gestures[i]));
 for(unsigned i=0;i<MIRROR_BANK;i++)for(unsigned f=0;f<CF_COUNT;f++){
  InputGesture *d=&in->desires[i][f];if(d->scope!=1&&d->scope!=2)memset(d,0,sizeof(*d));
 }
 memset(in->edges,0,sizeof(in->edges));in->edge_head=in->edge_count=0;
}
static inline void input_sync(MirrorInput *in,const MirrorBank *bank){
 for(unsigned i=0;i<MIRROR_BANK;i++){
  InputGesture *g=in->gestures+i;
  if(!bank->ready||!bank->faders[i].available||memcmp(&g->identity,&bank->faders[i].identity,sizeof(g->identity))||memcmp(g->effect,bank->faders[i].effect,sizeof(g->effect)))memset(g,0,sizeof(*g));
  for(unsigned f=CF_VOLUME;f<CF_COUNT;f++){
   InputGesture *d=&in->desires[i][f];
   const MotorIdentity *bound=d->scope==1?&bank->selected:d->scope==3?&bank->strips[i]:&bank->faders[i].identity;
   if(!bank->ready||d->identity.epoch!=bank->epoch||(d->scope!=2&&memcmp(&d->identity,bound,sizeof(*bound))))memset(d,0,sizeof(*d));
  }
 }
}
static inline void input_touch(MirrorInput *in,MirrorBank *bank,const CopiedMirror *s,unsigned strip,int down,uint32_t now){
 MirrorFader *f=bank->faders+strip;InputGesture *g=in->gestures+strip;
 if(down){
  if(f->touch==MT_DOWN)return; /* duplicate press cannot rebind a held gesture */
  const CopiedTrack *t=input_track(s,&f->identity);
  if(bank->ready&&f->available&&!in->error&&((bank->assignment==BA_QLINK&&bank->flip&&f->effect[0])||(t&&volume_capable(t->vptr)))){
   /* A position can arrive before capacitive touch-down. Preserve that valid
    * current-binding desire instead of losing the start of fast travel. */
   if(memcmp(&g->identity,&f->identity,sizeof(g->identity)))memset(g,0,sizeof(*g));
   g->identity=f->identity;memcpy(g->effect,f->effect,sizeof(g->effect));g->held=1;g->scope=bank->flip&&bank->assignment==BA_SEND?1:0;
  }else memset(g,0,sizeof(*g));
 }else if(f->touch==MT_DOWN&&g->held)g->held=0;
 bank_touch(bank,strip,down,now);
}
static inline void input_pitch(MirrorInput *in,MirrorBank *bank,unsigned strip,int position,uint32_t now){
 if(strip>=MIRROR_BANK||position<0||position>16383)return;
 InputGesture *g=in->gestures+strip;MirrorFader *f=bank->faders+strip;
 int previous=f->physical;f->physical=position; /* actual input, never motor target */
 if(!bank->ready||!f->available||in->error)return;
 /* A held gesture canceled by remap cannot bind to the replacement Track.
  * Without sensed touch, each position is ordinary current-bank MCU input. */
 if(f->touch==MT_DOWN&&(!g->held||memcmp(&g->identity,&f->identity,sizeof(g->identity))||memcmp(g->effect,f->effect,sizeof(g->effect))))return;
 if(f->touch!=MT_DOWN&&(memcmp(&g->identity,&f->identity,sizeof(g->identity))||memcmp(g->effect,f->effect,sizeof(g->effect)))){memset(g,0,sizeof(*g));g->identity=f->identity;memcpy(g->effect,f->effect,sizeof(g->effect));}
 g->scope=bank->flip&&bank->assignment==BA_SEND?1:0;g->reported=1;
 /* Touch-down may repeat the last physical position. Only actual previously
  * received equality establishes that duplicate; cold first input is accepted. */
 if(position==previous)return;
 if(now>UINT32_MAX-120000){in->error=C_EXPIRED;return;}
 float value=(float)position/16383.0f;memcpy(&g->bits,&value,4);g->changed=1;g->expires=now+120000;
 unsigned field=bank_field(bank,strip);if(field>=CF_COUNT)return;
 InputGesture *d=&in->desires[strip][field];*d=*g;d->pending=1;d->field_incarnation=field?f->field_incarnation:0;
}
/* Buttons/encoders use copied MPC state and retain the complete current binding.
 * No local button toggle is published as musical state. */
static inline void input_control_bound(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,unsigned strip,unsigned field,int delta,int push,uint32_t now,const MotorIdentity *id,unsigned scope){
 if(strip>=MIRROR_BANK||!bank->ready||!command_supported(field)||in->error||now>UINT32_MAX-120000)return;
 const CopiedTrack *t=input_track(s,id);if(field==CF_VOLUME&&t&&midi_volume(t->vptr))field=CF_MIDI_VOLUME;if(!t||(id->pad_owner&&!pad_controller(field)))return;
 CopiedField volume;const CopiedField *f=copied_field(s,t,command_source_field(field),&volume);
 if(!f||!f->available||((command_float(field)||field==CF_MUTE||field==CF_SOLO)&&!(field==CF_MIDI_VOLUME?midi_volume(t->vptr):mixable(t->vptr))))return;
 InputGesture *g=&in->desires[strip][field];if(memcmp(&g->identity,id,sizeof(*id)))memset(g,0,sizeof(*g));InputFlight *flight=input_existing(in,id,field);uint32_t bits;
 if(command_float(field)){float value;uint32_t prior=g->pending?g->bits:flight?flight->bits:f->bits;memcpy(&value,&prior,4);value=push?(field==CF_PAN?0.5f:field==CF_VOLUME&&bank->assignment==BA_SEND&&scope==3?0.707945764f:0.0f):value+(float)delta/127.0f;if(value<0)value=0;if(value>1)value=1;memcpy(&bits,&value,4);}
 else if(field==CF_SELECTION){
  /* Selection is project-global: only the latest accepted press remains
   * pending. A dispatched transaction keeps its separate settlement state. */
  for(unsigned i=0;i<MIRROR_BANK;i++)memset(&in->desires[i][CF_SELECTION],0,sizeof(in->desires[i][CF_SELECTION]));
  bits=1;
 }
 else{const CopiedField *ui=t->fields+field;if(!ui->available)return;uint32_t prior=flight?flight->bits:ui->bits;
  for(unsigned i=0;i<in->edge_count;i++){InputEdge *e=in->edges+(in->edge_head+i)%INPUT_EDGES;if(e->desire.pending&&e->field==field&&e->desire.identity.pad_owner==id->pad_owner&&(field==CF_ARM?e->desire.identity.track_owner==id->track_owner:e->desire.identity.program_owner==id->program_owner))prior=e->desire.bits;}
  if(in->edge_count==INPUT_EDGES){in->error=C_CAPACITY;return;}
  InputEdge *e=in->edges+(in->edge_head+in->edge_count++)%INPUT_EDGES;
  *e=(InputEdge){.desire={.identity=*id,.bits=!prior,.expires=now+120000,.field_incarnation=f->incarnation,.pending=1},.strip=strip,.field=field};return;
 }
 *g=(InputGesture){.identity=*id,.bits=bits,.expires=now+120000,.field_incarnation=f->incarnation,.pending=1,.scope=scope};
}
/* Relative encoder intent is scoped to the copied visible processor page.
 * Push toggles only a native two-state parameter. */
static inline float input_effect_clamp(float value,float low,float high){return value<low?low:value>high?high:value;}
static inline int input_mode_busy(const MirrorInput *in){
 if(in->mode_delta)return 1;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in->flights[i].flight&&command_qlink_mode(in->flights[i].field)&&!in->flights[i].acknowledged)return 1;
 return 0;
}
static inline void input_mode_add(MirrorInput *in,const CopiedMirror *s,int direction,uint32_t now){
 const QLinkCopy *q=&s->qlinks;if(!s->ready||!s->qlinks_available||!q->mode_valid||!q->mode_controller||!q->mode_generation||q->mode_tick>now||now-q->mode_tick>=QLINK_FRESH_MS||in->error)return;
 if(in->mode_request.epoch!=s->epoch||in->mode_request.global_owner!=q->mode_controller||in->mode_request.field_incarnation!=q->mode_generation)in->mode_delta=0;
 int next=in->mode_delta+direction;if(next<-32||next>32)return;
 in->mode_delta=next;in->mode_request=(CommandRequest){.reserved=QLINK_MODE,.epoch=s->epoch,.expires=now+120000,.project_owner=s->project_owner,.global_owner=q->mode_controller,.field_incarnation=q->mode_generation,.qlink_root=q->root};
 for(unsigned i=0;i<QLINK_SLOTS;i++)in->qlinks[i].pending=0;
}
static inline void input_mode_submit(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,uint32_t now){
 if(!in->mode_delta)return;
 const QLinkCopy *q=&s->qlinks;CommandRequest r=in->mode_request;
 if(bank->assignment!=BA_QLINK||!bank->ready||s->epoch!=r.epoch||now>r.expires){in->mode_delta=0;return;}
 if(!s->qlinks_available||!q->mode_valid)return;
 if(q->root!=r.qlink_root||q->mode_controller!=r.global_owner||q->mode_generation!=r.field_incarnation){in->mode_delta=0;return;}
 r.bits=(uint32_t)in->mode_delta;r.before_bits=q->mode_id;r.before_revision=q->mode_revision;
 unsigned at=command_free(in->commands);if(at>=COMMAND_SLOTS)return;
 r.seq=atomic_load(&in->commands->published)+1;r.pid=in->commands->pid;r.start_lo=in->commands->start_lo;r.start_hi=in->commands->start_hi;r.origin_sec=in->commands->origin_sec;r.origin_nsec=in->commands->origin_nsec;r.created=now;
 if(!command_publish_request_at(in->commands,&r,at))return;
 InputFlight *tx=in->flights+at;memset(tx,0,sizeof(*tx));tx->flight=r.seq;tx->field=r.reserved;tx->bits=r.bits;tx->expires=r.expires;tx->qlink_request=r;tx->field_incarnation=r.field_incarnation;in->mode_delta=0;in->submitted++;
 INPUT_LOG("QLINK_MODE_SUBMIT request=%u direction=%d controller=%u; UI owner resolves actual eligible mode\n",r.seq,(int32_t)r.bits,r.global_owner);
}
/* Routing choices remain relative until the native UI owner resolves the
 * current catalogue. No desired port/route is written into copied state. */
static inline int input_io_same(const CommandRequest *r,const CopiedMirror *s,unsigned field){
 const IOCopy *q=&s->io;const IOField *f=q->fields+field;
 return r->epoch==s->epoch&&r->serial==q->serial&&r->project_owner==s->project_owner&&r->track_owner==q->track_owner&&r->program_owner==q->program_owner&&r->io_generation==q->generation&&r->io_choice_generation==f->choice_generation&&r->field_incarnation==f->incarnation&&r->global_owner==f->property&&r->io_connector==f->connector;
}
static inline void input_io_edit(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,unsigned field,int delta,uint32_t now){
 if(field>=IO_FIELDS||!delta||bank->assignment!=BA_IO||!bank->ready||!s->io_available||in->error||now>UINT32_MAX-120000)return;
 const IOCopy *q=&s->io;const IOField *f=q->fields+field;
 if(q->serial!=bank->selected.serial||f->status!=IO_READY||!f->property||!f->incarnation||!f->choice_generation||!f->choice_count||f->tick>now||now-f->tick>=IO_FRESH_MS)return;
 if(!input_io_same(in->io_requests+field,s,field))in->io_delta[field]=0;
 int next=in->io_delta[field]+delta;if(next<-32||next>32)return;
 in->io_delta[field]=next;
 in->io_requests[field]=(CommandRequest){.reserved=IO_PARAMETER,.epoch=s->epoch,.serial=q->serial,.binding=q->binding,.incarnation=q->incarnation,.expires=now+120000,.track_owner=q->track_owner,.program_owner=q->program_owner,.project_owner=s->project_owner,.field_incarnation=f->incarnation,.global_owner=f->property,.io_generation=q->generation,.io_choice_generation=f->choice_generation,.io_field=field,.io_connector=f->connector};
}
static inline void input_io_submit(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,uint32_t now){
 for(unsigned field=0;field<IO_FIELDS;field++){
  if(!in->io_delta[field])continue;
  CommandRequest r=in->io_requests[field];
  if(bank->assignment!=BA_IO||!bank->ready||!s->ready||s->epoch!=r.epoch||bank->selected.serial!=r.serial||now>r.expires){in->io_delta[field]=0;continue;}
  if(!s->io_available)continue;
  const IOField *f=s->io.fields+field;
  if(!input_io_same(&r,s,field)||f->status!=IO_READY){in->io_delta[field]=0;continue;}
  if(f->tick>now||now-f->tick>=IO_FRESH_MS)continue;
  unsigned at=command_free(in->commands);if(at>=COMMAND_SLOTS)return;
  r.bits=(uint32_t)in->io_delta[field];r.before_bits=f->value;r.before_revision=f->revision;r.seq=atomic_load(&in->commands->published)+1;r.pid=in->commands->pid;r.start_lo=in->commands->start_lo;r.start_hi=in->commands->start_hi;r.origin_sec=in->commands->origin_sec;r.origin_nsec=in->commands->origin_nsec;r.created=now;
  if(!command_publish_request_at(in->commands,&r,at))continue;
  InputFlight *tx=in->flights+at;memset(tx,0,sizeof(*tx));tx->flight=r.seq;tx->field=r.reserved;tx->bits=r.bits;tx->expires=r.expires;tx->io_request=r;tx->field_incarnation=r.field_incarnation;in->io_delta[field]=0;in->submitted++;
  INPUT_LOG("IO_SUBMIT request=%u field=%u direction=%d track=%u; native UI owner resolves current choice\n",r.seq,field,(int32_t)r.bits,r.serial);
 }
}
static inline int input_qlink_same(const CommandRequest *r,const CopiedMirror *s,unsigned index){
 const QLinkCopy *q=&s->qlinks;return r->epoch==s->epoch&&r->project_owner==s->project_owner&&r->qlink_root==q->root&&r->qlink_wrapper==q->wrapper&&r->qlink_provider==q->provider&&r->qlink_generation==q->generation&&r->qlink_index==index;
}
static inline const QLinkSlot *input_qlink_slot(const CopiedMirror *s,unsigned index,uint32_t now){
 if(index>=QLINK_SLOTS||!s->ready||!s->qlinks_available||s->qlinks.epoch!=s->epoch||s->qlinks.status!=QL_SAMPLED)return NULL;
 const QLinkSlot *p=s->qlinks.slots+index;
 return p->sampled&&p->normalized_valid&&p->tick<=now&&now-p->tick<QLINK_FRESH_MS?p:NULL;
}
static inline int input_qlink_wait(MirrorInput *in,const CopiedMirror *s,unsigned index,uint32_t now){
 InputQLink *g=in->qlinks+index;if(!input_qlink_same(&g->request,s,index)){memset(g,0,sizeof(*g));return 0;}
 const QLinkSlot *p=input_qlink_slot(s,index,now);
 if(g->wait_tick&&p&&p->refresh_kind)for(unsigned lane=0;lane<COMMAND_LANES;lane++)if(p->refresh_matches[lane]>g->barriers[lane]){g->wait_tick=0;break;}
 return g->wait_tick!=0;
}
static inline void input_qlink_edit(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,unsigned strip,int delta,int absolute,uint32_t now){
 if(strip>=8||bank->assignment!=BA_QLINK||!bank->ready||in->error||now>UINT32_MAX-120000||(!delta&&absolute<0))return;
 if(input_mode_busy(in))return;
 unsigned index=bank->qlink_page*8+strip;const QLinkSlot *p=input_qlink_slot(s,index,now);if(!p)return;
 InputQLink *g=in->qlinks+index;unsigned compatible=input_qlink_same(&g->request,s,index),prior=p->bits;
 if(!compatible)memset(g,0,sizeof(*g));
 if(compatible&&g->pending)prior=g->request.bits;
 else for(unsigned i=0;i<COMMAND_SLOTS;i++){const InputFlight *t=in->flights+i;if(t->flight&&!t->acknowledged&&command_qlink(t->field)&&input_qlink_same(&t->qlink_request,s,index))prior=t->bits;}
 float value;memcpy(&value,&prior,4);
 if(absolute>=0){value=absolute/16383.0f;g->relative=0;}
 else{float step=delta/127.0f;if(!g->pending){g->relative=1;g->shift=g->lower=0;g->upper=1;}if(g->relative){g->shift+=step;g->lower=input_effect_clamp(g->lower+step,0,1);g->upper=input_effect_clamp(g->upper+step,0,1);if(g->lower==g->upper)g->shift=0;}value=input_effect_clamp(value+step,0,1);}
 unsigned bits;memcpy(&bits,&value,4);const QLinkCopy *q=&s->qlinks;
 g->request=(CommandRequest){.epoch=s->epoch,.bits=bits,.expires=now+120000,.before_bits=p->bits,.before_revision=p->revision,.reserved=QLINK_VALUE,.project_owner=s->project_owner,.field_incarnation=q->generation,.qlink_root=q->root,.qlink_wrapper=q->wrapper,.qlink_provider=q->provider,.qlink_generation=q->generation,.qlink_index=index};g->pending=1;
}
static inline void input_qlink_pitch(MirrorInput *in,MirrorBank *bank,const CopiedMirror *s,unsigned strip,int position,uint32_t now){
 InputGesture *g=in->gestures+strip;g->changed=0;input_pitch(in,bank,strip,position,now);if(g->changed)input_qlink_edit(in,bank,s,strip,0,position,now);
}
static inline void input_qlink_submit(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,uint32_t now){
 if(input_mode_busy(in))return;
 for(unsigned index=0;index<QLINK_SLOTS;index++){
  InputQLink *g=in->qlinks+index;if(!g->pending)continue;
  if(bank->assignment!=BA_QLINK||!bank->ready||index/8!=bank->qlink_page||!input_qlink_same(&g->request,s,index)||now>g->request.expires){g->pending=0;continue;}
  const QLinkSlot *p=input_qlink_slot(s,index,now);if(!p){g->pending=0;continue;}
  unsigned busy=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in->flights[i].flight&&command_qlink(in->flights[i].field)&&input_qlink_same(&in->flights[i].qlink_request,s,index))busy=1;
  if(busy)continue; /* Native admission checks current activity; a copied gesture byte is not an input delay. */
  /* Relative aftermath admission is completed with source refresh witnesses.
   * Callback/cache readback alone cannot clear this barrier. */
  if(g->relative&&input_qlink_wait(in,s,index,now))continue;
  CommandRequest r=g->request;
  if(g->relative){float actual;memcpy(&actual,&p->bits,4);float desired=input_effect_clamp(actual+g->shift,g->lower,g->upper);memcpy(&r.bits,&desired,4);}
  if(r.bits==p->bits&&!g->wait_tick){g->pending=0;continue;}
  CommandState *c=in->commands;unsigned at=command_free(c);if(at>=COMMAND_SLOTS||INPUT_STOPPED())return;
  r.seq=atomic_load(&c->published)+1;r.pid=c->pid;r.start_lo=c->start_lo;r.start_hi=c->start_hi;r.origin_sec=c->origin_sec;r.origin_nsec=c->origin_nsec;r.created=now;r.before_bits=p->bits;r.before_revision=p->revision;
  InputFlight *tx=in->flights+at;memset(tx,0,sizeof(*tx));tx->flight=r.seq;tx->field=QLINK_VALUE;tx->strip=index%8;tx->bits=r.bits;tx->expires=r.expires;tx->qlink_request=r;tx->field_incarnation=r.qlink_generation;
  if(!command_publish_request_at(c,&r,at)){memset(tx,0,sizeof(*tx));if(!atomic_load(&c->new_project_intent))in->error=C_CLOSED;return;}
  g->pending=0;in->submitted++;INPUT_LOG("QLINK_SUBMIT request=%u index=%u bits=%u generation=%u; native logical slot\n",r.seq,index,r.bits,r.qlink_generation);
 }
}
static inline void input_effect_edit(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,unsigned strip,int delta,int push,uint32_t now){
 if((!push&&!delta)||strip>=EFFECT_PAGE||in->error||!bank->ready||bank->assignment!=BA_EFFECT||!s->effects_available||s->effects.status!=EF_READY||s->effects.serial!=bank->selected.serial||s->effects.slot!=bank->effects_slot||s->effects.page!=bank->effects_page||s->effects.slot>=EFFECT_SLOTS||now>UINT32_MAX-120000)return;
 const EffectsCopy *e=&s->effects;const EffectParameter *p=e->parameters+strip;const EffectSlot *slot=e->slots+e->slot;
 if(p->status!=EF_READY||p->tick>now||now-p->tick>=EFFECT_FRESH_MS||slot->status!=EF_READY||p->index>=slot->count||(push==1&&p->steps!=2)||(push==2&&!p->default_valid))return;
 InputEffect *g=in->effects+strip;unsigned prior=p->bits;
 unsigned compatible=g->pending&&g->request.reserved==EFFECT_PARAMETER&&g->request.effect_generation==e->generation&&g->request.effect_slot==e->slot&&g->request.effect_index==p->index&&g->request.serial==e->serial;
 if(compatible)prior=g->request.bits;
 else for(unsigned i=0;i<COMMAND_SLOTS;i++){InputFlight *f=in->flights+i;if(f->flight&&!f->acknowledged&&f->field==EFFECT_PARAMETER&&f->effect_request.effect_generation==e->generation&&f->effect_request.serial==e->serial&&f->effect_request.effect_slot==e->slot&&f->effect_request.effect_index==p->index)prior=f->bits;}
 float value;memcpy(&value,&prior,4);
 if(push){g->relative=0;if(push==2)memcpy(&value,&p->default_bits,4);else value=value>=0.5f?0:1;}
 else{
  float step=(float)delta/127.0f;
  if(!compatible){g->relative=1;g->shift=g->lower=0;g->upper=1;}
  /* Compose ordered clamped relative motion. Rebase only this transform on
   * the actual source after an outstanding command releases its identity.
   * A toggle is an explicit endpoint and never gets this relative rebasing. */
  if(g->relative){g->shift+=step;g->lower=input_effect_clamp(g->lower+step,0,1);g->upper=input_effect_clamp(g->upper+step,0,1);if(g->lower==g->upper)g->shift=0;}
  value=input_effect_clamp(value+step,0,1);
 }
 unsigned bits;memcpy(&bits,&value,4);
 g->identity=bank->selected;g->request=(CommandRequest){.epoch=s->epoch,.serial=e->serial,.binding=bank->selected.binding,.incarnation=bank->selected.incarnation,.bits=bits,.expires=now+120000,.before_bits=p->bits,.before_revision=p->revision,.reserved=EFFECT_PARAMETER,.track_owner=e->track_owner,.program_owner=e->program_owner,.field_incarnation=e->generation,.project_owner=s->project_owner,.effect_generation=e->generation,.effect_slot=e->slot,.effect_index=p->index,.effect_key=slot->key,.effect_ap=slot->ap,.effect_position=p->position};g->pending=1;
}
static inline void input_effect_pitch(MirrorInput *in,MirrorBank *bank,const CopiedMirror *s,unsigned strip,int position,uint32_t now){
 InputGesture *g=in->gestures+strip;g->changed=0;input_pitch(in,bank,strip,position,now);if(!g->changed)return;
 InputEffect *d=in->effects+strip;d->pending=0;input_effect_edit(in,bank,s,strip,1,0,now);
 if(d->pending){d->relative=0;d->request.bits=g->bits;}
}
static inline void input_effect_enable(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,unsigned strip,uint32_t now){
 if(strip>=EFFECT_SLOTS||in->error||!bank->ready||bank->assignment!=BA_EFFECT||bank->effects_slot!=EFFECT_LIST||!s->effects_available||s->effects.status!=EF_READY||s->effects.slot!=EFFECT_LIST||s->effects.serial!=bank->selected.serial||now>UINT32_MAX-120000)return;
 const EffectsCopy *e=&s->effects;const EffectSlot *slot=e->slots+strip;
 if(slot->status!=EF_READY||!slot->key||!slot->ap||!slot->enable_valid||slot->enable_tick>now||now-slot->enable_tick>=EFFECT_FRESH_MS)return;
 InputEffect *g=in->effects+strip;uint32_t prior=slot->enable_bits;
 if(g->pending&&g->request.reserved==EFFECT_ENABLE&&g->request.effect_generation==e->generation&&g->request.effect_slot==strip&&g->request.serial==e->serial)prior=g->request.bits;
 else for(unsigned i=0;i<COMMAND_SLOTS;i++){const InputFlight *f=in->flights+i;if(f->flight&&!f->acknowledged&&f->field==EFFECT_ENABLE&&f->effect_request.effect_generation==e->generation&&f->effect_request.effect_slot==strip&&f->effect_request.serial==e->serial)prior=f->bits;}
 g->identity=bank->selected;g->relative=0;g->request=(CommandRequest){.epoch=s->epoch,.serial=e->serial,.binding=bank->selected.binding,.incarnation=bank->selected.incarnation,.bits=prior?0:0x3f800000u,.expires=now+120000,.before_bits=slot->enable_bits,.before_revision=slot->enable_revision,.reserved=EFFECT_ENABLE,.track_owner=e->track_owner,.program_owner=e->program_owner,.field_incarnation=e->generation,.project_owner=s->project_owner,.effect_generation=e->generation,.effect_slot=strip,.effect_key=slot->key,.effect_ap=slot->ap};g->pending=1;
}
static inline void input_effect_chooser(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,unsigned slot,unsigned replace,uint32_t now){
 if(slot>=EFFECT_SLOTS||replace>1||in->error||!bank->ready||bank->assignment!=BA_EFFECT||!s->effects_available||s->effects.status!=EF_READY||s->effects.serial!=bank->selected.serial||s->effects.slot!=bank->effects_slot||s->effects.page!=bank->effects_page||(replace?bank->effects_slot!=slot:bank->effects_slot!=EFFECT_LIST)||now>UINT32_MAX-120000)return;
 const EffectsCopy *e=&s->effects;const EffectSlot *item=e->slots+slot;
 if(replace?(item->status!=EF_READY||!item->key||!item->ap):(item->status!=EF_EMPTY||item->key||item->ap))return;
 if(e->chooser.status==EC_OPEN||e->chooser.status==EC_LOADING)return;
 InputEffect *g=in->effects+slot;memset(g,0,sizeof(*g));g->identity=bank->selected;
 g->request=(CommandRequest){.epoch=s->epoch,.serial=e->serial,.binding=bank->selected.binding,.incarnation=bank->selected.incarnation,.bits=replace,.expires=now+120000,.reserved=EFFECT_CHOOSER,.track_owner=e->track_owner,.program_owner=e->program_owner,.field_incarnation=e->generation,.project_owner=s->project_owner,.effect_generation=e->generation,.effect_slot=slot,.effect_key=item->key,.effect_ap=item->ap,.effect_position=e->page};g->pending=1;
}
static inline void input_effect_submit(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,uint32_t now){
 for(unsigned strip=0;strip<EFFECT_PAGE;strip++){
  InputEffect *g=in->effects+strip;if(!g->pending)continue;CommandRequest r=g->request;const EffectsCopy *e=&s->effects;
  unsigned enable=r.reserved==EFFECT_ENABLE,chooser=command_chooser(r.reserved);
  if(bank->assignment!=BA_EFFECT||!bank->ready||!s->effects_available||e->status!=EF_READY||r.epoch!=s->epoch||r.serial!=bank->selected.serial||r.effect_generation!=e->generation||r.effect_slot>=EFFECT_SLOTS||e->slot!=bank->effects_slot||e->page!=bank->effects_page||(chooser?(r.bits?r.effect_slot!=e->slot:e->slot!=EFFECT_LIST):enable?e->slot!=EFFECT_LIST:(r.effect_slot!=e->slot||r.effect_position/EFFECT_PAGE!=e->page))||r.effect_key!=e->slots[r.effect_slot].key||r.effect_ap!=e->slots[r.effect_slot].ap){g->pending=0;continue;}
  uint32_t before,revision;
  if(chooser){const EffectSlot *slot=e->slots+r.effect_slot;if(r.effect_position!=e->page||(r.bits?slot->status!=EF_READY:slot->status!=EF_EMPTY)||e->chooser.status==EC_OPEN||e->chooser.status==EC_LOADING){g->pending=0;continue;}before=revision=0;}
  else if(enable){const EffectSlot *slot=e->slots+r.effect_slot;if(slot->status!=EF_READY||!slot->enable_valid||slot->enable_tick>now||now-slot->enable_tick>=EFFECT_FRESH_MS){g->pending=0;continue;}before=slot->enable_bits;revision=slot->enable_revision;}
  else{const EffectParameter *p=e->parameters+strip;if(p->status!=EF_READY||p->index!=r.effect_index||p->position!=r.effect_position||p->tick>now||now-p->tick>=EFFECT_FRESH_MS){g->pending=0;continue;}before=p->bits;revision=p->revision;}
  if(now>r.expires){g->pending=0;continue;}unsigned busy=0;
  for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in->flights[i].flight&&(command_effect(in->flights[i].field)||command_chooser(in->flights[i].field))&&command_conflict(&r,&in->flights[i].effect_request))busy=1;
  if(busy)continue;
  if(g->relative){float actual;memcpy(&actual,&before,4);float desired=input_effect_clamp(actual+g->shift,g->lower,g->upper);memcpy(&r.bits,&desired,4);}
  if(!chooser&&r.bits==before){g->pending=0;continue;}
  CommandState *c=in->commands;unsigned at=command_free(c);if(at>=COMMAND_SLOTS||INPUT_STOPPED())return;
  r.seq=atomic_load(&c->published)+1;r.pid=c->pid;r.start_lo=c->start_lo;r.start_hi=c->start_hi;r.origin_sec=c->origin_sec;r.origin_nsec=c->origin_nsec;r.created=now;r.before_bits=before;r.before_revision=revision;
  InputFlight *tx=in->flights+at;memset(tx,0,sizeof(*tx));tx->flight=r.seq;tx->field=r.reserved;tx->target=g->identity;tx->strip=strip;tx->bits=r.bits;tx->expires=r.expires;tx->effect_request=r;tx->field_incarnation=r.field_incarnation;
  if(!command_publish_request_at(c,&r,at)){memset(tx,0,sizeof(*tx));if(!atomic_load(&c->new_project_intent))in->error=C_CLOSED;return;}
  g->pending=0;in->submitted++;INPUT_LOG("EFFECT_SUBMIT request=%u track=%u slot=%u parameter=%u generation=%u bits=%u\n",r.seq,r.serial,r.effect_slot,r.effect_index,r.effect_generation,r.bits);
 }
}
static inline void input_control(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,unsigned strip,unsigned field,int delta,int push,uint32_t now){
 if(bank->assignment==BA_SEND&&(field==CF_ARM||field==CF_SOLO||field==CF_MUTE||field==CF_SELECTION))return;
 if(strip<MIRROR_BANK)input_control_bound(in,bank,s,strip,field,delta,push,now,&bank->strips[strip],3);
}
/* Channel +/- changes the real selected Track via the existing selection
 * command. Unsent steps accumulate from the latest target, including off-bank
 * targets; actual source selection alone drives the bank-follow readback. */
static inline void input_channel(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,int direction,uint32_t now){
 if(!bank->ready||!s->selection.available)return;
 MotorIdentity from=bank->selected;
 if(bank->assignment==BA_SEND&&!from.serial)for(unsigned i=0;i<s->count;i++)if(s->tracks[i].track==s->selection.bits){from=bank_identity(s,s->tracks+i);break;}
 InputFlight *flight=input_existing(in,&from,CF_SELECTION);
 if(flight&&!flight->acknowledged)from=flight->target;
 for(unsigned i=0;i<MIRROR_BANK;i++)if(in->desires[i][CF_SELECTION].pending)from=in->desires[i][CF_SELECTION].identity;
 unsigned indices[MIRROR_TRACKS],n=0,at=MIRROR_TRACKS;
 for(unsigned i=0;i<s->count;i++)if(bank_role((bank->view==BV_DRUM_PADS||bank->assignment==BA_SEND)?BV_TRACK:bank->view,s->tracks[i].vptr)){if(s->tracks[i].serial==from.serial)at=n;indices[n++]=i;}
 unsigned next;
 if(at==MIRROR_TRACKS){
  if(bank->assignment!=BA_SEND||!n)return;
  next=direction<0?n-1:0;
 }else{
  if((!at&&direction<0)||(at+1>=n&&direction>0))return;
  next=direction<0?at-1:at+1;
 }
 const CopiedTrack *t=s->tracks+indices[next];
 MotorIdentity id={s->epoch,t->serial,t->binding,t->incarnation,t->track,t->program,t->track_owner,t->program_owner,0,0,0};
 input_control_bound(in,bank,s,0,CF_SELECTION,0,1,now,&id,2);
}
static inline int input_begin(MirrorInput *in,CommandState *commands){
 memset(in,0,sizeof(*in));in->commands=commands;
 if(!input_health(in))return 0;
 unsigned n=atomic_load_explicit(&commands->published,memory_order_acquire);
 if(n>COMMAND_SEQUENCE_LAST||!command_idle(commands)){in->error=C_BUSY;return 0;}
 return 1;
}
/* These identities authorize an intent; its value is read only on the native UI owner. */
static inline unsigned input_global_owner(const CopiedMirror *s,unsigned op){
 if(op==CF_AUTOMATION)return s->automation.available?s->automation.owner_incarnation:0;
 if(op==GLOBAL_RECORD_TOGGLE)return s->record_mode.available?s->record_mode.owner_incarnation:0;
 if(op==GLOBAL_CLICK_TOGGLE)return s->click.available?s->click.owner_incarnation:0;
 if(op==GLOBAL_LOOP_TOGGLE)return s->project_owner;
 return op==GLOBAL_TRACK_NEW||op==GLOBAL_TRACK_TYPE||command_transport(op)||command_history(op)||op==GLOBAL_SAVE||command_page(op)||command_key(op)?s->editor_owner:s->zoom_owner;
}
static inline unsigned input_global_field(const CopiedMirror *s,unsigned op){
 return op==GLOBAL_TRACK_TYPE?s->selected_serial:op==CF_AUTOMATION?s->automation.incarnation:op==GLOBAL_RECORD_TOGGLE?s->record_mode.incarnation:op==GLOBAL_CLICK_TOGGLE?s->click.incarnation:0;
}
static inline void input_global_add(MirrorInput *in,const CopiedMirror *s,unsigned op,unsigned bits,uint32_t now){
 if(!s->ready||!s->alive||s->error||!command_global(op)||in->error||now>UINT32_MAX-2000)return;
 if(in->jog_epoch!=s->epoch){input_jog_discard(in);in->jog_epoch=s->epoch;}
 unsigned owner=input_global_owner(s,op);
 if(!owner||(op==GLOBAL_TRACK_TYPE&&!s->selected_serial)||(op==CF_AUTOMATION&&!s->automation.available))return;
 if(in->global_count==INPUT_JOG_EVENTS){in->error=C_CAPACITY;return;}
 in->global_events[(in->global_head+in->global_count++)%INPUT_JOG_EVENTS]=(InputGlobalEvent){op,bits,s->epoch,owner,input_global_field(s,op),now+2000};
}
static inline int input_global_proof(MirrorInput *in,InputFlight *tx){
 unsigned wanted=tx->field==CF_AUTOMATION||command_toggle(tx->field)?7:5;
 const CommandEvent *begin=tx->global_events,*state=begin+1,*end=begin+2;
 if(tx->global_seen!=wanted||begin->token!=in->commands->owner_token||begin->program!=end->program||begin->bits!=tx->bits||begin->revision!=tx->global_epoch||end->revision!=tx->global_epoch){in->error=C_TRACE_AMBIGUOUS;return 0;}
 if(command_transport(tx->field)){if(!end->token||end->token==begin->token||!end->capture||!end->counter||!begin->sp||end->sp){in->error=C_TRACE_AMBIGUOUS;return 0;}}
 else if(end->token!=begin->token||begin->sp!=end->sp){in->error=C_TRACE_AMBIGUOUS;return 0;}
 if(tx->field==CF_AUTOMATION&&(state->token!=begin->token||state->program!=begin->program||state->revision!=tx->global_epoch||state->bits>2||state->counter>1||state->bits!=end->bits)){in->error=C_TRACE_AMBIGUOUS;return 0;}
 if(command_toggle(tx->field)&&(state->token!=begin->token||state->program!=begin->program||!begin->capture||state->capture!=begin->capture||state->revision!=tx->global_epoch||state->bits!=end->bits)){in->error=C_TRACE_AMBIGUOUS;return 0;}
 if(tx->field!=CF_AUTOMATION&&end->bits>1){in->error=C_TRACE_AMBIGUOUS;return 0;}
 tx->done_tick=end->tick;return 1;
}
/* An event may be newer than the caller's clock sample. Bound its tick by the
 * finite producer window; settlement waits a later fresh MMV7 heartbeat.
 * Events are immutable after each lane's release frontier. Source phases must
 * share a lane/tuple. Cross-lane dispatch/return order is deliberately unused. */
static inline void input_jog_add_origin(MirrorInput *in,const CopiedMirror *s,unsigned operation,int delta,uint32_t now,unsigned origin){
 if(in->jog_epoch!=s->epoch){input_jog_discard(in);in->jog_epoch=s->epoch;}
 if(!s->ready||!s->alive||s->error||!command_jog(operation)||!command_jog_delta((uint32_t)delta)||in->error)return;
 if(now>UINT32_MAX-120000){in->error=C_EXPIRED;return;}
 if(in->jog_count){InputJogEvent *last=in->jog_events+(in->jog_head+in->jog_count-1)%INPUT_JOG_EVENTS;
  int64_t sum=(int64_t)last->delta+delta;
  if(last->epoch==s->epoch&&last->operation==operation&&last->origin==origin&&((last->delta>0)==(delta>0))&&sum>=-JOG_DELTA_LIMIT&&sum<=JOG_DELTA_LIMIT){last->delta=(int32_t)sum;last->expires=now+1000;return;}
 }
 if(in->jog_count==INPUT_JOG_EVENTS){in->error=C_CAPACITY;return;}
 /* Preserve each packet's magnitude, unit and order: opposite steps cannot
  * cancel across native boundary/clamping/rounding semantics. */
 in->jog_events[(in->jog_head+in->jog_count++)%INPUT_JOG_EVENTS]=(InputJogEvent){delta,operation,s->epoch,now+1000,origin};
}
static inline void input_jog_add(MirrorInput *in,const CopiedMirror *s,unsigned operation,int delta,uint32_t now){input_jog_add_origin(in,s,operation,delta,now,0);}
static inline void input_jog_release(MirrorInput *in,unsigned origin){
 unsigned n=0;for(unsigned i=0;i<in->jog_count;i++){InputJogEvent e=in->jog_events[(in->jog_head+i)%INPUT_JOG_EVENTS];if(e.origin!=origin)in->jog_events[(in->jog_head+n++)%INPUT_JOG_EVENTS]=e;}in->jog_count=n;
}
/* Play and Stop are serialized native intents. A Stop after an unsampled Play
 * remains a Stop; only an independently observed stopped state returns home. */
static inline void input_stop_button(MirrorInput *in,const CopiedMirror *s,unsigned note,int down,uint32_t now){
 if(in->jog_epoch!=s->epoch){input_jog_discard(in);in->jog_epoch=s->epoch;}
 if(note==94){
  if(!down){in->play_down=0;return;}
  if(in->play_down)return;
  in->play_down=1;in->stop_play_wait=1;in->stop_home=0;input_jog_release(in,3);
  input_global_add(in,s,GLOBAL_PLAY,0,now);return;
 }
 if(!down){in->stop_down=0;return;}
 if(in->stop_down)return;
 in->stop_down=1;
 unsigned pending_play=in->stop_play_wait;in->stop_play_wait=0;
 if(s->playing.available&&(s->playing.bits||pending_play)){input_global_add(in,s,GLOBAL_STOP,0,now);return;}
 if(s->playing.available){
  in->stop_home=1;in->stop_until=now>UINT32_MAX-2000?UINT32_MAX:now+2000;in->stop_position_tick=0;
  in->jog_head=in->jog_count=0;in->jog_rewind=in->jog_forward=0;
 }
}
static inline void input_stop_home(MirrorInput *in,const CopiedMirror *s,uint32_t now){
 if(s->playing.available&&s->playing.bits)in->stop_play_wait=0;
 if(!in->stop_home)return;
 if(now>in->stop_until){in->stop_home=0;return;}
 if(!s->playing.available||s->playing.bits||!s->position_available)return;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in->flights[i].flight&&command_jog(in->flights[i].field))return;
 if(in->jog_count||(in->stop_position_tick&&s->position_tick<=in->stop_position_tick))return;
 unsigned amount=s->bar>=JOG_DELTA_LIMIT?JOG_DELTA_LIMIT:s->bar+1;
 input_jog_add_origin(in,s,JOG_BAR,-(int)amount,now,3);
 in->stop_position_tick=s->position_tick;
 if(s->bar<JOG_DELTA_LIMIT)in->stop_home=0;
}
static inline int input_jog_admit(MirrorInput *in,const CopiedMirror *s){
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in->flights[i].flight&&command_jog(in->flights[i].field))return 0;
 if(!s->playing.available)return 0;
 if(!s->playing.bits){in->jog_wait=0;return 1;}
 if(!in->jog_wait)return !atomic_load(&in->commands->navigation_error);
 CommandState *c=in->commands;uint32_t rev=atomic_load_explicit(&c->navigation_revision,memory_order_acquire);
 if(!rev||(rev&1)||atomic_load(&c->navigation_error))return 0;
 uint32_t owner=atomic_load(&c->navigation_owner),epoch=atomic_load(&c->navigation_epoch),count=atomic_load(&c->navigation_count);
 atomic_thread_fence(memory_order_acquire);
 if(rev!=atomic_load_explicit(&c->navigation_revision,memory_order_relaxed))return 0;
 if(owner==in->jog_wait_owner&&owner==s->playing.owner_incarnation&&epoch==in->jog_wait_epoch&&epoch==s->epoch&&count>in->jog_wait_count){in->jog_wait=0;return 1;}return 0;
}
static inline int input_jog_proof(MirrorInput *in,InputFlight *tx){
 if(tx->jog_seen!=7||!tx->jog_outcome_seen){in->error=C_TRACE_AMBIGUOUS;return 0;}
 const CommandEvent *q=tx->jog_events,*e=q+1,*d=q+2;
 if(!q->capture||!q->program||q->counter!=q->program+8||q->token!=in->commands->owner_token||q->revision!=tx->target.epoch||e->capture!=q->capture||d->capture!=q->capture||e->program!=q->program||d->program!=q->program||e->counter!=q->counter||d->counter!=q->counter||!e->token||e->token!=d->token||d->sp+0x30!=e->sp){in->error=C_TRACE_AMBIGUOUS;return 0;}
 const CommandEvent *o=&tx->jog_outcome;
 if(o->capture!=d->capture||o->program!=d->program||o->token!=d->token||o->sp!=d->sp||o->revision!=e->revision||o->arg_kind>31||(o->arg_kind&4&&!(o->arg_kind&2))){in->error=C_TRACE_AMBIGUOUS;return 0;}
 tx->done_tick=d->tick;return 1;
}
static inline int input_master_proof(MirrorInput *in,InputFlight *tx){
 if((tx->master_seen&63)!=63||tx->master_seen>127){in->error=C_TRACE_AMBIGUOUS;return 0;}
 const CommandEvent *create=tx->master_events,*queue=create+1,*enter=create+2,*set=create+3,*value=create+4,*done=create+5;
 if(create->capture!=create->counter+0x10||queue->counter!=create->capture+0x34||!queue->capture||create->program!=queue->program||queue->program+0xa8!=tx->property||create->token!=in->commands->owner_token||queue->token!=create->token||create->revision!=tx->global_epoch||queue->revision!=tx->global_epoch||!enter->token||enter->token!=set->token||enter->token!=value->token||enter->token!=done->token||enter->sp!=done->sp+8||set->sp!=value->sp||value->revision<tx->before_revision||(value->revision&1)||done->bits!=value->bits){in->error=C_TRACE_AMBIGUOUS;return 0;}
 for(unsigned i=2;i<6;i++)if(create[i].capture!=queue->capture||create[i].counter!=queue->counter||create[i].program!=queue->program){in->error=C_TRACE_AMBIGUOUS;return 0;}
 if(tx->master_seen&64){const CommandEvent *commit=create+6;if(commit->capture!=queue->capture||commit->counter!=queue->counter||commit->program!=queue->program||commit->token!=enter->token||commit->revision<=tx->before_revision||commit->revision>value->revision){in->error=C_TRACE_AMBIGUOUS;return 0;}}
 tx->commit_revision=value->revision;tx->done_tick=done->tick;return 1;
}
/* Native recording completion may be a valid no-op (no current ClipPlayer or
 * recording slot). A recorder callback handles MPC's buffer; it is not a
 * promise that every controller sample becomes a separate persisted point. */
static inline int input_recording_proof(MirrorInput *in,InputFlight *tx){
 unsigned seen=tx->recording_seen;const CommandEvent *e=tx->recording_events;
 if((seen&11)!=11||((seen&752)&&((seen&752)!=752))||(seen&256&&!(seen&4))){in->error=C_TRACE_AMBIGUOUS;return 0;}
 if(!e[0].capture||!e[0].counter||e[0].token!=in->commands->owner_token||!e[1].token||e[1].token!=e[3].token||e[3].sp+0x18!=e[1].sp){in->error=C_TRACE_AMBIGUOUS;return 0;}
 for(unsigned i=1;i<4;i++)if(seen&(1u<<i)){if(e[i].capture!=e[0].capture||e[i].counter!=e[0].counter||(i==2&&e[i].token!=e[1].token)){in->error=C_TRACE_AMBIGUOUS;return 0;}}
 if(seen&240){
  if(e[9].token!=e[4].token){in->error=C_TRACE_AMBIGUOUS;return 0;}
  if(!e[4].capture||!e[4].counter||e[4].token!=e[1].token||!e[5].token||e[5].token!=e[6].token||e[5].token!=e[7].token||e[5].sp!=e[6].sp+0x30||e[6].sp!=e[7].sp){in->error=C_TRACE_AMBIGUOUS;return 0;}
  for(unsigned i=5;i<8;i++)if(e[i].capture!=e[4].capture||e[i].counter!=e[4].counter){in->error=C_TRACE_AMBIGUOUS;return 0;}
 }
 if(tx->target.pad_owner&&(seen&256)&&!tx->pad_seen){in->error=C_TRACE_AMBIGUOUS;return 0;}
 if(tx->pad_seen&&(!(seen&4)||tx->pad_execution.token!=e[1].token)){in->error=C_TRACE_AMBIGUOUS;return 0;}
 unsigned property=tx->pad_seen?tx->pad_execution.capture+pad_offset(pad_field(command_source_field(tx->field))):tx->property;
 unsigned before=tx->pad_seen?tx->pad_execution.revision:tx->before_revision;
 if(seen&256){if(e[8].capture!=e[0].capture||e[8].counter!=e[0].counter||e[8].token!=e[1].token||e[8].property!=property||e[8].revision<=before||(e[8].revision&1)){in->error=C_TRACE_AMBIGUOUS;return 0;}tx->commit_revision=e[8].revision;}
 tx->done_tick=e[3].tick;if((seen&128)&&e[7].tick>tx->done_tick)tx->done_tick=e[7].tick;
 return 1;
}
static inline int input_midi_proof(MirrorInput *in,InputFlight *tx){
 const CommandEvent *q=tx->midi_events,*enter=q+1,*set=q+2,*commit=q+3,*done=q+4;unsigned seen=tx->midi_seen;
 if((seen&19)!=19||(seen&8&&!(seen&4))||q->revision<1||q->revision>2||enter->revision!=q->revision||done->revision!=q->revision||!q->capture||!q->counter||q->token!=in->commands->owner_token||!enter->token||enter->token!=done->token||enter->sp!=done->sp+(q->revision==1?8:0x18)||(q->revision==1&&q->counter!=tx->target.track+0x4d4)){in->error=C_TRACE_AMBIGUOUS;return 0;}
 for(unsigned i=1;i<5;i++)if(seen&(1u<<i))if(q[i].capture!=q->capture||q[i].counter!=q->counter||q[i].token!=enter->token){in->error=C_TRACE_AMBIGUOUS;return 0;}
 if(seen&4){
  if(!set->revision||(set->revision&1)){in->error=C_TRACE_AMBIGUOUS;return 0;}
  if(seen&8){if(commit->revision<=set->revision||(commit->revision&1)){in->error=C_TRACE_AMBIGUOUS;return 0;}tx->commit_revision=commit->revision;}
  else if(set->bits!=tx->bits){in->error=C_TRACE_AMBIGUOUS;return 0;}
 }
 tx->done_tick=done->tick;return 1;
}
static inline void input_events(MirrorInput *in,InputFlight *tx,unsigned at){
 if(!tx->flight)return;
 for(unsigned lane=0;lane<COMMAND_LANES;lane++){
  const CommandLane *l=in->commands->slots[at].lanes+lane;
  uint32_t sequence=atomic_load_explicit(&l->sequence,memory_order_acquire);if(sequence!=tx->flight){if(sequence>tx->flight)in->error=C_TRACE_AMBIGUOUS;continue;}
  unsigned n=atomic_load_explicit(&l->published,memory_order_acquire);
  if(n>COMMAND_EVENTS||n<tx->cursor[lane]){in->error=C_FORMAT;return;}
  while(tx->cursor[lane]<n){
   CommandEvent copy;if(!command_event_read(l,tx->flight,tx->cursor[lane]++,&copy)){in->error=C_TRACE_AMBIGUOUS;return;}const CommandEvent *e=&copy;
   if(command_io(tx->field)){
    const CommandRequest *r=&tx->io_request;
    unsigned program_field=r->io_field==IO_AUDIO_IN||r->io_field==IO_AUDIO_OUT;
    if(program_field&&e->kind!=CE_DISPATCH&&e->kind!=CE_RETURN&&e->kind!=CE_IO_VALUE){
     if(e->reserved!=IO_PARAMETER||e->incarnation!=r->field_incarnation||e->arg_kind!=0x101||e->controller!=(r->io_field==IO_AUDIO_IN?1162u:325u)||(e->property&&e->kind!=CE_RECORD_TIME)||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
     if(e->kind>=CE_RECORD_QUEUE&&e->kind<=CE_RECORD_TIME){unsigned part=e->kind-CE_RECORD_QUEUE;if(part==8||(tx->recording_seen&(1u<<part))||e->revision!=r->epoch){in->error=C_TRACE_AMBIGUOUS;return;}tx->recording_events[part]=*e;tx->recording_seen|=1u<<part;continue;}
     if(e->kind==CE_QUEUE){if(tx->phase||!e->token||e->counter!=e->program+0x40||!e->capture){in->error=C_TRACE_AMBIGUOUS;return;}tx->phase=1;tx->lane=lane;tx->token=e->token;tx->capture=e->capture;tx->counter=e->counter;tx->global_events[0]=*e;continue;}
     if(lane!=tx->lane||e->token!=tx->token||e->capture!=tx->capture||e->counter!=tx->counter||e->program!=tx->global_events[0].program||e->bits!=tx->global_events[0].bits){in->error=C_TRACE_AMBIGUOUS;return;}
     if(e->kind==CE_BODY&&tx->phase==1)tx->phase=2;else if(e->kind==CE_DONE&&tx->phase==2){tx->phase=4;tx->done_tick=e->tick;}else{in->error=C_TRACE_AMBIGUOUS;return;}continue;
    }
    unsigned queued=e->kind==CE_IO_QUEUE||e->kind==CE_IO_RESULT||e->kind==CE_IO_ENTER||e->kind==CE_IO_AUDIO_DONE;
    if(e->reserved!=IO_PARAMETER||(!queued&&(e->capture!=r->io_connector||e->counter!=r->track_owner))||e->arg_kind!=r->io_field||e->property!=r->global_owner||e->incarnation!=r->field_incarnation||(!queued&&e->token!=in->commands->owner_token)||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    unsigned part=e->kind==CE_DISPATCH?0:e->kind==CE_IO_SET?1:e->kind==CE_IO_COMMIT?2:e->kind==CE_IO_DONE?3:e->kind==CE_RETURN?4:e->kind==CE_IO_VALUE?5:e->kind==CE_IO_QUEUE?6:e->kind==CE_IO_RESULT?7:e->kind==CE_IO_ENTER?8:e->kind==CE_IO_AUDIO_DONE?9:10;
    if(part>=10||(tx->io_seen&(1u<<part))){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->io_events[part]=*e;tx->io_seen|=1u<<part;continue;
   }
   if(command_qlink_mode(tx->field)){
    const CommandRequest *r=&tx->qlink_request;
    if(e->reserved!=QLINK_MODE||e->program!=r->global_owner||e->counter!=r->global_owner+8||e->property!=r->global_owner||e->incarnation!=r->field_incarnation||e->controller>=15||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e->kind==CE_DISPATCH||e->kind==CE_RETURN){unsigned step=e->kind==CE_RETURN;if((tx->global_seen&(1u<<step))||e->token!=in->commands->owner_token||(e->arg_kind!=0&&e->arg_kind!=2&&e->arg_kind!=3)||e->bits!=e->controller||e->revision!=r->epoch){in->error=C_TRACE_AMBIGUOUS;return;}tx->global_events[step]=*e;tx->global_seen|=1u<<step;continue;}
    if(e->kind==CE_QM_ACCEPT){if(tx->mode_accept_seen||e->token!=in->commands->owner_token||e->revision>C_CLOSED){in->error=C_TRACE_AMBIGUOUS;return;}tx->mode_accept=*e;tx->mode_accept_seen=1;continue;}
    if(e->kind==CE_QM_VALUE){if(tx->qlink_value_seen||e->token!=in->commands->owner_token||(e->revision!=QL_SAMPLED&&e->revision!=QL_INVALID)||(e->revision==QL_SAMPLED&&(e->capture!=r->global_owner||e->bits>=15))){in->error=C_TRACE_AMBIGUOUS;return;}tx->qlink_value=*e;tx->qlink_value_seen=1;continue;}
    unsigned part=e->arg_kind,step=e->kind-CE_QM_QUEUE;if(part>=2||step>=4||(tx->qlink_seen[part]&(1u<<step))||e->revision!=r->epoch||e->bits!=(step==1?1:e->controller)){in->error=C_TRACE_AMBIGUOUS;return;}tx->qlink_events[part][step]=*e;tx->qlink_seen[part]|=1u<<step;continue;
   }
   if(command_qlink(tx->field)){
    const CommandRequest *r=&tx->qlink_request;
    if(e->reserved!=tx->field||e->program!=r->qlink_wrapper||e->incarnation!=r->qlink_generation||e->controller!=r->qlink_index||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e->kind==CE_DISPATCH||e->kind==CE_RETURN)continue;
    if(e->kind==CE_QL_BARRIER){if(e->arg_kind>=COMMAND_LANES||(tx->qlink_barriers_seen&(1u<<e->arg_kind))||e->capture!=r->qlink_wrapper||e->counter!=r->qlink_provider||e->token!=in->commands->owner_token||e->revision!=r->epoch||e->property){in->error=C_TRACE_AMBIGUOUS;return;}tx->qlink_barriers[e->arg_kind]=e->bits;tx->qlink_barriers_seen|=1u<<e->arg_kind;continue;}
    if(e->kind==CE_QL_VALUE){if(tx->qlink_value_seen||e->arg_kind>1||e->property||e->capture!=r->qlink_wrapper||e->counter!=r->qlink_provider||e->token!=in->commands->owner_token||(e->revision!=QL_SAMPLED&&e->revision!=QL_INVALID)){in->error=C_TRACE_AMBIGUOUS;return;}tx->qlink_value=*e;tx->qlink_value_seen=1;continue;}
    unsigned part=e->arg_kind,step=e->kind-CE_QL_QUEUE;
    if(part>=3||step>=4||(tx->qlink_seen[part]&(1u<<step))||e->counter!=r->qlink_wrapper||e->property!=r->qlink_index||e->revision!=r->epoch||e->bits!=(step==1?1:part==0?1:part==1?r->bits:0)){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->qlink_events[part][step]=*e;tx->qlink_seen[part]|=1u<<step;continue;
   }
   if(command_chooser(tx->field)){
    const CommandRequest *r=&tx->effect_request;unsigned part=e->kind==CE_DISPATCH?0:e->kind==CE_CHOOSER_ACCEPT?1:e->kind==CE_RETURN?2:3;
    if(part>=3||tx->global_seen!=((1u<<part)-1)||e->reserved!=EFFECT_CHOOSER||e->token!=in->commands->owner_token||!e->capture||!e->counter||!e->property||e->program!=tx->target.program||e->arg_kind!=0x101||e->controller!=r->effect_slot||e->bits!=r->bits||e->incarnation!=r->effect_generation||e->revision!=r->effect_generation||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(part&&(e->capture!=tx->global_events[0].capture||e->counter!=tx->global_events[0].counter||e->property!=tx->global_events[0].property||e->tick<tx->global_events[part-1].tick)){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->global_events[part]=*e;tx->global_seen|=1u<<part;continue;
   }
   if(command_effect(tx->field)){
    const CommandRequest *r=&tx->effect_request;unsigned controller=command_request_controller(r);
    if(e->reserved!=tx->field||e->program!=tx->target.program||e->incarnation!=r->effect_generation||e->arg_kind!=0x101||e->controller!=controller||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e->kind==CE_EFFECT_EXECUTE||e->kind==CE_EFFECT_VALUE){
     unsigned mask=e->kind==CE_EFFECT_EXECUTE?1:2;
     if((tx->field==EFFECT_ENABLE&&mask==1)||(tx->effect_seen&mask)||!e->capture||!e->counter||e->property!=r->effect_index||(mask==1&&(e->bits!=r->bits||e->revision!=r->effect_generation))||(mask==2&&(e->token!=in->commands->owner_token||(e->revision!=EF_READY&&e->revision!=EF_INVALID)))){in->error=C_TRACE_AMBIGUOUS;return;}
     if(mask==1)tx->effect_execution=*e;else tx->effect_value=*e;tx->effect_seen|=mask;continue;
    }
    if(e->bits!=tx->bits||(e->property&&e->kind!=CE_RECORD_TIME)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e->kind==CE_DISPATCH||e->kind==CE_RETURN)continue;
    if(e->kind>=CE_RECORD_QUEUE&&e->kind<=CE_RECORD_TIME){unsigned part=e->kind-CE_RECORD_QUEUE;if(part==8||(tx->recording_seen&(1u<<part))||e->revision!=r->epoch){in->error=C_TRACE_AMBIGUOUS;return;}tx->recording_events[part]=*e;tx->recording_seen|=1u<<part;continue;}
    if(e->kind==CE_QUEUE){if(tx->phase||!e->token||e->counter!=e->program+0x40||!e->capture){in->error=C_TRACE_AMBIGUOUS;return;}tx->phase=1;tx->lane=lane;tx->token=e->token;tx->capture=e->capture;tx->counter=e->counter;continue;}
    if(lane!=tx->lane||e->token!=tx->token||e->capture!=tx->capture||e->counter!=tx->counter){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e->kind==CE_BODY&&tx->phase==1)tx->phase=2;
    else if(e->kind==CE_DONE&&tx->phase==2){tx->phase=4;tx->done_tick=e->tick;}
    else{in->error=C_TRACE_AMBIGUOUS;return;}continue;
   }
   if(tx->field==CF_MIDI_VOLUME){
    if(e->reserved!=tx->field||e->incarnation!=tx->field_incarnation||e->controller!=7||e->arg_kind!=0x100||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e->kind==CE_DISPATCH||e->kind==CE_RETURN){if(e->program!=tx->target.program||e->bits!=tx->bits){in->error=C_TRACE_AMBIGUOUS;return;}continue;}
    unsigned part=e->kind-CE_MIDI_QUEUE;
    if(part>=5||(tx->midi_seen&(1u<<part))||e->program!=tx->target.track||e->property!=tx->property||(part!=2&&e->bits!=tx->bits)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(part==1){if(tx->phase||!e->token){in->error=C_TRACE_AMBIGUOUS;return;}tx->phase=1;tx->lane=lane;tx->token=e->token;}
    if(part>=2){if(!tx->phase||lane!=tx->lane||e->token!=tx->token||(part==2&&tx->phase!=1)||(part==3&&tx->phase!=2)||(part==4&&(tx->phase<1||tx->phase>3))){in->error=C_TRACE_AMBIGUOUS;return;}tx->phase=part;}
    tx->midi_events[part]=*e;tx->midi_seen|=1u<<part;continue;
   }
   if(command_global(tx->field)){
    unsigned part=e->kind==CE_GLOBAL_BEGIN?0:e->kind==CE_GLOBAL_STATE?1:e->kind==CE_GLOBAL_END?2:3;
    if(part==3||(tx->global_seen&(1u<<part))||e->reserved!=tx->field||e->controller!=tx->field||e->arg_kind||e->incarnation!=tx->field_incarnation||e->property!=tx->property||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->global_events[part]=*e;tx->global_seen|=1u<<part;continue;
   }
   if(tx->field==CF_MASTER){
    if(e->request!=tx->flight||e->reserved!=CF_MASTER||e->controller!=CF_MASTER||e->arg_kind||e->incarnation!=tx->field_incarnation||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e->kind==CE_DISPATCH||e->kind==CE_RETURN)continue;
    if(e->kind==CE_MASTER_COMMIT){if((tx->master_seen&64)||e->property!=tx->property||!e->revision||(e->revision&1)){in->error=C_TRACE_AMBIGUOUS;return;}tx->master_events[6]=*e;tx->master_seen|=64;continue;}
    unsigned part=e->kind==CE_MASTER_CREATE?0:e->kind==CE_MASTER_QUEUE?1:e->kind==CE_MASTER_ENTER?2:e->kind==CE_MASTER_SET?3:e->kind==CE_MASTER_VALUE?4:e->kind==CE_MASTER_DONE?5:6;
    if(part==6||(tx->master_seen&(1u<<part))||((part==3||part==4)?e->property!=tx->property:e->property!=0)||((part<3)&&e->bits!=tx->bits)){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->master_events[part]=*e;tx->master_seen|=1u<<part;continue;
   }
   if(command_jog(tx->field)){
    if(e->kind==CE_JOG_OUTCOME){
     if(tx->jog_outcome_seen||e->request!=tx->flight||e->reserved!=tx->field||e->controller!=tx->field||e->bits!=tx->bits||e->incarnation||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
     tx->jog_outcome=*e;tx->jog_outcome_seen=1;continue;
    }
    if(e->request!=tx->flight||e->reserved!=tx->field||e->bits!=tx->bits||e->controller!=tx->field||e->arg_kind||e->incarnation||e->property||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e->kind==CE_DISPATCH||e->kind==CE_RETURN)continue;
    unsigned part=e->kind==CE_JOG_ENQUEUE?0:e->kind==CE_JOG_ENTER?1:e->kind==CE_JOG_DONE?2:3;
    if(part==3||(tx->jog_seen&(1u<<part))){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->jog_events[part]=*e;tx->jog_seen|=1u<<part;continue;
   }
   if(e->kind==CE_PAD_EXECUTE){
    if(!tx->target.pad_owner||tx->pad_seen||e->program!=tx->target.program||e->reserved!=tx->field||e->arg_kind!=tx->target.pad_index||e->controller!=pad_controller(tx->field)||e->incarnation!=tx->field_incarnation||!e->bits||e->bits>PAD_OWNERS||!e->capture||!e->counter||!e->revision||(e->revision&1)||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
    /* Ordinary queue events and the Instrument setter share one lane.
     * Recording order is checked after all lane frontiers are consumed. */
    if(tx->phase&&(tx->phase!=2||lane!=tx->lane||e->token!=tx->token)){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->pad_execution=*e;tx->pad_seen=1;continue;
   }
   if(e->kind>=CE_RECORD_QUEUE&&e->kind<=CE_RECORD_TIME){
    unsigned part=e->kind-CE_RECORD_QUEUE;
    if((!command_float(tx->field)&&!tx->target.pad_owner)||tx->field==CF_MASTER||(tx->recording_seen&(1u<<part))||e->program!=tx->target.program||e->reserved!=tx->field||e->incarnation!=((tx->field||tx->target.pad_owner)?tx->field_incarnation:tx->target.incarnation)||e->bits!=command_native_bits(tx->field,tx->bits)||e->arg_kind!=(tx->target.pad_owner?tx->target.pad_index:0x101)||e->controller!=(tx->target.pad_owner?pad_controller(tx->field):command_controller(tx->field))||e->tick>command_tick_limit(in->commands->seconds)||(part!=8&&((part!=9&&e->property)||e->revision!=tx->target.epoch))){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->recording_events[part]=*e;tx->recording_seen|=1u<<part;continue;
   }
   if(e->program!=tx->target.program||e->incarnation!=((tx->field||tx->target.pad_owner)?tx->field_incarnation:tx->target.incarnation)||e->reserved!=tx->field||e->bits!=((e->kind==CE_COMMIT||e->kind==CE_OTHER_COMMIT)?tx->bits:command_native_bits(tx->field,tx->bits))||e->arg_kind!=((tx->field==CF_SELECTION||tx->field==CF_ARM)?0:tx->target.pad_owner?tx->target.pad_index:0x101)||e->controller!=(tx->target.pad_owner?pad_controller(tx->field):command_controller(tx->field))||e->tick>command_tick_limit(in->commands->seconds)){in->error=C_TRACE_AMBIGUOUS;return;}
   if(e->kind==CE_DISPATCH||e->kind==CE_RETURN)continue;
   if((tx->field==CF_SELECTION||tx->field==CF_ARM)&&e->kind==CE_OWNER_BEGIN){
    if(tx->phase||!e->token||e->token!=in->commands->owner_token||e->capture||e->counter){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->phase=2;tx->lane=lane;tx->token=e->token;tx->capture=tx->counter=0;continue;
   }
   if(e->kind==CE_QUEUE){
    if(tx->phase||!e->token||e->token!=atomic_load_explicit(&l->token,memory_order_acquire)||!e->capture||e->counter!=e->program+0x40){in->error=C_TRACE_AMBIGUOUS;return;}
    tx->phase=1;tx->lane=lane;tx->token=e->token;tx->capture=e->capture;tx->counter=e->counter;continue;
   }
   if(!tx->phase||lane!=tx->lane||e->token!=tx->token||e->capture!=tx->capture||e->counter!=tx->counter){in->error=C_TRACE_AMBIGUOUS;return;}
   if(e->kind==CE_BODY&&tx->phase==1)tx->phase=2;
   else if(e->kind==CE_COMMIT&&(tx->phase==2||tx->phase==3)&&(!tx->target.pad_owner||tx->pad_seen)&&e->property==(tx->target.pad_owner?tx->pad_execution.capture+pad_offset(pad_field(command_source_field(tx->field))):tx->property)&&!(e->revision&1)&&e->revision>(tx->target.pad_owner?tx->pad_execution.revision:tx->before_revision)&&e->revision>tx->commit_revision){tx->phase=3;tx->commit_revision=e->revision;tx->commit_tick=e->tick;}
   else if(e->kind==((tx->field==CF_SELECTION||tx->field==CF_ARM)?CE_OWNER_END:CE_DONE)&&(tx->phase==3||(tx->target.pad_owner&&tx->phase==2&&(!tx->pad_seen||tx->pad_execution.property==tx->bits)))){tx->phase=4;tx->done_tick=e->tick;}
   else{in->error=C_TRACE_AMBIGUOUS;return;}
  }
 }
}
/* Called only after current process, both file identities and MMV7 freshness
 * have been checked by the bridge. No desired value is written into MMV7. */
static inline void input_pump_mode(MirrorInput *in,MirrorBank *bank,const CopiedMirror *s,uint32_t now,int submit_new){
 if(!input_health(in))return;
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  InputFlight *tx=in->flights+at;if(!tx->flight)continue;
  if(tx->acknowledged){
   if(atomic_load_explicit(&in->commands->slots[at].reclaimed,memory_order_acquire)==tx->flight)memset(tx,0,sizeof(*tx));
   continue;
  }
  unsigned done=atomic_load_explicit(&in->commands->slots[at].done,memory_order_acquire)==tx->flight;
  unsigned sealed=atomic_load_explicit(&in->commands->slots[at].sealed,memory_order_acquire)==tx->flight;
  input_events(in,tx,at);if(in->error)return;
  const CommandSlot *slot=in->commands->slots+at;
  unsigned rejected=atomic_load_explicit(&slot->processed,memory_order_acquire)==tx->flight?atomic_load_explicit(&slot->rejected,memory_order_relaxed):0;
  if(rejected){
   /* Only the producer can prove that no native call began and that every
    * admitted hook drained. Its sequence-qualified reclamation is that proof;
    * a rejected flag, absent dispatch flag or elapsed time alone is not. */
   if(atomic_load_explicit(&slot->reclaimed,memory_order_acquire)==tx->flight){
    if(atomic_load(&slot->dispatched)==tx->flight||atomic_load(&slot->returned)==tx->flight||done||sealed||atomic_load(&slot->settled)==tx->flight){in->error=C_TRACE_AMBIGUOUS;return;}
    INPUT_LOG("REFUSED request=%u field=%u reason=%s; producer reclaimed without native dispatch; not settled or retried\n",tx->flight,tx->field,command_error_name(rejected));
    in->refused++;memset(tx,0,sizeof(*tx));continue;
   }
   if(now>tx->expires){in->error=rejected;return;}
   continue;
  }
  if(now>tx->expires){in->error=C_EXPIRED;return;}
  if(command_io(tx->field)){
   if(!done||!sealed)continue;
   const CommandRequest *r=&tx->io_request;const CommandEvent *e=tx->io_events;
   unsigned asynchronous=(tx->io_seen&64)!=0,unavailable=!e[5].revision;
   if(unavailable&&e[5].bits){in->error=C_TRACE_AMBIGUOUS;return;}
   unsigned program_field=r->io_field==IO_AUDIO_IN||r->io_field==IO_AUDIO_OUT;
   if(program_field){
    if(tx->io_seen!=(1u|16u|32u)||e[0].revision!=r->io_generation||e[4].revision!=r->io_generation||(e[5].revision&1)||e[0].controller!=e[4].controller||e[0].controller!=e[5].controller||e[0].bits!=e[0].controller||e[4].bits!=e[0].controller||(!unavailable&&e[5].bits!=e[0].controller)||e[0].program!=e[4].program||e[0].program!=e[5].program){in->error=C_TRACE_AMBIGUOUS;return;}
    float wanted=(float)e[0].controller;uint32_t bits;memcpy(&bits,&wanted,4);
    if(tx->recording_seen){if(tx->phase||!(tx->recording_seen&4)){in->error=C_TRACE_AMBIGUOUS;return;}if(!input_recording_proof(in,tx))return;for(unsigned i=0;i<10;i++)if(tx->recording_seen&(1u<<i))if(tx->recording_events[i].program!=e[0].program||tx->recording_events[i].bits!=bits){in->error=C_TRACE_AMBIGUOUS;return;}}
    else if(tx->phase!=4||tx->global_events[0].program!=e[0].program||tx->global_events[0].bits!=bits){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e[5].tick<tx->done_tick||e[5].tick<e[4].tick||e[4].tick<e[0].tick){in->error=C_TRACE_AMBIGUOUS;return;}
   }else if(asynchronous){
    if(tx->io_seen!=(3u|16u|32u|64u|128u|256u|512u)||e[0].revision!=r->io_generation||e[4].revision!=r->io_generation||!e[1].revision||(e[1].revision&1)||(!unavailable&&e[5].revision<=e[1].revision)||(e[5].revision&1)||(!unavailable&&e[5].bits!=e[0].controller)||e[1].bits==e[0].controller||e[7].bits!=1){in->error=C_TRACE_AMBIGUOUS;return;}
    for(unsigned i=0;i<10;i++)if(tx->io_seen&(1u<<i))if(e[i].controller!=e[0].controller||e[i].program!=e[0].program||((i==0||i==4||(i==5&&!unavailable)||i==6||i==8||i==9)&&e[i].bits!=e[0].controller)){in->error=C_TRACE_AMBIGUOUS;return;}
    for(unsigned i=6;i<10;i++)if(!e[i].capture||!e[i].counter||e[i].capture!=e[6].capture||e[i].counter!=e[6].counter||e[i].revision!=r->io_generation||(i<8&&e[i].token!=in->commands->owner_token)){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e[8].token!=e[9].token||e[8].sp!=e[9].sp||e[8].lr!=e[9].lr||e[6].tick<e[0].tick||e[7].tick<e[6].tick||e[8].tick<e[6].tick||e[9].tick<e[8].tick||e[4].tick<e[7].tick||e[5].tick<e[9].tick||e[5].tick<e[4].tick){in->error=C_TRACE_AMBIGUOUS;return;}
   }else{
   if((tx->io_seen&59)!=59||e[0].revision!=r->io_generation||e[3].revision!=r->io_generation||e[4].revision!=r->io_generation||e[1].sp!=e[3].sp||!e[1].revision||(e[1].revision&1)||(!unavailable&&!e[5].revision)||(e[5].revision&1)||(!unavailable&&e[5].bits!=e[0].controller)){in->error=C_TRACE_AMBIGUOUS;return;}
   unsigned last=0;for(unsigned i=0;i<6;i++)if(tx->io_seen&(1u<<i)){if(e[i].controller!=e[0].controller||e[i].program!=e[0].program||e[i].tick<last||(i!=1&&!(i==5&&unavailable)&&e[i].bits!=e[0].controller)){in->error=C_TRACE_AMBIGUOUS;return;}last=e[i].tick;}
   if(tx->io_seen&4){if((e[2].revision&1)||e[2].revision<=e[1].revision||(!unavailable&&e[5].revision<e[2].revision)){in->error=C_TRACE_AMBIGUOUS;return;}}
   else if(e[1].bits!=e[0].controller||(!unavailable&&e[5].revision!=e[1].revision)){in->error=C_TRACE_AMBIGUOUS;return;}
   }
   if(s->heartbeat<=e[5].tick)continue;
   if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
   tx->acknowledged=1;in->settled++;INPUT_LOG("IO_DONE request=%u field=%u actual=%u revision=%u outcome=%s; native bodies closed, not equipment routing proof\n",tx->flight,r->io_field,e[5].bits,e[5].revision,unavailable?"completed_original_source_unavailable":"native_configuration_accepted");continue;
  }
  if(command_qlink_mode(tx->field)){
   if(!done||!sealed)continue;
   unsigned mask=tx->global_events[0].arg_kind,target=tx->global_events[0].controller;
   if(tx->global_seen!=3||tx->global_events[1].arg_kind!=mask||tx->global_events[1].controller!=target||!tx->qlink_value_seen||tx->qlink_value.arg_kind!=mask||tx->qlink_value.controller!=target){in->error=C_TRACE_AMBIGUOUS;return;}
   const CommandEvent *a=&tx->mode_accept;
   if(!tx->mode_accept_seen||a->revision!=C_OK||a->capture!=tx->qlink_request.global_owner||a->bits!=target||a->controller!=target||a->arg_kind!=mask||a->tick<tx->global_events[0].tick||a->tick>tx->global_events[1].tick){in->error=C_TRACE_AMBIGUOUS;return;}
   for(unsigned p=0;p<2;p++){
    if(!(mask&(1u<<p))){if(tx->qlink_seen[p]){in->error=C_TRACE_AMBIGUOUS;return;}continue;}
    const CommandEvent *e=tx->qlink_events[p];if(tx->qlink_seen[p]!=15||!e[0].capture||e[0].token!=in->commands->owner_token||e[1].token!=e[0].token||!e[2].token||e[2].token!=e[3].token||e[2].sp!=e[3].sp+(p?16:32)||e[0].capture!=e[1].capture||e[0].capture!=e[2].capture||e[0].capture!=e[3].capture){in->error=C_TRACE_AMBIGUOUS;return;}
    for(unsigned j=0;j<4;j++)if(e[j].controller!=target){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e[3].tick>tx->done_tick)tx->done_tick=e[3].tick;
   }
   if(tx->qlink_value.tick<tx->done_tick){in->error=C_TRACE_AMBIGUOUS;return;}if(s->heartbeat<=tx->qlink_value.tick)continue;
   if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
   tx->acknowledged=1;in->settled++;INPUT_LOG("QLINK_MODE_DONE request=%u accepted=%u latest_selected=%u outcome=%s; exact controller callbacks drained, not provider/DSP completion\n",tx->flight,target,tx->qlink_value.bits,tx->qlink_value.revision==QL_SAMPLED?"sampled":"completed_owner_unavailable");continue;
  }
  if(command_qlink(tx->field)){
   if(!done||!sealed)continue;
   for(unsigned part=0;part<3;part++){
    const CommandEvent *e=tx->qlink_events[part];
    if(tx->qlink_seen[part]!=15||!e[0].capture||e[0].token!=in->commands->owner_token||e[1].token!=e[0].token||!e[2].token||e[2].token!=e[3].token||e[2].sp!=e[3].sp+(part==1?0xf0:8)||e[0].capture!=e[1].capture||e[0].capture!=e[2].capture||e[0].capture!=e[3].capture){in->error=C_TRACE_AMBIGUOUS;return;}
    if(e[3].tick>tx->done_tick)tx->done_tick=e[3].tick;
   }
   if(!tx->qlink_value_seen||tx->qlink_barriers_seen!=((1u<<COMMAND_LANES)-1)||tx->qlink_value.tick<tx->done_tick){in->error=C_TRACE_AMBIGUOUS;return;}
   if(s->heartbeat<=tx->qlink_value.tick)continue;
   if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
   InputQLink *g=in->qlinks+tx->qlink_request.qlink_index;
   if(input_qlink_same(&tx->qlink_request,s,tx->qlink_request.qlink_index)){if(!input_qlink_same(&g->request,s,tx->qlink_request.qlink_index)){memset(g,0,sizeof(*g));g->request=tx->qlink_request;}if(tx->qlink_value.arg_kind){g->wait_tick=tx->done_tick;memcpy(g->barriers,tx->qlink_barriers,sizeof(g->barriers));} /* A later no-op cannot erase an earlier changed-value refresh obligation. */}
   tx->acknowledged=1;in->settled++;
   INPUT_LOG("QLINK_DONE request=%u index=%u actual_bits=%u outcome=%s; begin/value/end callbacks completed; UI cache sample, not target-refresh or DSP settlement\n",tx->flight,tx->qlink_request.qlink_index,tx->qlink_value.bits,tx->qlink_value.revision==QL_SAMPLED?"sampled":"completed_binding_unavailable");continue;
  }
  if(command_chooser(tx->field)){
   if(!done||!sealed)continue;
   if(tx->global_seen!=7||atomic_load(&slot->returned)!=tx->flight||atomic_load(&slot->dispatched)!=tx->flight){in->error=C_TRACE_AMBIGUOUS;return;}
   if(s->heartbeat<=tx->global_events[2].tick)continue;
   if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
   tx->acknowledged=1;in->settled++;INPUT_LOG("CHOOSER_OPEN request=%u slot=%u; native popup/context accepted, loading remains native UI-owned\n",tx->flight,tx->effect_request.effect_slot);continue;
  }
  if(command_effect(tx->field)){
   if(!done||!sealed)continue;
   if(tx->recording_seen){if(tx->phase||!input_recording_proof(in,tx))return;}else if(tx->phase!=4){in->error=C_TRACE_AMBIGUOUS;return;}
   const CommandEvent *x=&tx->effect_execution,*v=&tx->effect_value;
   if(tx->field==EFFECT_PARAMETER){
    if(tx->effect_seen!=3||x->token!=(tx->recording_seen?tx->recording_events[1].token:tx->token)||x->tick>tx->done_tick||v->tick<tx->done_tick||x->capture!=v->capture||x->counter!=v->counter||(v->revision==EF_READY&&(v->capture!=tx->effect_request.effect_key||v->counter!=tx->effect_request.effect_ap))){in->error=C_TRACE_AMBIGUOUS;return;}
   }else if((v->revision==EF_READY&&tx->recording_seen&&!(tx->recording_seen&4))||tx->effect_seen!=2||v->tick<tx->done_tick||v->capture!=tx->effect_request.effect_key||v->counter!=tx->effect_request.effect_ap||(v->revision==EF_READY&&v->bits!=tx->bits)){in->error=C_TRACE_AMBIGUOUS;return;}

   if(s->heartbeat<=v->tick)continue;
   if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
   tx->acknowledged=1;in->settled++;INPUT_LOG("EFFECT_DONE operation=%u request=%u slot=%u parameter=%u actual_bits=%u outcome=%s; native completion and separate host UI readback\n",tx->field,tx->flight,tx->effect_request.effect_slot,tx->effect_request.effect_index,v->bits,v->revision==EF_READY?"sampled":"completed_binding_unavailable");continue;
  }
  if(tx->field==CF_MIDI_VOLUME){
   if(!done||!sealed)continue;
   if(!input_midi_proof(in,tx))return;
   const CopiedTrack *t=input_track(s,&tx->target);const CopiedField *v=t?t->fields+CF_MIDI_VOLUME:NULL;
   if(!t||!midi_volume(t->vptr)||!v->available||v->incarnation!=tx->field_incarnation||v->owner_incarnation!=tx->target.track_owner||v->property!=tx->property){in->error=C_IDENTITY;return;}
   if(s->heartbeat<=tx->done_tick||v->revision<tx->commit_revision)continue;
   if(tx->commit_revision&&v->revision==tx->commit_revision&&v->bits!=tx->bits){in->error=C_TRACE_AMBIGUOUS;return;}
   if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
   tx->acknowledged=1;in->settled++;INPUT_LOG("MIDI_DONE request=%u track=%u branch=%u source_revision=%u native_no_set=%u\n",tx->flight,tx->target.serial,tx->midi_events[0].revision,tx->commit_revision,!(tx->midi_seen&4));continue;
  }
  if(command_global(tx->field)){
   if(done&&sealed){
    if(!input_global_proof(in,tx))return;
    if(s->epoch!=tx->global_epoch){in->error=C_IDENTITY;return;}
    if(s->heartbeat<=tx->done_tick)continue;
    if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
    tx->acknowledged=1;in->settled++;INPUT_LOG("GLOBAL_DONE request=%u operation=%u result=%u epoch=%u tick=%u; native intent/facade receipt; downstream jobs, effective feedback and file persistence are separate\n",tx->flight,tx->field,tx->global_events[2].bits,s->epoch,now);
   }continue;
  }
  if(tx->field==CF_MASTER){
   if(done&&sealed){
    if(!input_master_proof(in,tx))return;
    if(!s->master.available||s->master.owner_incarnation!=tx->global_owner||s->master.incarnation!=tx->field_incarnation){in->error=C_IDENTITY;return;}
    if(s->heartbeat<=tx->done_tick||s->master.revision<tx->commit_revision)continue;
    if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}tx->acknowledged=1;in->settled++;
    INPUT_LOG("MASTER_DONE request=%u requested_bits=%u actual_bits=%u revision=%u submission_epoch=%u execution_epoch=%u completion_epoch=%u tick=%u\n",tx->flight,tx->bits,s->master.bits,s->master.revision,tx->global_epoch,tx->master_events[2].revision,tx->master_events[5].revision,now);
   }continue;
  }
  if(command_jog(tx->field)){
   if(done&&sealed){
    if(!input_jog_proof(in,tx))return;
    if(s->heartbeat<=tx->done_tick)continue;
    if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
    tx->acknowledged=1;in->settled++;
    if(tx->jog_playing||tx->jog_outcome.arg_kind){in->jog_wait=1;in->jog_wait_owner=tx->jog_outcome.property;in->jog_wait_epoch=tx->jog_outcome.revision;in->jog_wait_count=tx->jog_outcome.counter;}
    INPUT_LOG("JOG_OUTCOME request=%u flags=%u timeline_owner=%u progress_at_done=%u; restart milestone is not engine readiness\n",tx->flight,tx->jog_outcome.arg_kind,tx->jog_outcome.property,tx->jog_outcome.counter);
    INPUT_LOG("JOG_DONE request=%u operation=%u delta=%d submission_epoch=%u execution_epoch=%u completion_epoch=%u tick=%u; native completion, not desired position\n",tx->flight,tx->field,(int32_t)tx->bits,tx->target.epoch,tx->jog_events[1].revision,tx->jog_events[2].revision,now);
   }
   continue;
  }
  if(tx->target.pad_owner){
   if(!done||!sealed)continue;
   if(tx->recording_seen){
    if(tx->phase){in->error=C_TRACE_AMBIGUOUS;return;}
    if(!input_recording_proof(in,tx))return;
   }else if(tx->phase!=4){in->error=C_TRACE_AMBIGUOUS;return;}
   if(s->heartbeat<=tx->done_tick)continue;
   unsigned retired=0;
   if(tx->pad_seen){
    CopiedField actual;unsigned field_id=command_source_field(tx->field);
    if(!copy_pad_receipt(s,tx->pad_execution.bits,tx->target.program,tx->target.program_owner,field_id,&actual)||actual.property!=tx->pad_execution.capture+pad_offset(pad_field(field_id))){in->error=C_IDENTITY;return;}
    if(actual.revision<tx->commit_revision)continue;
    if((tx->commit_revision&&actual.revision==tx->commit_revision&&actual.bits!=tx->bits)||(!tx->commit_revision&&tx->pad_execution.property!=tx->bits)){in->error=C_TRACE_AMBIGUOUS;return;}
    MotorIdentity executed=tx->target;executed.pad_owner=tx->pad_execution.bits;executed.pad_generation=tx->pad_execution.counter;CopiedTrack current;
    retired=input_target(s,&executed,&current)==NULL;
   }
   if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
   tx->acknowledged=1;in->settled++;
   INPUT_LOG("PAD_DONE request=%u slot=%u submitted_instrument=%u executed_instrument=%u source_revision=%u completed_old_binding=%u native_no_set=%u; current motors use current membership only\n",tx->flight,tx->target.pad_index,tx->target.pad_owner,tx->pad_seen?tx->pad_execution.bits:0,tx->commit_revision,retired,!tx->pad_seen);continue;
  }
  const CopiedTrack *t=input_track(s,&tx->target);
  CopiedField volume;const CopiedField *field=t?copied_field(s,t,command_source_field(tx->field),&volume):NULL;
  if(!field||!field->available||(tx->field&&field->incarnation!=tx->field_incarnation)){in->error=C_IDENTITY;return;}
  if(done&&sealed&&tx->recording_seen){
   if(tx->phase||!input_recording_proof(in,tx))return;
   if(s->heartbeat<=tx->done_tick||field->revision<tx->commit_revision)continue;
   if(tx->commit_revision&&field->revision==tx->commit_revision&&field->bits!=tx->bits){in->error=C_TRACE_AMBIGUOUS;return;}
   if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
   tx->acknowledged=1;in->settled++;
   INPUT_LOG("RECORDING_DONE request=%u field=%u source_revision=%u recorder_handled=%u tick=%u; native buffered handling, not per-point persistence\n",tx->flight,tx->field,field->revision,(tx->recording_seen&128)!=0,now);continue;
  }
  if(done&&sealed){
   if(tx->phase!=4){in->error=C_TRACE_AMBIGUOUS;return;}
   if(s->heartbeat<=tx->done_tick||field->revision<tx->commit_revision)continue;
   if(field->revision==tx->commit_revision&&(tx->field==CF_SELECTION?field->bits!=t->track:field->bits!=tx->bits)){in->error=C_TRACE_AMBIGUOUS;return;}
   if(!command_publish_settlement(in->commands,tx->flight)){in->error=C_CLOSED;return;}
   /* Settlement does not change the controller's last received target. Keep
    * servo/host dedup intact; only a different actual MPC value needs output. */
   MirrorFader *f=bank->faders+tx->strip;if(tx->field==bank_field(bank,tx->strip)&&!memcmp(&f->identity,&tx->target,sizeof(tx->target))){f->wait=1;f->barrier=now;}
   INPUT_LOG("SETTLED request=%u fader=%u epoch=%u track=%u binding=%u incarnation=%u commit_revision=%u current_revision=%u current_bits=%u field=%u tick=%u commit_tick=%u done_tick=%u\n",tx->flight,tx->strip+1,tx->target.epoch,tx->target.serial,tx->target.binding,tx->target.incarnation,tx->commit_revision,field->revision,field->bits,tx->field,now,tx->commit_tick,tx->done_tick);
   tx->acknowledged=1;in->settled++;
  }
 }
 while(in->edge_count&&!in->edges[in->edge_head].desire.pending){in->edge_head=(in->edge_head+1)%INPUT_EDGES;in->edge_count--;}
 if(!submit_new)return;
 if(!s->ready||atomic_load_explicit(&in->commands->new_project_intent,memory_order_acquire)){input_discard(in);input_jog_discard(in);input_master_invalidate(in);return;}
 input_sync(in,bank);
 input_effect_submit(in,bank,s,now);if(in->error)return;
 input_mode_submit(in,bank,s,now);
 input_io_submit(in,bank,s,now);
 input_qlink_submit(in,bank,s,now);if(in->error)return;
 if(in->jog_epoch!=s->epoch){input_jog_discard(in);in->jog_epoch=s->epoch;}
 for(unsigned batch=0;batch<8&&in->global_count&&s->ready;batch++){
  InputGlobalEvent *g=in->global_events+in->global_head;
  unsigned owner=input_global_owner(s,g->operation);
  if(g->epoch!=s->epoch||owner!=g->owner||(g->operation==GLOBAL_TRACK_TYPE&&g->field!=s->selected_serial)){in->global_head=(in->global_head+1)%INPUT_JOG_EVENTS;in->global_count--;continue;}
  if(now>g->expires){in->error=C_EXPIRED;return;}
  unsigned at=command_free(in->commands);if(at==COMMAND_SLOTS)break;
  CommandState *c=in->commands;unsigned seq=atomic_load(&c->published)+1;
  CommandRequest r={.seq=seq,.pid=c->pid,.start_lo=c->start_lo,.start_hi=c->start_hi,.origin_sec=c->origin_sec,.origin_nsec=c->origin_nsec,.epoch=s->epoch,.bits=g->bits,.created=now,.expires=now+120000,.reserved=g->operation,.project_owner=s->project_owner,.global_owner=g->owner,.field_incarnation=g->field};
  unsigned conflict=0;for(unsigned i=0;i<COMMAND_SLOTS;i++){CommandRequest old;CommandSlot *q=c->slots+i;unsigned n=atomic_load(&q->published);if(n!=atomic_load(&q->reclaimed)&&(!command_request_read(q,n,&old)||command_conflict(&r,&old)))conflict=1;}if(conflict)break;
  InputFlight *tx=in->flights+at;memset(tx,0,sizeof(*tx));tx->flight=seq;tx->field=g->operation;tx->bits=g->bits;tx->expires=r.expires;tx->global_owner=g->owner;tx->global_epoch=s->epoch;tx->field_incarnation=g->field;tx->property=g->operation==CF_AUTOMATION?s->automation.property:0;
  if(!command_publish_request_at(c,&r,at)){memset(tx,0,sizeof(*tx));if(!atomic_load(&c->new_project_intent))in->error=C_CLOSED;return;}in->submitted++;in->global_head=(in->global_head+1)%INPUT_JOG_EVENTS;in->global_count--;
 }
 input_stop_home(in,s,now);
 while(in->jog_count&&in->jog_events[in->jog_head].epoch!=s->epoch){in->jog_head=(in->jog_head+1)%INPUT_JOG_EVENTS;in->jog_count--;in->jog_rewind=in->jog_forward=0;}
 if(!in->jog_count&&command_free(in->commands)<COMMAND_SLOTS&&(in->jog_rewind!=in->jog_forward)&&now>=in->jog_repeat_tick){
  input_jog_add_origin(in,s,JOG_BEAT,in->jog_forward?1:-1,now,in->jog_forward?1:2);in->jog_repeat_tick=now>UINT32_MAX-100?UINT32_MAX:now+100;
 }
 for(unsigned batch=0;batch<1&&in->jog_count&&s->ready&&input_jog_admit(in,s);batch++){
  InputJogEvent *j=in->jog_events+in->jog_head;
  if(j->origin==3&&(!s->playing.available||s->playing.bits))break;
  if(now>j->expires){in->jog_head=(in->jog_head+1)%INPUT_JOG_EVENTS;in->jog_count--;break;}
  unsigned at=command_free(in->commands),seq=atomic_load(&in->commands->published)+1;
  if(at<COMMAND_SLOTS){
   CommandState *c=in->commands;CommandRequest r={.seq=seq,.pid=c->pid,.start_lo=c->start_lo,.start_hi=c->start_hi,.origin_sec=c->origin_sec,.origin_nsec=c->origin_nsec,.epoch=s->epoch,.bits=(uint32_t)j->delta,.created=now,.expires=now+120000,.reserved=j->operation,.project_owner=s->project_owner};
   InputFlight *tx=in->flights+at;memset(tx,0,sizeof(*tx));tx->flight=seq;tx->field=j->operation;tx->bits=r.bits;tx->expires=r.expires;tx->target.epoch=s->epoch;tx->jog_playing=s->playing.available&&s->playing.bits;
   if(!command_navigation_watch(c,1)){memset(tx,0,sizeof(*tx));return;}
   if(!command_publish_request_at(c,&r,at)){memset(tx,0,sizeof(*tx));if(!atomic_load(&c->new_project_intent))in->error=C_CLOSED;return;}
   in->submitted++;in->jog_head=(in->jog_head+1)%INPUT_JOG_EVENTS;in->jog_count--;
   INPUT_LOG("JOG_SUBMIT request=%u operation=%u delta=%d submission_epoch=%u tick=%u\n",seq,r.reserved,(int32_t)r.bits,r.epoch,now);
  }else break;
 }
 if(atomic_load(&in->commands->navigation_watch)&&!in->jog_count&&!in->jog_rewind&&!in->jog_forward&&input_jog_admit(in,s))(void)command_navigation_watch(in->commands,0);
 input_master_sync(in,s);
 if(in->master.pending){
  InputMaster *m=&in->master;unsigned busy=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)busy|=in->flights[i].flight&&in->flights[i].field==CF_MASTER;
  if(now>m->expires){in->error=C_EXPIRED;return;}
  float current;memcpy(&current,&s->master.bits,4);if((int)(current*16383.0f+0.5f)==m->physical)m->pending=0;
  if(!busy&&m->pending){unsigned at=command_free(in->commands);if(at<COMMAND_SLOTS&&!INPUT_STOPPED()){
   CommandState *c=in->commands;unsigned seq=atomic_load(&c->published)+1;
   CommandRequest r={.seq=seq,.pid=c->pid,.start_lo=c->start_lo,.start_hi=c->start_hi,.origin_sec=c->origin_sec,.origin_nsec=c->origin_nsec,.epoch=s->epoch,.bits=m->bits,.created=now,.expires=m->expires,.before_bits=s->master.bits,.before_revision=s->master.revision,.reserved=CF_MASTER,.field_incarnation=m->incarnation,.project_owner=s->project_owner,.global_owner=m->owner};
   InputFlight *tx=in->flights+at;memset(tx,0,sizeof(*tx));tx->flight=seq;tx->field=CF_MASTER;tx->bits=r.bits;tx->expires=r.expires;tx->global_owner=r.global_owner;tx->global_epoch=r.epoch;tx->field_incarnation=r.field_incarnation;tx->property=s->master.property;tx->before_revision=r.before_revision;
   if(!command_publish_request_at(c,&r,at)){memset(tx,0,sizeof(*tx));if(!atomic_load(&c->new_project_intent))in->error=C_CLOSED;return;}m->pending=0;in->submitted++;INPUT_LOG("MASTER_SUBMIT request=%u owner=%u field_incarnation=%u bits=%u epoch=%u tick=%u\n",seq,r.global_owner,r.field_incarnation,r.bits,r.epoch,now);
  }}
 }
 unsigned start=in->next_strip,edges=in->edge_count,edges_submitted=0;
 for(unsigned step=0;step<edges+MIRROR_BANK*CF_COUNT;step++){
  unsigned at=(start+(step<edges?0:step-edges))%(MIRROR_BANK*CF_COUNT),strip=at/CF_COUNT,field_id=at%CF_COUNT;
  InputGesture *g;
  if(step<edges){if(edges_submitted==8)continue;InputEdge *e=in->edges+(in->edge_head+step)%INPUT_EDGES;strip=e->strip;field_id=e->field;g=&e->desire;
   if(!bank->ready||memcmp(&g->identity,&bank->strips[strip],sizeof(g->identity))){g->pending=0;continue;}
  }else g=&in->desires[strip][field_id];
  if(!g->pending)continue;
  if(input_existing(in,&g->identity,field_id))continue;
  if(now>g->expires){in->error=C_EXPIRED;return;}
  CopiedTrack pad_target;const CopiedTrack *t=input_target(s,&g->identity,&pad_target);
  CopiedField volume;const CopiedField *field=t?copied_field(s,t,command_source_field(field_id),&volume):NULL;
  if(!field||!field->available||(field_id&&field->incarnation!=g->field_incarnation)){g->pending=0;continue;}
  int equal;
  if(field_id==CF_VOLUME||field_id==CF_MIDI_VOLUME){float current,desired;memcpy(&current,&field->bits,4);memcpy(&desired,&g->bits,4);float resolution=field_id==CF_MIDI_VOLUME?127.0f:16383.0f;equal=(int)(current*resolution+0.5f)==(int)(desired*resolution+0.5f);}
  else equal=field_id==CF_SELECTION?field->bits==t->track:field->bits==g->bits;
  if(equal){g->pending=0;continue;}
  CommandState *c=in->commands;
  unsigned n=atomic_load_explicit(&c->published,memory_order_acquire);
  if(n>=COMMAND_SEQUENCE_LAST){in->error=C_CAPACITY;return;}
  unsigned slot=command_free(c);if(slot==COMMAND_SLOTS)return;
  /* A new writer may have inherited other completed history, but never another
   * unresolved control. The cooperating lock and publication gate enforce it. */
  if(INPUT_STOPPED())return;
  CommandRequest r={n+1,c->pid,c->start_lo,c->start_hi,c->origin_sec,c->origin_nsec,g->identity.epoch,g->identity.serial,g->identity.binding,g->identity.incarnation,g->bits,now,g->expires,field->bits,field->revision,field_id,t->track_owner,t->program_owner,field_id?field->incarnation:0,s->project_owner,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  r.pad_owner=g->identity.pad_owner;r.pad_index=g->identity.pad_index;r.pad_generation=g->identity.pad_generation;if(r.pad_owner)r.field_incarnation=field->incarnation;
  InputFlight *tx=in->flights+slot;memset(tx,0,sizeof(*tx));
  tx->flight=n+1;tx->target=g->identity;tx->strip=strip;tx->bits=g->bits;tx->expires=g->expires;tx->before_revision=field->revision;tx->field=field_id;tx->field_incarnation=field->incarnation;tx->property=field->property;tx->phase=tx->commit_revision=tx->commit_tick=tx->done_tick=0;
  memset(tx->cursor,0,sizeof(tx->cursor));if(!command_publish_request_at(c,&r,slot)){memset(tx,0,sizeof(*tx));if(!atomic_load(&c->new_project_intent))in->error=C_CLOSED;return;}
  g->pending=0;in->submitted++;if(step<edges)edges_submitted++;if(step>=edges)in->next_strip=(at+1)%(MIRROR_BANK*CF_COUNT);
  INPUT_LOG("SUBMIT request=%u fader=%u epoch=%u track=%u binding=%u incarnation=%u bits=%u field=%u tick=%u continuous; awaiting source settlement\n",n+1,strip+1,r.epoch,r.serial,r.binding,r.incarnation,r.bits,r.reserved,now);
 }
 while(in->edge_count&&!in->edges[in->edge_head].desire.pending){in->edge_head=(in->edge_head+1)%INPUT_EDGES;in->edge_count--;}
}
static inline void input_pump(MirrorInput *in,MirrorBank *bank,const CopiedMirror *s,uint32_t now){input_pump_mode(in,bank,s,now,1);}
static inline void input_drain(MirrorInput *in,MirrorBank *bank,const CopiedMirror *s,uint32_t now){input_pump_mode(in,bank,s,now,0);}
static inline int input_drained(const MirrorInput *in){return !input_pending(in)&&!in->error&&in->commands&&command_idle(in->commands);}
static inline int input_blocked_field(const MirrorInput *in,unsigned strip,unsigned field){
 if(field>=CF_COUNT)return 0;
 if(in->error||in->desires[strip][field].pending)return 1;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in->flights[i].flight&&!in->flights[i].acknowledged&&in->flights[i].strip==strip&&in->flights[i].field==field)return 1;
 return 0;
}
static inline int input_blocked(const MirrorInput *in,unsigned strip){return input_blocked_field(in,strip,CF_VOLUME);}
#endif
