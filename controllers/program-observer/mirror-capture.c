#include "observer.h"
#include <string.h>
/* No hook allocates, blocks, performs I/O, calls MPC or reads a native scalar. */
MirrorState *mirror_state;
uint32_t observer_image_bias;
_Atomic uint32_t observer_running;
static uint32_t word(uint32_t a){return *(const volatile uint32_t*)(uintptr_t)a;}
static uint32_t token(void){uint32_t n;__asm__ volatile("mrc p15,0,%0,c13,c0,3":"=r"(n));return n;}
static uint32_t source_sp(Context *c){return (uint32_t)(uintptr_t)c+sizeof(*c);}
static int pointer(uint32_t p){return p>=4096&&!(p&3)&&p<0xffffe000u;}
static int typed(uint32_t p,uint32_t rva){return pointer(p)&&word(p)==observer_image_bias+rva;}
void mirror_stop(unsigned why){
 if(!mirror_state)return;
 if(why)atomic_store_explicit(&mirror_state->error,why,memory_order_release);
 atomic_store_explicit(&mirror_state->alive,0,memory_order_release);
}
/* Four actual exclusive attempts, no libatomic/unbounded CAS helper. Different
 * cells and topology/registration guards occupy disjoint 2KiB granules. */
#ifdef MIRROR_ADMISSION_TEST
static __thread _Atomic uint32_t *claim_test_guard;
static __thread uint32_t claim_test_losses,claim_test_calls;
#endif
static unsigned claim_result(_Atomic uint32_t *guard){
 uint32_t result,scratch,left;
#ifdef MIRROR_ADMISSION_TEST
 claim_test_calls++;
 uint32_t losses=guard==claim_test_guard?claim_test_losses:0;
#endif
 __asm__ volatile("mov %[left],#4\n1: ldrex %[result],[%[guard]]\n cmp %[result],#0\n bne 3f\n mov %[scratch],#1\n"
#ifdef MIRROR_ADMISSION_TEST
 "cmp %[losses],#0\n beq 5f\n clrex\n sub %[losses],%[losses],#1\n5:\n"
#endif
 "strex %[result],%[scratch],[%[guard]]\n cmp %[result],#0\n beq 4f\n subs %[left],%[left],#1\n bne 1b\n mov %[result],#3\n b 4f\n3: clrex\n mov %[result],#2\n4: dmb ish"
 :[result]"=&r"(result),[scratch]"=&r"(scratch),[left]"=&r"(left)
#ifdef MIRROR_ADMISSION_TEST
 ,[losses]"+r"(losses)
#endif
 :[guard]"r"(guard):"cc","memory");
 return result;
}
static __attribute__((unused)) int claim(_Atomic uint32_t *guard){
 unsigned result=claim_result(guard);
 if(result)mirror_stop(result);
 return !result;
}
static void release(_Atomic uint32_t *guard){atomic_store_explicit(guard,0,memory_order_release);}
static int active(void){return mirror_state&&atomic_load_explicit(&mirror_state->alive,memory_order_acquire)&&!atomic_load_explicit(&mirror_state->error,memory_order_acquire);}
#include "channel-capture.inc"
#include "pad-capture.inc"
static unsigned hash(uint32_t property){return ((property>>4)*2654435761u)&(MIRROR_HASH-1);}
/* Lookup returns a fixed cell, latched before any guard/queue. Immutable native
 * keys and cell incarnation never change even after closure or address reuse. */
static MirrorCell *lookup(uint32_t property){
 unsigned h=hash(property);
 for(unsigned i=0;i<MIRROR_PROBES;i++){
  uint32_t n=atomic_load_explicit(mirror_state->directory+((h+i)&(MIRROR_HASH-1)),memory_order_acquire);
  if(!n)return NULL;
  if(n>MIRROR_CELLS){mirror_stop(MV_LIFETIME);return NULL;}
  MirrorCell *c=mirror_state->cells+n-1;
  if(atomic_load_explicit(&c->property,memory_order_relaxed)==property)return c;
 }
 return NULL;
}
/* Native same-Property ownership serializes these synchronous writes. No
 * exclusive instruction occurs on the commit path. A closed latched cell is
 * never redirected to a later incarnation. */
