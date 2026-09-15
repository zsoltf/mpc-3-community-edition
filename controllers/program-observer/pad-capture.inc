/* No native scalar polling. Common constructor stores and native Property
 * commits supply values; native topology seams supply only child identity. */
static void pad_error(unsigned why){atomic_store_explicit(&mirror_state->pads.error,why,memory_order_release);}
static unsigned pad_hash(uint32_t a,unsigned mask){return ((a>>4)*2654435761u)&mask;}
static PadOwner *pad_owner(uint32_t address){
 PadState *s=&mirror_state->pads;unsigned h=pad_hash(address,PAD_HASH-1);
 for(unsigned i=0;i<PAD_PROBES;i++){unsigned n=atomic_load_explicit(s->directory+((h+i)&(PAD_HASH-1)),memory_order_acquire);if(!n||n>PAD_OWNERS)return NULL;PadOwner *p=s->owners+n-1;if(atomic_load(&p->address)==address)return p;}return NULL;
}
static PadParent *pad_parent(uint32_t address,int create){
 PadState *s=&mirror_state->pads;unsigned owner=channel_owner_id(address),h=pad_hash(address,PAD_PARENT_HASH-1);
 if(!owner)return NULL;
 for(unsigned i=0;i<PAD_PROBES;i++){
  unsigned at=(h+i)&(PAD_PARENT_HASH-1),n=atomic_load_explicit(s->parent_directory+at,memory_order_acquire);
  if(n>PAD_PARENTS){pad_error(PD_CAPACITY);return NULL;}
  PadParent *p=n?s->parents+n-1:NULL;
  if(p&&atomic_load(&p->address)!=address)continue;
  if(p&&atomic_load(&p->owner)==owner&&atomic_load(&p->live))return p;
  if(!create)return NULL;
  unsigned used=atomic_load(&s->parents_used);if(used==PAD_PARENTS){pad_error(PD_CAPACITY);return NULL;}
  if(p)atomic_store_explicit(&p->live,0,memory_order_release);
  p=s->parents+used;atomic_store(&p->address,address);atomic_store(&p->owner,owner);atomic_store(&p->live,1);
  atomic_store_explicit(&s->parents_used,used+1,memory_order_release);atomic_store_explicit(s->parent_directory+at,used+1,memory_order_release);return p;
 }pad_error(PD_CAPACITY);return NULL;
}
static int pad_on_owner(void){if(token()==atomic_load_explicit(&mirror_state->channel.owner_token,memory_order_acquire))return 1;pad_error(PD_OWNER);return 0;}
static void pad_birth(uint32_t address,uint32_t parent){
 if(!pointer(address)||!pointer(parent)||!pad_on_owner())return;
 PadState *s=&mirror_state->pads;unsigned po=channel_owner_id(parent),used=atomic_load(&s->used),h=pad_hash(address,PAD_HASH-1);
 if(!po){pad_error(PD_LIFETIME);return;}if(used==PAD_OWNERS){pad_error(PD_CAPACITY);return;}
 PadOwner *old=pad_owner(address);
 for(unsigned i=0;i<PAD_PROBES;i++){
  unsigned at=(h+i)&(PAD_HASH-1),n=atomic_load(s->directory+at);
  if(n&&(!old||n!=atomic_load(&old->incarnation)))continue;
  if(old)atomic_store_explicit(&old->live,0,memory_order_release);
  PadOwner *p=s->owners+used;atomic_store(&p->address,address);atomic_store(&p->incarnation,used+1);atomic_store(&p->parent,parent);atomic_store(&p->parent_owner,po);atomic_store(&p->live,1);
  atomic_store_explicit(&s->used,used+1,memory_order_release);atomic_store_explicit(s->directory+at,used+1,memory_order_release);pad_parent(parent,1);return;
 }pad_error(PD_CAPACITY);
}
static void pad_write(PadOwner *p,unsigned f,uint32_t bits,unsigned seed,int death){
 if(f>=PF_COUNT||!p||!atomic_load_explicit(&p->live,memory_order_acquire))return;
 if(f==PF_SOLO&&!pad_on_owner())return;
 PadScalar *v=p->fields+f;unsigned rev=atomic_load(&v->revision);
 if((rev&1)||rev>UINT32_MAX-2){pad_error(PD_WRAP);return;}
 if(seed&&atomic_load(&v->seed)){pad_error(PD_LIFETIME);return;}
 if(!seed&&!atomic_load(&v->live))return;
 atomic_store_explicit(&v->revision,rev+1,memory_order_relaxed);atomic_thread_fence(memory_order_release);
 if(death)atomic_store(&v->live,0);
 else{atomic_store(&v->bits,bits);if(seed){atomic_store(&v->live,1);atomic_store(&v->seed,seed);}else atomic_store(&v->updates,atomic_load(&v->updates)+1);}
 atomic_store_explicit(&v->revision,rev+2,memory_order_release);
}
static PadOwner *pad_property(uint32_t property,unsigned *field){
 PadOwner *found=NULL;
 for(unsigned f=0;f<PF_COUNT;f++){unsigned offset=pad_offset(f);if(property<offset)continue;PadOwner *p=pad_owner(property-offset);if(!p||!atomic_load_explicit(&p->live,memory_order_acquire)||!atomic_load(&p->fields[f].seed)||!atomic_load(&p->fields[f].live))continue;if(found){*field=PF_COUNT;return found;}found=p;*field=f;}return found;
}
static uint32_t pad_frame(Context *c,int copy,unsigned extra){
 uint32_t sp=source_sp(c)+extra,p;
 if(extra&&word(source_sp(c)+0x8c)!=observer_image_bias+(copy?0x236faf4:0x236f090))return 0;
 if(!copy){if(word(sp+0xb4)!=observer_image_bias+0x237034c||word(sp+0x114)!=observer_image_bias+0x23705f8||word(sp+0x13c)!=observer_image_bias+0x243727c)return 0;p=word(sp+0x12c);}
 else{if(word(sp+0x8c)!=observer_image_bias+0x2370710||word(sp+0xf4)!=observer_image_bias+0x2370a7c||word(sp+0x10c)!=observer_image_bias+0x2435c28)return 0;p=word(sp+0xf8);}
 PadOwner *o=pad_owner(p);return pointer(p)&&c->r[4]==p+(extra?0xc:0)&&o&&atomic_load(&o->live)&&!atomic_load(&o->complete)?p:0;
}
typedef struct {uint32_t parent,owner,sp,index,second,swap;} PadMutation;
static PadMutation pad_mutations[4];static unsigned pad_depth;
static int pad_membership_begin(uint32_t parent,unsigned a,unsigned b,int swap,uint32_t sp){
 if(!pad_on_owner())return 0;
 PadParent *p=pad_parent(parent,1);
 if(!p||a>=PAD_SLOTS||(swap&&b>=PAD_SLOTS)||pad_depth==4){pad_error(PD_MEMBERSHIP);return 0;}
 unsigned rev=atomic_load(&p->revision);
 /* Bind calls nested inside native swap share its entire publication interval. */
 int nested=pad_depth&&pad_mutations[pad_depth-1].swap&&pad_mutations[pad_depth-1].parent==parent;
 if((rev&1)&&!nested){pad_error(PD_MEMBERSHIP);return 0;}
 if(!nested){if(rev>UINT32_MAX-2){pad_error(PD_WRAP);return 0;}atomic_store_explicit(&p->revision,rev+1,memory_order_release);}
 pad_mutations[pad_depth++]=(PadMutation){parent,atomic_load(&p->owner),sp,a,b,swap?1u:0u};return 1;
}
static int pad_publish_slot(PadParent *p,unsigned index){
 uint32_t d=atomic_load(&p->address),a=word(d+0x1fec),b=word(d+0x1ff0),cap=word(d+0x1ff4);
 if(!pointer(a)||b<a||cap<b||(b-a)%4||(cap-a)%4||(cap-a)/4>PAD_SLOTS||index>=(b-a)/4)return 0;
 uint32_t child=word(a+index*4);unsigned id=0;
 if(child){PadOwner *o=pad_owner(child);if(!o||!atomic_load(&o->live)||!atomic_load(&o->complete)||atomic_load(&o->parent)!=d||atomic_load(&o->parent_owner)!=atomic_load(&p->owner)||!typed(child,0x692f650)||word(child+0x13b0)!=d||word(child+0x12dc)!=index)return 0;id=atomic_load(&o->incarnation);}
 PadMembership *m=p->slots+index;unsigned gen=atomic_load(&m->generation);if(gen==UINT32_MAX){pad_error(PD_WRAP);return 0;}
 atomic_store(&m->owner,id);atomic_store(&m->generation,gen+1);return 1;
}
static void pad_membership_end(uint32_t sp,int swap){
 if(!pad_on_owner()||!pad_depth){pad_error(PD_MEMBERSHIP);return;}
 PadMutation m=pad_mutations[--pad_depth];PadParent *p=pad_parent(m.parent,0);
 if(!p||m.owner!=atomic_load(&p->owner)||m.swap!=(unsigned)swap||sp+(swap?0x20:0x58)!=m.sp){pad_error(PD_MEMBERSHIP);return;}
 if(pad_depth&&pad_mutations[pad_depth-1].swap&&pad_mutations[pad_depth-1].parent==m.parent)return;
 if(!pad_publish_slot(p,m.index)||(swap&&m.second!=m.index&&!pad_publish_slot(p,m.second))){pad_error(PD_MEMBERSHIP);return;}
 unsigned rev=atomic_load(&p->revision);atomic_store_explicit(&p->revision,rev+1,memory_order_release);
}
static void pad_hook(Context *c,unsigned site){
 if(site==PD_BIRTH||site==PD_COPY){pad_birth(c->r[0],c->r[site==PD_COPY?2:1]);return;}
 if(site==PD_COMPLETE||site==PD_COPY_COMPLETE){PadOwner *p=pad_owner(c->r[4]);if(!pad_on_owner()||!p||!atomic_load(&p->live)||!typed(c->r[4],0x692f650)){pad_error(PD_LIFETIME);return;}for(unsigned f=0;f<PF_COUNT;f++)if(!atomic_load(&p->fields[f].live)){pad_error(PD_LIFETIME);return;}atomic_store_explicit(&p->complete,1,memory_order_release);return;}
 if(site==PD_DEATH||site==PD_DELETE){PadOwner *p=pad_owner(c->r[0]);if(p)atomic_store_explicit(&p->live,0,memory_order_release);return;}
 if(site==PD_PARENT_CHANGE){PadOwner *p=pad_owner(c->r[0]);if(p&&atomic_load(&p->live)&&atomic_load(&p->parent)!=c->r[2]){atomic_store_explicit(&p->live,0,memory_order_release);pad_error(PD_MEMBERSHIP);}return;}
 if(site==PD_BIND||site==PD_SWAP){pad_membership_begin(c->r[0],c->r[1],c->r[2],site==PD_SWAP,source_sp(c));return;}
 if(site==PD_BOUND||site==PD_SWAPPED){pad_membership_end(source_sp(c),site==PD_SWAPPED);return;}
 if(site==CH_PROGRAM_DEATH){PadParent *p=pad_parent(c->r[0],0);if(p)atomic_store_explicit(&p->live,0,memory_order_release);return;}
 unsigned f=PF_COUNT,bits=0,copy=0;uint32_t owner=0;
 if(site==MIRROR_SEED_NORMAL||site==MIRROR_SEED_COPY){copy=site==MIRROR_SEED_COPY;owner=pad_frame(c,copy,0);f=PF_VOLUME;bits=copy?c->r[3]:(uint32_t)c->d[8];if(owner&&c->r[copy?7:5]!=owner+0x574){pad_error(PD_LIFETIME);return;}}
 else if(site==CH_PAN_BIRTH||site==CH_PAN_COPY){copy=site==CH_PAN_COPY;owner=pad_frame(c,copy,0);f=PF_PAN;bits=copy?c->r[7]:(uint32_t)c->d[16];}
 else if(site>=CH_MUTE_BIRTH&&site<=CH_SOLO_AUDIO_COPY){unsigned n=site-CH_MUTE_BIRTH;copy=n&1;owner=pad_frame(c,copy,0x90);f=n<2?PF_MUTE:n<4?PF_SOLO:PF_SOLO_AUDIO;bits=(f==PF_MUTE?(copy?c->r[2]:c->r[3]):copy?c->r[6]:c->r[3])&255;}
 if(owner){pad_write(pad_owner(owner),f,bits,copy?2:1,0);return;}
 if(site==M_FLOAT||site==M_MUTE||site==M_BOOL||site==CH_BOOL_COMMIT||site==MIRROR_PROPERTY_DESTROY){
  PadOwner *p=pad_property(c->r[site==MIRROR_PROPERTY_DESTROY?0:5],&f);if(!p)return;
  if(f==PF_COUNT){pad_error(PD_LIFETIME);return;}
  if(site==MIRROR_PROPERTY_DESTROY)pad_write(p,f,0,0,1);
  else if(site==M_FLOAT&&(f==PF_VOLUME||f==PF_PAN))pad_write(p,f,(uint32_t)c->d[8],0,0);
  else if((site==M_MUTE||site==CH_BOOL_COMMIT)&&(f==PF_MUTE||f==PF_SOLO_AUDIO))pad_write(p,f,c->r[7]&255,0,0);
  else if(site==M_BOOL&&f==PF_SOLO)pad_write(p,f,c->r[7]&255,0,0);
 }
}