static __attribute__((noinline)) void publish_value(MirrorCell *cell,uint32_t bits,int death){
 uint32_t revision=atomic_load_explicit(&cell->revision,memory_order_relaxed);
 if(!atomic_load_explicit(&cell->live,memory_order_relaxed)||(revision&1)||revision>UINT32_MAX-2){mirror_stop(MV_LIFETIME);return;}
 atomic_store_explicit(&cell->revision,revision+1,memory_order_relaxed);
 atomic_thread_fence(memory_order_release);
#ifdef COMMAND_COMPONENT
 COMMAND_PAUSE(death?4:3);
#endif
 if(death)atomic_store_explicit(&cell->live,0,memory_order_relaxed);
 else{atomic_store_explicit(&cell->bits,bits,memory_order_relaxed);atomic_store_explicit(&cell->updates,atomic_load_explicit(&cell->updates,memory_order_relaxed)+1,memory_order_relaxed);}
 atomic_store_explicit(&cell->revision,revision+2,memory_order_release);

}
#ifdef MIRROR_COMMAND
static int command_birth_owner(unsigned site);
#endif
static void birth(uint32_t property,uint32_t program,uint32_t bits,unsigned kind){
#ifdef MIRROR_COMMAND
 if(!command_birth_owner(kind==1?MIRROR_SEED_NORMAL:MIRROR_SEED_COPY))return;
#else
 if(!claim(&mirror_state->birth_guard))return;
#endif
 MirrorCell *old=lookup(property);
 if(old&&atomic_load_explicit(&old->live,memory_order_acquire)){mirror_stop(MV_LIFETIME);goto out;}
 uint32_t n=atomic_load_explicit(&mirror_state->allocated,memory_order_relaxed);
 if(n>=MIRROR_CELLS){mirror_stop(MV_CAPACITY);goto out;}
 unsigned h=hash(property),slot=MIRROR_HASH;
 for(unsigned i=0;i<MIRROR_PROBES;i++){
  unsigned s=(h+i)&(MIRROR_HASH-1);uint32_t index=atomic_load_explicit(mirror_state->directory+s,memory_order_relaxed);
  if(!index||(old&&index==atomic_load_explicit(&old->incarnation,memory_order_relaxed))){slot=s;break;}
 }
 if(slot==MIRROR_HASH){mirror_stop(MV_CAPACITY);goto out;}
 MirrorCell *c=mirror_state->cells+n;
#ifdef COMMAND_COMPONENT
 COMMAND_PAUSE(2);
#endif
 atomic_store_explicit(&c->property,property,memory_order_relaxed);atomic_store_explicit(&c->program,program,memory_order_relaxed);
 atomic_store_explicit(&c->incarnation,n+1,memory_order_relaxed);atomic_store_explicit(&c->bits,bits,memory_order_relaxed);
 atomic_store_explicit(&c->seed,kind,memory_order_relaxed);atomic_store_explicit(&c->live,1,memory_order_relaxed);atomic_store_explicit(&c->revision,2,memory_order_release);
 atomic_store_explicit(&mirror_state->allocated,n+1,memory_order_release);
 atomic_store_explicit(mirror_state->directory+slot,n+1,memory_order_release);
out:
#ifndef MIRROR_COMMAND
 release(&mirror_state->birth_guard);
#endif
 return;
}
/* Read only this live constructor's own proven frame chain. Stop at the first
 * non-Program caller; common Mixable objects (pads etc.) never become cells. */
static void seed(Context *c,unsigned site){
 uint32_t sp=source_sp(c),program,property;
 if(site==MIRROR_SEED_NORMAL){
  if(word(sp+0xb4)!=observer_image_bias+0x237034c)return;
  if(word(sp+0x114)!=observer_image_bias+0x23705f8)return;
  if(word(sp+0x13c)!=observer_image_bias+0x2514c08)return;
  program=word(sp+0x12c);property=c->r[5];
 }else{
  if(word(sp+0x8c)!=observer_image_bias+0x2370710)return;
  if(word(sp+0xf4)!=observer_image_bias+0x2370a7c)return;
  if(word(sp+0x10c)!=observer_image_bias+0x2514334)return;
  program=word(sp+0xf8);property=c->r[7];
 }
 if(!pointer(program)||program+0x3c!=c->r[4]||program+0x5b0!=property){mirror_stop(MV_LIFETIME);return;}
 birth(property,program,site==MIRROR_SEED_NORMAL?(uint32_t)c->d[8]:c->r[3],site==MIRROR_SEED_NORMAL?1:2);
}
/* Only topology uses this copied working set and its own admission. It never
 * gates scalar publication. Track addresses are never followed after removal. */
typedef struct {uint32_t track,serial,program,kind,incarnation,binding,transition,replacement,replace_token,vptr,family,track_owner,program_owner,meter;} Track;
typedef struct {_Atomic uint32_t track;uint32_t serial;_Atomic uint32_t closed;} Retired;
#ifdef MIRROR_COMMAND
static int command_type_inventory(unsigned,const Track*);
#endif
static Track tracks[MIRROR_TRACKS],next_tracks[MIRROR_TRACKS];
static Retired retired[MIRROR_CELLS];static _Atomic unsigned retired_count;static unsigned count;
static _Atomic uint32_t root,project,pool;
static uint32_t epoch,serial,binding,generation,owner,ready,reset_pending;
static uint32_t loading,load_token,load_sp,returned;
/* Separate from epoch: native Project/pool clears advance it during creation. */
static struct {_Atomic uint32_t token;uint32_t sp,lr,root,project,pool,project_owner,creator;} creating;
static int create_source(Context *c,unsigned site){
 if(site==M_CREATE_READY)return atomic_load_explicit(&creating.token,memory_order_acquire)==token();
 uint32_t lr=c->lr-observer_image_bias;
 /* The constructor e35a10 supplies NewProject arg3=0. The selected empty
  * project path190bb1c supplies1;2570b1c forwards this copied argument inr1. */
 return c->r[1]==1&&(lr==0x2570b7c||lr==0x2570bc4||lr==0x2570c34);
}
static struct {uint32_t method,token,sp,track,committed;} mutation;
static unsigned depth;
static uint32_t bump(uint32_t *n){if(*n==UINT32_MAX){mirror_stop(MV_WRAP);return 0;}return ++*n;}
static uint32_t incarnation(uint32_t program){
 if(!program)return 0;
 MirrorCell *c=lookup(program+0x5b0);
 return c&&atomic_load_explicit(&c->live,memory_order_acquire)?atomic_load_explicit(&c->incarnation,memory_order_relaxed):0;
}
static void export_topology(void){
 uint32_t rev=atomic_load_explicit(&mirror_state->revision,memory_order_relaxed);
 if(rev>UINT32_MAX-2){mirror_stop(MV_WRAP);return;}
 atomic_store_explicit(&mirror_state->revision,rev+1,memory_order_relaxed);
 atomic_thread_fence(memory_order_release);
 unsigned pending=depth;for(unsigned i=0;i<count;i++)pending|=tracks[i].transition;
 atomic_store_explicit(&mirror_state->ready,ready&&!pending,memory_order_relaxed);
 atomic_store_explicit(&mirror_state->channel.project_owner,channel_owner_id(project),memory_order_relaxed);
 atomic_store_explicit(&mirror_state->epoch,epoch,memory_order_relaxed);atomic_store_explicit(&mirror_state->generation,generation,memory_order_relaxed);atomic_store_explicit(&mirror_state->count,count,memory_order_relaxed);
 for(unsigned i=0;i<count;i++){
  MirrorBinding *b=mirror_state->tracks+i;Track *t=tracks+i;
  atomic_store_explicit(&b->meter,t->meter,memory_order_relaxed);
  atomic_store_explicit(&b->serial,t->serial,memory_order_relaxed);atomic_store_explicit(&b->track,t->track,memory_order_relaxed);atomic_store_explicit(&b->program,t->program,memory_order_relaxed);
  atomic_store_explicit(&b->kind,t->kind,memory_order_relaxed);atomic_store_explicit(&b->incarnation,t->incarnation,memory_order_relaxed);atomic_store_explicit(&b->binding,t->binding,memory_order_relaxed);atomic_store_explicit(&b->vptr,t->vptr,memory_order_relaxed);atomic_store_explicit(&b->track_owner,t->track_owner,memory_order_relaxed);atomic_store_explicit(&b->program_owner,t->program_owner,memory_order_relaxed);
 }
 atomic_store_explicit(&mirror_state->revision,rev+2,memory_order_release);
}
static int retire_track(const Track *t){
 if(retired_count==MIRROR_CELLS){mirror_stop(MV_CAPACITY);return 0;}
 unsigned n=atomic_load_explicit(&retired_count,memory_order_relaxed);
 retired[n].serial=t->serial;atomic_store_explicit(&retired[n].closed,0,memory_order_relaxed);atomic_store_explicit(&retired[n].track,t->track,memory_order_release);
 atomic_store_explicit(&retired_count,n+1,memory_order_release);return 1;
}
static void invalidate(void){
 ready=0;bump(&epoch);
 for(unsigned i=0;i<count;i++)if(!retire_track(tracks+i))break;
 count=0;export_topology();
}
static int unresolved(uint32_t track){for(unsigned i=0;i<retired_count;i++)if(retired[i].track==track&&!retired[i].closed)return 1;return 0;}
/* Native membership envelope is read only in owner-qualified postcommit/load
 * callbacks. Existing bindings come from our copies. New live Track payloads
 * are copied once; scalar Property memory is never sampled. */
static void inventory(unsigned site){
 if(!typed(pool,0x068ad2c4)){mirror_stop(MV_ROOT);return;}
 uint32_t begin=word(pool+0x34),end=word(pool+0x38),cap=word(pool+0x3c);
 if((begin&&!pointer(begin))||end<begin||cap<end||(end-begin)%4||(end-begin)/4>MIRROR_TRACKS){mirror_stop(MV_TOPOLOGY);return;}
 unsigned n=(end-begin)/4;
 for(unsigned i=0;i<n;i++){
  uint32_t address=word(begin+4*i);unsigned j;
  for(j=0;j<i;j++)if(next_tracks[j].track==address){mirror_stop(MV_TOPOLOGY);return;}
  for(j=0;j<count;j++)if(tracks[j].track==address)break;
  if(j<count)next_tracks[i]=tracks[j];
  else{
   if(unresolved(address)||!typed(address,0x06935554)){mirror_stop(MV_LIFETIME);return;}
   uint32_t p=word(address+0x648),kind=word(address+0x64c);
   next_tracks[i]=(Track){address,bump(&serial),p,kind,incarnation(p),bump(&binding),0,0,0,p&&pointer(p)?word(p)-observer_image_bias:0,0,channel_owner_id(address),channel_owner_id(p),0};
  }
  Track *t=next_tracks+i;
  if(t->transition){
   if(!(site==M_PROGRAM_COMMIT&&mutation.track==t->track&&t->transition==3&&t->replace_token==mutation.token)
#ifdef MIRROR_COMMAND
    &&!command_type_inventory(site,t)
#endif
   ){mirror_stop(MV_TOPOLOGY);return;}
   t->transition=0;t->program=t->replacement;t->incarnation=incarnation(t->program);t->binding=bump(&binding);t->program_owner=channel_owner_id(t->program);
  }
 }
 if(begin!=word(pool+0x34)||end!=word(pool+0x38)||cap!=word(pool+0x3c)){mirror_stop(MV_TOPOLOGY);return;}
 for(unsigned i=0;i<n;i++)if(word(begin+i*4)!=next_tracks[i].track){mirror_stop(MV_TOPOLOGY);return;}
 for(unsigned i=0;i<count;i++){unsigned j;for(j=0;j<n;j++)if(tracks[i].serial==next_tracks[j].serial)break;if(j==n&&!retire_track(tracks+i))return;}
 memcpy(tracks,next_tracks,n*sizeof(Track));count=n;
}
static int known_track(uint32_t address,int include_retired){
 unsigned n=atomic_load_explicit(&mirror_state->count,memory_order_acquire);if(n>MIRROR_TRACKS){mirror_stop(MV_TOPOLOGY);return 0;}
 for(unsigned i=0;i<n;i++)if(atomic_load_explicit(&mirror_state->tracks[i].track,memory_order_acquire)==address)return 1;
 if(include_retired){n=atomic_load_explicit(&retired_count,memory_order_acquire);for(unsigned i=0;i<n;i++)if(atomic_load_explicit(&retired[i].track,memory_order_acquire)==address&&!atomic_load_explicit(&retired[i].closed,memory_order_acquire))return 1;}
 return 0;
}
static void topology(Context *c,unsigned site){
 uint32_t tok=token();unsigned method=site>=M_ADD_ENTER&&site<=M_PROGRAM_EXIT?(site-M_ADD_ENTER)/3+1:0;
 /* Filter unrelated owners before topology admission using atomic copied keys. */
 if(site==M_TRACK_DESTROY&&!known_track(c->r[0],1))return;
 if(site>=M_REPLACE_SIMPLE_PRE&&site<=M_REPLACE_NOTIFY&&!known_track(c->r[4],0))return;
 if(method){unsigned phase=(site-M_ADD_ENTER)%3;uint32_t receiver=c->r[phase?(method==1?4:5):0];if(!pool||receiver!=pool)return;}
 if(site==M_PROJECT_CLEAR&&c->r[0]!=project)return;
 if(site==M_POOL_CLEAR&&c->r[0]!=pool)return;
 if(site==M_POOL_DESTROY&&c->r[5]!=project)return;
 if(site==M_ROOT_DESTROY&&c->r[0]!=root)return;
 if(site==M_PROJECT_DESTROY&&c->r[0]!=project)return;
#if !defined(UI_WITNESS) && !defined(MIRROR_COMMAND)
 if(!claim(&mirror_state->topology_guard))return;
#endif
 if(site==M_RESET||site==M_CREATE_ENTER){
  if(depth||loading||creating.token){atomic_store(&creating.token,0);mirror_stop(MV_TOPOLOGY);goto out;}
  invalidate();if(!active())goto out;
  root=c->r[0];if(!typed(root,0x068d7bf8)){mirror_stop(MV_ROOT);goto out;}
  project=word(root+0x24c);if(!typed(project,0x06933c4c)){mirror_stop(MV_ROOT);goto out;}
  pool=word(project+0xa94);if(!typed(pool,0x068ad2c4)){mirror_stop(MV_ROOT);goto out;}
  owner=tok;reset_pending=1;
  if(site==M_CREATE_ENTER){
   uint32_t creator=c->r[4],id=channel_owner_id(project);
   if(!id||!pointer(creator)||word(creator+8)!=root||word(root+0x240)!=creator){mirror_stop(MV_ROOT);goto out;}
   creating.sp=source_sp(c);creating.lr=c->lr;creating.root=root;creating.project=project;creating.pool=pool;creating.project_owner=id;creating.creator=creator;
   atomic_store_explicit(&creating.token,tok,memory_order_release);reset_pending=0;
  }
 atomic_store(&mirror_state->channel.automation_owner,channel_owner_id(word(project+0xaac)));
#ifdef MIRROR_COMMAND
  /* Resolve only at the existing qualified Engine reset boundary. Native
   * constructors/destructors own the retained field lifetimes. */
  uint32_t audio=word(root+0x2e4),mixer=0,timeline=0;
  if(pointer(audio)&&audio<=UINT32_MAX-0x400){mixer=audio+0x260;timeline=word(audio+0x3a4);}
  atomic_store(&mirror_state->channel.mixer_owner,typed(mixer,0x6898b34)?channel_owner_id(mixer):0);
  atomic_store(&mirror_state->channel.timeline_owner,typed(timeline,0x69091b0)?channel_owner_id(timeline):0);
  atomic_store(&mirror_state->channel.loop_owner,typed(timeline,0x69091b0)?channel_owner_id(word(timeline+0x14c)):0);
  uint32_t sequencer=pointer(audio)&&audio<=UINT32_MAX-0x400?word(audio+0x3bc):0;
  if(!pointer(sequencer)||sequencer>UINT32_MAX-0x404||word(sequencer+0x400)!=timeline)sequencer=0;
  atomic_store(&mirror_state->sequencer,sequencer);
  uint32_t click=pointer(audio)&&audio<=UINT32_MAX-0x400?word(audio+0x3c0):0;
  atomic_store(&mirror_state->channel.click_owner,typed(click,0x689fed4)&&word(click+0x64)==sequencer?channel_owner_id(click):0);
  uint32_t recorder=sequencer?word(sequencer+0x3ec):0,rp=pointer(recorder)&&recorder<=UINT32_MAX-0x9c?word(recorder+0x98):0;
  atomic_store(&mirror_state->channel.record_owner,typed(rp,0x68a0340)?channel_owner_id(rp):0);
#endif
 }else if(site==M_CREATE_READY){
  int same=creating.token==tok&&creating.sp==source_sp(c)+0x128&&
   c->r[4]==creating.root&&word(source_sp(c)+0x11c)==creating.lr&&
   root==creating.root&&project==creating.project&&pool==creating.pool&&
   !depth&&!loading&&channel_owner_id(project)==creating.project_owner&&
   typed(root,0x68d7bf8)&&typed(project,0x6933c4c)&&typed(pool,0x68ad2c4)&&
   word(root+0x24c)==project&&word(project+0xa94)==pool&&
   word(root+0x240)==creating.creator;
  atomic_store(&creating.token,0);
  if(!same){mirror_stop(MV_LIFETIME);goto out;}
  inventory(site);if(!active())goto out;ready=1;
 }else if(site==M_TRACK_DESTROY){
  unsigned i;for(i=0;i<count;i++)if(tracks[i].track==c->r[0])break;
  if(i<count)invalidate();
  for(i=0;i<retired_count;i++)if(retired[i].track==c->r[0])retired[i].closed=1;
 }else if(site==M_ROOT_DESTROY||site==M_PROJECT_DESTROY||site==M_PROJECT_CLEAR||site==M_POOL_CLEAR||site==M_POOL_DESTROY){
  if(tok!=owner||depth){mirror_stop(MV_TOPOLOGY);goto out;}invalidate();
  if(site==M_ROOT_DESTROY||site==M_PROJECT_DESTROY||site==M_POOL_DESTROY){root=project=pool=loading=returned=reset_pending=0;atomic_store(&creating.token,0);}
 }else if(site==M_LOAD_ENTER){
  if(!root)goto out;
  if(tok!=owner||loading||depth||!reset_pending||c->r[2]!=project||c->lr!=observer_image_bias+0x17b7fb8){mirror_stop(MV_LOAD);goto out;}
  loading=1;returned=0;load_token=tok;load_sp=source_sp(c);reset_pending=0;
 }else if(site==M_LOAD_RETURN||site==M_LOAD_READY){
  if(!root)goto out;
  if(!loading||tok!=load_token||source_sp(c)!=load_sp||depth){mirror_stop(MV_LOAD);goto out;}
  if(site==M_LOAD_RETURN){returned=c->r[0]==4;if(!returned)loading=0;}
  else{
   if(!returned||!pointer(c->r[5])||word(c->r[5]+0x68)!=project||word(project+0xa94)!=pool){mirror_stop(MV_LOAD);goto out;}
   inventory(site);if(!active())goto out;loading=returned=0;ready=1;
  }
 }else if(method){
  unsigned phase=(site-M_ADD_ENTER)%3;
  if(tok!=owner){mirror_stop(MV_TOPOLOGY);goto out;}
  if(!phase){
   if(depth){mirror_stop(MV_TOPOLOGY);goto out;}
   mutation.method=method;mutation.token=tok;mutation.sp=source_sp(c);mutation.track=c->r[method==1?2:1];mutation.committed=0;depth=1;
  }else if(depth){
   uint32_t frame=method==1?0x38:method==5?0x28:0x30;
   if(mutation.method!=method||mutation.token!=tok||source_sp(c)+frame!=mutation.sp){mirror_stop(MV_TOPOLOGY);goto out;}
   if(phase==1){
    if(mutation.committed||c->r[method==1?4:5]!=pool||c->r[method==1?5:4]!=mutation.track){mirror_stop(MV_TOPOLOGY);goto out;}
    mutation.committed=1;bump(&generation);if(ready)inventory(site);
   }else depth=0;
  }
 }else if(site>=M_REPLACE_SIMPLE_PRE&&site<=M_REPLACE_NOTIFY){
  Track *t=NULL;for(unsigned i=0;i<count;i++)if(tracks[i].track==c->r[4]){t=tracks+i;break;}
  if(!t)goto out;
  if(tok!=owner){mirror_stop(MV_TOPOLOGY);goto out;}
  if(site==M_REPLACE_SIMPLE_PRE||site==M_REPLACE_FULL_PRE){
   uint32_t old=c->r[site==M_REPLACE_SIMPLE_PRE?7:9],next=c->r[5];
   if(t->transition||old!=t->program){mirror_stop(MV_LIFETIME);goto out;}
   if(old!=next){t->transition=1;t->replacement=next;t->replace_token=tok;t->family=site;}
  }else if(site==M_REPLACE_SIMPLE_STORE||site==M_REPLACE_FULL_STORE){
   if(t->transition){if(t->transition!=1||t->replacement!=c->r[5]||t->family+2!=site){mirror_stop(MV_LIFETIME);goto out;}t->transition=2;}
   t->kind=c->r[site==M_REPLACE_SIMPLE_STORE?6:3];
   t->vptr=c->r[5]&&pointer(c->r[5])?word(c->r[5])-observer_image_bias:0;
  }else if(t->transition){if(t->transition!=2||c->r[5]!=t->replacement||word(source_sp(c)+0xc)!=t->program){mirror_stop(MV_LIFETIME);goto out;}t->transition=3;}
 }
 export_topology();
out:
#if !defined(UI_WITNESS) && !defined(MIRROR_COMMAND)
 release(&mirror_state->topology_guard);
#endif
 return;
}
void observer_hook(Context *c,unsigned kind){
 if(!active()||!c||kind<MODEL_HOOK_BASE)return;
 unsigned site=kind-MODEL_HOOK_BASE;
 if(site==M_CREATE_ENTER||site==M_CREATE_READY){if(create_source(c,site))topology(c,site);return;}
 pad_hook(c,site);channel_hook(c,site);if(channel_source(site))return;
 if(site==MIRROR_SEED_NORMAL||site==MIRROR_SEED_COPY){seed(c,site);return;}
 if(site==M_FLOAT||site==MIRROR_PROPERTY_DESTROY){
  MirrorCell *cell=lookup(c->r[site==M_FLOAT?5:0]);
  if(cell&&atomic_load_explicit(&cell->live,memory_order_acquire))publish_value(cell,(uint32_t)c->d[8],site==MIRROR_PROPERTY_DESTROY);
  return;
 }
 topology(c,site);
}
