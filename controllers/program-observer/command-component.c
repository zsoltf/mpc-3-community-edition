#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#ifndef COMMAND_COMPONENT
#define COMMAND_COMPONENT
#endif
/* Owner publication has no revision word; only the store order stops a reader
 * that has matched a token from pairing it with a retired lifetime's address or
 * kind. This seam sits between the identity stores and the incarnation store
 * and holds that order to its contract. Declared before the allocator is
 * included, so it takes scalars rather than a ChannelOwner. */
static unsigned channel_owner_publishes;
static void channel_owner_publish(uint32_t,unsigned,unsigned,uint32_t,unsigned,unsigned);
#define CHANNEL_OWNER_PUBLISH(o,a,k,i) channel_owner_publish(atomic_load(&(o)->address),atomic_load(&(o)->kind),atomic_load(&(o)->incarnation),(a),(k),(i))
/* Counts the drain scan's own 39-word request reads for the skip regression. */
static unsigned drain_request_reads;
#define COMMAND_DRAIN_READ() (drain_request_reads++)
#include "command-capture.c"
#include "patch.c"
#include "observer.c"
#define MIRROR_READER_COMPONENT 1
/* Deterministic reader seam: the reclamation regression re-identifies the
 * matched cell here, between the probe match and copy_channel's in-window
 * re-validation, with the pinned owner still live. */
static void channel_read_pause(unsigned);
#define CHANNEL_READ_PAUSE(n) channel_read_pause(n)
#include "mirror-motor-core.h"
#include "mirror-input-core.h"
#include "mirror-poison.h"
uint32_t route,exercise_adjust,dispatch_count;
extern uint32_t saved_sp,captured_sp,captured_pair[2];
void exercise(Context*,void*,Context*);void capture_a(void);
void component_listener(void){}
static unsigned char root_object[0xa00],project_object[0xb00],pool_object[0x130],file_handler[0x80];
static unsigned char track_objects[MIRROR_TRACKS+1][0x650] __attribute__((aligned(8)));
static unsigned char program_objects[MIRROR_TRACKS+1][0x3000] __attribute__((aligned(8)));
static uint32_t members[MIRROR_TRACKS+1],program_members[MIRROR_TRACKS];
static unsigned char program_pool[0x100],queue_object[0x180],recording_fixture_factory[0x60],recording_fixture_audio[0x400];
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s error=%u depth=%u\n",why,mirror_state?atomic_load(&mirror_state->error):0,depth);exit(1);}}
static uint32_t ptr(void *p){return (uint32_t)(uintptr_t)p;}
static void put(uint32_t p,uint32_t v){*(uint32_t*)(uintptr_t)p=v;}
static Context context(void){Context c={0};for(unsigned i=0;i<32;i++)c.d[i]=0x1020304050607080ull+i;for(unsigned i=0;i<13;i++)c.r[i]=0x11223000+i*4;c.lr=0x12345678;c.apsr=0xa80f0010;c.fpscr=0x01000000;return c;}
/* Exact displaced pairs execute first at their real raw addresses, then through
 * installed gates. Synthetic continuations return to the context harness. */
static Context probe_saved;
static void probe_context(Context *c,unsigned kind){(void)kind;probe_saved=*c;}
/* Test-only fault injection replaces SUB with a branch here, leaving the
 * native load/store-exclusive and conditional retry instructions at real PCs. */
__attribute__((naked)) static void retry_once(void){
 __asm__ volatile("add r9,r9,#1\n"
                  "cmp r9,#1\n"
                  "bne 1f\n"
                  "clrex\n"
                  "1: sub r1,r1,#1\n"
                  "bx r8\n");
}
static int branch_enters_patch_interior(uint32_t pc,uint32_t instruction,uint32_t patch){
 if((instruction&0x0e000000u)!=0x0a000000u)return 0;
 int32_t displacement=(int32_t)(instruction<<8)>>6;
 return pc+8+displacement==patch+4;
}
static void exclusive_retry_checks(void){
 static uint32_t payload[2][8] __attribute__((aligned(8)));
 static uint32_t pending[16] __attribute__((aligned(64)));
 const uint32_t starts[2]={0x128f750,0x12c5aec};
 for(unsigned n=0;n<2;n++){
  uint32_t a=starts[n],loop=a+8,branch=loop+20;unsigned site=0;
  while(site<PATCH_COUNT&&anchor[site]!=a)site++;
  require(site<PATCH_COUNT&&capture_after_pair[site],"retry fixture uses corrected production post-store hook");
  const unsigned char *native=NULL;
  for(unsigned g=0;g<sizeof(guards)/sizeof(guards[0]);g++)
   if(guards[g].address<=loop&&loop+24<=guards[g].address+guards[g].length){native=guards[g].bytes+loop-guards[g].address;break;}
  require(native!=NULL,"native exclusive continuation retained by exact ELF guard");
  const uint32_t expected_loop[6]={0xf57ff05b,0xe1931f9f,0xe2411001,0xe1832f91,0xe3520000,0x1afffffa};
  require(!memcmp(native,expected_loop,sizeof(expected_loop)),"observed native DMB/load/sub/store/compare/retry words");
  require(branch_enters_patch_interior(branch,expected_loop[5],loop),"reject old DMB+LDREX patch: native retry enters its literal word");
  require(!branch_enters_patch_interior(branch,expected_loop[5],a),"corrected final-store patch leaves retry target outside patch");
  memcpy((void*)(uintptr_t)loop,native,24);
  /* Only reservation loss is injected. The native STREX result and BNE decide
   * the retry; on its second visit the helper leaves the reservation intact. */
  int32_t displacement=(int32_t)ptr(retry_once)-(int32_t)(loop+8+8);
  require(!(displacement&3)&&displacement>=-33554432&&displacement<33554432,"fault helper ARM branch range");
  put(loop+8,0xea000000u|(((uint32_t)(displacement>>2))&0xffffffu));
  uint32_t *out=(uint32_t*)(uintptr_t)(loop+24);jump(&out,ptr(capture_a));
  __builtin___clear_cache((char*)(uintptr_t)a,(char*)(uintptr_t)(loop+32));
  Context in=context(),actual;pending[0]=9;memset(payload[n],0,sizeof(payload[n]));
  in.r[0]=in.r[1]=in.r[4]=ptr(payload[n]);in.r[3]=ptr(pending);in.r[8]=loop+12;in.r[9]=0;
  exercise(&in,(void*)(uintptr_t)a,&actual);
  require(actual.r[9]==2&&actual.r[2]==0&&pending[0]==8,"forced first STREX failure retries native LDREX and decrements exactly once");
  require(!memcmp(payload[n],&in.d[16],8)&&payload[n][3]==(n?in.lr:in.r[12]),"displaced final vector stores survive retry continuation");
 }
 puts("PASS both actual installed vector-store detours plus native exclusive continuations: injected first reservation loss, two attempts/one decrement, old interior-entry spans rejected (helper and return substituted)");
}
/* Original and patched single-pair executions receive the same memory as
 * well as the same registers. Store-heavy routing seeds must not change an
 * earlier property's input in the next replay pass. */
static void replay_memory(int save){
#define REPLAY_MEMORY(name) do{static unsigned char saved[sizeof(name)];if(save)memcpy(saved,name,sizeof(name));else memcpy(name,saved,sizeof(name));}while(0)
 REPLAY_MEMORY(root_object);REPLAY_MEMORY(project_object);REPLAY_MEMORY(pool_object);REPLAY_MEMORY(file_handler);REPLAY_MEMORY(track_objects);REPLAY_MEMORY(program_objects);REPLAY_MEMORY(program_pool);REPLAY_MEMORY(queue_object);
#undef REPLAY_MEMORY
}
static void replay_checks(void){
 static uint32_t replay_enqueuer[3],replay_vtable[3];
 static uint32_t replay_io_vectors[6][8] __attribute__((aligned(8)));
 static uint32_t replay_vectors[2][8] __attribute__((aligned(8)));replay_enqueuer[0]=ptr(replay_vtable);
 uint32_t pages[256];unsigned np=0;
 for(unsigned i=0;i<sizeof(guards)/sizeof(guards[0]);i++){
  uint32_t first=guards[i].address&~4095u,last=(guards[i].address+guards[i].length-1)&~4095u;
  for(uint32_t page=first;page<=last;page+=4096){unsigned j;for(j=0;j<np;j++)if(pages[j]==page)break;if(j==np)pages[np++]=page;}
 }
 for(unsigned i=0;i<PATCH_COUNT;i++){
  uint32_t words[2];memcpy(words,expected[i],8);
  for(unsigned k=0;k<3;k++){
   uint32_t address=k==2?anchor[i]:anchor[i]+4*k+8+(words[k]&4095);
   if(k<2&&!relocated[i][k])continue;
   uint32_t page=address&~4095u;unsigned j;for(j=0;j<np;j++)if(pages[j]==page)break;if(j==np)pages[np++]=page;
  }
 }
 for(unsigned i=0;i<np;i++)require(mmap((void*)(uintptr_t)pages[i],4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED,"synthetic exact-address page");
 for(unsigned i=0;i<sizeof(guards)/sizeof(guards[0]);i++)memcpy((void*)(uintptr_t)guards[i].address,guards[i].bytes,guards[i].length);
 for(unsigned i=0;i<PATCH_COUNT;i++){
  memcpy((void*)(uintptr_t)anchor[i],expected[i],8);
  for(unsigned k=0;k<2;k++)if(relocated[i][k]){uint32_t w;memcpy(&w,expected[i]+4*k,4);put(anchor[i]+4*k+8+(w&4095),relocated[i][k]);}
 }
 void *gates=NULL;
 for(unsigned failure=0;failure<PATCH_COUNT;failure++){
  require(!install_patches(0,(int)failure,&gates),"transaction injected failure");
  for(unsigned i=0;i<PATCH_COUNT;i++)require(!memcmp((void*)(uintptr_t)anchor[i],expected[i],8),"all model sites rolled back");
 }
 Context inputs[PATCH_COUNT],outputs[PATCH_COUNT];uint32_t shifts[PATCH_COUNT];
 for(unsigned i=0;i<np;i++)require(!mprotect((void*)(uintptr_t)pages[i],4096,PROT_READ|PROT_WRITE|PROT_EXEC),"fixture continuation protection");
 for(unsigned i=0;i<PATCH_COUNT;i++){
  uint32_t *p=(uint32_t*)(uintptr_t)(anchor[i]+8);jump(&p,(uint32_t)(uintptr_t)capture_a);
 }
 for(unsigned i=0;i<np;i++)__builtin___clear_cache((char*)(uintptr_t)pages[i],(char*)(uintptr_t)(pages[i]+4096));
 mirror_state=NULL;observer_running=0;put(ptr(file_handler)+0x68,ptr(project_object));replay_memory(1);
 for(unsigned i=0;i<PATCH_COUNT;i++){
  replay_memory(0);Context in=context();in.r[0]=ptr(pool_object);in.r[3]=ptr(project_object);in.r[5]=ptr(file_handler);
  if(hook_ids[i]==M_REMOVE_ENTER||hook_ids[i]==M_POOL_CLEAR)in.r[0]=ptr(pool_object);
  if(hook_ids[i]==M_POOL_DESTROY)in.r[5]=ptr(project_object);
  if(hook_ids[i]==M_QUEUE_DONE)in.r[4]=ptr(file_handler);
  if(hook_ids[i]==M_NAME)in.r[5]=ptr(track_objects[0])+0x440;
  if(hook_ids[i]==M_REPLACE_SIMPLE_STORE||hook_ids[i]==M_REPLACE_FULL_STORE)in.r[4]=ptr(track_objects[0]);
  if(hook_ids[i]==MIRROR_SEED_NORMAL||hook_ids[i]==MIRROR_SEED_COPY){in.r[4]=ptr(track_objects[0]);in.r[9]=ptr(program_objects[0]);}
  if(hook_ids[i]>=CH_PROGRAM_BIRTH){in.r[1]=ptr(program_objects[2]);in.r[2]=ptr(program_objects[2]);in.r[4]=ptr(program_objects[2]);in.r[5]=ptr(program_objects[3]);in.r[8]=ptr(program_objects[2]);}
  /* These native final stores mutate their vector payload. Keep that payload
   * separate from other sites' input objects across original/patched replay. */
  if(hook_ids[i]==QS_TARGETS||hook_ids[i]==QS_GROUPS){unsigned v=hook_ids[i]==QS_GROUPS;in.r[0]=in.r[1]=in.r[4]=ptr(replay_vectors[v]);}
  if(hook_ids[i]==IO_INPUT_CATALOGUE_SEED)in.r[1]=ptr(replay_io_vectors[0]);
  if(hook_ids[i]==IO_OUTPUT_CATALOGUE_SEED)in.lr=ptr(replay_io_vectors[1]);
  if(hook_ids[i]==RC_ENQUEUER)in.r[0]=ptr(replay_enqueuer);
  if(hook_ids[i]==JN_OPERATION)in.r[11]=ptr(replay_enqueuer);
  if(hook_ids[i]==PX_RETIRE)put(in.r[3]+0x34,ptr(replay_enqueuer)); /* exact displaced manager load */
  if(hook_ids[i]==QS_MODE_VALUE){in.r[0]=ptr(program_objects[0]);put(in.r[0]+0x50,ptr(program_objects[1]));} /* Native tailstub loads P before entering the second captured pair. */
  if(hook_ids[i]==QJ_VALUE_FANOUT)in.r[6]=ptr(replay_enqueuer); /* Displaced LDREX reads the native pending word. */
  if(hook_ids[i]==IO_MONITOR_BIRTH)in.r[3]=ptr(replay_io_vectors[2]);
  if(hook_ids[i]==IO_SEND_BIRTH)in.r[3]=ptr(replay_io_vectors[3]);
  if(hook_ids[i]==IO_AUDIO_MONITOR_BIRTH)in.r[3]=ptr(replay_io_vectors[4]);
  if(hook_ids[i]==IO_PC_EMPTY)in.r[3]=ptr(replay_io_vectors[5]);
  if(hook_ids[i]==IO_AUDIO_MONITOR_COPY)in.r[7]=ptr(program_objects[2]); /* copied enum load uses native source P+2000 */
  if(anchor[i]==0x2374f88)in.r[2]=0; /* displaced indexed load uses a byte offset */
  if(hook_ids[i]==GL_AUTO_STATE)in.r[7]=ptr(program_objects[2]); /* displaced pair loads through r7 */
  if(hook_ids[i]==M_POINTER_DEVICE)in.r[6]=ptr(track_objects[0]); /* displaced pair loads cursor x/y through the cursor state object in r6 */
  if(hook_ids[i]==M_WHEEL_DATA)in.r[4]=ptr(track_objects[0]); /* displaced NEON pair loads the cursor x/y vector through r4 */
  if(hook_ids[i]>=M_WHEEL_FLUSH&&hook_ids[i]<=M_WHEEL_FLUSH_LAST){in.r[7]=ptr(track_objects[0]);in.r[4]=ptr(track_objects[0]);} /* displaced ldrsh reads +114 through r7 or r4 */
  /* Adjacent native hooks are legal. Install this fixture's one-pair
   * continuation immediately before its exercise, so another site's +8
   * continuation cannot overwrite the pair under test. */
  memcpy((void*)(uintptr_t)anchor[i],expected[i],8);uint32_t *continuation=(uint32_t*)(uintptr_t)(anchor[i]+8);jump(&continuation,ptr(capture_a));__builtin___clear_cache((char*)(uintptr_t)anchor[i],(char*)(uintptr_t)(anchor[i]+16));
  inputs[i]=in;exercise(&in,(void*)(uintptr_t)anchor[i],outputs+i);shifts[i]=captured_sp-saved_sp;
 }
 /* Restore guards overwritten by synthetic continuations, install exact path. */
 for(unsigned i=0;i<sizeof(guards)/sizeof(guards[0]);i++)memcpy((void*)(uintptr_t)guards[i].address,guards[i].bytes,guards[i].length);
 for(unsigned i=0;i<PATCH_COUNT;i++)memcpy((void*)(uintptr_t)anchor[i],expected[i],8);
 require(install_patches(0,-1,&gates),"complete model detour transaction");
 for(unsigned i=0;i<np;i++)require(!mprotect((void*)(uintptr_t)pages[i],4096,PROT_READ|PROT_WRITE|PROT_EXEC),"fixture continuation setup");
 for(unsigned i=0;i<PATCH_COUNT;i++){uint32_t *p=(uint32_t*)(uintptr_t)(anchor[i]+8);jump(&p,(uint32_t)(uintptr_t)capture_a);}
 for(unsigned i=0;i<np;i++)__builtin___clear_cache((char*)(uintptr_t)pages[i],(char*)(uintptr_t)(pages[i]+4096));
 for(unsigned i=0;i<PATCH_COUNT;i++){
  replay_memory(0);
  if(hook_ids[i]==PX_RETIRE)put(inputs[i].r[3]+0x34,ptr(replay_enqueuer));
  if(hook_ids[i]==QS_MODE_VALUE)put(inputs[i].r[0]+0x50,ptr(program_objects[1]));
  if(hook_ids[i]==GL_EDITOR_PUBLISH)put(inputs[i].r[4]+0x904,0); /* displaced pair loads then stores this slot */
  uint32_t *entry=(uint32_t*)(uintptr_t)anchor[i];jump(&entry,ptr((unsigned char*)gates+i*STUB_BYTES));uint32_t *continuation=(uint32_t*)(uintptr_t)(anchor[i]+8);jump(&continuation,ptr(capture_a));__builtin___clear_cache((char*)(uintptr_t)anchor[i],(char*)(uintptr_t)(anchor[i]+16));
  Context actual;exercise(inputs+i,(void*)(uintptr_t)anchor[i],&actual);
  if(memcmp(&actual,outputs+i,sizeof(actual))){fprintf(stderr,"replay site=%u\n",i);for(unsigned k=0;k<80;k++)if(((uint32_t*)&actual)[k]!=((uint32_t*)&outputs[i])[k])fprintf(stderr,"word%u %08x != %08x\n",k,((uint32_t*)&actual)[k],((uint32_t*)&outputs[i])[k]);}
  require(!memcmp(&actual,outputs+i,sizeof(actual))&&captured_sp-saved_sp==shifts[i],"GPR/APSR/FPSCR/d0-d31 and displaced ARM replay");
 }
 for(unsigned i=0;i<PATCH_COUNT;i++)if(capture_after_pair[i]){
  replay_memory(0);
  if(hook_ids[i]==PX_RETIRE)put(inputs[i].r[3]+0x34,ptr(replay_enqueuer));
  if(hook_ids[i]==QS_MODE_VALUE)put(inputs[i].r[0]+0x50,ptr(program_objects[1]));
  unsigned char *stub=(unsigned char*)gates+i*STUB_BYTES;
  require(!memcmp(stub,expected[i],8),"post-atomic DMB/store pair precedes gate memory traffic");
  require(!mprotect((void*)((uintptr_t)stub&~4095u),4096,PROT_READ|PROT_WRITE|PROT_EXEC),"probe handler protection");
  *(uint32_t*)(stub+8+(gate_handler-gate_begin))=ptr(probe_context);
  __builtin___clear_cache((char*)stub,(char*)stub+STUB_BYTES);
  uint32_t *entry=(uint32_t*)(uintptr_t)anchor[i];jump(&entry,ptr((unsigned char*)gates+i*STUB_BYTES));uint32_t *continuation=(uint32_t*)(uintptr_t)(anchor[i]+8);jump(&continuation,ptr(capture_a));__builtin___clear_cache((char*)(uintptr_t)anchor[i],(char*)(uintptr_t)(anchor[i]+16));
  Context actual;exercise(inputs+i,(void*)(uintptr_t)anchor[i],&actual);
  require(!memcmp(&probe_saved,outputs+i,sizeof(Context)),"post-pair GPR/flags observed before callback");
 }
 exclusive_retry_checks();
 printf("PASS exact ARM%u-site replay, post-DMB/store capture order and every partial install rollback (synthetic continuations)\n",PATCH_COUNT);
}
static MirrorState *fixture;
static void call(Context *c,unsigned site){observer_hook(c,MODEL_HOOK_BASE+site);}
static void recording_fallback_fixture(uint32_t,uint32_t,unsigned,unsigned,float);
static void initial(unsigned n){
 component_recording_dispatch=recording_fallback_fixture;
 mirror_state=fixture;memset(fixture,0,sizeof(*fixture));fixture->magic=MIRROR_MAGIC;fixture->version=MIRROR_VERSION;fixture->bytes=sizeof(*fixture);fixture->pid=getpid();fixture->capacity=MIRROR_CELLS;
 struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);fixture->origin_sec=now.tv_sec;fixture->origin_nsec=now.tv_nsec;atomic_store(&fixture->alive,1);
 count=retired_count=depth=root=project=pool=epoch=serial=binding=generation=owner=ready=reset_pending=loading=load_token=load_sp=returned=0;memset(tracks,0,sizeof(tracks));memset(&mutation,0,sizeof(mutation));memset(&creating,0,sizeof(creating));
 put(ptr(root_object)+0x318,ptr(recording_fixture_factory));put(ptr(root_object)+0x2e4,ptr(recording_fixture_audio));
 put(ptr(recording_fixture_factory),0x6899fa0);put(ptr(recording_fixture_factory)+4,ptr(queue_object));put(ptr(recording_fixture_factory)+0x10,ptr(queue_object));put(ptr(recording_fixture_factory)+0x14,ptr(project_object));put(ptr(recording_fixture_factory)+0x28,word(ptr(recording_fixture_audio)+0x3bc));put(ptr(recording_fixture_factory)+0x30,word(ptr(project_object)+0xab0));
 put(ptr(root_object)+0xf8,ptr(queue_object));put(ptr(queue_object),0x68aeaf0);put(ptr(queue_object)+4,0x68aeadc);put(ptr(project_object)+0xa90,ptr(program_pool));put(ptr(program_pool),0x68ad07c);put(ptr(program_pool)+0x1c,ptr(program_members));put(ptr(program_pool)+0x20,ptr(program_members)+4*n);put(ptr(program_pool)+0x24,ptr(program_members)+sizeof(program_members));
 put(ptr(root_object),0x68d7bf8);put(ptr(root_object)+0x24c,ptr(project_object));put(ptr(project_object),0x6933c4c);put(ptr(project_object)+0xa94,ptr(pool_object));put(ptr(pool_object),0x68ad2c4);put(ptr(pool_object)+0x34,ptr(members));put(ptr(pool_object)+0x38,ptr(members)+4*n);put(ptr(pool_object)+0x3c,ptr(members)+MIRROR_TRACKS*4);put(ptr(file_handler)+0x68,ptr(project_object));
 for(unsigned i=0;i<n;i++){program_members[i]=ptr(program_objects[i]);members[i]=ptr(track_objects[i]);put(members[i],0x6935554);put(members[i]+0x46c,0x69355e0);put(members[i]+0x648,ptr(program_objects[i]));put(members[i]+0x64c,0);put(ptr(program_objects[i]),0x6930c00);put(ptr(program_objects[i])+0x5d8,0x7fc00001);}
}
static void seed_fixture(unsigned index,int copy,uint32_t bits,int qualified){
 struct {Context c;uint32_t stack[96];} f={0};f.c=context();uint32_t p=ptr(program_objects[index]);f.c.r[4]=p+0x3c;
 if(!copy){f.c.r[5]=p+0x5b0;f.c.d[8]=bits;f.stack[0xb4/4]=0x237034c;f.stack[0x114/4]=0x23705f8;f.stack[0x13c/4]=qualified?0x2514c08:0x243727c;f.stack[0x12c/4]=p;}
 else{f.c.r[7]=p+0x5b0;f.c.r[3]=bits;f.stack[0x8c/4]=0x2370710;f.stack[0xf4/4]=0x2370a7c;f.stack[0x10c/4]=qualified?0x2514334:0x2435c28;f.stack[0xf8/4]=p;}
 call(&f.c,copy?MIRROR_SEED_COPY:MIRROR_SEED_NORMAL);
}
static void channel_fixture(unsigned n){
 struct {Context c;uint32_t stack[128];} f={0};f.c=context();
 if(!channel_owner_id(ptr(project_object))){f.c.r[0]=ptr(project_object);call(&f.c,CH_PROJECT_BIRTH);f.c.r[4]=ptr(project_object);f.c.d[16]=0;call(&f.c,CH_SELECTION_BIRTH);}
 for(unsigned i=0;i<n;i++){
  uint32_t t=ptr(track_objects[i]),p=ptr(program_objects[i]);
  if(channel_owner_id(t))continue;
  f.c.r[0]=t;call(&f.c,CH_TRACK_BIRTH);f.c.r[0]=p;call(&f.c,CH_PROGRAM_BIRTH);
  f.c.r[4]=p+0x3c;f.c.d[16]=0x3f000000;f.stack[0xb4/4]=0x237034c;f.stack[0x114/4]=0x23705f8;f.stack[0x13c/4]=0x2514c08;f.stack[0x12c/4]=p;call(&f.c,CH_PAN_BIRTH);
  for(unsigned send=0;send<4;send++){f.c.r[8]=p+0x428+0x38*send;f.c.r[7]=0;call(&f.c,CH_SEND_BIRTH);}
  f.c.r[4]=p+0x48;f.c.r[3]=f.c.r[10]=0;f.stack[0x8c/4]=0x236f090;f.stack[0x144/4]=0x237034c;f.stack[0x1a4/4]=0x23705f8;f.stack[0x1cc/4]=0x2514c08;f.stack[0x1bc/4]=p;
  call(&f.c,CH_MUTE_BIRTH);call(&f.c,CH_SOLO_BIRTH);call(&f.c,CH_SOLO_AUDIO_BIRTH);call(&f.c,CH_EFFECTIVE_BIRTH);
  put(t+0x468,ptr("Channel"));f.c.r[4]=t;call(&f.c,CH_NAME_BIRTH);f.c.r[3]=0xffdd4488;call(&f.c,CH_COLOR_BIRTH);
  f.c.r[4]=t+0x46c;f.c.r[3]=0;f.stack[0x6c/4]=0x2652458;call(&f.c,CH_ARM_BIRTH);put(t+0x48c,1);
 }
}
static void load_fixture(void){channel_fixture((word(ptr(pool_object)+0x38)-ptr(members))/4);Context c=context();c.r[0]=ptr(root_object);call(&c,M_RESET);c.r[2]=ptr(project_object);c.lr=0x17b7fb8;call(&c,M_LOAD_ENTER);c.r[0]=4;c.r[5]=ptr(file_handler);call(&c,M_LOAD_RETURN);call(&c,M_LOAD_READY);require(active()&&ready,"owner-qualified initial inventory");}

static unsigned dispatched_calls,defer_audio,queue_foreign;
static uint32_t fake_program,fake_bits,fake_controller=7,fake_field;
static void *fake_audio(void *unused){
 (void)unused;uint32_t tuple[5]={fake_program+0x40,fake_program,0x101,fake_controller,fake_bits};
 uint32_t other[5]={fake_program+0x40,fake_program,0x101,fake_controller,fake_bits^1u};
 union {uint64_t align;unsigned char bytes[sizeof(Context)+8];} stack;
 Context *entry=(Context*)(stack.bytes+8),*body=(Context*)stack.bytes;
 *entry=context();entry->r[0]=ptr(tuple);call(entry,M_QUEUE_ENTER);
 /* The same field moved from the touchscreen beside the surface: another
  * writer's queue item, body and completion inside this request's window. */
 if(queue_foreign){
  Context f=context();f.r[0]=ptr(other);call(&f,M_QUEUE_ENTER);
  f=context();f.lr=0x25178d4;f.r[0]=fake_program;f.r[1]=0x101;f.r[2]=fake_controller;f.d[0]=fake_bits^1u;call(&f,M_COMMAND_BODY);
  f=context();f.lr=0x25178d4;f.r[0]=fake_program;f.r[1]=0x101;f.r[2]=fake_controller+1;f.d[0]=fake_bits;call(&f,M_COMMAND_BODY);
  f=context();f.r[4]=ptr(other);f.r[3]=other[0];call(&f,M_QUEUE_DONE);
  require(!atomic_load(&command_state->trace_error),"another writer's queue item, body and completion inside this request's window must not poison it");
 }
 *body=context();body->lr=0x25178d4;body->r[0]=fake_program;body->r[1]=0x101;body->r[2]=fake_controller;body->d[0]=fake_bits;call(body,M_COMMAND_BODY);
 Context scalar=context();scalar.r[5]=fake_program+channel_offset(command_source_field(fake_field));scalar.d[8]=fake_bits;scalar.r[7]=accepted[0].request.bits;call(&scalar,fake_field==CF_MUTE?M_MUTE:fake_field==CF_SOLO?CH_BOOL_COMMIT:M_FLOAT);
 *body=context();body->r[4]=ptr(tuple);body->r[3]=tuple[0];call(body,M_QUEUE_DONE);return NULL;
}
static void fake_dispatch(uint32_t p,uint32_t kind,uint32_t controller,float value){
 dispatched_calls++;
 if(kind!=0x101){Context c=context();require(kind==CF_SELECTION||kind==CF_ARM,"owner command ABI");c.r[5]=p+channel_offset(kind);c.r[7]=controller;call(&c,kind==CF_SELECTION?CH_SELECTION_COMMIT:M_BOOL);return;}
 require(controller==command_controller(accepted[0].request.reserved)&&p==ptr(program_objects[0]),"typed hard-float request ABI reaches substituted target");fake_program=p;fake_controller=controller;fake_field=accepted[0].request.reserved;memcpy(&fake_bits,&value,4);
 if(defer_audio)return;
 pthread_t t;require(!pthread_create(&t,NULL,fake_audio,NULL)&&!pthread_join(t,NULL),"separate source execution thread");
 if(fake_field==CF_SOLO){Context c=context();c.r[5]=p+0x88;c.r[7]=accepted[0].request.bits;call(&c,M_BOOL);}
}
static void recording_fallback_fixture(uint32_t f,uint32_t p,unsigned kind,unsigned controller,float value){
 require(f==ptr(recording_fixture_factory)&&kind==0x101,"GUI facade keeps rooted CommandFactory identity");fake_dispatch(p,0x101,controller,value);
}
static CommandRequest request(uint32_t bits){
 MirrorCell *cell=lookup(ptr(program_objects[0])+0x5b0);CommandRequest r={0,command_state->pid,command_state->start_lo,command_state->start_hi,command_state->origin_sec,command_state->origin_nsec,epoch,tracks[0].serial,tracks[0].binding,tracks[0].incarnation,bits,0,120000,atomic_load(&cell->bits),atomic_load(&cell->revision),0,tracks[0].track_owner,tracks[0].program_owner,0,channel_owner_id(project),0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};return r;
}
static unsigned submit_fixture(CommandRequest r){
 unsigned at=command_free(command_state);r.seq=atomic_load(&command_state->published)+1;
 atomic_store(&command_state->alive,1);require(command_publish_request_at(command_state,&r,at),"actual fixture publication");
 Context c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);return at;
}
/* Component-only deterministic scheduling and final-call substitution probes. */
static _Atomic unsigned pause_at,pause_reached,pause_resume;
static unsigned probe_operation,probe_inject;
static void *foreign_required(void *unused){(void)unused;Context c=context();call(&c,M_RESET);return NULL;}
static void component_pause(unsigned at){
 if(at==5&&probe_inject){
  if(probe_inject==1){pthread_t t;require(!pthread_create(&t,NULL,foreign_required,NULL)&&!pthread_join(t,NULL),"foreign violation before BLX");}
  else if(probe_inject==3){uint32_t m=word(root+0x2e4)+0x260,offset=tracks[0].kind==7?0x78:tracks[0].kind==8?0x84:0x90;put(m+offset+4,word(m+offset));}
  else if(probe_inject==4)channel_owner_death(word(root+0x2e4)+0x260);
  else put(0x6930c28,0x250ba78);
  return;
 }
 if(at!=atomic_load(&pause_at))return;
 if(at==20&&atomic_exchange_explicit(&pause_reached,1,memory_order_acq_rel))return;
 atomic_store_explicit(&pause_reached,1,memory_order_release);
 while(!atomic_load_explicit(&pause_resume,memory_order_acquire)){struct timespec wait={0,1000000};nanosleep(&wait,NULL);}
}
static unsigned probe_pin_constructor_owner;
static void *paused_source(void *unused){
 (void)unused;
 /* Fixture setup pins the constructor thread before the existing close pause;
  * unlike scalar callbacks, supported Program births cannot use a foreign owner. */
 if(probe_pin_constructor_owner&&atomic_load(&observer_running))command_initialize();
 if(probe_operation==1||probe_operation==2)seed_fixture(0,probe_operation==2,0x3f100000,1);
 else{Context c=context();c.r[5]=c.r[0]=ptr(program_objects[0])+0x5b0;c.d[8]=0x3f100000;call(&c,probe_operation==3?M_FLOAT:MIRROR_PROPERTY_DESTROY);}
 return NULL;
}
static unsigned external_operation,external_result;
static CommandRequest external_request;
static void *paused_external(void *unused){
 (void)unused;external_result=external_operation==2?command_request_stop(command_state):external_operation?command_publish_settlement(command_state,1):command_publish_request(command_state,&external_request);return NULL;
}
static void external_close_checks(void){
 MirrorState *saved_fixture=fixture;CommandState *saved_commands=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"external close fixtures");
 /* Both sides of admission, then the three real publication boundaries. */
 for(unsigned test=0;test<7;test++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;atomic_store(&command_state->alive,1);
  external_operation=test==4||test==6;external_result=0;external_request=(CommandRequest){.seq=1,.bits=0x3f200000};
  if(external_operation){
   command_request_store(command_state->slots,&external_request);atomic_store(&command_state->published,1);atomic_store(&command_state->slots[0].published,1);atomic_store(&command_state->consumed,1);
   CommandSlot *slot=command_state->slots;atomic_store(&slot->processed,1);atomic_store(&slot->returned,1);atomic_store(&slot->done,1);atomic_store(&slot->sealed,1);
  }
  unsigned pause=test==0||test>=5?8:test==1?12:test==2?9:test==3?10:11;
  atomic_store(&pause_at,pause);atomic_store(&pause_reached,0);atomic_store(&pause_resume,0);
  pthread_t writer;require(!pthread_create(&writer,NULL,paused_external,NULL),"paused real external helper");
  while(!atomic_load_explicit(&pause_reached,memory_order_acquire)){struct timespec delay={0,1000000};nanosleep(&delay,NULL);}
  CommandState *before=malloc(sizeof(*before));require(before!=NULL,"closed external snapshot");
  command_close();
  if(pause==8){
   require(command_finalize(),"stop permanently shuts gate before admission");memcpy(before,command_state,sizeof(*before));
  }else require(!command_finalize()&&!atomic_load(&command_state->closed),"close cannot finalize an admitted external publication");
  atomic_store_explicit(&pause_resume,1,memory_order_release);require(!pthread_join(writer,NULL),"external writer releases flight");
  if(pause==8){
   require(!external_result&&!memcmp(before,command_state,sizeof(*before)),"late writer cannot mutate closed payload/frontier/settlement or admission metadata");
  }else{
   require(command_finalize()&&atomic_load(&command_state->external_gate)==2,"close finalizes after admitted writer leaves");
   if(test==1)require(!external_result&&!atomic_load(&command_state->published)&&!atomic_load(&command_state->error),"admitted writer rechecks stop before payload");
   else if(test==4)require(external_result&&atomic_load(&command_state->slots[0].settled)==1&&atomic_load(&command_state->error)==C_CLOSED,"admitted settlement finishes before close; unreclaimed slot stays incomplete");
   else require(external_result&&atomic_load(&command_state->published)==1&&!atomic_load(&command_state->consumed)&&atomic_load(&command_state->error)==C_CLOSED,"completed publication stranded by stop is explicitly incomplete");
   memcpy(before,command_state,sizeof(*before));
  }
  atomic_store(&pause_at,0);require(!command_publish_request(command_state,&external_request)&&!command_publish_settlement(command_state,1)&&!memcmp(before,command_state,sizeof(*before)),"every shared byte immutable after closed for both clients' helpers");
  free(before);
 }
 /* Writer wins completely, then normal finite stop detects unconsumed work. */
 memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;atomic_store(&command_state->alive,1);
 require(command_publish_request(command_state,&external_request),"external publication before stop");command_close();require(command_finalize()&&atomic_load(&command_state->error)==C_CLOSED,"publication-before-stop cannot masquerade as successful consumption");
 free(fixture);free(command_state);fixture=saved_fixture;mirror_state=fixture;command_state=saved_commands;
 puts("PASS actual CMD13 external helpers: paused admission/payload/frontier/settled vs finite close; stop recheck, admitted work drain, explicit stranded error and byte-immutable closed state");
}
/* USB mouse support. The enable is a native call into the app's own setter, so
 * the recipe, the called function and the host handler are checked against the
 * exact image bytes the installer verifies; the substituted seam then exercises
 * the observer's actual decision. The native setter body is NOT executed here. */
static unsigned pointer_enable_calls,pointer_enable_argument;
static void pointer_enable_fixture(unsigned on){pointer_enable_calls++;pointer_enable_argument=on;}
static const unsigned char *guarded_bytes(uint32_t address,unsigned length){
 for(unsigned g=0;g<sizeof(guards)/sizeof(guards[0]);g++)
  if(guards[g].address<=address&&address+length<=guards[g].address+guards[g].length)return guards[g].bytes+address-guards[g].address;
 return NULL;
}
/* An `ldr rN,[pc,#..]` literal completed by its `add rN,pc,rN`: the address the
 * application itself forms, decoded out of the image rather than restated. */
static uint32_t key_image_word(const unsigned char *body,uint32_t base,uint32_t literal,uint32_t add,uint32_t offset){
 uint32_t value;memcpy(&value,body+(literal-base),4);
 return add+8+value+offset;
}
static void pointer_device_checks(void){
 unsigned site=0;while(site<PATCH_COUNT&&anchor[site]!=0xba6884)site++;
 require(site<PATCH_COUNT&&hook_ids[site]==M_POINTER_DEVICE,"peer pointer-device join is an installed site with its own hook id");
 const unsigned char join[8]={0x30,0x10,0x96,0xe5,0x34,0x20,0x96,0xe5};
 require(!memcmp(expected[site],join,8),"displaced pair is the exact cursor x/y load pair");
 require(!capture_after_pair[site]&&!relocated[site][0]&&!relocated[site][1]&&!branch_target[site][0]&&!branch_target[site][1],"plain displaced pair with no literal or branch word");
 /* One hook body at four drain-loop anchors: consecutive ids, same recipe
  * shape, and both host wrapper functions guarded whole. */
 const uint32_t flush_anchors[4]={0xa0a0d0,0xa0d9f0,0xa09c90,0xa0d5e4};
 const unsigned char flush_pair[2][8]={{0xf2,0x37,0xd7,0xe1,0x00,0x00,0x53,0xe3},{0xf2,0x37,0xd4,0xe1,0x00,0x00,0x53,0xe3}};
 for(unsigned f=0;f<4;f++){
  unsigned at=0;while(at<PATCH_COUNT&&anchor[at]!=flush_anchors[f])at++;
  require(at<PATCH_COUNT,"each drain-loop end is an installed site");
  require(hook_ids[at]>=M_WHEEL_FLUSH&&hook_ids[at]<=M_WHEEL_FLUSH_LAST,"and carries a hook id the flush body answers");
  require(!memcmp(expected[at],flush_pair[f&1],8),"displaced pair is the exact signed half-word load and compare");
  require(!capture_after_pair[at]&&!relocated[at][0]&&!relocated[at][1]&&!branch_target[at][0]&&!branch_target[at][1],"plain displaced pair with no literal or branch word");
 }
 require(guarded_bytes(0xa09c24,0xab4)&&guarded_bytes(0xa0d50c,0xa78),"both libinput drain wrappers guarded as whole native functions");
 const unsigned char *setter=guarded_bytes(0xba6658,0x128),*handler=guarded_bytes(0xba6780,0x308);
 require(setter&&handler,"called setter and host handler guarded as whole native functions");
 const uint32_t enable_store=0xe5c04038,setter_tail=0xea028663,handler_tail=0xea0285e1;
 require(!memcmp(setter+(0xba6690-0xba6658),&enable_store,4),"setter still stores the enable byte at cursor object+0x38");
 require(!memcmp(setter+(0xba66a0-0xba6658),&setter_tail,4),"setter still tail-calls the cursor show/move routine");
 require(!memcmp(handler+(0xba68a8-0xba6780),&handler_tail,4),"host handler reaches that same routine itself right after the join");
 /* The cursor slot the hook checks is the one the host handler itself loads:
  * decode its pc-relative literal rather than restating the address. */
 const uint32_t table_load=0xe59f8288,table_add=0xe08f8008,cursor_load=0xe598602c;
 require(!memcmp(handler+(0xba67e0-0xba6780),&table_load,4)&&!memcmp(handler+(0xba67e4-0xba6780),&table_add,4),"host handler still forms the peer static table from one pc-relative literal");
 require(!memcmp(handler+(0xba67e8-0xba6780),&cursor_load,4),"it still takes the cursor state object from that table +0x2c");
 uint32_t table_literal;memcpy(&table_literal,handler+(0xba6a70-0xba6780),4);
 require(0xba67ecu+table_literal+0x2cu==POINTER_STATE_RVA,"the handler's own table literal names the exact slot the hook checks");
 static unsigned char cursor[0x3c] __attribute__((aligned(8)));
 static uint32_t devices[2];
 void *peer_page=mmap((void*)0x6b22000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 require(peer_page!=MAP_FAILED,"peer static table fixture page");
 memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 component_pointer_enable=pointer_enable_fixture;pointer_enable_calls=0;pointer_enable_argument=0;atomic_store(&pointer_enabled,0);
 memset(cursor,0,sizeof cursor);
 Context c=context();c.r[6]=ptr(cursor);
 put(0x6b220d4,0);call(&c,M_POINTER_DEVICE);
 require(!pointer_enable_calls,"no enable before the peer owns a cursor state object");
 put(0x6b220d4,ptr(cursor)+4);call(&c,M_POINTER_DEVICE);
 require(!pointer_enable_calls,"no enable for a receiver the peer's own slot does not name");
 put(0x6b220d4,ptr(cursor));put(ptr(cursor)+0x24,ptr(devices));put(ptr(cursor)+0x28,ptr(devices));
 call(&c,M_POINTER_DEVICE);
 require(!pointer_enable_calls,"no enable while no pointer device is present");
 devices[0]=0x1000;put(ptr(cursor)+0x28,ptr(devices)+4);
 uint32_t owner=command_owner;command_owner=owner+1;call(&c,M_POINTER_DEVICE);command_owner=owner;
 require(!pointer_enable_calls,"no enable off the thread that owns the observer's command work");
 call(&c,M_POINTER_DEVICE);
 require(pointer_enable_calls==1&&pointer_enable_argument==1,"present pointer device enables once through the app's own setter");
 devices[1]=0x2000;put(ptr(cursor)+0x28,ptr(devices)+8);call(&c,M_POINTER_DEVICE);
 put(ptr(cursor)+0x28,ptr(devices));call(&c,M_POINTER_DEVICE);
 require(pointer_enable_calls==1,"further device add and remove events never call the setter again");
 require(!atomic_load(&command_violation)&&!atomic_load(&command_state->error)&&!atomic_load(&mirror_state->error)&&atomic_load(&observer_running),"product hook enrolls no lane and publishes no diagnostic failure");
 component_pointer_enable=NULL;
 puts("PASS pointer enable recipe matches the exact image, and the observer calls the app's own setter once, only with a live peer cursor object, a present pointer device and the owning thread (native setter body substituted)");
}
/* USB mouse wheel -> data wheel. The recipe, the host scroll handler that
 * supplies the delta and the called DataWheelUp/DataWheelDown pair are checked
 * against the exact image bytes the installer verifies; the observer's own
 * decision is then exercised with the app's active-controller slot as a
 * fixture. The native dispatch bodies are NOT executed here. */
static uint32_t wheel_functions[8],wheel_receivers[8],wheel_coarse[8];
static int wheel_deltas[8];
static uint32_t wheel_focus_receiver,wheel_focus_focused;
static unsigned wheel_calls;
static void wheel_call_fixture(uint32_t function,uint32_t receiver,int delta,unsigned coarse){
 if(wheel_calls<8){wheel_functions[wheel_calls]=function;wheel_receivers[wheel_calls]=receiver;wheel_deltas[wheel_calls]=delta;wheel_coarse[wheel_calls]=coarse;}
 wheel_calls++;
}
static void wheel_put_float(uint32_t address,float v){uint32_t bits;memcpy(&bits,&v,sizeof bits);put(address,bits);}
/* Resolved target of an ARM BL, or 0 if the word is not one. */
static uint32_t wheel_branch(uint32_t pc,uint32_t instruction){
 if((instruction&0xff000000u)!=0xeb000000u)return 0;
 int32_t displacement=(int32_t)(instruction<<8)>>6;
 return pc+8+(uint32_t)displacement;
}
/* Exactly one call, to the rotation entry, on the fixture's receiver, with this
 * signed count and coarse 0. The count is what the first device build got
 * wrong, so nothing here accepts a repeated call. */
static unsigned wheel_one(int delta){
 return wheel_calls==1&&wheel_functions[0]==0x35c28f4u&&wheel_receivers[0]==wheel_focus_receiver&&wheel_deltas[0]==delta&&wheel_coarse[0]==0;
}
/* Publish one JOG_DATA request, let the app's own UI drain service it, then
 * seal, settle and reclaim it so the mailbox is idle for the next case. Returns
 * the step the observer says it actually delivered. */
static int wheel_command_case(int delta){
 CommandRequest r={.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.bits=(uint32_t)delta,.expires=120000,.reserved=JOG_DATA};
 wheel_calls=0;unsigned at=submit_fixture(r),seq=atomic_load(&command_state->published);
 CommandSlot *slot=command_state->slots+at;
 require(!atomic_load(&slot->rejected)&&atomic_load(&slot->done)==seq&&atomic_load(&slot->returned)==seq&&!atomic_load(&command_state->trace_error),"an accepted data-wheel request completes synchronously in the UI drain");
 int delivered=0;unsigned parts[3]={0,0,0};
 for(unsigned lane=0;lane<COMMAND_LANES;lane++)for(unsigned j=0;j<atomic_load(&slot->lanes[lane].published);j++){
  CommandEvent e;require(command_event_read(slot->lanes+lane,seq,j,&e),"data-wheel immutable source event");
  require(e.reserved==JOG_DATA&&e.controller==JOG_DATA&&!e.arg_kind&&!e.program&&!e.property&&!e.incarnation&&!e.capture&&!e.counter,"data-wheel evidence names no Track, Program, Property or native queue");
  /* The exact tuple the bridge's input_events and input_wheel_proof demand. */
  require(e.request==seq&&e.revision==epoch&&e.token==command_state->owner_token&&(e.kind==CE_WHEEL_STEP||e.bits==r.bits),"data-wheel evidence carries the flight, the submission epoch, the UI owner token and the published count");
  if(e.kind==CE_DISPATCH)parts[0]++;
  else if(e.kind==CE_WHEEL_STEP){parts[1]++;delivered=(int32_t)e.bits;}
  else if(e.kind==CE_RETURN)parts[2]++;
  else require(0,"a data-wheel flight publishes no jog, queue or commit receipt");
 }
 require(parts[0]==1&&parts[1]==1&&parts[2]==1,"exactly one dispatch, one delivered step and one return per accepted request");
 command_retire();require(atomic_load(&slot->sealed)==seq&&command_publish_settlement(command_state,seq),"data-wheel request seals after its synchronous return");
 command_retire();require(command_idle(command_state),"settled data-wheel request reclaims on the same protocol");
 return delivered;
}
static void wheel_data_checks(void){
 unsigned site=0;while(site<PATCH_COUNT&&anchor[site]!=0xbd20e0)site++;
 require(site<PATCH_COUNT&&hook_ids[site]==M_WHEEL_DATA,"peer SCROLL_WHEEL handler is an installed site with its own hook id");
 const unsigned char scroll[8]={0x8f,0x07,0x64,0xf4,0x10,0x30,0x8d,0xe2};
 require(!memcmp(expected[site],scroll,8),"displaced pair is the exact cursor vector load and stack add");
 require(!capture_after_pair[site]&&!relocated[site][0]&&!relocated[site][1]&&!branch_target[site][0]&&!branch_target[site][1],"plain displaced pair with no literal or branch word");
 const unsigned char *handler=guarded_bytes(0xbd1e2c,0x3d4),*dispatch=guarded_bytes(0x35c28f4,0xe8),*accessor=guarded_bytes(0x2b64174,0x10),*dispatcher=guarded_bytes(0xbd2200,0x188);
 require(handler&&dispatch&&accessor&&dispatcher,"folded scroll-handler range, the peer event switch, the data-wheel rotation entry and the focus accessor guarded as whole native ranges");
 /* The mouse's own event type is 404 SCROLL_WHEEL. The switch at bd2224 is a
  * pc-relative table: it reaches the wheel handler bd2008 through bd2300 and
  * the finger/continuous handler bd1e2c through bd229c. The site must sit in
  * the first, which a touchpad-only range would never see. */
 const uint32_t table_jump=0x908ff103,wheel_arm=0xea00002d,wheel_tail=0xeaffff3c,finger_arm=0xea000013;
 require(!memcmp(dispatcher+(0xbd222c-0xbd2200),&table_jump,4),"the switch still indexes its arms from bd2234, so type 404 is the fifth arm");
 require(!memcmp(dispatcher+(0xbd2244-0xbd2200),&wheel_arm,4),"event switch still sends libinput type 404 to the SCROLL_WHEEL arm");
 require(!memcmp(dispatcher+(0xbd2310-0xbd2200),&wheel_tail,4),"that arm still tail-calls the handler holding this site");
 require(!memcmp(dispatcher+(0xbd2248-0xbd2200),&finger_arm,4),"types 405 and 406 still go to the separate finger/continuous handler");
 const uint32_t enable_load=0xe5d43038,details_base=0xe28d6028,scale_load=0xeddf0b47,scale_multiply=0xee200b20,delta_y=0xed8d0a0b,juce_delivery=0xebfffd20;
 require(!memcmp(handler+(0xbd2044-0xbd1e2c),&enable_load,4),"wheel handler still reads the same cursor-state enable byte at +0x38");
 require(!memcmp(handler+(0xbd2098-0xbd1e2c),&details_base,4),"r6 at the site is still the wheel details the handler builds at sp+40");
 require(!memcmp(handler+(0xbd20c4-0xbd1e2c),&scale_load,4)&&!memcmp(handler+(0xbd20d0-0xbd1e2c),&scale_multiply,4),"vertical v120 result is still scaled by the pc-relative double literal");
 require(!memcmp(handler+(0xbd20d8-0xbd1e2c),&delta_y,4),"the scaled vertical delta is still stored at the wheel details +4");
 require(!memcmp(handler+(0xbd21c8-0xbd1e2c),&juce_delivery,4),"the app's own JUCE wheel delivery still follows the site");
 /* WHEEL_NOTCH is derived from the image, not asserted against it: both scroll
  * reads must be the v120 API (whose detent is 120 units, unlike the plain
  * get_scroll_value the finger handler uses), and the vertical scale literal
  * supplies the divisor. */
 uint32_t axis[2];memcpy(axis,handler+(0xbd20a4-0xbd1e2c),4);memcpy(axis+1,handler+(0xbd20c0-0xbd1e2c),4);
 uint32_t v120=wheel_branch(0xbd20a4,axis[0]),other=wheel_branch(0xbd20c0,axis[1]);
 require(v120&&v120==other,"both scroll reads still call one and the same import");
 uint32_t plain[1];memcpy(plain,handler+(0xbd1ec8-0xbd1e2c),4);
 require(v120!=wheel_branch(0xbd1ec8,plain[0]),"that import is not the plain get_scroll_value the finger handler calls, so it is the v120 API");
 double horizontal,vertical;
 memcpy(&horizontal,handler+(0xbd21e0-0xbd1e2c),8);memcpy(&vertical,handler+(0xbd21e8-0xbd1e2c),8);
 require(horizontal==1.0/512.0&&vertical==-1.0/512.0,"the handler still scales v120 units by 1/512, negating the vertical axis");
 require(WHEEL_NOTCH==(float)(120.0*-vertical),"WHEEL_NOTCH is one 120-unit v120 detent through the image's own scale");
 /* The slot the hook reads is the one the app's own accessor loads: decode its
  * pc-relative literal rather than restating the address. */
 uint32_t accessor_words[4];memcpy(accessor_words,accessor,16);
 require(accessor_words[0]==0xe59f3004u&&accessor_words[1]==0xe79f0003u&&accessor_words[2]==0xe12fff1eu,"focus accessor is still ldr/ldr/bx over one pc-relative literal");
 require(0x2b64180u+accessor_words[3]==FOCUS_SLOT_RVA,"the accessor still names the exact slot the hook reads");
 uint32_t focused_load=0xe59000e4,rotate_cast=0xeb08df1d,rotate_slot=0xe5933028,pass_coarse=0xe1a02006,pass_delta=0xe1a01005,tail=0xe12fff13;
 require(!memcmp(dispatch+(0x35c28f4-0x35c28f4),&focused_load,4),"ProcessDataWheelRotation still starts from the controller's own focused component at +0xe4");
 require(!memcmp(dispatch+(0x35c2944-0x35c28f4),&rotate_cast,4),"it still casts that component with the app's __dynamic_cast");
 require(!memcmp(dispatch+(0x35c295c-0x35c28f4),&rotate_slot,4),"it still reads HWFocusable slot 10, the rotation slot that takes a count");
 require(!memcmp(dispatch+(0x35c297c-0x35c28f4),&pass_coarse,4)&&!memcmp(dispatch+(0x35c2980-0x35c28f4),&pass_delta,4),"it still forwards this hook's delta in r1 and coarse in r2");
 require(!memcmp(dispatch+(0x35c2998-0x35c28f4),&tail,4),"it still tail-calls that slot");
 /* The visible flag the hook tests is the one the hardware path's own liveness
  * walk tests, read out of that walk rather than restated. */
 const unsigned char *liveness=guarded_bytes(0xb74904,0x54);
 require(liveness!=NULL,"hardware liveness walk guarded as a native range");
 const uint32_t parent_load=0xe590300c,flags_load=0xe5d02068,flag_test=0xe3120002,climb=0x1afffff8;
 require(!memcmp(liveness+(0xb7491c-0xb74904),&parent_load,4),"the walk still climbs juce::Component parents at +0xc");
 uint32_t flags_word;memcpy(&flags_word,liveness+(0xb7492c-0xb74904),4);
 require(flags_word==flags_load&&(flags_word&0xfff)==WHEEL_FLAGS_OFFSET,"the walk still reads the component flags byte at the offset the hook reads");
 uint32_t test_word;memcpy(&test_word,liveness+(0xb74930-0xb74904),4);
 require(test_word==flag_test&&(test_word&0xff)==WHEEL_VISIBLE_FLAG,"the walk still requires the flag bit the hook requires");
 require(!memcmp(liveness+(0xb74934-0xb74904),&climb,4),"a set bit still means keep walking, a clear bit still means not showing");
 /* The four legal vptrs are pinned where they live, in the RW image, by
  * model-prepare.py at recipe time: their runtime words carry the load bias,
  * so a byte guard over them would fail at install and cannot be checked here. */
 static unsigned char details[16] __attribute__((aligned(8)));
 static unsigned char controller[0x100] __attribute__((aligned(8)));
 static unsigned char focused[0x80] __attribute__((aligned(8)));
 void *focus_page=mmap((void*)0x6b6f000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 require(focus_page!=MAP_FAILED,"active focus controller slot fixture page");
 memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 command_state->magic=COMMAND_MAGIC;command_state->version=COMMAND_VERSION;command_state->bytes=sizeof(*command_state);command_state->capacity=COMMAND_SLOTS;
 component_wheel_call=wheel_call_fixture;wheel_calls=0;wheel_remainder=0.0f;wheel_pending=0;
 wheel_focus_receiver=ptr(controller);
 memset(controller,0,sizeof controller);memset(focused,0,sizeof focused);
 put(wheel_focus_receiver+WHEEL_FOCUSED_OFFSET,ptr(focused));
 put(ptr(focused)+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG);
 put(0x6b6f798,wheel_focus_receiver);put(wheel_focus_receiver,wheel_focus_vtables[2]);
 Context c=context();c.r[6]=ptr(details);
 /* One turn of the wheel is one event plus one drain end. */
#define WHEEL_EVENT(delta) do{wheel_put_float(ptr(details),0.0f);wheel_put_float(ptr(details)+4,(delta));call(&c,M_WHEEL_DATA);}while(0)
#define WHEEL_DRAIN() call(&c,M_WHEEL_FLUSH)
 uint32_t owner=command_owner;command_owner=owner+1;
 wheel_put_float(ptr(details)+4,WHEEL_NOTCH);call(&c,M_WHEEL_DATA);WHEEL_DRAIN();
 command_owner=owner;
 require(!wheel_calls&&!wheel_pending,"no accumulation and no step off the thread that owns the observer's command work");
 require(word(ptr(details)+4)!=0,"and the app's own wheel delivery is left alone there");
 Context bad=c;bad.r[6]=1;call(&bad,M_WHEEL_DATA);WHEEL_DRAIN();
 require(!wheel_calls,"no step for a non-pointer wheel-details register");
 /* The JUCE delivery that follows the site must carry a zero wheel, so a list
  * moves its selection without also scrolling its view. */
 WHEEL_EVENT(WHEEL_NOTCH);
 require(!word(ptr(details))&&!word(ptr(details)+4),"the hook zeroes both deltas so the app's own wheel delivery is a no-op");
 require(!wheel_calls&&wheel_pending==1,"the event banks its notch and calls nothing");
 WHEEL_DRAIN();
 require(wheel_one(1)&&!wheel_pending,"the drain end delivers one call with the banked count");
 wheel_calls=0;
 WHEEL_DRAIN();
 require(!wheel_calls,"a drain end with nothing banked calls nothing");
 /* Two events inside one drain are one call carrying two. */
 WHEEL_EVENT(WHEEL_NOTCH);WHEEL_EVENT(WHEEL_NOTCH);
 require(!wheel_calls&&wheel_pending==2,"events inside one drain bank without calling");
 WHEEL_DRAIN();
 require(wheel_one(2),"two events in one drain are exactly one call with a count of two");
 wheel_calls=0;
 WHEEL_EVENT(-WHEEL_NOTCH);WHEEL_DRAIN();
 require(wheel_one(-1),"one notch down is one call with a negative count");
 wheel_calls=0;
 put(wheel_focus_receiver+WHEEL_FOCUSED_OFFSET,0);
 WHEEL_EVENT(WHEEL_NOTCH);WHEEL_DRAIN();
 require(!wheel_calls,"no step while the focus controller has nothing focused");
 put(wheel_focus_receiver+WHEEL_FOCUSED_OFFSET,ptr(focused));
 put(ptr(focused)+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG^0xffffffffu);
 WHEEL_EVENT(WHEEL_NOTCH);WHEEL_DRAIN();
 require(!wheel_calls,"no step for a focused component without the visible flag the app's own liveness walk requires");
 put(ptr(focused)+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG);
 put(0x6b6f798,0);WHEEL_EVENT(WHEEL_NOTCH);WHEEL_DRAIN();
 require(!wheel_calls,"no step while the app has no active focus controller");
 put(0x6b6f798,1);WHEEL_EVENT(WHEEL_NOTCH);WHEEL_DRAIN();
 require(!wheel_calls,"no step for a non-pointer focus controller slot");
 put(0x6b6f798,wheel_focus_receiver);
 put(wheel_focus_receiver,wheel_focus_vtables[3]+4);
 WHEEL_EVENT(WHEEL_NOTCH);WHEEL_DRAIN();
 require(!wheel_calls,"no step for a receiver whose vptr is not one of the four focus controllers");
 require(!wheel_pending,"a rejected flush still clears what it banked");
 wheel_remainder=0.0f;
 for(unsigned v=0;v<4;v++){
  put(wheel_focus_receiver,wheel_focus_vtables[v]);
  WHEEL_EVENT(WHEEL_NOTCH);WHEEL_DRAIN();
  require(wheel_one(1),"each of the four legal focus-controller vptrs takes one call with a count of one and coarse 0");
  wheel_calls=0;
 }
 require(wheel_remainder==0.0f,"whole notches leave no remainder");
 WHEEL_EVENT(WHEEL_NOTCH*3.0f);
 wheel_put_float(ptr(details),WHEEL_NOTCH*3.0f);wheel_put_float(ptr(details)+4,0.0f);call(&c,M_WHEEL_DATA);
 require(!word(ptr(details)),"a horizontal-only event is zeroed too");
 WHEEL_DRAIN();
 require(wheel_one(3),"horizontal scroll alone adds nothing to the count");
 wheel_calls=0;
 WHEEL_EVENT(WHEEL_NOTCH*0.5f);WHEEL_DRAIN();
 require(!wheel_calls&&wheel_remainder>0.0f,"a sub-notch delta moves nothing and keeps its remainder");
 WHEEL_EVENT(WHEEL_NOTCH*0.5f);WHEEL_DRAIN();
 require(wheel_one(1)&&wheel_remainder==0.0f,"two sub-notch deltas across two drains complete one notch");
 wheel_calls=0;
 WHEEL_EVENT(WHEEL_NOTCH*6.0f);WHEEL_DRAIN();
 require(wheel_one(WHEEL_MAX_STEPS),"six notches in one event are clamped to one call of four");
 require(wheel_remainder==0.0f,"clamping drops the excess whole notches instead of carrying them");
 wheel_calls=0;
 for(unsigned e=0;e<6;e++)WHEEL_EVENT(WHEEL_NOTCH*3.0f);
 require(wheel_pending==WHEEL_MAX_PENDING,"a long spin inside one drain saturates the counter instead of overflowing");
 WHEEL_DRAIN();
 require(wheel_one(WHEEL_MAX_STEPS),"and still delivers one clamped call");
 wheel_calls=0;
 uint32_t rejected[3]={0x7fc00000u,0x7f800000u,0};
 for(unsigned i=0;i<3;i++){wheel_put_float(ptr(details),0.0f);put(ptr(details)+4,rejected[i]);call(&c,M_WHEEL_DATA);WHEEL_DRAIN();}
 require(!wheel_calls&&wheel_remainder==0.0f&&!wheel_pending,"NaN, infinity and a zero delta move nothing and never poison the accumulator");
 /* A delta this large would overflow the notch quotient, which is undefined and
  * saturates on VFP, leaving a remainder of millions of notches that would bank
  * the maximum on every later event. It must be refused outright. */
 WHEEL_EVENT(6.0e8f);
 require(!wheel_calls&&!wheel_pending&&wheel_remainder==0.0f,"a delta past the sanity bound is refused and leaves the accumulator untouched");
 WHEEL_DRAIN();
 require(!wheel_calls,"and banks nothing for a later drain to deliver");
 /* The largest accepted delta still leaves a sub-notch remainder. */
 WHEEL_EVENT(9.99e5f);
 require(wheel_pending==WHEEL_MAX_PENDING&&wheel_remainder>=0.0f&&wheel_remainder<WHEEL_NOTCH,"the largest accepted delta saturates the counter and keeps a bounded remainder");
 WHEEL_DRAIN();
 require(wheel_one(WHEEL_MAX_STEPS),"and delivers one clamped call");
 wheel_calls=0;wheel_remainder=0.0f;wheel_pending=0;
 require(command_state->magic==COMMAND_MAGIC&&command_state->version==COMMAND_VERSION&&mirror_state->magic==MIRROR_MAGIC&&mirror_state->version==MIRROR_VERSION,"CMD31 and MMV17 unchanged");
 require(!atomic_load(&command_violation)&&!atomic_load(&command_state->error)&&!atomic_load(&mirror_state->error)&&atomic_load(&observer_running),"product hook enrolls no lane and publishes no diagnostic failure");
 require(!command_state->published&&!command_state->reclaimed,"no record published and no lane enrolled");
#undef WHEEL_EVENT
#undef WHEEL_DRAIN
 /* Second consumer of the same clamp and the same single call: the X-Touch
  * data-wheel command lane. Here the step arrives as a published JOG_DATA
  * request serviced inside MPC's own UI drain, not from a libinput drain. */
 wheel_focus_focused=word(wheel_focus_receiver+WHEEL_FOCUSED_OFFSET);
 require(pointer(wheel_focus_focused),"data-wheel fixture keeps a focused component");
 MirrorState *saved_mirror=fixture;CommandState *saved_commands=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"data-wheel lane fixture storage");
 memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 command_state->magic=COMMAND_MAGIC;command_state->version=COMMAND_VERSION;command_state->bytes=sizeof(*command_state);command_state->capacity=COMMAND_SLOTS;
 component_wheel_call=wheel_call_fixture;wheel_calls=0;wheel_remainder=0.0f;wheel_pending=0;
 put(0x6b6f798,wheel_focus_receiver);put(wheel_focus_receiver,wheel_focus_vtables[2]);
 put(wheel_focus_receiver+WHEEL_FOCUSED_OFFSET,wheel_focus_focused);put(wheel_focus_focused+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG);
 seed_fixture(0,0,0x3f000000,1);load_fixture();
 require(wheel_command_case(2)==2&&wheel_one(2),"a published data-wheel request is exactly one call carrying its own count");
 require(wheel_command_case(-1)==-1&&wheel_one(-1),"a negative published count turns the app's data wheel down once");
 require(wheel_command_case(9)==WHEEL_MAX_STEPS&&wheel_one(WHEEL_MAX_STEPS),"a published count past the bound is clamped to the same four steps the mouse flush uses");
 require(wheel_command_case(-9)==-WHEEL_MAX_STEPS&&wheel_one(-WHEEL_MAX_STEPS),"and clamped symmetrically downwards");
 /* Suppression is a completed request with a zero step, never a rejection: the
  * hardware panel path drops these steps silently too. */
 put(wheel_focus_receiver+WHEEL_FOCUSED_OFFSET,0);
 require(!wheel_command_case(2)&&!wheel_calls,"nothing focused completes the request with a zero step and no call");
 put(wheel_focus_receiver+WHEEL_FOCUSED_OFFSET,wheel_focus_focused);
 put(wheel_focus_focused+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG^0xffffffffu);
 require(!wheel_command_case(2)&&!wheel_calls,"a focused component without the app's own visible flag completes with a zero step");
 put(wheel_focus_focused+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG);
 put(wheel_focus_receiver,wheel_focus_vtables[3]+4);
 require(!wheel_command_case(2)&&!wheel_calls,"a receiver whose vptr is none of the four focus controllers completes with a zero step");
 put(wheel_focus_receiver,wheel_focus_vtables[2]);
 put(0x6b6f798,0);
 require(!wheel_command_case(2)&&!wheel_calls,"no active focus controller completes with a zero step");
 put(0x6b6f798,wheel_focus_receiver);
 require(wheel_command_case(1)==1,"recovery: the next request after four suppressed ones still turns the wheel once");
 /* Single flight. service_one can service several slots inside one drain, so
  * only refusing the second publication keeps the app's accelerator seeing one
  * counted call per accepted request. */
 CommandRequest live={.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.bits=1,.expires=120000,.reserved=JOG_DATA};
 live.seq=atomic_load(&command_state->published)+1;
 unsigned first_slot=command_free(command_state);
 require(first_slot<COMMAND_SLOTS&&command_publish_request_at(command_state,&live,first_slot),"first data-wheel request publishes");
 CommandRequest second=live;second.seq=live.seq+1;second.bits=(uint32_t)-1;
 unsigned second_slot=command_free(command_state);
 require(second_slot<COMMAND_SLOTS&&second_slot!=first_slot&&!command_publish_request_at(command_state,&second,second_slot),"command_conflict refuses a second data-wheel request while one is alive");
 wheel_calls=0;Context wheel_drain=context();wheel_drain.r[0]=ptr(queue_object)+4;call(&wheel_drain,COMMAND_DRAIN);
 require(wheel_one(1)&&atomic_load(&command_state->slots[first_slot].done)==live.seq,"the one live request is serviced normally");
 command_retire();require(command_publish_settlement(command_state,live.seq),"single-flight request settles");command_retire();
 require(command_idle(command_state)&&command_publish_request_at(command_state,&second,command_free(command_state)),"the next data-wheel request publishes only after the previous one is reclaimed");
 wheel_calls=0;call(&wheel_drain,COMMAND_DRAIN);require(wheel_one(-1),"and is then delivered as its own single call");
 command_retire();require(command_publish_settlement(command_state,second.seq),"second request settles");command_retire();
 /* A request carrying another family's field is a format refusal, not a call. */
 CommandRequest foreign=live;foreign.seq=atomic_load(&command_state->published)+1;foreign.bits=1;foreign.track_owner=7;
 wheel_calls=0;unsigned bad_at=submit_fixture(foreign);
 require(!wheel_calls&&atomic_load(&command_state->slots[bad_at].rejected)==C_FORMAT&&atomic_load(&command_state->slots[bad_at].done)!=atomic_load(&command_state->slots[bad_at].published),"a data-wheel request naming any other target is refused without a call");
 command_retire();
 require(command_state->magic==COMMAND_MAGIC&&command_state->version==COMMAND_VERSION&&mirror_state->magic==MIRROR_MAGIC&&mirror_state->version==MIRROR_VERSION,"CMD31 and MMV17 unchanged by the data-wheel lane");
 require(!atomic_load(&command_violation)&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error),"data-wheel lane publishes no diagnostic failure");
 observer_running=0;put(ptr(root_object)+0x2e4,0);free(fixture);free(command_state);fixture=saved_mirror;mirror_state=saved_mirror;command_state=saved_commands;
 require(!atomic_load(&command_state->published)&&!atomic_load(&command_state->reclaimed),"the data-wheel lane fixture leaves the shared mailbox untouched");
 component_wheel_call=NULL;wheel_remainder=0.0f;wheel_pending=0;
 puts("PASS wheel recipe matches the exact image, and the observer banks the peer's own vertical delta per event, zeroes the JUCE wheel it passes on, and delivers one clamped signed data-wheel count per drain to the app's active focus controller on the owning thread only (native ProcessDataWheelRotation substituted)");
 puts("PASS X-Touch data-wheel command lane: one published JOG_DATA request is one clamped signed ProcessDataWheelRotation call with its own dispatch/step/return evidence, suppression completes with a zero step, a foreign target is refused, and command_conflict keeps exactly one request in flight (native rotation entry substituted)");
}
/* The MPC data wheel's push as a command lane. The press entry this lane calls,
 * and the app's own performer that proves its receiver, its vtable slot and its
 * int argument, are checked against the exact image bytes the installer
 * verifies; the observer's own decision is then exercised with the app's
 * active-controller slot as a fixture. No native body runs here, and no hook
 * site is installed at the press entry: it is called, exactly as the rotation
 * entry is. */
static uint32_t press_functions[8],press_receivers[8];
static int press_buttons[8];
static unsigned press_calls;
static void press_call_fixture(uint32_t function,uint32_t receiver,int button){
 if(press_calls<8){press_functions[press_calls]=function;press_receivers[press_calls]=receiver;press_buttons[press_calls]=button;}
 press_calls++;
}
static uint32_t press_receiver;
/* Exactly one call, to the press entry, on the fixture's receiver, carrying the
 * device-measured id. Nothing here accepts a repeated call: the app's own panel
 * makes one per push. */
static unsigned press_one(void){
 return press_calls==1&&press_functions[0]==PRESS_ENTRY_RVA&&press_receivers[0]==press_receiver&&press_buttons[0]==(int)FOCUS_PRESS_BUTTON;
}
/* Publish one JOG_PRESS request, let the app's own UI drain service it, then
 * seal, settle and reclaim it so the mailbox is idle for the next case. Returns
 * the delivered flag the observer filed, 0 for a suppressed press. */
static unsigned press_command_case(void){
 CommandRequest r={.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.bits=FOCUS_PRESS_BUTTON,.expires=120000,.reserved=JOG_PRESS};
 press_calls=0;unsigned at=submit_fixture(r),seq=atomic_load(&command_state->published);
 CommandSlot *slot=command_state->slots+at;
 require(!atomic_load(&slot->rejected)&&atomic_load(&slot->done)==seq&&atomic_load(&slot->returned)==seq&&!atomic_load(&command_state->trace_error),"an accepted press request completes synchronously in the UI drain");
 unsigned delivered=0,parts[3]={0,0,0};
 for(unsigned lane=0;lane<COMMAND_LANES;lane++)for(unsigned j=0;j<atomic_load(&slot->lanes[lane].published);j++){
  CommandEvent e;require(command_event_read(slot->lanes+lane,seq,j,&e),"press immutable source event");
  require(e.reserved==JOG_PRESS&&e.controller==JOG_PRESS&&!e.arg_kind&&!e.program&&!e.property&&!e.incarnation&&!e.capture&&!e.counter,"press evidence names no Track, Program, Property or native queue");
  /* The exact tuple the bridge's input_events and input_press_proof demand. */
  require(e.request==seq&&e.revision==epoch&&e.token==command_state->owner_token&&(e.kind==CE_WHEEL_STEP||e.bits==r.bits),"press evidence carries the flight, the submission epoch, the UI owner token and the published button id");
  if(e.kind==CE_DISPATCH)parts[0]++;
  else if(e.kind==CE_WHEEL_STEP){parts[1]++;require(e.bits<=1u,"the press receipt is a boolean call flag");delivered=e.bits;}
  else if(e.kind==CE_RETURN)parts[2]++;
  else require(0,"a press flight publishes no jog, queue or commit receipt");
 }
 require(parts[0]==1&&parts[1]==1&&parts[2]==1,"exactly one dispatch, one call receipt and one return per accepted request");
 command_retire();require(atomic_load(&slot->sealed)==seq&&command_publish_settlement(command_state,seq),"press request seals after its synchronous return");
 command_retire();require(command_idle(command_state),"settled press request reclaims on the same protocol");
 return delivered;
}
static void focus_press_checks(void){
 /* No new hook site: the press entry is called, never patched. */
 for(unsigned i=0;i<PATCH_COUNT;i++)require(anchor[i]!=PRESS_ENTRY_RVA,"the controller press entry hosts no patch site");
 const unsigned char *body=guarded_bytes(PRESS_ENTRY_RVA,PRESS_ENTRY_LENGTH),*performer=guarded_bytes(0x331e6f4,0x490),*accessor=guarded_bytes(0x2b64174,0x10);
 require(body&&performer&&accessor,"press entry, its global-action performer group and the focus accessor guarded as whole native ranges");
 const uint32_t focused_load=0xe59000e4,keep_id=0xe1a05001,hwfocusable_slot=0xe592201c,restore_id=0xe1a01005,press_tail=0xe12fff12;
 require(!memcmp(body,&focused_load,4)&&(focused_load&0xfff)==WHEEL_FOCUSED_OFFSET,"slot 5 still starts from the controller's own focused component at the offset the data-wheel entry uses");
 require(!memcmp(body+(0x35c2670-PRESS_ENTRY_RVA),&keep_id,4),"it still keeps its int argument across the dynamic cast");
 require(!memcmp(body+(0x35c26a0-PRESS_ENTRY_RVA),&hwfocusable_slot,4),"it still resolves HWFocusable slot 7 at vtable+0x1c on that component");
 require(!memcmp(body+(0x35c26c0-PRESS_ENTRY_RVA),&restore_id,4)&&!memcmp(body+(0x35c26d4-PRESS_ENTRY_RVA),&press_tail,4),"and still tail-calls that slot with the same int this lane supplies");
 uint32_t accessor_call;memcpy(&accessor_call,performer+(0x331e8fc-0x331e6f4),4);
 require(wheel_branch(0x331e8fc,accessor_call)==0x2b64174,"the app's own wheel-push performer still takes its receiver from the active-focus-controller accessor this lane reads");
 const uint32_t receiver_vtable=0xe5953000,button_load=0xe5941008,receiver_argument=0xe1a00005,slot_five=0xe5933014,virtual_call=0xe12fff33;
 require(!memcmp(performer+(0x331e944-0x331e6f4),&receiver_vtable,4),"it still reads that receiver's own vptr");
 require(!memcmp(performer+(0x331e948-0x331e6f4),&button_load,4),"the id it passes is still its action object's own field at +8, which is the field the device reading came from");
 require(!memcmp(performer+(0x331e94c-0x331e6f4),&receiver_argument,4)&&!memcmp(performer+(0x331e950-0x331e6f4),&slot_five,4)&&!memcmp(performer+(0x331e954-0x331e6f4),&virtual_call,4),"and it still reaches the press entry as vtable slot 5 at +0x14 with that receiver in r0 and that id in r1");
 /* The four legal vptrs carrying slot 5 at +0x14 are pinned where they live, in
  * the RW image, by model-prepare.py at recipe time: their runtime words carry
  * the load bias, so a byte guard over them would fail at install. */
 static unsigned char controller[0x100] __attribute__((aligned(8)));
 static unsigned char focused[0x80] __attribute__((aligned(8)));
 void *focus_page=mmap((void*)0x6b6f000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);
 require(focus_page!=MAP_FAILED,"active focus controller slot fixture page");
 MirrorState *saved_mirror=fixture;CommandState *saved_commands=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"press lane fixture storage");
 memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 command_state->magic=COMMAND_MAGIC;command_state->version=COMMAND_VERSION;command_state->bytes=sizeof(*command_state);command_state->capacity=COMMAND_SLOTS;
 component_press_call=press_call_fixture;component_wheel_call=wheel_call_fixture;press_calls=0;wheel_calls=0;
 memset(controller,0,sizeof controller);memset(focused,0,sizeof focused);
 press_receiver=ptr(controller);
 put(press_receiver+WHEEL_FOCUSED_OFFSET,ptr(focused));put(ptr(focused)+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG);
 put(0x6b6f798,press_receiver);put(press_receiver,wheel_focus_vtables[0]);
 /* The data-wheel lane shares this receiver here, so one drain can be shown to
  * carry exactly one of the two operations. */
 wheel_focus_receiver=press_receiver;
 seed_fixture(0,0,0x3f000000,1);load_fixture();
 require(press_command_case()==1&&press_one(),"a published press request is exactly one call to the app's own press entry carrying the device-measured id");
 for(unsigned v=0;v<4;v++){
  put(press_receiver,wheel_focus_vtables[v]);
  require(press_command_case()==1&&press_one(),"each of the four legal focus-controller vptrs takes one press call with that id");
 }
 /* Suppression is a completed request with a zero call flag, never a rejection:
  * the app's own panel path drops these pushes silently too. */
 put(press_receiver,wheel_focus_vtables[0]);
 put(press_receiver+WHEEL_FOCUSED_OFFSET,0);
 require(!press_command_case()&&!press_calls,"nothing focused completes the press with a zero call flag and no call");
 put(press_receiver+WHEEL_FOCUSED_OFFSET,ptr(focused));
 put(ptr(focused)+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG^0xffffffffu);
 require(!press_command_case()&&!press_calls,"a focused component without the app's own visible flag completes with a zero call flag");
 put(ptr(focused)+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG);
 put(press_receiver,wheel_focus_vtables[3]+4);
 require(!press_command_case()&&!press_calls,"a receiver whose vptr is none of the four focus controllers completes with a zero call flag");
 put(press_receiver,wheel_focus_vtables[0]);
 put(0x6b6f798,0);
 require(!press_command_case()&&!press_calls,"no active focus controller completes with a zero call flag");
 put(0x6b6f798,press_receiver);
 require(press_command_case()==1&&press_one(),"recovery: the next press after four suppressed ones still reaches the app's press entry");
 /* Any id but the measured one is a format refusal, not a call: this lane
  * delivers the panel data wheel's own push and nothing else. */
 CommandRequest base={.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.bits=FOCUS_PRESS_BUTTON,.expires=120000,.reserved=JOG_PRESS};
 const uint32_t wrong[3]={0,FOCUS_PRESS_BUTTON-1u,FOCUS_PRESS_BUTTON+1u};
 for(unsigned i=0;i<3;i++){
  CommandRequest bad=base;bad.bits=wrong[i];bad.seq=atomic_load(&command_state->published)+1;
  press_calls=0;unsigned bad_at=submit_fixture(bad);
  require(!press_calls&&atomic_load(&command_state->slots[bad_at].rejected)==C_FORMAT&&atomic_load(&command_state->slots[bad_at].done)!=atomic_load(&command_state->slots[bad_at].published),"a press request carrying any other button id is refused without a call");
  command_retire();
 }
 CommandRequest foreign=base;foreign.seq=atomic_load(&command_state->published)+1;foreign.track_owner=7;
 press_calls=0;unsigned foreign_at=submit_fixture(foreign);
 require(!press_calls&&atomic_load(&command_state->slots[foreign_at].rejected)==C_FORMAT&&atomic_load(&command_state->slots[foreign_at].done)!=atomic_load(&command_state->slots[foreign_at].published),"a press request naming any other target is refused without a call");
 command_retire();
 /* Single flight across both focus-controller operations: they act on the one
  * active controller, so a press and a data-wheel step never overlap. */
 CommandRequest press=base;press.seq=atomic_load(&command_state->published)+1;
 unsigned first_slot=command_free(command_state);
 require(first_slot<COMMAND_SLOTS&&command_publish_request_at(command_state,&press,first_slot),"first press request publishes");
 CommandRequest step={.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.bits=1,.expires=120000,.reserved=JOG_DATA};
 step.seq=press.seq+1;
 unsigned second_slot=command_free(command_state);
 require(second_slot<COMMAND_SLOTS&&second_slot!=first_slot&&!command_publish_request_at(command_state,&step,second_slot),"command_conflict refuses a data-wheel request while a press is alive");
 CommandRequest again=press;again.seq=press.seq+1;
 require(!command_publish_request_at(command_state,&again,second_slot),"and refuses a second press while one is alive");
 press_calls=0;wheel_calls=0;Context press_drain=context();press_drain.r[0]=ptr(queue_object)+4;call(&press_drain,COMMAND_DRAIN);
 require(press_one()&&!wheel_calls&&atomic_load(&command_state->slots[first_slot].done)==press.seq,"the one live press is serviced normally");
 command_retire();require(command_publish_settlement(command_state,press.seq),"single-flight press settles");command_retire();
 require(command_idle(command_state)&&command_publish_request_at(command_state,&step,command_free(command_state)),"the data-wheel request publishes only after the press is reclaimed");
 CommandRequest blocked=press;blocked.seq=step.seq+1;
 require(!command_publish_request_at(command_state,&blocked,command_free(command_state)),"and a press is refused while that data-wheel request is alive");
 press_calls=0;wheel_calls=0;call(&press_drain,COMMAND_DRAIN);
 require(wheel_one(1)&&!press_calls,"the data-wheel request is then delivered as its own single call");
 command_retire();require(command_publish_settlement(command_state,step.seq),"the data-wheel request settles");command_retire();
 require(command_state->magic==COMMAND_MAGIC&&command_state->version==COMMAND_VERSION&&mirror_state->magic==MIRROR_MAGIC&&mirror_state->version==MIRROR_VERSION,"CMD31 and MMV17 unchanged by the press lane");
 require(!atomic_load(&command_violation)&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error),"press lane publishes no diagnostic failure");
 observer_running=0;put(ptr(root_object)+0x2e4,0);free(fixture);free(command_state);fixture=saved_mirror;mirror_state=saved_mirror;command_state=saved_commands;
 require(!atomic_load(&command_state->published)&&!atomic_load(&command_state->reclaimed),"the press lane fixture leaves the shared mailbox untouched");
 component_press_call=NULL;component_wheel_call=NULL;press_calls=0;wheel_calls=0;
 puts("PASS X-Touch data-wheel push lane: the app's own performer still reaches the guarded press entry as focus-controller slot 5 with its action id, no site is patched there, one published JOG_PRESS request is one call carrying the device-measured id on each legal receiver, suppression completes with a zero call flag, a wrong id or foreign target is refused, and command_conflict keeps one focus-controller request in flight across presses and data-wheel steps (native press entry substituted)");
}
static void new_project_checks(void){
 MirrorState *saved_fixture=fixture;CommandState *saved_commands=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"intent fixture storage");
 void *env_page=mmap((void*)0x6b26000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);require(env_page!=MAP_FAILED,"Environment singleton fixture page");
 put(0x6b26814,ptr(root_object));
 for(unsigned test=0;test<4;test++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;atomic_store(&command_state->alive,1);
  Context c=context();c.r[0]=ptr(root_object);c.r[1]=test==1?1:0;c.lr=test==2?0x25703f0:0x25703f4;if(test==3)atomic_store(&command_state->stop_requested,1);
  call(&c,158);require(atomic_load(&command_state->new_project_intent)==(test?3u:1u),"only exact accepted caller, zero flag, live owner and no stop grants native intent");
  if(!test){c.lr=0x1595000;call(&c,158);require(atomic_load(&command_state->new_project_intent)==3,"later native shutdown revokes positive intent");}
 }
 for(unsigned pause=9;pause<=10;pause++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;atomic_store(&command_state->alive,1);
  external_operation=external_result=0;external_request=(CommandRequest){.seq=1,.bits=0x3f200000};atomic_store(&pause_at,pause);atomic_store(&pause_reached,0);atomic_store(&pause_resume,0);
  pthread_t writer;require(!pthread_create(&writer,NULL,paused_external,NULL),"intent publication race writer");
  while(!atomic_load(&pause_reached)){struct timespec delay={0,1000000};nanosleep(&delay,NULL);}
  Context c=context();c.r[0]=ptr(root_object);c.r[1]=0;c.lr=0x25703f4;call(&c,158);
  atomic_store(&pause_resume,1);require(!pthread_join(writer,NULL),"intent publication race leaves");atomic_store(&pause_at,0);
  require(!external_result&&!atomic_load(&command_state->published)&&!atomic_load(&command_state->slots[0].published)&&!atomic_load(&command_state->error),"native intent inhibits admission/frontier without manufacturing rejection or receipt");
  CommandSlot *q=command_state->slots;command_request_store(q,&external_request);atomic_store(&q->published,1);atomic_store(&command_state->published,1);atomic_store(&q->sealed,1);
  require(command_publish_settlement(command_state,1)&&atomic_load(&q->settled)==1,"intent leaves actual sealed request settlement gate open");
 }
 munmap(env_page,4096);free(fixture);free(command_state);fixture=saved_fixture;mirror_state=fixture;command_state=saved_commands;
 puts("PASS actual new-project hook predicate and paused publication/settlement helpers; singleton/context and sealed request substituted; literal entry instructions separately replayed");
}
static void repair_checks(void){
 MirrorState *saved_fixture=fixture;CommandState *saved_commands=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"private scheduling fixtures");
 for(unsigned test=0;test<5;test++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;
  if(test>=3)seed_fixture(0,0,0x3f000000,1);
  probe_pin_constructor_owner=test==1||test==2;
  probe_operation=test<2?1:test==2?2:test==3?3:4;
  atomic_store(&pause_reached,0);atomic_store(&pause_resume,0);atomic_store(&pause_at,test==0?1:test<3?2:test==3?3:4);
  pthread_t source;require(!pthread_create(&source,NULL,paused_source,NULL),"paused source thread");
  while(!atomic_load_explicit(&pause_reached,memory_order_acquire)){struct timespec wait={0,1000000};nanosleep(&wait,NULL);}
  command_close();require(!command_quiescent(),"close waits pre-registration or complete MMV7 publication flight");
  atomic_store_explicit(&pause_resume,1,memory_order_release);require(!pthread_join(source,NULL)&&command_quiescent(),"flight finishes before closed");
  if(!test)require(!atomic_load(&fixture->allocated)&&!atomic_load(&command_state->lane_tokens[1]),"close wins registration: no token or seed publication");
  else if(test<3)require(atomic_load(&fixture->allocated)==1,"admitted normal/copy seed finishes before close");
  else{MirrorCell *cell=lookup(ptr(program_objects[0])+0x5b0);require(cell&&atomic_load(&cell->revision)==4&&(test==3?atomic_load(&cell->bits)==0x3f100000:!atomic_load(&cell->live)),"admitted float/death finishes before close");}
  require(command_finalize(),"actual production close finalization");
  MirrorState *before=malloc(sizeof(*before));CommandState *commands_before=malloc(sizeof(*commands_before));require(before&&commands_before,"closed snapshots");memcpy(before,fixture,sizeof(*before));memcpy(commands_before,command_state,sizeof(*commands_before));
  require(!pthread_create(&source,NULL,paused_source,NULL)&&!pthread_join(source,NULL),"late unadmitted callback");
  require(!memcmp(before,fixture,sizeof(*before))&&!memcmp(commands_before,command_state,sizeof(*commands_before)),"closed MMV7, lane tokens and errors remain immutable");free(before);free(commands_before);atomic_store(&pause_at,0);
 }
 probe_pin_constructor_owner=0;
 for(unsigned injection=1;injection<=2;injection++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;seed_fixture(0,0,0x3f000000,1);load_fixture();
  component_dispatch=fake_dispatch;dispatched_calls=0;probe_inject=injection;unsigned n=submit_fixture(request(0x3f200000));probe_inject=0;
  require(!dispatched_calls&&atomic_load(&command_state->slots[n].rejected)&&!atomic_load(&command_state->slots[n].dispatched)&&!atomic_load(&command_state->slots[n].returned)&&!atomic_load(&command_state->slots[n].done)&&!atomic_load(&observer_running),"foreign stop/changed method aborts before target and successful dispatch flags");
  put(0x6930c28,0x250ba74);put(0x6930c78,0x1375c68);
 }
 free(fixture);free(command_state);fixture=saved_fixture;mirror_state=fixture;command_state=saved_commands;dispatched_calls=0;
 puts("PASS deterministic pre-registration/normal-seed/copy-seed/float/death close flights and immutable closed output; foreign violation and method replacement after resolve call neither target");
}
static void heartbeat_checks(void){
 MirrorState *saved_fixture=fixture;CommandState *saved_commands=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"heartbeat request fixtures");
 for(unsigned test=0;test<8;test++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;atomic_store(&command_state->alive,1);component_dispatch=fake_dispatch;dispatched_calls=0;
  seed_fixture(0,0,1065350144,1);load_fixture();
  /* Exact failed bank2 value/timestamps at the configured900-second window. */
  uint32_t created=!WINDOW_SECONDS||WINDOW_SECONDS>=522?521408:WINDOW_SECONDS*500u;
  CommandRequest r=request(1065034733);r.seq=1;r.created=created;r.expires=created+120000;
  if(test==1)r.created=command_tick_limit(WINDOW_SECONDS);
  if(test==2)r.expires=r.created-1;
  if(test==3)r.expires=r.created+120001;
  if(test==4)r.origin_nsec++;
  atomic_store(&fixture->heartbeat,created-8);require(command_publish_request(command_state,&r),"actual external request publication before stale heartbeat drain");
  CommandState *before=malloc(sizeof(*before));require(before!=NULL,"deferred request snapshot");memcpy(before,command_state,sizeof(*before));
  Context drain=context();drain.r[0]=ptr(queue_object)+4;call(&drain,COMMAND_DRAIN);command_retire();
  if(test>=1&&test<=4){
   require(atomic_load(&command_state->slots[0].processed)==1&&atomic_load(&command_state->slots[0].rejected)==(test==4?C_IDENTITY:C_EXPIRED)&&!dispatched_calls,"impossible window/span/origin does not defer into a valid request");
  }else{
   require(!memcmp(before,command_state,sizeof(*before))&&!dispatched_calls&&!atomic_load(active_request),"lagging heartbeat defers without consuming, resetting evidence or calling native target");
   if(test==7){command_close();require(command_finalize()&&atomic_load(&command_state->error)==C_CLOSED&&!dispatched_calls,"finite close reports deferred unconsumed request incomplete");}
   else{
    if(test==6){Context reset=context();reset.r[0]=ptr(root_object);call(&reset,M_RESET);}
    atomic_store(&fixture->heartbeat,test==5?r.expires+1:created);call(&drain,COMMAND_DRAIN);command_retire();
    if(test==5||test==6)require(atomic_load(&command_state->slots[0].processed)==1&&atomic_load(&command_state->slots[0].rejected)==(test==5?C_EXPIRED:C_NOT_READY)&&!dispatched_calls,"deferred request still expires or rejects reset before dispatch");
    else{
     require(dispatched_calls==1&&atomic_load(&command_state->slots[0].done)==1&&!atomic_load(&command_state->slots[0].rejected),"caught-up heartbeat dispatches fullscale-to-lower value exactly once");
     call(&drain,COMMAND_DRAIN);command_retire();MIRROR_COPY(out);require(copy_mirror(fixture,&out)&&out.tracks[0].bits==r.bits&&atomic_load(&command_state->slots[0].sealed)==1,"actual copied source commit and done seal deferred request");
     require(command_publish_settlement(command_state,1),"deferred request source settlement");call(&drain,COMMAND_DRAIN);command_retire();require(atomic_load(&command_state->reclaimed)==1&&dispatched_calls==1,"settled deferred request reclaims without duplicate dispatch");
    }
   }
  }
  free(before);
 }
 free(fixture);free(command_state);fixture=saved_fixture;mirror_state=fixture;command_state=saved_commands;dispatched_calls=0;
 puts("PASS captured bank2 fullscale-to-lower request: stale heartbeat defers unchanged, catch-up dispatches/source-settles once; malformed timing/origin, true expiry, reset and finite close remain guarded (native target substituted)");
}
/* The drain's ordinary slot scan reads a 39-word request for every slot it
 * inspects. Every slot keeps its last published sequence forever, so without
 * the skip the UI thread re-reads all thirty-two on every drain for nothing.
 * These checks pin both directions: an already processed or reclaimed slot is
 * not read, and a slot with fresh work still is. */
static void drain_scan_checks(void){
 MirrorState *saved_fixture=fixture;CommandState *saved_commands=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"drain scan fixtures");
 initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);component_dispatch=fake_dispatch;dispatched_calls=0;
 seed_fixture(0,0,0x3f000000,1);load_fixture();
 Context drain=context();drain.r[0]=ptr(queue_object)+4;
 drain_request_reads=0;call(&drain,COMMAND_DRAIN);
 require(!drain_request_reads,"an idle mailbox costs the drain no request read");
 CommandRequest r=request(0x3f400000);r.seq=1;unsigned at=command_free(command_state);
 require(at<COMMAND_SLOTS&&command_publish_request_at(command_state,&r,at),"drain scan request publication");
 drain_request_reads=0;call(&drain,COMMAND_DRAIN);
 require(drain_request_reads==1&&atomic_load(&command_state->slots[at].processed)==1&&dispatched_calls==1,"a published unprocessed slot is read once and dispatched");
 drain_request_reads=0;call(&drain,COMMAND_DRAIN);command_retire();
 require(!drain_request_reads&&atomic_load(&command_state->slots[at].sealed)==1,"a processed slot is not re-read while its receipt seals");
 require(command_publish_settlement(command_state,1),"drain scan source settlement");
 drain_request_reads=0;call(&drain,COMMAND_DRAIN);command_retire();
 require(!drain_request_reads&&atomic_load(&command_state->reclaimed)==1,"a settled slot reclaims without the scan re-reading it");
 drain_request_reads=0;call(&drain,COMMAND_DRAIN);
 require(!drain_request_reads,"a reclaimed slot whose published sequence remains is never read again");
 /* Reuse of the same slot: published moves past both processed and reclaimed,
  * so the skip must let the next request through on the very next drain. */
 MIRROR_COPY(out);require(copy_mirror(fixture,&out),"drain scan reuse identity");
 CommandRequest next=request(0x3f600000);next.seq=2;
 require(command_publish_request_at(command_state,&next,at),"same slot republished");
 drain_request_reads=0;call(&drain,COMMAND_DRAIN);
 require(drain_request_reads==1&&atomic_load(&command_state->slots[at].processed)==2&&dispatched_calls==2,"a reused slot is read and dispatched on the first drain after publication");
 require(!atomic_load(&command_state->trace_error)&&!atomic_load(&command_state->error),"drain scan skip changes no receipt or fault state");
 free(fixture);free(command_state);fixture=saved_fixture;mirror_state=fixture;command_state=saved_commands;dispatched_calls=0;drain_request_reads=0;
 puts("PASS UI drain slot scan reads a request only for a slot with fresh work: idle, processed, sealed, settled and reclaimed slots cost no 39-word read, and a reused slot is still dispatched on the next drain (native target substituted)");
}
/* The snapshot copy no longer clears every byte it is about to overwrite, and
 * copy_channel no longer re-clears the text it never writes. Two separate
 * properties, pinned separately below.
 * P1, every byte a copy MUST write is written: two copies into oppositely
 *     poisoned destinations agree. The text arrays are restored to zero by the
 *     poison helper, because those are the bytes a destination is required to
 *     start zero and the copy maintains rather than rewrites.
 * P2, the text maintenance is exact: a destination reused across copies whose
 *     contents differ - long name, then short name, then a name field that
 *     stops resolving at all - is byte-for-byte identical to a destination
 *     that has only ever seen that one state. */
/* tails=1 compares every byte, including the text past each field's length,
 * which agrees only when the tail maintenance is exact. tails=0 compares the
 * scalars and the written part of each text, which is what a poisoned pair can
 * be held to, since a poisoned destination's tails are zero by construction. */
static int field_same(const CopiedField *a,const CopiedField *b,int tails){
 if(memcmp(a,b,offsetof(CopiedField,text)))return 0;
 unsigned n=tails?sizeof(a->text):(a->length<=CHANNEL_TEXT?a->length+1u:0u);
 return !memcmp(a->text,b->text,n);
}
static int track_same(const CopiedTrack *a,const CopiedTrack *b,int tails){
 if(memcmp(a,b,offsetof(CopiedTrack,fields)))return 0;
 for(unsigned f=0;f<CF_COUNT;f++)if(!field_same(a->fields+f,b->fields+f,tails))return 0;
 return !memcmp(&a->meter,&b->meter,sizeof(a->meter));
}
static int copy_same(const CopiedMirror *a,const CopiedMirror *b,int tails){
 if(memcmp(a,b,offsetof(CopiedMirror,selection)))return 0;
 const CopiedField *ha[]={&a->selection,&a->master,&a->playing,&a->automation,&a->loop,&a->record_mode,&a->click};
 const CopiedField *hb[]={&b->selection,&b->master,&b->playing,&b->automation,&b->loop,&b->record_mode,&b->click};
 for(unsigned i=0;i<sizeof(ha)/sizeof(ha[0]);i++)if(!field_same(ha[i],hb[i],tails))return 0;
 if(memcmp(&a->position_available,&b->position_available,offsetof(CopiedMirror,tracks)-offsetof(CopiedMirror,position_available)))return 0;
 if(a->count!=b->count)return 0;
 for(unsigned i=0;i<a->count;i++)if(!track_same(a->tracks+i,b->tracks+i,tails))return 0;
 if(memcmp(&a->master_meter,&b->master_meter,offsetof(CopiedMirror,pads)-offsetof(CopiedMirror,master_meter)))return 0;
 if(a->pad_count!=b->pad_count)return 0;
 for(unsigned i=0;i<a->pad_count;i++)if(!track_same(a->pads+i,b->pads+i,tails))return 0;
 return 1;
}
static void rename_fixture(unsigned row,char *name){
 put(ptr(track_objects[row])+0x468,ptr(name));
 Context c=context();c.r[5]=ptr(track_objects[row])+0x440;call(&c,M_NAME);
}
static void snapshot_clear_checks(void){
 MirrorState *saved_fixture=fixture;CommandState *saved_commands=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"snapshot clear fixtures");
 initial(3);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 /* Row 2 is deliberately left unseeded, so it has no volume cell and takes the
  * no-incarnation path that writes none of the five cell-derived words. */
 seed_fixture(0,0,0x3f000000,1);seed_fixture(1,0,0x3f400000,1);load_fixture();
 MIRROR_COPY(pa);MIRROR_COPY(pb);
 for(unsigned pass=0;pass<3;pass++){
  /* pass 1 retires one Program owner, so its fields take copy_channel's early
   * returns; pass 2 also kills a live volume cell, so the seqlock path stops
   * qualifying. Both leave rows populated and the copy accepted. */
  if(pass==1){Context d=context();d.r[0]=ptr(program_objects[1]);call(&d,CH_PROGRAM_DEATH);}
  if(pass==2){Context d=context();d.r[0]=ptr(program_objects[0])+0x5b0;d.d[8]=0;call(&d,MIRROR_PROPERTY_DESTROY);}
  mirror_poison(&pa,0xaa);require(copy_mirror(fixture,&pa)==1,"first poisoned copy accepted");
  mirror_poison(&pb,0x55);require(copy_mirror(fixture,&pb)==1,"second poisoned copy accepted");
  require(pa.count==3&&pb.count==3,"both copies see the same three rows");
  require(copy_same(&pa,&pb,0),"every byte a copy must write is written, not inherited from the buffer");
  /* The rows must also still carry the values, so a clear that simply zeroed
   * everything could not pass this by making both copies identically empty. */
  require(!pa.tracks[2].incarnation&&!pa.tracks[2].available&&!pa.tracks[2].revision&&!pa.tracks[2].bits&&!pa.tracks[2].seed&&!pa.tracks[2].updates,"a row with no volume cell reads as unavailable with no stale value");
  require(pa.tracks[0].fields[CF_PAN].bits==0x3f000000&&!strcmp(pa.tracks[0].fields[CF_NAME].text,"Channel"),"the copy still carries the real field values and text");
  require(!pa.tracks[0].fields[CF_MASTER].available&&!pa.tracks[0].fields[CF_SELECTION].available&&!pa.tracks[0].fields[CF_CLICK].available,"fields no track copy fills read as absent rather than as poison");
  if(pass==0)require(pa.tracks[0].available&&pa.tracks[0].bits==0x3f000000&&pa.tracks[1].fields[CF_PAN].available,"a healthy row still reports its own volume and its Program fields");
  if(pass>=1)require(!pa.tracks[1].fields[CF_PAN].available&&!pa.tracks[1].fields[CF_MUTE].available,"a retired Program owner withdraws its fields without leaving poison");
  if(pass==2)require(!pa.tracks[0].available,"a dead volume cell withdraws the row value without leaving poison");
 }
 /* P2. One destination carried through five states against five destinations
  * that each see exactly one. Step 2 is the long-name-then-short-name case and
  * step 4 is the one that writes no text at all: a latched CF_NAME text fault
  * makes every name copy return at its first check, so the tail the previous
  * step left behind is the only thing that can differ. */
 MIRROR_COPY(reused);MIRROR_COPY(f0);MIRROR_COPY(f1);MIRROR_COPY(f2);MIRROR_COPY(f3);MIRROR_COPY(f4);
 CopiedMirror *fresh[]={&f0,&f1,&f2,&f3,&f4};
 static char name_long[]="a deliberately long channel name here",name_short[]="Hi",name_max[]="a name of ninety characters that is very nearly the longest this observer will ever copy!";
 static char *names[]={NULL,name_long,name_short,name_max,NULL};
 for(unsigned step=0;step<5;step++){
  if(step&&names[step])rename_fixture(0,names[step]);
  if(step==4){put(ptr(track_objects[0])+0x468,1);Context c=context();c.r[5]=ptr(track_objects[0])+0x440;call(&c,M_NAME);}
  require(copy_mirror(fixture,&reused)==1,"reused destination accepts the copy");
  require(copy_mirror(fixture,fresh[step])==1,"fresh destination accepts the copy");
  require(copy_same(&reused,fresh[step],1),"a destination reused across differing copies is byte-for-byte a freshly zeroed one");
  if(step&&step<4)require(reused.tracks[0].fields[CF_NAME].available&&!strcmp(reused.tracks[0].fields[CF_NAME].text,names[step])&&reused.tracks[0].fields[CF_NAME].length==strlen(names[step]),"each renamed step really changed the copied text");
  if(step==4)require(!reused.tracks[0].fields[CF_NAME].available&&!reused.tracks[0].fields[CF_NAME].length&&!reused.tracks[0].fields[CF_NAME].text[0],"a name field that stops resolving leaves no part of the previous name behind");
 }
 free(fixture);free(command_state);fixture=saved_fixture;mirror_state=fixture;command_state=saved_commands;
 puts("PASS snapshot copy writes every byte it must and maintains the text it does not rewrite: oppositely poisoned copies agree across healthy rows, a row with no volume cell, a retired Program owner and a dead volume cell, and one destination reused through a long name, a short name, a ninety-character name and a name that stops resolving is byte-for-byte a freshly zeroed destination (native objects substituted)");
}
static void channel_checks(void){
 MirrorState *save=fixture;CommandState *csave=command_state;fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"channel fixtures");
 require(mmap((void*)0x6935000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED,"Track method fixture");put(0x693557c,0x26500f8);
 initial(2);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);component_dispatch=fake_dispatch;
 seed_fixture(0,0,0x3f000000,1);seed_fixture(1,0,0x3f000000,1);load_fixture();
 MIRROR_COPY(out);require(copy_mirror(fixture,&out),"retained channel snapshot");
 for(unsigned i=0;i<2;i++)for(unsigned f=CF_PAN;f<CF_MASTER;f++)if(f!=CF_SELECTION)require(out.tracks[i].fields[f].available&&out.tracks[i].fields[f].seed==1,"all field default births joined through independent Track/Program owners");
 require(out.selection.available&&!out.selection.bits&&out.tracks[0].fields[CF_PAN].bits==0x3f000000&&!strcmp(out.tracks[0].fields[CF_NAME].text,"Channel"),"selection/pan/default name copied at source");
 Context drain=context();drain.r[0]=ptr(queue_object)+4;
 unsigned fields[]={CF_PAN,CF_MUTE,CF_SOLO,CF_ARM,CF_SELECTION,CF_SEND1,CF_SEND2,CF_SEND3,CF_SEND4};
 for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++){
  unsigned field=fields[i];require(copy_mirror(fixture,&out),"fresh channel request identity");MIRROR_FIELD(volume);const CopiedField *f=copied_field(&out,out.tracks,command_source_field(field),&volume);
  CommandRequest r=request(command_float(field)?0x3f400000:1);r.reserved=field;r.field_incarnation=f->incarnation;r.before_bits=f->bits;r.before_revision=f->revision;
  queue_foreign=field==CF_MUTE||field==CF_SOLO;
  submit_fixture(r);queue_foreign=0;unsigned seq=i+1;
  require(atomic_load(&command_state->slots[0].done)==seq&&!atomic_load(&command_state->slots[0].rejected)&&!atomic_load(&command_state->trace_error),"all typed and owner command families reach matching source completion");
  call(&drain,COMMAND_DRAIN);command_retire();require(atomic_load(&command_state->slots[0].sealed)==seq&&copy_mirror(fixture,&out),"channel evidence seals after source flights");f=copied_field(&out,out.tracks,command_source_field(field),&volume);
  require(f->available&&f->bits==(field==CF_SELECTION?ptr(track_objects[0]):r.bits),"actual copied source state settles each family");
  unsigned commits=0;for(unsigned lane=0;lane<COMMAND_LANES;lane++)for(unsigned j=0;j<atomic_load(&command_state->slots[0].lanes[lane].published);j++){CommandEvent e;require(command_event_read(command_state->slots[0].lanes+lane,seq,j,&e)&&e.reserved==field,"field-qualified immutable source evidence");if(e.kind==CE_COMMIT){commits++;require(e.incarnation==f->incarnation&&e.bits==r.bits&&e.revision==f->revision,"contained field commit identity/value/revision");}}
  require(commits==1,"one real fixture source commit per operation");require(command_publish_settlement(command_state,seq),"channel external settlement");call(&drain,COMMAND_DRAIN);command_retire();require(atomic_load(&command_state->reclaimed)==seq,"same reusable protocol across field families");
 }
 /* Nonzero copy constructor seeds, before any generic setter notification. */
 struct {Context c;uint32_t stack[128];} f={0};uint32_t p=ptr(program_objects[1]);f.c=context();f.c.r[0]=p;call(&f.c,CH_PROGRAM_COPY);
 f.c.r[4]=p+0x3c;f.c.r[7]=0x3e800000;f.stack[0x8c/4]=0x2370710;f.stack[0xf4/4]=0x2370a7c;f.stack[0x10c/4]=0x2514334;f.stack[0xf8/4]=p;call(&f.c,CH_PAN_COPY);
 for(unsigned send=0;send<4;send++){f.c.r[5]=p+0x428+0x38*(send+1);f.c.r[3]=0x3e800000;call(&f.c,CH_SEND_COPY);}
 f.c.r[4]=p+0x48;f.c.r[2]=f.c.r[3]=f.c.r[6]=1;f.stack[0x8c/4]=0x236faf4;f.stack[0x11c/4]=0x2370710;f.stack[0x184/4]=0x2370a7c;f.stack[0x19c/4]=0x2514334;f.stack[0x188/4]=p;
 call(&f.c,CH_MUTE_COPY);call(&f.c,CH_SOLO_COPY);call(&f.c,CH_SOLO_AUDIO_COPY);call(&f.c,CH_EFFECTIVE_COPY);
 unsigned copied[]={CF_PAN,CF_MUTE,CF_SOLO,CF_SOLO_AUDIO,CF_EFFECTIVE_MUTE};for(unsigned i=0;i<5;i++){MIRROR_FIELD(field);require(copy_channel(&fixture->channel,channel_owner_id(p),copied[i],&field)&&field.seed==2&&!field.updates&&field.bits==(i?1:0x3e800000),"nonzero copy seed without changes");}
 Context c=context();c.r[0]=p+0x3d0;call(&c,MIRROR_PROPERTY_DESTROY);MIRROR_FIELD(field);require(!copy_channel(&fixture->channel,channel_owner_id(p),CF_PAN,&field)&&copy_channel(&fixture->channel,channel_owner_id(p),CF_MUTE,&field),"property death isolates unavailable field");
 c.r[0]=p;call(&c,CH_PROGRAM_DEATH);require(!copy_channel(&fixture->channel,channel_owner_id(p),CF_MUTE,&field),"Program death closes bound field owner");

 /* New non-Track state owners and source-return BBT use real publication code;
  * this fixture substitutes native owner construction and getter execution. */
 uint32_t ownerp=ptr(program_objects[3]);Context gc=context();gc.r[0]=ownerp;call(&gc,CH_MIXER_BIRTH);gc.r[4]=ownerp;gc.r[5]=0x3f400000;call(&gc,CH_MASTER_BIRTH);
 require(copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_MASTER,&field)&&field.bits==0x3f400000,"distinct master constructor seed");
 gc.r[5]=ownerp+0xa8;gc.d[8]=0x3e800000;call(&gc,M_FLOAT);require(copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_MASTER,&field)&&field.bits==0x3e800000,"master generic committed source");gc.r[0]=ownerp;call(&gc,CH_MIXER_DEATH);require(!copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_MASTER,&field),"master owner destruction unavailable");
 gc.r[0]=ownerp;call(&gc,CH_TIMELINE_BIRTH);gc.r[4]=ownerp;gc.r[3]=0;call(&gc,CH_PLAYING_BIRTH);gc.r[5]=ownerp+0xec;gc.r[7]=1;call(&gc,M_MUTE);require(copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_PLAYING,&field)&&field.bits==1,"Timeline native playing commit");
 uint32_t tuple[3]={12,2,480};atomic_store(&fixture->sequencer,ownerp);atomic_store(&fixture->heartbeat,100);gc.r[0]=ptr(tuple);gc.r[1]=ownerp;call(&gc,CH_POSITION_ENTER);positions[position_depth-1].sp=source_sp(&gc)+24;gc.r[4]=ptr(tuple);gc.r[5]=ownerp;call(&gc,CH_POSITION_EXIT);require(copy_mirror(fixture,&out)&&out.position_available&&out.bar==12&&out.beat==2&&out.clock==480,"completed signature-aware BBT copied from current native output");
 atomic_store(&fixture->heartbeat,1100);require(copy_mirror(fixture,&out)&&!out.position_available,"on-demand BBT sample visibly expires");atomic_store(&fixture->heartbeat,1200);gc.r[0]=ptr(tuple);gc.r[1]=ownerp;call(&gc,CH_POSITION_ENTER);positions[position_depth-1].sp=source_sp(&gc)+24;atomic_store(&fixture->epoch,99);call(&gc,CH_POSITION_EXIT);require(copy_mirror(fixture,&out)&&!out.position_available,"BBT call spanning project generation is discarded");

 call(&gc,CH_POSITION_EXIT);require(copy_mirror(fixture,&out)&&out.position_error==CH_LIFETIME&&!out.position_available&&!atomic_load(&command_state->trace_error),"unpaired BBT output inhibits only position, not other channel state");
 gc.r[0]=ownerp;call(&gc,CH_AUTOMATION_OWNER);gc.r[4]=ownerp;gc.r[2]=1;call(&gc,CH_AUTOMATION_BIRTH);gc.r[5]=ownerp+0xc;gc.r[7]=2;call(&gc,CH_ENUM_COMMIT);require(copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_AUTOMATION,&field)&&field.bits==2,"native aggregate automation enum committed readback");
 gc.r[0]=ownerp;call(&gc,CH_LOOP_OWNER);gc.r[4]=ownerp;gc.r[3]=1;call(&gc,CH_LOOP_BIRTH);gc.r[5]=ownerp+0x68;gc.r[7]=0;call(&gc,M_MUTE);require(copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_LOOP,&field)&&field.bits==0,"Timeline-owned effective loop seed and commit");gc.r[0]=ownerp+0x68;call(&gc,MIRROR_PROPERTY_DESTROY);require(!copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_LOOP,&field),"loop Property death invalidates copied feedback");
 gc.r[0]=ownerp;call(&gc,CH_RECORD_OWNER);gc.r[4]=ownerp;gc.r[3]=0;call(&gc,CH_RECORD_BIRTH);
 require(copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_RECORD_MODE,&field)&&field.bits==0,"RecorderProperties default recording mode source seed");
 gc.r[5]=ownerp+0x500;gc.r[7]=3;call(&gc,CH_RECORD_COMMIT);require(copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_RECORD_MODE,&field)&&field.bits==3,"locked recording mode commit retains actual enum");
 gc.r[0]=ownerp;call(&gc,CH_RECORD_DEATH);require(!copy_channel(&fixture->channel,channel_owner_id(ownerp),CF_RECORD_MODE,&field),"record owner death invalidates independent feedback");
 command_close();require(command_finalize(),"channel source finite closure");free(fixture);free(command_state);fixture=save;mirror_state=save;command_state=csave;dispatched_calls=0;
 puts("PASS channel default/copy seeds, independent field/owner death, retained readout; queued pan/mute/Solo and synchronous Track arm/Project selection source settlement (native calls substituted)");
}
/* Channel table reclamation. One saved-project load costs a few hundred field
 * cells; a bump-only table exhausts CHANNEL_FIELDS after a handful of loads and
 * then registers no name or colour for anything loaded afterwards. This churns
 * far past both capacities and holds the observable contract instead of the
 * mechanism: the current generation always reads back, the app-global owners
 * and their cells are never victims, retired lifetimes resolve to nothing, the
 * directory stays inside a pinned bound, and an unsatisfiable birth stays
 * visible instead of being wiped by the next success. */
#define RECLAIM_GENERATIONS 60u
#define RECLAIM_OWNERS 25u
#define RECLAIM_GLOBALS 7u
#define RECLAIM_TRACK_FIELDS 4u
#define RECLAIM_PROGRAM_FIELDS 9u
#define RECLAIM_CELLS (RECLAIM_TRACK_FIELDS+RECLAIM_PROGRAM_FIELDS)
#define RECLAIM_DIRECTORY_BOUND 7700u
#define RECLAIM_OWNER_DIRECTORY_BOUND 3400u
#define CLUSTER_GENERATIONS 40u
#define CLUSTER_OWNERS 48u
#define CLUSTER_LIVE 8u
#define CLUSTER_FIELDS 4u
static unsigned channel_read_probe;
static ChannelCell *channel_read_target;
static uint32_t channel_read_property,channel_read_owner;
static void channel_owner_publish(uint32_t seen_address,unsigned seen_kind,unsigned seen_incarnation,uint32_t address,unsigned kind,unsigned incarnation){
 channel_owner_publishes++;
 require(seen_address==address&&seen_kind==kind,"an owner's address and kind are published before its incarnation");
 require(seen_incarnation!=incarnation,"an owner's incarnation is published last, after the address and kind it names");
}
/* Mode 1 re-identifies the matched cell completely; mode 2 changes only its
 * owner_incarnation, so the in-window property and field clauses both still
 * match and the owner clause is the only thing left to reject. */
static void channel_read_pause(unsigned at){
 if(at||!channel_read_probe)return;
 unsigned mode=channel_read_probe;channel_read_probe=0;
 ChannelCell *c=channel_read_target;unsigned rev=atomic_load(&c->revision);
 atomic_store(&c->revision,rev+1);atomic_thread_fence(memory_order_release);
 atomic_store(&c->live,0);
 if(mode==1){atomic_store(&c->property,channel_read_property);atomic_store(&c->field,CF_COLOR);atomic_store(&c->bits,0x0badf00d);atomic_store(&c->length,0);}
 atomic_store(&c->owner_incarnation,channel_read_owner);atomic_store(&c->live,1);
 atomic_thread_fence(memory_order_release);atomic_store(&c->revision,rev+2);
}
static uint32_t reclaim_float(float v){uint32_t bits;memcpy(&bits,&v,4);return bits;}
/* Odd stride in the hashed word: a power-of-two spacing folds every owner into
 * 32 buckets of the 8192-word directory. The clustering churn below uses that
 * spacing deliberately; this one must not, or it would measure probe exhaustion
 * instead of reclamation. */
static uint32_t reclaim_owner_address(unsigned generation,unsigned index,unsigned program){return 0x40000000u+((generation*RECLAIM_OWNERS+index)*2u+program)*0x1030u;}
static const unsigned reclaim_track_fields[RECLAIM_TRACK_FIELDS]={CF_NAME,CF_COLOR,CF_ARM,CF_MIDI_VOLUME};
static const unsigned reclaim_program_fields[RECLAIM_PROGRAM_FIELDS]={CF_PAN,CF_MUTE,CF_SOLO,CF_SOLO_AUDIO,CF_EFFECTIVE_MUTE,CF_SEND1,CF_SEND2,CF_SEND3,CF_SEND4};
static uint32_t reclaim_value(unsigned generation,unsigned index,unsigned field){
 switch(field){
  case CF_COLOR:return 0xff000000u|((generation&0xfffu)<<12)|index;
  case CF_ARM:case CF_MUTE:case CF_SOLO_AUDIO:return (generation+index)&1;
  case CF_SOLO:case CF_EFFECTIVE_MUTE:return (generation+index+1)&1;
  case CF_MIDI_VOLUME:return reclaim_float((float)(index%50)/100.0f);
  case CF_PAN:return reclaim_float((float)((index+generation)%50)/100.0f+0.25f);
  default:return reclaim_float((float)((index+field)%25)/100.0f);
 }
}
static void reclaim_name(unsigned generation,unsigned index,char text[CHANNEL_TEXT],unsigned *length){
 int n=snprintf(text,CHANNEL_TEXT,"Gen%03u-Track%03u",generation,index);*length=(unsigned)n;
}
static void reclaim_birth_generation(unsigned generation){
 for(unsigned i=0;i<RECLAIM_OWNERS;i++){
  uint32_t t=reclaim_owner_address(generation,i,0),p=reclaim_owner_address(generation,i,1);
  channel_owner_birth(t,CO_TRACK);channel_owner_birth(p,CO_PROGRAM);
  for(unsigned f=0;f<RECLAIM_TRACK_FIELDS;f++){
   unsigned field=reclaim_track_fields[f];
   if(field==CF_NAME){char text[CHANNEL_TEXT];unsigned length;reclaim_name(generation,i,text,&length);channel_birth(t,t+channel_offset(field),field,0,text,length,1);}
   else channel_birth(t,t+channel_offset(field),field,reclaim_value(generation,i,field),NULL,0,1);
  }
  for(unsigned f=0;f<RECLAIM_PROGRAM_FIELDS;f++){unsigned field=reclaim_program_fields[f];channel_birth(p,p+channel_offset(field),field,reclaim_value(generation,i,field),NULL,0,1);}
 }
}
static void reclaim_require_generation(const ChannelState *s,unsigned generation){
 for(unsigned i=0;i<RECLAIM_OWNERS;i++){
  uint32_t t=reclaim_owner_address(generation,i,0),p=reclaim_owner_address(generation,i,1);
  unsigned track_owner=channel_owner_id(t),program_owner=channel_owner_id(p);MIRROR_FIELD(f);
  require(track_owner&&program_owner,"churned generation keeps its own live owners");
  require(channel_owner_slot(track_owner)&&channel_owner_slot(program_owner),"a live owner token still names its own slot");
  for(unsigned k=0;k<RECLAIM_TRACK_FIELDS;k++){
   unsigned field=reclaim_track_fields[k];
   require(copy_channel(s,track_owner,field,&f),"every field of the current generation still resolves to its own cell");
   if(field==CF_NAME){char text[CHANNEL_TEXT];unsigned length;reclaim_name(generation,i,text,&length);require(f.length==length&&!strcmp(f.text,text),"reclaimed table returns this cell's own name text");}
   else require(f.bits==reclaim_value(generation,i,field),"reclaimed table returns this cell's own value");
   require(f.property==t+channel_offset(field)&&f.owner_incarnation==track_owner,"resolved cell carries its own property and owner lifetime");
  }
  for(unsigned k=0;k<RECLAIM_PROGRAM_FIELDS;k++){
   unsigned field=reclaim_program_fields[k];
   require(copy_channel(s,program_owner,field,&f)&&f.bits==reclaim_value(generation,i,field),"Program-owned fields of the current generation resolve with their own value");
   require(f.property==p+channel_offset(field)&&f.owner_incarnation==program_owner,"Program cell carries its own property and owner lifetime");
  }
 }
}
static uint32_t reclaim_free_head(const ChannelState *s,uint32_t base){
 for(unsigned n=1;n<65536u;n++){uint32_t a=base+n*0x1030u;if(!atomic_load(s->owner_directory+channel_hash(a)))return a;}
 return 0;
}
static unsigned reclaim_directory_used(const ChannelState *s){unsigned n=0;for(unsigned i=0;i<CHANNEL_HASH;i++)if(atomic_load(s->field_directory+i))n++;return n;}
static unsigned reclaim_owner_directory_used(const ChannelState *s){unsigned n=0;for(unsigned i=0;i<CHANNEL_HASH;i++)if(atomic_load(s->owner_directory+i))n++;return n;}
/* Chain integrity over the whole table, not only the current generation: every
 * cell of a live lifetime must still be reachable through its own property.
 * Zeroing a retired directory word instead of leaving it occupied truncates the
 * chain it sits in, and this is what sees it. */
static void reclaim_require_chains(const ChannelState *s,const char *why){
 unsigned used=atomic_load(&s->fields_used);
 for(unsigned i=0;i<used;i++){
  const ChannelCell *c=s->fields+i;
  unsigned id=atomic_load(&c->owner_incarnation),slot=channel_owner_slot(id);
  if(!atomic_load(&c->live)||!slot)continue;
  if(!atomic_load(&s->owners[slot-1].live)||atomic_load(&s->owners[slot-1].incarnation)!=id)continue;
  require(channel_cell(atomic_load(&c->property))==c,why);
 }
}
static uint32_t cluster_owner_address(unsigned generation,unsigned index){return 0x60000000u+(generation*CLUSTER_OWNERS+index)*0x1000u;}
static const unsigned cluster_fields[CLUSTER_FIELDS]={CF_NAME,CF_COLOR,CF_ARM,CF_MIDI_VOLUME};
static uint32_t cluster_value(unsigned generation,unsigned index,unsigned field){
 switch(field){
  case CF_COLOR:return 0xfe000000u|((generation&0xffu)<<8)|index;
  case CF_ARM:return (generation+index)&1;
  default:return reclaim_float((float)((generation+index)%50)/100.0f);
 }
}
/* Deliberately power-of-two spaced, the case the spread churn above cannot see:
 * every address folds into the same 32 of the 8192 directory buckets, so chains
 * run deep and a retired word sits in the middle of live keys. A rolling window
 * of generations stays alive at once, so a truncated chain loses a key that is
 * still being read rather than one that is already gone. */
static void cluster_birth(unsigned generation){
 for(unsigned i=0;i<CLUSTER_OWNERS;i++){
  uint32_t t=cluster_owner_address(generation,i);channel_owner_birth(t,CO_TRACK);
  for(unsigned f=0;f<CLUSTER_FIELDS;f++){
   unsigned field=cluster_fields[f];
   if(field==CF_NAME){char text[CHANNEL_TEXT];unsigned length;reclaim_name(900+generation,i,text,&length);channel_birth(t,t+channel_offset(field),field,0,text,length,1);}
   else channel_birth(t,t+channel_offset(field),field,cluster_value(generation,i,field),NULL,0,1);
  }
 }
}
static void cluster_require(const ChannelState *s,unsigned generation){
 for(unsigned i=0;i<CLUSTER_OWNERS;i++){
  uint32_t t=cluster_owner_address(generation,i);unsigned owner=channel_owner_id(t);MIRROR_FIELD(f);
  require(owner,"a clustered generation inside the live window keeps its own owner");
  for(unsigned k=0;k<CLUSTER_FIELDS;k++){
   unsigned field=cluster_fields[k];
   require(copy_channel(s,owner,field,&f)&&f.property==t+channel_offset(field)&&f.owner_incarnation==owner,"a clustered live property still resolves to its own cell");
   if(field==CF_NAME){char text[CHANNEL_TEXT];unsigned length;reclaim_name(900+generation,i,text,&length);require(f.length==length&&!strcmp(f.text,text),"a clustered live name still reads its own text");}
   else require(f.bits==cluster_value(generation,i,field),"a clustered live field still reads its own value");
  }
 }
}
static void cluster_phase(ChannelState *ch){
 for(unsigned g=0;g<CLUSTER_GENERATIONS;g++){
  cluster_birth(g);
  if(g>=CLUSTER_LIVE)for(unsigned i=0;i<CLUSTER_OWNERS;i++)channel_owner_death(cluster_owner_address(g-CLUSTER_LIVE,i));
  unsigned lo=g+1>CLUSTER_LIVE?g+1-CLUSTER_LIVE:0;
  for(unsigned gen=lo;gen<=g;gen++)cluster_require(ch,gen);
  reclaim_require_chains(ch,"a retired directory word never truncates the probe chain of a live clustered key");
 }
 for(unsigned g=CLUSTER_GENERATIONS>CLUSTER_LIVE?CLUSTER_GENERATIONS-CLUSTER_LIVE:0;g<CLUSTER_GENERATIONS;g++)for(unsigned i=0;i<CLUSTER_OWNERS;i++)channel_owner_death(cluster_owner_address(g,i));
}
static void channel_reclaim_checks(void){
 MirrorState *save=fixture;CommandState *csave=command_state;fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"reclamation fixtures");
 initial(0);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 ChannelState *ch=&fixture->channel;
 /* App-global owners: born once, never killed, exactly the owner kinds the
  * device probe found at 1 live / 0 dead across two saved-project loads. */
 static const unsigned global_kinds[RECLAIM_GLOBALS]={CO_MIXER,CO_TIMELINE,CO_CLICK,CO_LOOP,CO_AUTOMATION,CO_RECORDING,CO_PROJECT};
 static const unsigned global_fields[RECLAIM_GLOBALS]={CF_MASTER,CF_PLAYING,CF_CLICK,CF_LOOP,CF_AUTOMATION,CF_RECORD_MODE,CF_SELECTION};
 static const uint32_t global_values[RECLAIM_GLOBALS]={0x3f000000u,1,1,0,2,3,0x00c0ffeeu};
 uint32_t global_address[RECLAIM_GLOBALS];unsigned global_owner[RECLAIM_GLOBALS];const ChannelCell *global_cell[RECLAIM_GLOBALS];const ChannelOwner *global_slot[RECLAIM_GLOBALS];
 for(unsigned g=0;g<RECLAIM_GLOBALS;g++){
  global_address[g]=0x30000000u+g*0x1030u;
  channel_owner_birth(global_address[g],global_kinds[g]);
  channel_birth(global_address[g],global_address[g]+channel_offset(global_fields[g]),global_fields[g],global_values[g],NULL,0,1);
  global_owner[g]=channel_owner_id(global_address[g]);
  global_cell[g]=channel_cell(global_address[g]+channel_offset(global_fields[g]));
  global_slot[g]=channel_owner(global_address[g]);
  require(global_owner[g]&&global_cell[g]!=NULL&&global_slot[g]!=NULL,"app-global owner and cell registered");
 }
 unsigned first_track_owner=0,first_track_field=0;uint32_t first_track=0;
 for(unsigned generation=0;generation<RECLAIM_GENERATIONS;generation++){
  reclaim_birth_generation(generation);
  require(atomic_load(&ch->fields_used)<=CHANNEL_FIELDS,"field table never grows past its own capacity");
  require(atomic_load(&ch->owners_used)<=CHANNEL_OWNERS,"owner table never grows past its own capacity");
  reclaim_require_generation(ch,generation);
  for(unsigned g=0;g<RECLAIM_GLOBALS;g++){MIRROR_FIELD(f);require(copy_channel(ch,global_owner[g],global_fields[g],&f)&&f.bits==global_values[g],"app-global field survives every project generation");}
  if(!generation){
   first_track=reclaim_owner_address(0,0,0);first_track_owner=channel_owner_id(first_track);
   const ChannelCell *c=channel_cell(first_track+channel_offset(CF_NAME));require(c!=NULL,"first generation name cell");first_track_field=atomic_load(&c->incarnation);
  }
  for(unsigned i=0;i<RECLAIM_OWNERS;i++){channel_owner_death(reclaim_owner_address(generation,i,0));channel_owner_death(reclaim_owner_address(generation,i,1));}
 }
 /* Both tables are churned well past their own capacity, so neither can have
  * satisfied this by growing. */
 require(RECLAIM_GENERATIONS*RECLAIM_OWNERS*RECLAIM_CELLS>2u*CHANNEL_FIELDS&&RECLAIM_GENERATIONS*RECLAIM_OWNERS*2u>CHANNEL_OWNERS,"churn clearly exceeds both capacities");
 require(atomic_load(&ch->fields_used)==CHANNEL_FIELDS&&channel_reclaims,"field capacity is reached and satisfied by reuse, not by growth");
 require(atomic_load(&ch->owners_used)==CHANNEL_OWNERS&&channel_owner_reclaims,"owner capacity is reached and satisfied by reuse, not by growth");
 reclaim_require_chains(ch,"every live lifetime's cell stays reachable through its own property after the churn");
 unsigned directory=reclaim_directory_used(ch),owner_directory=reclaim_owner_directory_used(ch);
 require(directory<CHANNEL_HASH&&directory<=RECLAIM_DIRECTORY_BOUND,"field directory occupancy stays under its pinned bound");
 require(owner_directory<CHANNEL_HASH&&owner_directory<=RECLAIM_OWNER_DIRECTORY_BOUND,"owner directory occupancy stays under its pinned bound, near the owner table it describes");
 require(!channel_refusals&&!channel_owner_refusals,"no birth in the whole churn was refused");
 /* A directory this full has windows with no empty word left. channel_birth only
  * ever stores a key inside its own probe window, so a full walk with no match
  * is proof of absence; answering "indeterminate" there would silently disable
  * unrelated_source_key's hook-admission fast path exactly when the table is
  * busiest. */
 for(unsigned i=0;i<4096;i++)require(known_channel_key(0x7f000000u+i*0x40u)==0,"an absent key is still reported absent once the directory has no empty word left");
 for(unsigned f=0;f<CHANNEL_FIELD_KINDS;f++)require(!atomic_load(ch->errors+f),"sustained churn leaves no latched channel error");
 /* Only dead lifetimes are reused: each global owner is still the same slot
  * with the same token, and each global cell is still the same slot. */
 for(unsigned g=0;g<RECLAIM_GLOBALS;g++){
  require(channel_owner(global_address[g])==global_slot[g]&&channel_owner_id(global_address[g])==global_owner[g],"an app-global owner slot is never taken as a reclamation victim");
  require(channel_cell(global_address[g]+channel_offset(global_fields[g]))==global_cell[g],"an app-global cell is never taken as a reclamation victim");
  require(atomic_load(&global_cell[g]->owner_incarnation)==global_owner[g]&&atomic_load(&global_cell[g]->field)==global_fields[g],"app-global cell keeps its own identity across the churn");
 }
 /* A retired lifetime resolves to nothing rather than to whatever now owns its
  * old slot, and the slot it used has demonstrably been handed to someone else. */
 MIRROR_FIELD(stale);
 require(first_track_owner&&!copy_channel(ch,first_track_owner,CF_NAME,&stale),"a retired owner token reads no value, not the new lifetime's");
 require(channel_owner_slot(first_track_owner)&&atomic_load(&ch->owners[channel_owner_slot(first_track_owner)-1].incarnation)!=first_track_owner,"the retired owner's slot now carries a different lifetime token");
 require(!channel_cell(first_track+channel_offset(CF_NAME))||atomic_load(&channel_cell(first_track+channel_offset(CF_NAME))->owner_incarnation)!=first_track_owner,"the retired property no longer resolves to a cell of its old lifetime");
 /* In-flight command safety: the pair a queued gesture pins cannot be honoured
  * against whatever occupies the slots now. This is the triple request_cell,
  * master_copy and midi_current all carry. */
 {
  ChannelCell *c=channel_cell(first_track+channel_offset(CF_NAME));
  int honoured=c&&atomic_load(&c->live)&&atomic_load(&c->field)==CF_NAME&&atomic_load(&c->owner_incarnation)==first_track_owner&&atomic_load(&c->incarnation)==first_track_field&&channel_owner_id(first_track)==first_track_owner;
  require(!honoured,"a gesture pinned to a reclaimed cell's owner and field incarnation cannot submit against the new occupant");
 }
 /* The clustering regime: long chains, where zeroing a retired directory word
  * instead of leaving it occupied loses live keys. */
 cluster_phase(ch);
 require(!channel_refusals&&!channel_owner_refusals,"the clustered churn is satisfied without a single refusal");
 /* Exhaustion that reuse cannot satisfy must stay visible: revive every
  * lifetime so the scan has no victim at all. */
 unsigned refusals=channel_refusals;
 uint32_t spare=cluster_owner_address(CLUSTER_GENERATIONS,0),witness=cluster_owner_address(CLUSTER_GENERATIONS,1);
 char text[CHANNEL_TEXT],seen[CHANNEL_TEXT];unsigned length,seen_length;
 reclaim_name(999,1,text,&length);reclaim_name(998,2,seen,&seen_length);
 channel_owner_birth(spare,CO_TRACK);channel_owner_birth(witness,CO_TRACK);
 unsigned spare_owner=channel_owner_id(spare),witness_owner=channel_owner_id(witness);
 require(spare_owner&&witness_owner,"spare and witness owners registered before the forced refusal");
 channel_birth(witness,witness+channel_offset(CF_NAME),CF_NAME,0,seen,seen_length,1);
 MIRROR_FIELD(named);
 require(copy_channel(ch,witness_owner,CF_NAME,&named)&&!strcmp(named.text,seen),"witness name registered before the forced refusal");
 /* Give every cell and every owner a live lifetime, so the scan has no victim
  * at all and the birth genuinely cannot be satisfied. Both tables are put back
  * exactly as they were afterwards. */
 unsigned used=atomic_load(&ch->owners_used);
 uint32_t *held_cell=malloc(CHANNEL_FIELDS*sizeof(uint32_t)),*held_owner=malloc(CHANNEL_OWNERS*sizeof(uint32_t));
 require(held_cell!=NULL&&held_owner!=NULL,"forced-refusal scratch");
 for(unsigned i=0;i<CHANNEL_FIELDS;i++){held_cell[i]=atomic_load(&ch->fields[i].owner_incarnation);atomic_store(&ch->fields[i].owner_incarnation,spare_owner);}
 for(unsigned i=0;i<used;i++){held_owner[i]=atomic_load(&ch->owners[i].live);atomic_store(&ch->owners[i].live,1);}
 channel_birth(spare,spare+channel_offset(CF_NAME),CF_NAME,0,text,length,1);
 require(channel_refusals>refusals&&atomic_load(ch->errors+CF_NAME)==CH_CAPACITY,"an unsatisfiable birth is refused and recorded where the reader can see it");
 require(!copy_channel(ch,spare_owner,CF_NAME,&named),"the refused cell itself is unavailable, which is the dash the owner sees");
 for(unsigned i=0;i<CHANNEL_FIELDS;i++)atomic_store(&ch->fields[i].owner_incarnation,held_cell[i]);
 for(unsigned i=0;i<used;i++)atomic_store(&ch->owners[i].live,held_owner[i]);
 free(held_cell);free(held_owner);
 require(copy_channel(ch,witness_owner,CF_NAME,&named)&&!strcmp(named.text,seen),"a recorded capacity refusal no longer dashes the names that did register");
 channel_birth(spare,spare+channel_offset(CF_NAME),CF_NAME,0,text,length,1);
 require(copy_channel(ch,channel_owner_id(spare),CF_NAME,&named)&&named.length==length&&!strcmp(named.text,text),"the same birth succeeds once lifetimes are retired again");
 require(atomic_load(ch->errors+CF_NAME)==CH_CAPACITY,"the refusal stays recorded after the later success instead of being wiped");
 /* Owner-slot reuse is safe for a reader only because of seven token guards:
  * three in copy_channel (owner level, and the cell-level match before and
  * inside the seqlock window) and four in copy_general (mixer, sends, editor,
  * zoom). None of them does any work unless a retired token's slot is actually
  * occupied by a LIVE lifetime, and the churn above never leaves one in that
  * state by arithmetic accident. Build it on purpose, with the real allocator:
  * pick an address whose directory chain starts on an empty word, so the
  * in-window victim rule cannot fire and the cursor scan chooses the slot. */
 uint32_t alias_a=reclaim_free_head(ch,0x48000000u),alias_b=reclaim_free_head(ch,0x49000000u);
 uint32_t alias_p=reclaim_free_head(ch,0x4a000000u),alias_q=reclaim_free_head(ch,0x4b000000u);
 require(alias_a&&alias_b&&alias_p&&alias_q,"four addresses whose owner chains start empty");
 /* Track pair: retire a lifetime, then hand its own slot to a live lifetime of
  * the same kind, with the retired lifetime's CF_NAME cell sitting exactly
  * where the new lifetime's address resolves it. */
 channel_owner_birth(alias_a,CO_TRACK);
 unsigned alias_old=channel_owner_id(alias_a),alias_slot=channel_owner_slot(alias_old);
 require(alias_old&&alias_slot,"retired-lifetime owner registered");
 char aliasname[CHANNEL_TEXT];unsigned aliaslen;reclaim_name(996,1,aliasname,&aliaslen);
 channel_birth(alias_a,alias_b+channel_offset(CF_NAME),CF_NAME,0,aliasname,aliaslen,1);
 channel_owner_death(alias_a);
 channel_owner_cursor=alias_slot-1;channel_owner_birth(alias_b,CO_TRACK);
 unsigned alias_new=channel_owner_id(alias_b);
 require(alias_new&&channel_owner_slot(alias_new)==alias_slot&&alias_new!=alias_old,"the retired token's own slot now carries a different live lifetime of the same kind");
 ChannelCell *aliased=channel_cell(alias_b+channel_offset(CF_NAME));
 require(aliased!=NULL&&atomic_load(&aliased->live)&&atomic_load(&aliased->owner_incarnation)==alias_old,"the retired lifetime's cell still sits where the new lifetime's address resolves it");
 require(!copy_channel(ch,alias_old,CF_NAME,&named),"a retired owner token does not read the cell that its own reused slot now resolves to");
 require(!copy_channel(ch,alias_new,CF_NAME,&named),"a live lifetime does not adopt the retired lifetime's cell left at its own property");
 channel_birth(alias_b,alias_b+channel_offset(CF_COLOR),CF_COLOR,0xfd000000u,NULL,0,1);
 require(copy_channel(ch,alias_new,CF_COLOR,&named)&&named.bits==0xfd000000u,"the live lifetime still reads its own cells, so the two checks above are not passing on a dead path");
 /* Program pair, for the send-owner guard, which additionally matches address
  * and kind: the retired token must name a slot holding a live Program at the
  * same address. */
 channel_owner_birth(alias_p,CO_PROGRAM);
 unsigned prog_old=channel_owner_id(alias_p),prog_slot=channel_owner_slot(prog_old);
 require(prog_old&&prog_slot,"retired Program lifetime registered");
 channel_owner_death(alias_p);
 channel_owner_cursor=prog_slot-1;channel_owner_birth(alias_q,CO_PROGRAM);
 unsigned prog_new=channel_owner_id(alias_q);
 require(prog_new&&channel_owner_slot(prog_new)==prog_slot&&prog_new!=prog_old,"the retired Program token's slot now carries a different live Program");
 /* copy_general: every cached owner word the bridge reads is a token, and each
  * one is matched against the slot it names. */
 GeneralState *g=&fixture->general;MIRROR_COPY(view);
 atomic_store(&fixture->ready,1);atomic_store(&g->sends_revision,2);atomic_store(&g->sends_epoch,atomic_load(&fixture->epoch));
 atomic_store(&g->sends_programs[0],alias_q);atomic_store(&g->sends_owners[0],prog_new);
 atomic_store(&g->sends_mixer,global_owner[0]);atomic_store(&ch->mixer_owner,global_owner[0]);
 atomic_store(&g->editor_owner,alias_new);atomic_store(&g->zoom_owner,alias_new);
 require(copy_mirror(fixture,&view),"reader snapshot over the aliased tables");
 require(view.editor_owner==alias_new&&view.zoom_owner==alias_new,"a live editor and zoom token still resolve");
 require(view.send_owners[0]==prog_new&&view.send_programs[0]==alias_q,"a live send owner still resolves");
 atomic_store(&g->editor_owner,alias_old);atomic_store(&g->zoom_owner,alias_new);
 require(copy_mirror(fixture,&view)&&!view.editor_owner&&view.zoom_owner==alias_new,"a retired editor token does not resolve through its reused slot");
 atomic_store(&g->editor_owner,alias_new);atomic_store(&g->zoom_owner,alias_old);
 require(copy_mirror(fixture,&view)&&view.editor_owner==alias_new&&!view.zoom_owner,"a retired zoom token does not resolve through its reused slot");
 atomic_store(&g->zoom_owner,alias_new);
 atomic_store(&g->sends_owners[0],prog_old);
 require(copy_mirror(fixture,&view)&&!view.send_owners[0]&&!view.send_programs[0],"a retired send-owner token does not resolve through its reused slot at the same address");
 atomic_store(&g->sends_owners[0],prog_new);
 atomic_store(&g->sends_mixer,alias_old);atomic_store(&ch->mixer_owner,alias_old);
 require(copy_mirror(fixture,&view)&&!view.send_owners[0]&&!view.send_programs[0],"a retired mixer token does not open the send block through its reused slot");
 atomic_store(&g->sends_mixer,global_owner[0]);atomic_store(&ch->mixer_owner,global_owner[0]);
 require(copy_mirror(fixture,&view)&&view.send_owners[0]==prog_new,"the send block reopens for the live mixer token, so the two checks above are not passing on a dead path");
 atomic_store(&fixture->ready,0);atomic_store(&g->sends_revision,0);atomic_store(&g->editor_owner,0);atomic_store(&g->zoom_owner,0);
 atomic_store(&g->sends_owners[0],0);atomic_store(&g->sends_programs[0],0);atomic_store(&g->sends_mixer,0);atomic_store(&ch->mixer_owner,0);
 /* Reader contract: a cell re-identified under its seqlock between the probe
  * match and the payload read is never attributed to the matched property. */
 unsigned pinned=channel_owner_id(witness);
 channel_read_target=channel_cell(witness+channel_offset(CF_NAME));
 channel_read_property=spare+0x5f0;channel_read_owner=channel_owner_id(spare);channel_read_probe=1;
 atomic_store(ch->errors+CF_NAME,CH_OK);
 require(pinned&&channel_read_target&&!copy_channel(ch,pinned,CF_NAME,&named),"a cell rewritten under its seqlock is not attributed to the previously matched property");
 require(!channel_read_probe,"the reader reached the re-identification seam");
 /* Same seam, owner lifetime only: the cell keeps its property and its field,
  * so the in-window owner match is the single thing standing between the reader
  * and a payload the token it asked with no longer owns. */
 channel_birth(witness,witness+channel_offset(CF_NAME),CF_NAME,0,seen,seen_length,1);
 pinned=channel_owner_id(witness);channel_read_target=channel_cell(witness+channel_offset(CF_NAME));
 channel_read_owner=alias_new;channel_read_probe=2;
 require(pinned&&channel_read_target&&copy_channel(ch,pinned,CF_NAME,&named)==0,"a cell whose owner lifetime is rewritten under its seqlock is not attributed to the token that matched it");
 require(!channel_read_probe,"the reader reached the owner-only re-identification seam");
 require(channel_owner_publishes>RECLAIM_GENERATIONS*RECLAIM_OWNERS*2u,"every owner publication in the churn went through the store-order seam");
 command_close();require(command_finalize(),"reclamation source finite closure");free(fixture);free(command_state);fixture=save;mirror_state=save;command_state=csave;dispatched_calls=0;
 printf("PASS channel table reclamation:%u generations x%u owners =%u field and%u owner births against%u/%u capacities stay capped and are satisfied by reuse, every live property and owner reads its own value through the reader, app-global owners and cells are never victims, a retired token whose own slot now holds a live same-kind lifetime resolves to nothing through copy_channel and through every cached mixer/send/editor/zoom word, pinned gestures resolve to nothing, a clustered %u-generation churn keeps every chain intact, %u field and %u owner directory words of%u stay occupied, and an unsatisfiable birth stays recorded\n",RECLAIM_GENERATIONS,RECLAIM_OWNERS*2u,RECLAIM_GENERATIONS*RECLAIM_OWNERS*RECLAIM_CELLS,RECLAIM_GENERATIONS*RECLAIM_OWNERS*2u,CHANNEL_FIELDS,CHANNEL_OWNERS,CLUSTER_GENERATIONS,directory,owner_directory,CHANNEL_HASH);
}
/* The reuse path republishes a reclaimed cell's identity and payload inside one
 * seqlock window. Nothing above can see that: under the reclamation predicate a
 * victim's owner is already dead, so every consumer refuses it on owner
 * liveness whether or not the window exists. These two threads look at the
 * cells directly, which is the contract the cross-process bridge depends on. */
#define SEQLOCK_FILL_OWNERS 128u
#define SEQLOCK_OWNER_CELLS 32u
#define SEQLOCK_ROUNDS 3000u
static _Atomic unsigned seqlock_stop,seqlock_reads,seqlock_torn,seqlock_writes;
static uint32_t seqlock_property(unsigned serial){return 0x70000000u+serial*0x30u;}
static void seqlock_text(uint32_t property,uint32_t words[CHANNEL_TEXT/4]){for(unsigned i=0;i<CHANNEL_TEXT/4;i++)words[i]=property*(i+1)+i;}
/* Every cell this fixture creates is a CF_NAME whose whole payload is a pure
 * function of its own property, so one stable window showing one lifetime's
 * identity over another lifetime's payload is a torn publication. */
static void *seqlock_reader(void *arg){
 const ChannelState *s=arg;uint32_t expect[CHANNEL_TEXT/4],words[CHANNEL_TEXT/4];unsigned base=0;
 while(!atomic_load_explicit(&seqlock_stop,memory_order_acquire)){
  base=(base+256u)&(CHANNEL_FIELDS-1u);
  for(unsigned k=0;k<256u;k++){
   unsigned i=(base+k)&(CHANNEL_FIELDS-1u);
   const ChannelCell *c=s->fields+i;
   unsigned rev=atomic_load_explicit(&c->revision,memory_order_acquire);
   if(!rev||(rev&1))continue;
   uint32_t property=atomic_load_explicit(&c->property,memory_order_relaxed),bits=atomic_load_explicit(&c->bits,memory_order_relaxed);
   unsigned field=atomic_load_explicit(&c->field,memory_order_relaxed),live=atomic_load_explicit(&c->live,memory_order_relaxed),length=atomic_load_explicit(&c->length,memory_order_relaxed);
   for(unsigned w=0;w<CHANNEL_TEXT/4;w++)words[w]=atomic_load_explicit(c->text+w,memory_order_relaxed);
   atomic_thread_fence(memory_order_acquire);
   if(rev!=atomic_load_explicit(&c->revision,memory_order_relaxed))continue;
   atomic_fetch_add_explicit(&seqlock_reads,1,memory_order_relaxed);
   if(!live)continue;
   seqlock_text(property,expect);
   if(field!=CF_NAME||bits!=property||length!=CHANNEL_TEXT||memcmp(words,expect,sizeof(words)))atomic_fetch_add_explicit(&seqlock_torn,1,memory_order_relaxed);
  }
 }
 return NULL;
}
/* Two writers on one cell. channel_write acquires the seqlock with a CAS and
 * the loser drops its update, so a stable window always shows one writer's
 * whole payload. A plain load-then-store lets both proceed and mixes them. */
static ChannelCell *seqlock_shared;
static void *seqlock_writer(void *arg){
 uintptr_t which=(uintptr_t)arg;uint32_t words[CHANNEL_TEXT/4];
 for(unsigned n=0;n<200000u&&!atomic_load_explicit(&seqlock_stop,memory_order_acquire);n++){
  uint32_t property=seqlock_property(which?7u:9u);
  seqlock_text(property,words);
  channel_write(seqlock_shared,property,(const char*)words,CHANNEL_TEXT,0);
  atomic_fetch_add_explicit(&seqlock_writes,1,memory_order_relaxed);
 }
 return NULL;
}
static void *seqlock_shared_reader(void *unused){
 (void)unused;uint32_t words[CHANNEL_TEXT/4],expect[CHANNEL_TEXT/4];
 while(!atomic_load_explicit(&seqlock_stop,memory_order_acquire)){
  const ChannelCell *c=seqlock_shared;
  unsigned rev=atomic_load_explicit(&c->revision,memory_order_acquire);if(!rev||(rev&1))continue;
  uint32_t bits=atomic_load_explicit(&c->bits,memory_order_relaxed);
  for(unsigned w=0;w<CHANNEL_TEXT/4;w++)words[w]=atomic_load_explicit(c->text+w,memory_order_relaxed);
  atomic_thread_fence(memory_order_acquire);
  if(rev!=atomic_load_explicit(&c->revision,memory_order_relaxed))continue;
  atomic_fetch_add_explicit(&seqlock_reads,1,memory_order_relaxed);
  seqlock_text(bits,expect);
  if(memcmp(words,expect,sizeof(words)))atomic_fetch_add_explicit(&seqlock_torn,1,memory_order_relaxed);
 }
 return NULL;
}
static void channel_seqlock_checks(void){
 MirrorState *save=fixture;CommandState *csave=command_state;fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"seqlock fixtures");
 initial(0);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 ChannelState *ch=&fixture->channel;
 atomic_store(&seqlock_stop,0);atomic_store(&seqlock_reads,0);atomic_store(&seqlock_torn,0);atomic_store(&seqlock_writes,0);
 uint32_t words[CHANNEL_TEXT/4];unsigned serial=0;
 for(unsigned o=0;o<SEQLOCK_FILL_OWNERS;o++){
  uint32_t owner=0x20000000u+o*0x1030u;channel_owner_birth(owner,CO_TRACK);
  for(unsigned k=0;k<SEQLOCK_OWNER_CELLS;k++){uint32_t property=seqlock_property(serial++);seqlock_text(property,words);channel_birth(owner,property,CF_NAME,property,(const char*)words,CHANNEL_TEXT,1);}
  channel_owner_death(owner);
 }
 require(atomic_load(&ch->fields_used)==CHANNEL_FIELDS&&!channel_refusals,"seqlock fixture fills the field table exactly once");
 pthread_t reader;require(!pthread_create(&reader,NULL,seqlock_reader,ch),"seqlock reader thread");
 unsigned before=channel_reclaims;
 for(unsigned round=0;round<SEQLOCK_ROUNDS;round++){
  uint32_t owner=0x21000000u+round*0x1030u;channel_owner_birth(owner,CO_TRACK);
  for(unsigned k=0;k<SEQLOCK_OWNER_CELLS;k++){uint32_t property=seqlock_property(serial++);seqlock_text(property,words);channel_birth(owner,property,CF_NAME,property,(const char*)words,CHANNEL_TEXT,1);}
  channel_owner_death(owner);
 }
 atomic_store_explicit(&seqlock_stop,1,memory_order_release);require(!pthread_join(reader,NULL),"seqlock reader joins");
 require(channel_reclaims-before>=SEQLOCK_ROUNDS*SEQLOCK_OWNER_CELLS/2u,"the churn ran against a full table and reclaimed");
 require(atomic_load(&seqlock_reads)>1000u,"the reader observed stable windows while the reclaimer ran");
 require(!atomic_load(&seqlock_torn),"no stable window ever shows one lifetime's identity over another lifetime's payload");
 /* Second phase: two writers, one cell. */
 atomic_store(&seqlock_stop,0);atomic_store(&seqlock_reads,0);atomic_store(&seqlock_torn,0);
 uint32_t host=0x22000000u;channel_owner_birth(host,CO_TRACK);
 uint32_t shared=seqlock_property(7u);seqlock_text(shared,words);
 channel_birth(host,shared,CF_NAME,shared,(const char*)words,CHANNEL_TEXT,1);
 seqlock_shared=channel_cell(shared);require(seqlock_shared!=NULL,"shared write target");
 pthread_t w0,w1,r0;
 require(!pthread_create(&r0,NULL,seqlock_shared_reader,NULL)&&!pthread_create(&w0,NULL,seqlock_writer,(void*)0)&&!pthread_create(&w1,NULL,seqlock_writer,(void*)1),"two writers and a reader on one cell");
 require(!pthread_join(w0,NULL)&&!pthread_join(w1,NULL),"both writers finish");
 atomic_store_explicit(&seqlock_stop,1,memory_order_release);require(!pthread_join(r0,NULL),"shared reader joins");
 require(atomic_load(&seqlock_writes)>=2u*200000u&&atomic_load(&seqlock_reads)>1000u,"both writers ran against a live reader");
 require(!atomic_load(&seqlock_torn),"two concurrent writers never leave a stable window holding a mixed payload");
 command_close();require(command_finalize(),"seqlock source finite closure");free(fixture);free(command_state);fixture=save;mirror_state=save;command_state=csave;dispatched_calls=0;
 printf("PASS reclamation seqlock discipline:%u concurrent reads over a table being reclaimed under it and%u concurrent writes to one cell leave no stable window holding a torn or mixed payload\n",atomic_load(&seqlock_reads),atomic_load(&seqlock_writes));
}
static unsigned char jog_audio_object[0x400],jog_sequencer_object[0x408],jog_timeline_object[0x200],jog_time_object[0x100],jog_async_object[0x48];
static uint32_t jog_native_slots[COMMAND_SLOTS][5],*jog_native_slot;static unsigned jog_fixture_ops[COMMAND_SLOTS],jog_fixture_op,jog_fixture_fault,jog_fixture_calls,jog_fixture_epoch,jog_fixture_defer;
/* An unrelated listener's queue published on the same generic seam while a jog
 * submission is open, the way master_unrelated_slot models it for master. */
static uint32_t jog_unrelated_slot[5]={0x1477d3c,0x1472d90,0,0,0};static unsigned jog_fixture_foreign;
static void *jog_audio_fixture(void *unused){
 (void)unused;union {uint64_t align;unsigned char bytes[sizeof(Context)+0x30];} frames;
 Context *enter=(Context*)(frames.bytes+0x30),*done=(Context*)frames.bytes;
 if(jog_fixture_epoch)atomic_store(&fixture->epoch,jog_fixture_epoch);
 *enter=context();enter->r[0]=ptr(jog_native_slot+2);call(enter,JG_BAR_ENTER+3*(jog_fixture_op-JOG_BAR));
 *done=context();done->r[4]=ptr(jog_native_slot+2);call(done,JG_BAR_DONE+3*(jog_fixture_op-JOG_BAR));return NULL;
}
static void jog_dispatch_fixture(uint32_t x,unsigned op,int32_t delta){
 unsigned index=jog_fixture_calls++;jog_native_slot=jog_native_slots[index];jog_fixture_ops[index]=op;jog_fixture_op=op;unsigned unit=op-JOG_BAR;
 require(x==ptr(jog_async_object)&&command_jog_delta((uint32_t)delta),"signed relative native facade ABI");
 jog_native_slot[0]=jog_invokers[unit];jog_native_slot[1]=jog_managers[unit];jog_native_slot[2]=x+8;jog_native_slot[3]=x;jog_native_slot[4]=(uint32_t)delta;
 if(jog_fixture_fault)jog_native_slot[1]+=4;
 if(jog_fixture_foreign){
  Context u=context();u.r[7]=ptr(queue_object)+0x14;u.r[9]=ptr(project_object)|1;u.r[10]=ptr(jog_unrelated_slot);call(&u,JG_PUBLISH);
  require(!atomic_load(&command_state->trace_error),"unrelated native history publication on the shared seam must not poison an open jog submission");
 }
 Context c=context();c.r[7]=ptr(queue_object)+0xfc;c.r[9]=(x+8)|1;c.r[10]=ptr(jog_native_slot);call(&c,JG_PUBLISH);
 if(jog_fixture_fault||jog_fixture_defer){c.r[0]=1;call(&c,JG_BAR_RESULT+3*unit);return;}
 /* Real audio may finish as soon as publication resumes, before the native
  * facade reaches its enqueue result or returns to the UI caller. */
 pthread_t audio;require(!pthread_create(&audio,NULL,jog_audio_fixture,NULL)&&!pthread_join(audio,NULL),"actual separate ARM callback source thread (native navigation body substituted)");
 unsigned at=command_find(command_state,jog_submission);require(at<COMMAND_SLOTS&&atomic_load(&command_state->slots[at].done)==jog_submission&&!atomic_load(active_request+at),"early callback retires only audio payload matcher");
 command_retire();require(!atomic_load(&command_state->slots[at].sealed),"UI facade flight prevents sealing/reuse before enqueue result and return");
 c.r[0]=1;call(&c,JG_BAR_RESULT+3*unit);require(!atomic_load(&command_state->trace_error),"completed callback retains independent UI enqueue-result attribution");
}
static void jog_nested_fixture(unsigned index){
 union {uint64_t align;unsigned char bytes[sizeof(Context)+0x30];} frames;
 Context *enter=(Context*)(frames.bytes+0x30),*done=(Context*)frames.bytes;
 unsigned unit=jog_fixture_ops[index]-JOG_BAR;*enter=context();enter->r[0]=ptr(jog_native_slots[index]+2);call(enter,JG_BAR_ENTER+3*unit);

 *done=context();done->r[4]=ptr(jog_native_slots[index]+2);call(done,JG_BAR_DONE+3*unit);
 require(atomic_load(&command_state->slots[index].done)==index+1,"each nested native payload completes independently");

}
static void *jog_nested_audio(void *index){jog_nested_fixture((unsigned)(uintptr_t)index);return NULL;}
static void navigation_source_checks(uint32_t timeline){
 Context c=context();c.r[0]=timeline;call(&c,CH_TIMELINE_BIRTH);atomic_store(&command_state->navigation_watch,1);
 c=context();c.r[4]=timeline;c.r[12]=0;call(&c,JN_PLAYING);call(&c,JN_ADVANCE);require(!atomic_load(&command_state->navigation_count),"Timeline +44 independent advance is not playing progress");
 c.r[12]=1;call(&c,JN_PLAYING);call(&c,JN_ADVANCE);require(atomic_load(&command_state->navigation_count)==1&&atomic_load(&command_state->navigation_owner)==channel_owner_id(timeline)&&!(atomic_load(&command_state->navigation_revision)&1),"same native playing frame and owned Timeline publishes coherent monotonic progress");
 call(&c,JN_ADVANCE);require(atomic_load(&command_state->navigation_count)==1,"unpaired store cannot invent another playing advance");
 call(&c,JN_PLAYING);channel_owner_death(timeline);call(&c,JN_ADVANCE);require(atomic_load(&command_state->navigation_count)==1,"retired Timeline frame cannot release navigation");atomic_store(&command_state->navigation_watch,0);
}
static void jog_checks(void){
 MirrorState *saved=fixture;CommandState *saved_command=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"jog component state");
 for(unsigned trial=0;trial<6;trial++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);component_jog_dispatch=jog_dispatch_fixture;jog_fixture_fault=trial==3;jog_fixture_epoch=trial==2?2:0;jog_fixture_calls=0;jog_fixture_defer=trial==5;jog_fixture_foreign=trial==0;
  uint32_t a=ptr(jog_audio_object),s=ptr(jog_sequencer_object),t=ptr(jog_timeline_object),u=ptr(jog_time_object),x=ptr(jog_async_object);
  put(ptr(root_object)+0x2e4,a);put(a+0xa8,ptr(queue_object));put(a+0x3d8,x);put(a+0x3bc,s);put(a+0x3a4,t);put(a+0x3a0,u);put(x,0x689fdac);put(x+8,ptr(queue_object));put(x+0x40,s);put(t,0x69091b0);put(s+0x400,t);put(s+0x3fc,u);
  seed_fixture(0,0,0x3f000000,1);load_fixture();
  CommandRequest r={.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.bits=(uint32_t)(trial==1?-2:3),.expires=120000,.reserved=JOG_BAR+(trial%3),.project_owner=channel_owner_id(project)};
  if(trial==0)navigation_source_checks(t);
  if(trial==4){Context d=context();d.r[0]=x;call(&d,JG_DESTROY);}
  if(trial==5){
   for(unsigned i=0;i<3;i++){r.seq=i+1;r.reserved=i==2?JOG_PULSE:JOG_BEAT;r.bits=(uint32_t)(i==0?-1:i==1?1:7);require(command_publish_request_at(command_state,&r,i),"ordered independent jog publication");}
   Context drain=context();drain.r[0]=ptr(queue_object)+4;
   for(unsigned i=0;i<3;i++){
    call(&drain,COMMAND_DRAIN);require(jog_fixture_calls==i+1&&!atomic_load(&command_state->trace_error),"owner dispatch waits prior native DONE");
    call(&drain,COMMAND_DRAIN);require(jog_fixture_calls==i+1,"facade return does not release next jog");
    require((int32_t)jog_native_slots[i][4]==(i==0?-1:i==1?1:7),"opposite and accelerated signed order retained");
    pthread_t audio;require(!pthread_create(&audio,NULL,jog_nested_audio,(void*)(uintptr_t)i)&&!pthread_join(audio,NULL),"actual ARM callback thread with substituted native body");
    command_retire();require(atomic_load(&command_state->slots[i].sealed)==i+1&&command_publish_settlement(command_state,i+1),"completed native callback seals before settlement");command_retire();
   }
   command_retire();require(command_idle(command_state),"all independently completed jogs reclaimed");continue;
  }
  unsigned at=submit_fixture(r);CommandSlot *slot=command_state->slots+at;
  if(trial>=3){require(!atomic_load(&slot->done)&&((trial==3&&atomic_load(&command_state->trace_error)==C_TRACE_AMBIGUOUS)||(trial==4&&jog_fixture_calls==0&&atomic_load(&slot->rejected)==C_NATIVE_MEMBERSHIP)),"wrong native callable/cancelled owner cannot certify jog");continue;}
  require(jog_fixture_calls==1&&atomic_load(&slot->done)==1&&!atomic_load(&slot->rejected)&&!atomic_load(&command_state->trace_error),"relative native callback completion accepts no-op without fake position commit");
  unsigned seen[3]={0};for(unsigned lane=0;lane<COMMAND_LANES;lane++)for(unsigned j=0;j<atomic_load(&slot->lanes[lane].published);j++){CommandEvent e;require(command_event_read(slot->lanes+lane,1,j,&e),"jog immutable source event");if(e.kind>=CE_JOG_ENQUEUE&&e.kind<=CE_JOG_DONE){seen[e.kind-CE_JOG_ENQUEUE]++;require(e.capture==ptr(jog_native_slot+2)&&e.bits==r.bits&&e.program==x,"exact native payload/signed units");require(e.revision==(e.kind==CE_JOG_ENQUEUE?1:jog_fixture_epoch?2:1),"submission and actual execution epoch remain distinct");}require(e.kind!=CE_COMMIT,"no manufactured changed-position acknowledgement for native no-op");}
  require(seen[0]==1&&seen[1]==1&&seen[2]==1,"enqueue/entry/completion all separately observed");command_retire();require(atomic_load(&slot->sealed)==1&&command_publish_settlement(command_state,1),"jog seals only after callback source leaves");command_retire();require(command_idle(command_state),"jog settled and reclaimed same protocol");
 }
 /* The data-wheel lane is not the jog lane. It never calls jog_resolve, so a
  * retired AsyncSequencer cannot stop it, and a retained native jog awaiting its
  * audio-thread DONE neither blocks it nor is completed by it. */
 {
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
  component_jog_dispatch=jog_dispatch_fixture;component_wheel_call=wheel_call_fixture;
  jog_fixture_fault=0;jog_fixture_epoch=0;jog_fixture_calls=0;jog_fixture_defer=1;
  uint32_t a=ptr(jog_audio_object),sq=ptr(jog_sequencer_object),tl=ptr(jog_timeline_object),tm=ptr(jog_time_object),x=ptr(jog_async_object);
  put(ptr(root_object)+0x2e4,a);put(a+0xa8,ptr(queue_object));put(a+0x3d8,x);put(a+0x3bc,sq);put(a+0x3a4,tl);put(a+0x3a0,tm);put(x,0x689fdac);put(x+8,ptr(queue_object));put(x+0x40,sq);put(tl,0x69091b0);put(sq+0x400,tl);put(sq+0x3fc,tm);
  seed_fixture(0,0,0x3f000000,1);load_fixture();
  put(0x6b6f798,wheel_focus_receiver);put(wheel_focus_receiver,wheel_focus_vtables[2]);
  put(wheel_focus_receiver+WHEEL_FOCUSED_OFFSET,wheel_focus_focused);put(wheel_focus_focused+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG);
  CommandRequest base={.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.expires=120000};
  CommandRequest jog=base;jog.seq=1;jog.bits=1;jog.reserved=JOG_BEAT;jog.project_owner=channel_owner_id(project);
  CommandRequest data=base;data.seq=2;data.bits=3;data.reserved=JOG_DATA;
  require(command_publish_request_at(command_state,&jog,0),"native jog publishes");
  require(command_publish_request_at(command_state,&data,1),"a data-wheel request does not conflict with a live native jog");
  wheel_calls=0;Context drain=context();drain.r[0]=ptr(queue_object)+4;call(&drain,COMMAND_DRAIN);
  require(jog_fixture_calls==1&&wheel_one(3),"one native jog facade call and one data-wheel call come out of the same drain");
  require(atomic_load(&command_state->slots[1].done)==2&&!atomic_load(&command_state->slots[1].rejected)&&!atomic_load(&command_state->trace_error),"the data-wheel request completes while the native jog is still retained");
  require(!atomic_load(&command_state->slots[0].done)&&atomic_load(&command_state->slots[0].returned)==1,"the retained native jog keeps its own pending callback and is not completed by the data-wheel lane");
  command_retire();require(atomic_load(&command_state->slots[1].sealed)==2&&command_publish_settlement(command_state,2),"the data-wheel request seals and settles without waiting for the jog");
  command_retire();require(atomic_load(&command_state->slots[1].reclaimed)==2&&!atomic_load(&command_state->slots[0].sealed),"only the data-wheel slot is reclaimed");
  component_wheel_call=NULL;
 }
 /* Retiring the AsyncSequencer is exactly what jog_resolve refuses on, so the
  * same drain that rejects a native jog still turns the data wheel. */
 {
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
  component_jog_dispatch=jog_dispatch_fixture;component_wheel_call=wheel_call_fixture;
  jog_fixture_fault=0;jog_fixture_epoch=0;jog_fixture_calls=0;jog_fixture_defer=0;
  uint32_t a=ptr(jog_audio_object),sq=ptr(jog_sequencer_object),tl=ptr(jog_timeline_object),tm=ptr(jog_time_object),x=ptr(jog_async_object);
  put(ptr(root_object)+0x2e4,a);put(a+0xa8,ptr(queue_object));put(a+0x3d8,x);put(a+0x3bc,sq);put(a+0x3a4,tl);put(a+0x3a0,tm);put(x,0x689fdac);put(x+8,ptr(queue_object));put(x+0x40,sq);put(tl,0x69091b0);put(sq+0x400,tl);put(sq+0x3fc,tm);
  seed_fixture(0,0,0x3f000000,1);load_fixture();
  put(0x6b6f798,wheel_focus_receiver);put(wheel_focus_receiver,wheel_focus_vtables[2]);
  put(wheel_focus_receiver+WHEEL_FOCUSED_OFFSET,wheel_focus_focused);put(wheel_focus_focused+WHEEL_FLAGS_OFFSET,WHEEL_VISIBLE_FLAG);
  Context destroy=context();destroy.r[0]=x;call(&destroy,JG_DESTROY);
  CommandRequest base={.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.expires=120000};
  CommandRequest jog=base;jog.seq=1;jog.bits=1;jog.reserved=JOG_BEAT;jog.project_owner=channel_owner_id(project);
  CommandRequest data=base;data.seq=2;data.bits=(uint32_t)-2;data.reserved=JOG_DATA;
  require(command_publish_request_at(command_state,&jog,0)&&command_publish_request_at(command_state,&data,1),"jog and data-wheel requests publish against a retired sequencer");
  wheel_calls=0;jog_fixture_calls=0;Context drain=context();drain.r[0]=ptr(queue_object)+4;call(&drain,COMMAND_DRAIN);
  require(!jog_fixture_calls&&atomic_load(&command_state->slots[0].rejected)==C_NATIVE_MEMBERSHIP,"the retired sequencer is exactly what jog_resolve refuses on");
  require(wheel_one(-2)&&atomic_load(&command_state->slots[1].done)==2&&!atomic_load(&command_state->slots[1].rejected),"the same drain still turns the data wheel, so the data-wheel lane does not consult jog_resolve");
  component_wheel_call=NULL;
 }
 observer_running=0;put(ptr(root_object)+0x2e4,0);free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=saved_command;
 puts("PASS data-wheel requests stay out of the jog lane: no jog_resolve, no facade call, no shared retention, and they complete beside a retained native jog and after its AsyncSequencer is retired (native bodies substituted)");
 puts("PASS exact native queue payload correlation, early callback completion before enqueue result/return, independent UI retirement, signed bar/beat/pulse ABI, actual callback thread, no-op completion, execution epoch, wrong-callable and owner-destruction rejection (native body substituted)");
}
static unsigned char master_fixture_factory[0x60],master_fixture_block[0x60];
static uint32_t master_fixture_slot[5];static unsigned master_fixture_trial,master_fixture_calls;
static uint32_t master_unrelated_slot[2]={0x1477d3c,0x1472d90};
static void *master_audio_fixture(void *unused){
 (void)unused;union {uint64_t align;unsigned char bytes[sizeof(Context)+8];} frames;
 Context *enter=(Context*)(frames.bytes+8),*done=(Context*)frames.bytes;uint32_t m=ptr(jog_audio_object)+0x260;
 *enter=context();enter->r[0]=ptr(master_fixture_slot+2);call(enter,MS_ENTER);
 /* Automation Write armed: a master setter call on another Mixer object inside
  * this command's window is proof it is not this command's own call. */
 if(master_fixture_trial==7){Context foreign=context();foreign.r[0]=ptr(jog_audio_object);foreign.r[1]=0;foreign.d[0]=0x3f400000;call(&foreign,MS_SET_ENTER);foreign=context();foreign.r[4]=ptr(jog_audio_object);call(&foreign,MS_SET_DONE);}
 Context setter=context();setter.r[0]=m;setter.r[1]=0;setter.d[0]=master_fixture_trial==2?0x3e800000:0x3f200000;call(&setter,MS_SET_ENTER);
 if(master_fixture_trial!=1){Context scalar=context();scalar.r[5]=m+0xa8;scalar.d[8]=setter.d[0];call(&scalar,M_FLOAT);}
 setter=context();setter.r[4]=m;call(&setter,MS_SET_DONE);
 *done=context();done->r[4]=ptr(master_fixture_slot+2);call(done,MS_DONE);return NULL;
}
static void master_dispatch_fixture(uint32_t f,unsigned index,float value){
 master_fixture_calls++;uint32_t bits;memcpy(&bits,&value,4);require(f==ptr(master_fixture_factory)&&index==0&&bits==0x3f200000,"actual ARM hard-float factory ABI r0/r1/s0");
 uint32_t b=ptr(master_fixture_block),c=b+0x10,m=ptr(jog_audio_object)+0x260;
 put(c,0x68eeaa8);put(c+0x34,ptr(queue_object));put(c+0x3c,m);put(c+0x40,0);put(c+0x44,bits);
 Context frame=context();frame.r[5]=f;frame.r[4]=b;frame.r[6]=c;call(&frame,MS_CREATE);
 put(c+0x2c,c);put(c+0x30,b);master_fixture_slot[0]=0x13bfd1c;master_fixture_slot[1]=master_fixture_trial==3?0x13bfd64:0x13bfdf0;master_fixture_slot[2]=c+0x34;master_fixture_slot[3]=c;master_fixture_slot[4]=b;
 frame=context();frame.r[7]=ptr(queue_object)+0xfc;frame.r[9]=(c+0x34)|1;frame.r[10]=ptr(master_fixture_slot);call(&frame,JG_PUBLISH);
 if(master_fixture_trial==3)return;
 if(master_fixture_trial==6){call(&frame,JG_PUBLISH);frame.r[0]=1;call(&frame,MS_RESULT);return;}
 if(master_fixture_trial==5){frame.r[0]=1;call(&frame,MS_RESULT);frame.r[7]=ptr(queue_object)+0x14;frame.r[9]=ptr(project_object)|1;frame.r[10]=ptr(master_unrelated_slot);call(&frame,JG_PUBLISH);return;}
 pthread_t audio;require(!pthread_create(&audio,NULL,master_audio_fixture,NULL)&&!pthread_join(audio,NULL),"actual ARM callback thread with substituted native history/Mixer body");
 require(atomic_load(&command_state->slots[0].done)==1&&!atomic_load(active_request),"master callback completes before enqueue result and factory return");
 command_retire();require(!atomic_load(&command_state->slots[0].sealed),"factory flight still prevents early seal/reuse");
 frame=context();frame.r[0]=1;call(&frame,MS_RESULT);
}
static void master_checks(void){
 MirrorState *saved=fixture;CommandState *saved_command=command_state;fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"master state fixture");
 for(unsigned trial=0;trial<8;trial++){
  master_fixture_trial=trial;master_fixture_calls=0;memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);component_master_dispatch=master_dispatch_fixture;
  uint32_t a=ptr(jog_audio_object),m=a+0x260,f=ptr(master_fixture_factory);memset(jog_audio_object,0,sizeof(jog_audio_object));put(ptr(root_object)+0x2e4,a);put(ptr(root_object)+0x318,f);put(a+0xa8,ptr(queue_object));put(m,0x6898b34);put(f,0x6899fa0);put(f+4,ptr(queue_object));put(f+0x10,ptr(queue_object));put(f+0x14,ptr(project_object));put(f+0x30,word(ptr(project_object)+0xab0));put(f+0x40,m);
  Context frame=context();frame.r[0]=m;call(&frame,CH_MIXER_BIRTH);frame.r[4]=m;frame.r[5]=trial==1?0x3f200000:0x3f000000;call(&frame,CH_MASTER_BIRTH);seed_fixture(0,0,0x3f000000,1);load_fixture();
  MIRROR_COPY(out);require(copy_mirror(fixture,&out)&&out.master.available,"actual retained master field joins current embedded Mixer");
  CommandRequest r={.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.bits=0x3f200000,.expires=120000,.before_bits=out.master.bits,.before_revision=out.master.revision,.reserved=CF_MASTER,.field_incarnation=out.master.incarnation,.project_owner=out.project_owner,.global_owner=out.master.owner_incarnation};
  if(trial==4){frame.r[0]=f;call(&frame,MS_FACTORY_DEATH);}
  unsigned at=submit_fixture(r);CommandSlot *slot=command_state->slots+at;
  if(trial==5){pthread_t audio;require(!pthread_create(&audio,NULL,master_audio_fixture,NULL)&&!pthread_join(audio,NULL),"delayed master callback after real factory-return ordering");require(!atomic_load(&command_state->trace_error),"unrelated native history publication must not poison exact master command");}
  if(trial==3||trial==4||trial==6){require(!atomic_load(&slot->done)&&(trial!=4?atomic_load(&command_state->trace_error)==C_TRACE_AMBIGUOUS:!master_fixture_calls&&atomic_load(&slot->rejected)==C_NATIVE_MEMBERSHIP),"wrong callable/dead factory never certify master execution");continue;}
  require(master_fixture_calls==1&&atomic_load(&slot->done)==1&&atomic_load(&slot->returned)==1&&!atomic_load(&slot->rejected)&&!atomic_load(&command_state->trace_error),"independent master UI/audio milestones retain attribution");command_retire();require(atomic_load(&slot->sealed)==1,"master source leaves before seal");
  atomic_store(&fixture->heartbeat,1);require(copy_mirror(fixture,&out),"read actual source publication after body completion");MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);bank_apply(&bank,&out,1);
  in.flights[at]=(InputFlight){.flight=1,.field=CF_MASTER,.bits=r.bits,.expires=120000,.before_revision=r.before_revision,.field_incarnation=r.field_incarnation,.property=m+0xa8,.global_owner=r.global_owner,.global_epoch=r.epoch};
  input_drain(&in,&bank,&out,1);require(!in.error&&in.settled==1,"actual producer events consumed by production master settlement policy");
  require(out.master.bits==(trial==2?0x3e800000:r.bits)&&out.master.revision==(trial==1?r.before_revision:r.before_revision+2),"native no-change and history-adjusted actual source are not desired-value acknowledgements");
  unsigned commits=0,sets=0;for(unsigned lane=0;lane<COMMAND_LANES;lane++)for(unsigned j=0;j<atomic_load(&slot->lanes[lane].published);j++){CommandEvent e;require(command_event_read(slot->lanes+lane,1,j,&e),"master retained event read");commits+=e.kind==CE_MASTER_COMMIT;sets+=e.kind==CE_MASTER_SET;}
  require(commits==(trial==1?0:1)&&sets==1,"no manufactured commit for native no-op, and exactly one setter receipt beside foreign master traffic");command_retire();require(command_idle(command_state),"master settled source request reclaims independently");
 }
 observer_running=0;put(ptr(root_object)+0x318,0);put(ptr(root_object)+0x2e4,0);free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=saved_command;
 puts("PASS master hard-float factory ABI, retained command/payload correlation, early audio completion, actual source/consumer settlement, no-change/history-adjusted value, unrelated history publication/delayed callback, duplicate exact publication, malformed callable and dead factory rejection (native body substituted)");
}
static uint32_t recording_fixture_n[24],recording_fixture_w[34],recording_fixture_cp[48],recording_fixture_q[4],recording_fixture_vtable[4];
/* The application's own record activity reaching the same callbacks while this
 * command's recording call is open: a producer on another Track, and applies
 * from another call site and for another receiver Program. */
static uint32_t recording_foreign_cp[48],recording_foreign_q[4];static unsigned recording_foreign;
enum {RS_LOCKING,RS_NONLOCKING,RS_NOOP,RS_UNREADABLE};static unsigned recording_shape;
static unsigned recording_trial;
static void *recording_second_fixture(void *unused){
 (void)unused;union {uint64_t align;unsigned char bytes[sizeof(Context)+0x30];} frames;
 Context *enter=(Context*)(frames.bytes+0x30),*end=(Context*)frames.bytes;
 *enter=context();enter->r[0]=ptr(recording_fixture_w+2);call(enter,RC_SECOND_ENTER);
 *end=context();end->r[4]=ptr(recording_fixture_w+2);call(end,RC_SECOND_RETURN);call(end,RC_SECOND_DONE);return NULL;
}
static void *recording_first_fixture(void *unused){
 (void)unused;union {uint64_t align;unsigned char bytes[sizeof(Context)+0x18];} frames;
 Context *enter=(Context*)(frames.bytes+0x18),*end=(Context*)frames.bytes;
 uint32_t n=ptr(recording_fixture_n+2),cp=ptr(recording_fixture_cp),p=ptr(program_objects[0]);
 *enter=context();enter->r[0]=n;call(enter,RC_ENTER);
 if(recording_foreign){
  uint32_t fcp=ptr(recording_foreign_cp);
  put(fcp,0x689fe14);put(fcp+0x80,ptr(track_objects[1]));put(fcp+0x18,ptr(queue_object));recording_foreign_q[0]=ptr(recording_fixture_vtable);
  Context f=context();f.r[0]=fcp;f.r[2]=n+0x18;f.lr=0x1c04b60;call(&f,RC_PRODUCER);
  f=context();f.r[0]=ptr(recording_foreign_q);f.r[4]=fcp;f.r[6]=fcp+0x1c;f.r[8]=fcp+0x18;call(&f,RC_ENQUEUER);
  f=context();f.r[0]=p;f.r[2]=n+0x18;f.lr=0x1c04b60;call(&f,RC_APPLY);
  f=context();f.r[0]=ptr(program_objects[1]);f.r[2]=n+0x18;f.lr=0x1c04b94;call(&f,RC_APPLY);
  require(!atomic_load(&command_state->trace_error),"the application's own record callbacks inside this command's recording call must not poison it");
 }
 if(recording_trial!=1){
  if(recording_trial>=2){
   Context c=context();c.r[0]=cp;c.r[2]=n+0x18;c.lr=0x1c04b60;call(&c,RC_PRODUCER);
   c=context();c.r[0]=ptr(recording_fixture_q);c.r[4]=cp;c.r[6]=cp+0x1c;c.r[8]=cp+0x18;call(&c,RC_ENQUEUER);
   if(recording_foreign){
    uint32_t fcp=ptr(recording_foreign_cp);
    Context f=context();f.r[0]=fcp;f.r[2]=n+0x18;f.lr=0x1c04b60;call(&f,RC_PRODUCER);
    f=context();f.r[0]=ptr(recording_foreign_q);f.r[4]=fcp;f.r[6]=fcp+0x1c;f.r[8]=fcp+0x18;call(&f,RC_ENQUEUER);
    require(!atomic_load(&command_state->trace_error),"another producer and its enqueuer after this command's own was retained must not poison it");
   }
   c=context();c.r[7]=ptr(recording_fixture_q)+4;c.r[9]=(cp+0x18)|1u;c.r[10]=ptr(recording_fixture_w);call(&c,JG_PUBLISH);
   if(recording_trial==4){call(&c,JG_PUBLISH);return NULL;}
   /* The real native second queue may finish before its producer returns. */
   if(recording_trial==2){pthread_t second;require(!pthread_create(&second,NULL,recording_second_fixture,NULL)&&!pthread_join(second,NULL),"second native recorder callback runs on independent ARM thread (body substituted)");}
   c=context();c.r[0]=1;call(&c,RC_ENQUEUE_RETURN);
  }
  Context c=context();c.r[0]=p;c.r[2]=n+0x18;c.lr=0x1c04b94;call(&c,RC_APPLY);
  c=context();c.r[5]=p+0x5b0;c.d[8]=0x3f200000;call(&c,M_FLOAT);
 }
 *end=context();end->r[4]=n;call(end,RC_DONE);return NULL;
}
static void recording_dispatch_fixture(uint32_t f,uint32_t p,unsigned kind,unsigned ctrl,float value){
 uint32_t bits;memcpy(&bits,&value,4);require(f==ptr(recording_fixture_factory)&&p==ptr(program_objects[0])&&kind==0x101&&ctrl==7&&bits==0x3f200000,"recording GUI facade r0F/r1P/r2kind/r3controller/s0value seam");
 memset(recording_fixture_n,0,sizeof(recording_fixture_n));memset(recording_fixture_w,0,sizeof(recording_fixture_w));memset(recording_fixture_cp,0,sizeof(recording_fixture_cp));
 uint32_t n=ptr(recording_fixture_n+2),w=ptr(recording_fixture_w+2),cp=ptr(recording_fixture_cp);
 recording_fixture_n[0]=0x13cf830;recording_fixture_n[1]=0x13d18b0;put(n,f+4);put(n+8,f);put(n+0xc,0x101);put(n+0x10,7);put(n+0x14,p);put(n+0x20,2);put(n+0x28,0x101);put(n+0x2c,bits);put(n+0x30,7);put(n+0x50,ptr(track_objects[0]));
 recording_fixture_w[0]=0x1bbce38;recording_fixture_w[1]=0x1bbf2e8;put(w,cp+0x18);put(w+8,cp);put(w+0xc,ptr(track_objects[0]));put(w+0x18,1234);put(w+0x20,2);put(w+0x28,0x101);put(w+0x2c,bits);put(w+0x30,7);
 put(cp,0x689fe14);put(cp+0x80,ptr(track_objects[0]));put(cp+0x18,ptr(queue_object));/* The producer chooses its enqueuer: RS_NONLOCKING is the thread-specific arm
  * hardware measured on the record path, RS_LOCKING the embedded fallback,
  * RS_NOOP the implementation that drops the work, RS_UNREADABLE an object
  * whose class cannot be read. */
 recording_fixture_q[0]=recording_shape==RS_UNREADABLE?1u:ptr(recording_fixture_vtable);
 recording_fixture_vtable[2]=recording_shape==RS_NONLOCKING?0x134b794:recording_shape==RS_NOOP?0x28c8108:0x28c4078;
 Context c=context();c.r[7]=ptr(queue_object)+0xfc;c.r[9]=(f+4)|1u;c.r[10]=ptr(recording_fixture_n);call(&c,JG_PUBLISH);
 pthread_t first;require(!pthread_create(&first,NULL,recording_first_fixture,NULL)&&!pthread_join(first,NULL),"first GUI recording queue callback on ARM source thread (native body substituted)");
 command_retire();require(!atomic_load(&command_state->slots[0].sealed),"GUI submission flight prevents recording reclaim before facade return");
}
static void recording_checks(void){
 MirrorState *saved=fixture;CommandState *csaved=command_state;fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"recording state fixture");
 for(unsigned trial=0;trial<8;trial++){
  recording_trial=trial>=5?2:trial;recording_shape=trial==5?RS_NONLOCKING:trial==6?RS_NOOP:trial==7?RS_UNREADABLE:RS_LOCKING;recording_foreign=trial==0||trial==2;memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);component_recording_dispatch=recording_dispatch_fixture;
  seed_fixture(0,0,0x3f000000,1);load_fixture();CommandRequest r=request(0x3f200000);unsigned at=submit_fixture(r);CommandSlot *slot=command_state->slots+at;
  if(trial==6||trial==7){require(atomic_load(&command_state->trace_error)==C_TRACE_AMBIGUOUS&&!recording_queue[at]&&!atomic_load(recording_work+at),"an enqueuer that drops the work, and one whose class cannot be read, are both refused and retain neither queue nor recorder job");continue;}
  if(trial==5){
   /* The arm the device actually takes: a thread-specific enqueuer, not the
    * one embedded at manager+0x18, so a regression cannot silently go back to
    * modelling only the fallback. The retained identity is q+4, which the
    * second publication then has to match. */
   require(recording_fixture_vtable[2]==0x134b794&&ptr(recording_fixture_q)!=word(ptr(recording_fixture_cp)+0x18)+0x18,"trial models the thread-specific NonLockingQueueEnqueuer arm, not the embedded fallback");
   require(recording_queue[at]==ptr(recording_fixture_q)+4,"the admitted thread-specific enqueuer retains its own queue identity");
  }
  if(trial==4){require(atomic_load(&command_state->trace_error)==C_TRACE_AMBIGUOUS&&!atomic_load(&slot->done),"duplicate exact recorder publication cannot certify completion");continue;}
  if(trial==3){require(!atomic_load(&slot->done)&&atomic_load(active_request),"first callback alone cannot complete a published recorder job");pthread_t second;require(!pthread_create(&second,NULL,recording_second_fixture,NULL)&&!pthread_join(second,NULL),"second callback can finish after first and GUI return");}
  require(atomic_load(&slot->done)==1&&!atomic_load(&slot->rejected)&&!atomic_load(&command_state->trace_error),"recording callback completion retains source identity");command_retire();require(atomic_load(&slot->sealed)==1,"both native flights leave before record evidence seals");
  atomic_store(&fixture->heartbeat,1);MIRROR_COPY(out);require(copy_mirror(fixture,&out),"recording source copied");MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);bank_apply(&bank,&out,1);
  in.flights[at]=(InputFlight){.flight=1,.field=CF_VOLUME,.bits=r.bits,.expires=120000,.before_revision=r.before_revision,.property=ptr(program_objects[0])+0x5b0,.target={r.epoch,r.serial,r.binding,r.incarnation,ptr(track_objects[0]),ptr(program_objects[0]),r.track_owner,r.program_owner}};
  input_drain(&in,&bank,&out,1);require(!in.error&&in.settled==1,"production input consumes first/second recording callback proof without demanding per-point persistence");
  require(out.tracks[0].bits==(trial==1?r.before_bits:r.bits),"no ClipPlayer native no-op preserves actual source value");command_retire();require(command_idle(command_state),"recording handling reclaims only after consumer settlement");
 }
 observer_running=0;free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=csaved;
 puts("PASS GUI recording facade, source application, native no-Clip no-op, first/second queue correlation, early/delayed recorder callback, duplicate publication rejection and real consumer settlement (native bodies/history/recorder buffering substituted)");
}
static unsigned char general_fixture_auto[0xc0],general_fixture_editor[0x560],general_fixture_controls[0x100],general_fixture_responder[0x234],general_fixture_timeline[0x160],general_fixture_zoom[0x158];
/* juce::Desktop, its desktopComponents storage and the top-level component the
 * application's own key path reaches: the objects the corrected peer
 * resolution reads instead of the enrolled Editor. */
static unsigned char general_fixture_desktop[0x50] __attribute__((aligned(8))),general_fixture_desktop_list[0x10] __attribute__((aligned(8))),general_fixture_window[0x80] __attribute__((aligned(8)));
/* Select through the generated raw-address recipe. Naming a synthetic bool
 * hook here once masked a real e29938/M_MUTE versus M_BOOL dispatch mismatch. */
static unsigned source_site_at(uint32_t address){
 for(unsigned i=0;i<PATCH_COUNT;i++)if(anchor[i]==address)return hook_ids[i];
 require(0,"native source address must have an installed detour");return 0;
}
static uint32_t toggle_recorder,toggle_properties,toggle_click,toggle_sequence;static unsigned toggle_calls,toggle_fault,transport_calls;
/* The key lane's four native calls, recorded in order with the borrowed words
 * as they stood at each call, so the regression reads the sequence the peer is
 * actually handed and the state the application would have seen. */
enum {KEY_CALL_PEER=1,KEY_CALL_MODIFIERS,KEY_CALL_DOWN,KEY_CALL_PRESS,KEY_CALL_UP};
#define KEY_CALL_MAX 8
static struct {unsigned kind;uint32_t argument,modifiers;unsigned char bitmap;} key_calls[KEY_CALL_MAX];
static unsigned key_call_count,key_call_overflow,key_press_result,key_fixture_op;
static uint32_t key_fixture_editor,key_fixture_top,key_fixture_peer,key_press_triple[3];
static uint32_t key_modifier_address(void){return KEY_STATICS_RVA+KEY_MODIFIERS_OFFSET;}
/* The exact byte the peer's own handler indexes, keysym>>3, and the bit inside
 * it, keysym&7, both taken from the shared table rather than restated. */
static uint32_t key_bitmap_address(unsigned op){return KEY_DOWN_BITMAP_RVA+(command_key_symbol(op).keysym>>3);}
static unsigned char key_bitmap_mask(unsigned op){return (unsigned char)(1u<<(command_key_symbol(op).keysym&7));}
static unsigned char key_bitmap_byte(unsigned op){return *(const unsigned char*)(uintptr_t)key_bitmap_address(op);}
static void key_record(unsigned kind,uint32_t argument){
 if(key_call_count>=KEY_CALL_MAX){key_call_overflow=1;return;}
 key_calls[key_call_count].kind=kind;key_calls[key_call_count].argument=argument;
 key_calls[key_call_count].modifiers=word(key_modifier_address());
 key_calls[key_call_count].bitmap=key_bitmap_byte(key_fixture_op);
 key_call_count++;
}
/* The JUCE peer statics the key lane borrows live past the writable segment's
 * file image, so the component build gives them real pages at their exact
 * image addresses; observer_image_bias is 0 here. Reserved before the harness
 * allocates, and required to land exactly where asked: a kernel that ignored
 * the placement would otherwise hand back an unrelated page and let the lane
 * write into the heap. */
#define KEY_FIXTURE_BASE (KEY_STATICS_RVA&~0xfffu)
#define KEY_FIXTURE_BYTES 0x3000u
static void key_statics_reserve(void){
 void *page=mmap((void*)(uintptr_t)KEY_FIXTURE_BASE,KEY_FIXTURE_BYTES,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 require(page==(void*)(uintptr_t)KEY_FIXTURE_BASE,"JUCE peer statics fixture pages at their exact image addresses");
 require(KEY_STATICS_RVA+KEY_FOCUS_OFFSET<KEY_FIXTURE_BASE+KEY_FIXTURE_BYTES&&KEY_STATICS_RVA+KEY_DESKTOP_OFFSET<KEY_FIXTURE_BASE+KEY_FIXTURE_BYTES&&KEY_DOWN_BITMAP_RVA+(0xff54u>>3)<KEY_FIXTURE_BASE+KEY_FIXTURE_BYTES,"every borrowed static is inside those pages");
}
/* Component::getPeer is substituted on the component the lane actually hands it:
 * the application's own first desktop component, read out of juce::Desktop, not
 * the enrolled Editor whose getPeer returned null on the device. */
static uint32_t key_peer_fixture(uint32_t component){
 require(component==key_fixture_top&&component!=key_fixture_editor,"the peer lookup receives the desktop's own top-level component, not the enrolled Editor");
 key_record(KEY_CALL_PEER,component);return key_fixture_peer;
}
static void key_modifiers_fixture(uint32_t peer){
 require(peer==key_fixture_peer,"handleModifierKeysChange is told on the same peer");
 key_record(KEY_CALL_MODIFIERS,peer);
}
static void key_up_down_fixture(uint32_t peer,unsigned down){
 require(peer==key_fixture_peer&&down<=1,"handleKeyUpOrDown brackets the press on the same peer with a boolean");
 key_record(down?KEY_CALL_DOWN:KEY_CALL_UP,down);
}
static unsigned key_press_fixture(uint32_t peer,const uint32_t *key){
 require(peer==key_fixture_peer,"handleKeyPress is delivered to the same peer");
 memcpy(key_press_triple,key,sizeof key_press_triple);
 key_record(KEY_CALL_PRESS,peer);return key_press_result;
}
static void global_dispatch_fixture(unsigned op,uint32_t target,uint32_t function,unsigned bits){
 if(command_toggle(op)){
  Context c=context();unsigned site;
  require(bits<=1,"native intent is boolean");toggle_calls++;
  if(op==GLOBAL_RECORD_TOGGLE){require(target==toggle_recorder,"Record receiver differs from qualified RP owner");unsigned mode=bits?3:0;put(toggle_properties+0x528,mode);c.r[5]=toggle_properties+0x500;c.r[7]=mode;site=source_site_at(0x1c2ce9c);}
  else if(op==GLOBAL_CLICK_TOGGLE){require(target==toggle_click,"Click native receiver");*(unsigned char*)(uintptr_t)(target+0x34)=bits;c.r[5]=target+0xc;c.r[7]=bits;site=source_site_at(0xe29938);}
  else {require(target==word(ptr(project_object)+0xe4)+0x210,"Loop freshly uses current configured Sequence");*(unsigned char*)(uintptr_t)(target+0x28)=bits;c.r[5]=target;c.r[7]=bits;site=source_site_at(0xe29938);}
  if(toggle_fault!=1)call(&c,site);
  if(toggle_fault==2)put(ptr(project_object)+0xe4,1); /* invalid new link: postguard must not dereference it */
  return;
 }

 if(command_transport(op)){
  require(token()!=command_owner&&!atomic_load(lane_in_hook+local_lane-1),"transport native call runs on audio thread outside trace hook interval");
  unsigned lane=trace_enter(0x132);require(lane<COMMAND_LANES,"native transport may enter ordinary source hooks");trace_leave(lane);transport_calls++;
  require(target==ptr(general_fixture_responder),"transport uses responder reached from qualified Editor root");
  require(function==(op==GLOBAL_PLAY?0xf2358c:0xf23628),"transport uses exact native Play/Stop method");return;
 }
 if(op!=CF_AUTOMATION){require(target==ptr(op==GLOBAL_SAVE||command_page(op)||command_key(op)||command_history(op)||command_sequence_duplicate(op)?general_fixture_editor:general_fixture_zoom),"native utility keeps enrolled editor/zoom owner");require(function==(command_key(op)?0xbc18ec:command_sequence_duplicate(op)?SEQ_DUP_CONFIRM_RVA:0x2c70f20)||!command_history(op),"Undo/Redo use qualified Editor perform facade");
  /* Duplicate Sequence names the confirm entry it calls, not the generic
   * action dispatcher, and it builds no key press. */
  if(command_sequence_duplicate(op))require(function==SEQ_DUP_CONFIRM_RVA&&!bits,"the duplicate operation names the application's own CopySequence confirm and carries no value");
  /* The production path builds the juce::KeyPress before the dispatch, so this
   * reads the triple the native peer injection is handed, not a restatement. */
  if(command_key(op)){
   static const uint32_t expected_code[]={0x0d,0x1b,0x10000051,0x10000052,0x10000053,0x10000054};
   uint32_t code=op==GLOBAL_KEY_TAB||op==GLOBAL_KEY_BACKTAB?0x09u:expected_code[op-GLOBAL_KEY_ENTER];
   uint32_t text=op==GLOBAL_KEY_ENTER?0x0du:op==GLOBAL_KEY_CANCEL?0x1bu:op==GLOBAL_KEY_TAB||op==GLOBAL_KEY_BACKTAB?0x09u:0u;
   require(component_global_key[0]==code&&component_global_key[1]==(op==GLOBAL_KEY_BACKTAB?COMMAND_KEY_SHIFT:0u)&&component_global_key[2]==text,"each key operation injects its own JUCE key code, its shift flag and the text character the keyboard path derives");
  }else require(!component_global_key[0]&&!component_global_key[1]&&!component_global_key[2],"a non-key global operation builds no key press");
  return;}
 union {uint64_t align;unsigned char bytes[sizeof(Context)+8];} frames;
 Context *enter=(Context*)(frames.bytes+8),*end=(Context*)frames.bytes;
 *enter=context();enter->r[0]=target;call(enter,GL_AUTO_ENTER);
 *end=context();end->r[4]=target;call(end,GL_AUTO_FANOUT);
 put(target+0x34,bits);Context commit=context();commit.r[5]=target+0xc;commit.r[7]=bits;call(&commit,CH_ENUM_COMMIT);
 call(end,GL_AUTO_EMPTY);
}
static void *transport_queue_runner(void *unused){(void)unused;transport_queue_component_run();return 0;}
static void request_meter_bank(unsigned offset){
 MIRROR_COPY(out);require(copy_mirror(fixture,&out),"meter bank source copy");
 MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);bank_apply(&bank,&out,atomic_load(&fixture->heartbeat));
 if(offset==UINT32_MAX){for(unsigned i=0;i<out.count&&i<MIRROR_BANK;i++)bank.strips[i]=bank_identity(&out,out.tracks+i);}
 else{bank.offset=offset;bank_apply(&bank,&out,atomic_load(&fixture->heartbeat));}
 input_meter_interest(&in,&bank,&out,atomic_load(&fixture->heartbeat),1);
}

/* Duplicate Sequence. The application's own CopySequence Confirm, its first-
 * unused-slot rule, its default name and the String release are substituted one
 * for one, so the regression reads the real properties block, its six words and
 * the call order the lane produces, and the image assertions below hold each
 * substituted body to the shape it has in the exact 3.9.1 binary. */
enum {DUP_CALL_FIRST=1,DUP_CALL_NAME,DUP_CALL_CONFIRM,DUP_CALL_FREE};
#define DUP_CALL_MAX 8
static unsigned duplicate_order[DUP_CALL_MAX],duplicate_order_count,duplicate_order_overflow;
static uint32_t duplicate_block,duplicate_block_words[SEQ_DUP_BLOCK_BYTES/4];
static uint32_t duplicate_name_at,duplicate_free_at,duplicate_first_project;
static int duplicate_name_index;
static unsigned duplicate_name_live,duplicate_name_live_at_confirm;
static void duplicate_record(unsigned kind){
 if(duplicate_order_count>=DUP_CALL_MAX){duplicate_order_overflow=1;return;}
 duplicate_order[duplicate_order_count++]=kind;
}
/* The application's own rule, re-run over the same Project array the lane
 * pinned: walk the slots in order and take the first whose used byte is clear.
 * The image assertions require 0x0255c32c to still be exactly that walk. */
static void duplicate_first_unused_fixture(uint32_t out,uint32_t proj){
 duplicate_record(DUP_CALL_FIRST);duplicate_first_project=proj;
 uint32_t count=word(proj+SEQ_DUP_PROJECT_COUNT),slots=word(proj+SEQ_DUP_PROJECT_SEQUENCES);
 put(out,0);*(volatile unsigned char*)(uintptr_t)(out+4)=0;
 for(uint32_t i=0;i<count;i++){
  uint32_t s=word(slots+4u*i);
  if(!*(const volatile unsigned char*)(uintptr_t)(s+SEQ_DUP_SEQUENCE_USED)){put(out,i);*(volatile unsigned char*)(uintptr_t)(out+4)=1;return;}
 }
}
static void duplicate_name_fixture(uint32_t name,int index){
 duplicate_record(DUP_CALL_NAME);duplicate_name_at=name;duplicate_name_index=index;
 put(name,0xd0000000u|(uint32_t)index);duplicate_name_live=1;
}
static void duplicate_confirm_fixture(uint32_t block){
 duplicate_record(DUP_CALL_CONFIRM);duplicate_block=block;
 for(unsigned i=0;i<SEQ_DUP_BLOCK_BYTES/4;i++)duplicate_block_words[i]=word(block+4u*i);
 duplicate_name_live_at_confirm=duplicate_name_live;
}
static void duplicate_string_free_fixture(uint32_t name){duplicate_record(DUP_CALL_FREE);duplicate_free_at=name;duplicate_name_live=0;}
static unsigned char duplicate_sequence_objects[4][0x260] __attribute__((aligned(8)));
static unsigned char duplicate_navigator_object[0x48] __attribute__((aligned(8)));
static unsigned char duplicate_manager_object[0x100] __attribute__((aligned(8)));
static uint32_t duplicate_slot_array[4];
static void duplicate_used(unsigned slot,unsigned used){duplicate_sequence_objects[slot][SEQ_DUP_SEQUENCE_USED]=(unsigned char)used;}
/* Publish one GLOBAL_SEQ_DUPLICATE request through the real publisher, let the
 * app's own UI drain service it, and return the refusal code (0 on acceptance).
 * The slot is retired either way, so the mailbox is idle for the next case. */
static unsigned duplicate_case(uint32_t editor,unsigned *seq_out,unsigned *at_out){
 MIRROR_COPY(out);require(copy_mirror(fixture,&out),"duplicate snapshot");
 CommandRequest r={.epoch=epoch,.expires=120000,.reserved=GLOBAL_SEQ_DUPLICATE,.project_owner=out.project_owner,.global_owner=channel_owner_id(editor)};
 duplicate_order_count=duplicate_order_overflow=0;duplicate_block=0;duplicate_name_at=duplicate_free_at=0;
 memset(duplicate_block_words,0,sizeof duplicate_block_words);
 unsigned at=submit_fixture(r),seq=atomic_load(&command_state->published);
 if(at_out)*at_out=at;
 if(seq_out)*seq_out=seq;
 return atomic_load(&command_state->slots[at].rejected);
}
static void sequence_duplicate_checks(uint32_t editor,uint32_t audio){
 /* No new hook site: Confirm is called, never patched. */
 for(unsigned i=0;i<PATCH_COUNT;i++)require(anchor[i]!=SEQ_DUP_CONFIRM_RVA,"the CopySequence confirm hosts no patch site");
 const unsigned char *confirm=guarded_bytes(SEQ_DUP_CONFIRM_RVA,SEQ_DUP_CONFIRM_LENGTH);
 const unsigned char *ctor=guarded_bytes(0x13fabdc,0x1b8),*execute=guarded_bytes(0x13fb584,0x9d4),*undo=guarded_bytes(0x13f8b8c,0x538);
 const unsigned char *accessors=guarded_bytes(SEQ_DUP_ACCESSORS_RVA,SEQ_DUP_ACCESSORS_LENGTH),*naming=guarded_bytes(SEQ_DUP_DEFAULT_NAME_RVA,SEQ_DUP_DEFAULT_NAME_LENGTH);
 const unsigned char *dispatcher=guarded_bytes(0x2c70f20,0x2300),*navigate=guarded_bytes(0x1bba0e0,0x178),*copy=guarded_bytes(0x25c3990,0xd94);
 require(confirm&&ctor&&execute&&undo&&accessors&&naming&&dispatcher&&navigate&&copy&&guarded_bytes(0x1477a08,0x22c)&&guarded_bytes(0x911e40,0x270)&&guarded_bytes(0x925c70,0x2d8),"confirm, the command constructor, execute, undo, the sequence accessors, the default name, CommandManager::perform, the sequence copy, the navigator call and both String ranges guarded as whole native ranges");
#define DUP_WORD(body,base,address,value) do{uint32_t dup_w;memcpy(&dup_w,(body)+((address)-(base)),4);require(dup_w==(value),"image shape");}while(0)
 /* Confirm reads exactly the six block words this lane fills, and nothing more:
  * the destination, the source, the navigator, the Project, the dead seventh
  * constructor argument and the name String. */
 DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x195183c,0xe5902038u);
 DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x1951850,0xe5903024u);
 DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x1951864,0xe5942054u);
 DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x1951868,0xe5943004u);
 DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x195186c,0xe594b050u);
 DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x1951894,0xe284104cu);
 require((0xe5902038u&0xfffu)==SEQ_DUP_BLOCK_TO&&(0xe5903024u&0xfffu)==SEQ_DUP_BLOCK_FROM&&(0xe5942054u&0xfffu)==SEQ_DUP_BLOCK_NAV&&(0xe5943004u&0xfffu)==SEQ_DUP_BLOCK_PROJECT&&(0xe594b050u&0xfffu)==SEQ_DUP_BLOCK_SPARE&&(0xe284104cu&0xfffu)==SEQ_DUP_BLOCK_NAME,"the block offsets this lane fills are the ones those instructions name");
 DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x1951854,0xe1520003u);
 DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x1951858,0x0a000041u);
 DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x1951860,0xe3000668u);
 {uint32_t call_word;
  memcpy(&call_word,confirm+(0x19518d4-SEQ_DUP_CONFIRM_RVA),4);
  require(wheel_branch(0x19518d4,call_word)==0x13fabdc,"confirm still builds the command with the CopySequenceCommand constructor");
  memcpy(&call_word,confirm+(0x1951914-SEQ_DUP_CONFIRM_RVA),4);
  require(wheel_branch(0x1951914,call_word)==0x1477a08,"and still submits it through CommandManager::perform");
  DUP_WORD(confirm,SEQ_DUP_CONFIRM_RVA,0x19518e4,0xe5930ab0u);
  require((0xe5930ab0u&0xfffu)==SEQ_DUP_PROJECT_MANAGER,"on the manager the Project holds at the offset this lane pins");}
 /* The constructor keeps the navigator, the source and the destination where
  * execute and undo read them, and takes the "Copy Sequence" undo name id. */
 DUP_WORD(ctor,0x13fabdc,0x13fac84,0xe5848030u);
 DUP_WORD(ctor,0x13fabdc,0x13fac70,0xe5847034u);
 DUP_WORD(ctor,0x13fabdc,0x13fac8c,0xe5849038u);
 DUP_WORD(ctor,0x13fabdc,0x13fac20,0xe3a0103au);
 /* Execute resolves both sequences through GetSequence, and its make-current
  * step is a +0x250 compare that no name can skip, taking the navigator from
  * the word the constructor stored. That is why a navigator is mandatory. */
 DUP_WORD(execute,0x13fb584,0x13fb5b0,0xe5961034u);
 DUP_WORD(execute,0x13fb584,0x13fb5d4,0xe5961038u);
 DUP_WORD(execute,0x13fb584,0x13fb9f0,0xe5984250u);
 DUP_WORD(execute,0x13fb584,0x13fb9fc,0xe1500004u);
 DUP_WORD(execute,0x13fb584,0x13fba74,0xeaffffddu);
 DUP_WORD(execute,0x13fb584,0x13fba18,0xe5960030u);
 require((0xe5984250u&0xfffu)==SEQ_DUP_SEQUENCE_SLOT,"the gate compares the slot index this lane reads");
 /* The direction of the copy, instruction by instruction, rather than only by
  * the whole-range byte guard. The two GetSequence results are kept apart
  * (source in r11, destination in r8), the copy is called with the DESTINATION
  * in r0 and the SOURCE in r1, and the copy's own prologue takes r0 as the
  * object it writes and r1 as the object it reads - its first act on the two is
  * to carry the source's used byte over to the destination. A swap of those two
  * registers would overwrite the sequence the user is working on. */
 DUP_WORD(execute,0x13fb584,0x13fb5d8,0xe1a0b000u);
 DUP_WORD(execute,0x13fb584,0x13fb5e8,0xe1a08000u);
 DUP_WORD(execute,0x13fb584,0x13fb908,0xe1a0100bu);
 DUP_WORD(execute,0x13fb584,0x13fb910,0xe1a00008u);
 DUP_WORD(copy,0x25c3990,0x25c3994,0xe1a04000u);
 DUP_WORD(copy,0x25c3990,0x25c39a0,0xe1a0b001u);
 DUP_WORD(copy,0x25c3990,0x25c3a24,0xe5db3258u);
 DUP_WORD(copy,0x25c3990,0x25c3a30,0xe5c43258u);
 require((0xe5db3258u&0xfffu)==SEQ_DUP_SEQUENCE_USED&&(0xe5c43258u&0xfffu)==SEQ_DUP_SEQUENCE_USED,"and the byte it carries across is the used byte this lane reads, so the destination becomes used and the next press moves on");
 /* Execute navigates and records undo with the DESTINATION OBJECT's own slot
  * word, not with the index the command carries, which is why the lane round
  * trips the destination through the array the same way it does the source. */
 DUP_WORD(execute,0x13fb584,0x13fba1c,0xe5981250u);
 DUP_WORD(execute,0x13fb584,0x13fba10,0xe5860650u);
 require((0xe5981250u&0xfffu)==SEQ_DUP_SEQUENCE_SLOT,"the index the navigation is given is that same slot word");
 {uint32_t call_word;
  memcpy(&call_word,execute+(0x13fb9f8-0x13fb584),4);
  require(wheel_branch(0x13fb9f8,call_word)==0x255c3b0,"the gate's other side is the application's own current-sequence index");
  memcpy(&call_word,execute+(0x13fba20-0x13fb584),4);
  require(wheel_branch(0x13fba20,call_word)==0x1bba0e0,"and the step it gates is the navigator call");
  memcpy(&call_word,execute+(0x13fb918-0x13fb584),4);
  require(wheel_branch(0x13fb918,call_word)==0x25c3990,"the call those two registers are set up for is the sequence copy itself");
  memcpy(&call_word,execute+(0x13fb728-0x13fb584),4);
  require(wheel_branch(0x13fb728,call_word)==0x25c3990,"and the earlier one is the same copy reading the destination into the command's own undo snapshot at +0x40, the opposite direction of the one that follows");
  DUP_WORD(execute,0x13fb584,0x13fb628,0xe2864040u);
  DUP_WORD(execute,0x13fb584,0x13fb718,0xe1a00004u);
  DUP_WORD(execute,0x13fb584,0x13fb720,0xe1a01008u);
  memcpy(&call_word,undo+(0x13f8e38-0x13f8b8c),4);
  require(wheel_branch(0x13f8e38,call_word)==0x1bba0e0,"undo makes the same navigator call when execute navigated");
  DUP_WORD(undo,0x13f8b8c,0x13f8e30,0xe5960030u);
  memcpy(&call_word,navigate,4);
  require(call_word!=0,"the navigator entry is real code, not a stub");}
 /* The first-unused-slot rule the fixture re-runs: the count, the array, the
  * used byte and the two out stores, at the offsets this lane pins. */
 DUP_WORD(accessors,SEQ_DUP_ACCESSORS_RVA,0x255c32c,0xe591ca78u);
 DUP_WORD(accessors,SEQ_DUP_ACCESSORS_RVA,0x255c338,0xe5911a70u);
 DUP_WORD(accessors,SEQ_DUP_ACCESSORS_RVA,0x255c344,0xe5d22258u);
 DUP_WORD(accessors,SEQ_DUP_ACCESSORS_RVA,0x255c36c,0xe5803000u);
 DUP_WORD(accessors,SEQ_DUP_ACCESSORS_RVA,0x255c370,0xe5c02004u);
 DUP_WORD(accessors,SEQ_DUP_ACCESSORS_RVA,0x255c360,0xe5c03004u);
 require((0xe591ca78u&0xfffu)==SEQ_DUP_PROJECT_COUNT&&(0xe5911a70u&0xfffu)==SEQ_DUP_PROJECT_SEQUENCES&&(0xe5d22258u&0xfffu)==SEQ_DUP_SEQUENCE_USED,"and it walks the same count, array and used byte");
 DUP_WORD(accessors,SEQ_DUP_ACCESSORS_RVA,0x255c3b0,0xe59000e4u);
 DUP_WORD(accessors,SEQ_DUP_ACCESSORS_RVA,0x255c3b8,0x15900250u);
 require((0xe59000e4u&0xfffu)==SEQ_DUP_PROJECT_CURRENT,"the current-sequence index is that Project member's own slot word");
 /* The name is the application's own "Sequence %02d" with index+1. */
 DUP_WORD(naming,SEQ_DUP_DEFAULT_NAME_RVA,0x25b9688,0xe2844001u);
 /* The navigator chain is the literal chain the application's own generic
  * action dispatcher uses on this very editor pointer. */
 DUP_WORD(dispatcher,0x2c70f20,0x2c71b68,0xe594337cu);
 DUP_WORD(dispatcher,0x2c70f20,0x2c71b6c,0xe59332e4u);
 DUP_WORD(dispatcher,0x2c70f20,0x2c71b70,0xe59303d8u);
 require((0xe594337cu&0xfffu)==SEQ_DUP_EDITOR_ROOT&&(0xe59332e4u&0xfffu)==SEQ_DUP_ROOT_AUDIO&&(0xe59303d8u&0xfffu)==SEQ_DUP_AUDIO_NAVIGATOR,"hop for hop, at the three offsets this lane walks");
#undef DUP_WORD
 component_sequence_first_unused=duplicate_first_unused_fixture;component_sequence_default_name=duplicate_name_fixture;
 component_sequence_confirm=duplicate_confirm_fixture;component_sequence_string_free=duplicate_string_free_fixture;
 uint32_t proj=ptr(project_object),navigator=ptr(duplicate_navigator_object),manager=ptr(duplicate_manager_object);
 memset(duplicate_sequence_objects,0,sizeof duplicate_sequence_objects);
 for(unsigned i=0;i<4;i++){duplicate_slot_array[i]=ptr(duplicate_sequence_objects[i]);put(duplicate_slot_array[i]+SEQ_DUP_SEQUENCE_SLOT,i);}
 duplicate_used(0,1);duplicate_used(1,1);duplicate_used(2,0);duplicate_used(3,0);
 put(navigator,SEQ_DUP_NAV_VTABLE_RVA);
 put(proj+SEQ_DUP_PROJECT_SEQUENCES,ptr(duplicate_slot_array));put(proj+SEQ_DUP_PROJECT_COUNT,4);
 put(proj+SEQ_DUP_PROJECT_CURRENT,duplicate_slot_array[1]);put(proj+SEQ_DUP_PROJECT_MANAGER,manager);
 put(audio+SEQ_DUP_AUDIO_NAVIGATOR,navigator);
 require(typed(proj,0x6933c4c)&&word(ptr(root_object)+0x24c)==proj&&word(editor+SEQ_DUP_EDITOR_ROOT)==ptr(root_object)&&word(ptr(root_object)+SEQ_DUP_ROOT_AUDIO)==audio,"the duplicate fixture uses the same enrolled Project, root, editor and engine object the other global operations use");
 /* One accepted duplicate: the current sequence, slot 1, into the first unused
  * slot, 2, named for that slot, submitted once. */
 unsigned seq=0,at=0;duplicate_name_live=duplicate_name_live_at_confirm=0;
 require(!duplicate_case(editor,&seq,&at),"an accepted duplicate request completes in the UI drain");
 require(!duplicate_order_overflow&&duplicate_order_count==4&&duplicate_order[0]==DUP_CALL_FIRST&&duplicate_order[1]==DUP_CALL_NAME&&duplicate_order[2]==DUP_CALL_CONFIRM&&duplicate_order[3]==DUP_CALL_FREE,"one duplicate is the application's first-unused rule, its default name, one confirm and one String release, in that order");
 require(duplicate_first_project==proj,"the first-unused rule is asked about the enrolled Project");
 require(duplicate_name_index==2&&duplicate_name_at==duplicate_block+SEQ_DUP_BLOCK_NAME&&duplicate_free_at==duplicate_name_at,"the name is built for the chosen slot straight into the block and released from the same word");
 require(duplicate_name_live_at_confirm&&!duplicate_name_live,"the name String is alive for the confirm and released after it");
 require(duplicate_block_words[SEQ_DUP_BLOCK_PROJECT/4]==proj&&duplicate_block_words[SEQ_DUP_BLOCK_FROM/4]==1&&duplicate_block_words[SEQ_DUP_BLOCK_TO/4]==2&&duplicate_block_words[SEQ_DUP_BLOCK_NAV/4]==navigator&&duplicate_block_words[SEQ_DUP_BLOCK_NAME/4]==(0xd0000000u|2u),"the block carries the Project, the current sequence's own slot as the source, the first unused slot as the destination, the resolved navigator and the built name");
 require(!duplicate_block_words[SEQ_DUP_BLOCK_SPARE/4],"the dead seventh constructor argument goes in as zero");
 for(unsigned i=0;i<SEQ_DUP_BLOCK_BYTES/4;i++){
  unsigned offset=4u*i;
  if(offset==SEQ_DUP_BLOCK_PROJECT||offset==SEQ_DUP_BLOCK_FROM||offset==SEQ_DUP_BLOCK_TO||offset==SEQ_DUP_BLOCK_NAME||offset==SEQ_DUP_BLOCK_NAV)continue;
  require(!duplicate_block_words[i],"every other word of the block is zero: this is a plain aggregate, not a live property object");
 }
 /* The settlement evidence: the begin, the slot record and the end, in order,
  * with the two slots the observer resolved. */
 {CommandSlot *slot=command_state->slots+at;unsigned parts[3]={0,0,0};uint32_t state_source=0,state_destination=0,end_bits=0;
  for(unsigned lane=0;lane<COMMAND_LANES;lane++)for(unsigned j=0;j<atomic_load(&slot->lanes[lane].published);j++){
   CommandEvent ev;require(command_event_read(slot->lanes+lane,seq,j,&ev),"duplicate settlement event read");
   require(ev.reserved==GLOBAL_SEQ_DUPLICATE&&ev.controller==GLOBAL_SEQ_DUPLICATE&&!ev.arg_kind&&!ev.property&&!ev.incarnation&&!ev.capture&&ev.revision==epoch&&ev.token==command_state->owner_token,"duplicate evidence names no Track, Program, Property or native queue");
   if(ev.kind==CE_GLOBAL_BEGIN)parts[0]++;
   else if(ev.kind==CE_GLOBAL_STATE){parts[1]++;state_source=ev.counter;state_destination=ev.bits;}
   else if(ev.kind==CE_GLOBAL_END){parts[2]++;end_bits=ev.bits;}
   else require(0,"a duplicate flight publishes no jog, queue or commit receipt");
  }
  require(parts[0]==1&&parts[1]==1&&parts[2]==1,"exactly one begin, one slot record and one end per accepted duplicate");
  require(state_source==1&&state_destination==2,"the slot record names the source and the destination the block carried");
  require(end_bits==1,"the end carries the submission, not a claim about the copy's contents");}
 command_retire();require(atomic_load(&command_state->slots[at].sealed)==seq,"the duplicate seals after its synchronous return");
 {MIRROR_COPY(out);require(copy_mirror(fixture,&out),"duplicate settled snapshot");
  MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);
  unsigned tick=atomic_load(&fixture->heartbeat)+1;atomic_store(&fixture->heartbeat,tick);
  require(copy_mirror(fixture,&out),"duplicate settled snapshot readback");bank_apply(&bank,&out,tick);
  in.flights[at]=(InputFlight){.flight=seq,.field=GLOBAL_SEQ_DUPLICATE,.expires=120000,.global_owner=channel_owner_id(editor),.global_epoch=epoch};
  input_drain(&in,&bank,&out,tick);require(!in.error&&in.settled==1,"the production consumer accepts the duplicate's three-event proof");}
 command_retire();require(command_idle(command_state),"the settled duplicate reclaims its slot");
 /* Every refusal: nothing is built, nothing is submitted, and the slot is
  * released. A failed navigator hop or pin keeps the codes that already mean
  * exactly that; a full project is the application's own "All Sequences are
  * used!" refusal and gets the one appended code. */
 struct {const char *why;unsigned code;unsigned calls;} expected[9]={
  {"a null navigator word refuses instead of submitting a command the application would dereference",C_NATIVE_MEMBERSHIP,0},
  {"a navigator whose vptr is not the pinned one refuses",C_NATIVE_MEMBERSHIP,0},
  {"a missing engine object refuses at the middle hop",C_NATIVE_MEMBERSHIP,0},
  /* The one refusal that reaches the application at all, and only its
   * read-only first-unused rule: nothing is built and nothing is submitted. */
  {"a project with every slot used refuses with the distinct no-destination code",C_NO_DESTINATION,1},
  {"a current sequence whose slot index does not name it back refuses",C_NATIVE_MEMBERSHIP,0},
  {"no current sequence refuses",C_NATIVE_MEMBERSHIP,0},
  {"no CommandManager refuses",C_NATIVE_MEMBERSHIP,0},
  /* Execute navigates with the destination object's own slot word, so a
   * destination that does not name its own array position is refused after the
   * application's read-only first-unused rule has run and before anything is
   * built or submitted. */
  {"a destination slot whose own slot index does not name it back refuses, after the first-unused rule and before any confirm",C_NATIVE_MEMBERSHIP,1},
  {"an editor that no longer names the enrolled root refuses at identity",C_IDENTITY,0}};
 for(unsigned bad=0;bad<9;bad++){
  if(bad==0)put(audio+SEQ_DUP_AUDIO_NAVIGATOR,0);
  else if(bad==1)put(navigator,SEQ_DUP_NAV_VTABLE_RVA+0x20);
  else if(bad==2)put(ptr(root_object)+SEQ_DUP_ROOT_AUDIO,0);
  else if(bad==3){duplicate_used(2,1);duplicate_used(3,1);}
  else if(bad==4)put(duplicate_slot_array[1]+SEQ_DUP_SEQUENCE_SLOT,3);
  else if(bad==5)put(proj+SEQ_DUP_PROJECT_CURRENT,0);
  else if(bad==6)put(proj+SEQ_DUP_PROJECT_MANAGER,0);
  else if(bad==7)put(duplicate_slot_array[2]+SEQ_DUP_SEQUENCE_SLOT,0);
  else put(editor+SEQ_DUP_EDITOR_ROOT,ptr(root_object)+4);
  unsigned bad_seq=0,bad_at=0,code=duplicate_case(editor,&bad_seq,&bad_at);
  require(code==expected[bad].code,expected[bad].why);
  require(duplicate_order_count==expected[bad].calls&&(!expected[bad].calls||duplicate_order[0]==DUP_CALL_FIRST),"a refused duplicate builds no block and no name and makes no confirm call, and reaches nothing native beyond the read-only first-unused rule");
  require(atomic_load(&command_state->slots[bad_at].dispatched)!=bad_seq&&atomic_load(&command_state->slots[bad_at].done)!=bad_seq&&!native_started[bad_at]&&!atomic_load(&command_state->trace_error),"a refused duplicate neither dispatches nor completes and starts nothing native");
  command_retire();require(atomic_load(&command_state->slots[bad_at].reclaimed)==bad_seq&&command_idle(command_state),"a refused duplicate releases its slot");
  put(audio+SEQ_DUP_AUDIO_NAVIGATOR,navigator);put(navigator,SEQ_DUP_NAV_VTABLE_RVA);put(ptr(root_object)+SEQ_DUP_ROOT_AUDIO,audio);
  duplicate_used(2,0);duplicate_used(3,0);put(duplicate_slot_array[1]+SEQ_DUP_SEQUENCE_SLOT,1);put(duplicate_slot_array[2]+SEQ_DUP_SEQUENCE_SLOT,2);
  put(proj+SEQ_DUP_PROJECT_CURRENT,duplicate_slot_array[1]);put(proj+SEQ_DUP_PROJECT_MANAGER,manager);put(editor+SEQ_DUP_EDITOR_ROOT,ptr(root_object));
 }
 /* Recovery, and the second duplicate taking the next free slot once the first
  * one is in use: the destination is the application's rule, not a counter. */
 duplicate_used(2,1);
 require(!duplicate_case(editor,&seq,&at)&&duplicate_block_words[SEQ_DUP_BLOCK_TO/4]==3&&duplicate_name_index==3,"the next duplicate after a refusal takes the next unused slot and its own default name");
 command_retire();require(command_publish_settlement(command_state,seq),"the second duplicate settles");command_retire();
 require(command_idle(command_state)&&!atomic_load(&command_violation)&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error),"the duplicate lane publishes no diagnostic failure");
 put(proj+SEQ_DUP_PROJECT_SEQUENCES,0);put(proj+SEQ_DUP_PROJECT_COUNT,0);put(proj+SEQ_DUP_PROJECT_CURRENT,0);put(proj+SEQ_DUP_PROJECT_MANAGER,0);put(audio+SEQ_DUP_AUDIO_NAVIGATOR,0);
 component_sequence_first_unused=NULL;component_sequence_default_name=NULL;component_sequence_confirm=NULL;component_sequence_string_free=NULL;
 puts("PASS Duplicate Sequence lane: the application's own CopySequence confirm still reads exactly the six block words this lane fills and still builds and submits the CopySequenceCommand through CommandManager::perform, its constructor still keeps the navigator, source and destination where execute and undo read them, execute's make-current gate is still a slot-index compare no name can skip and still calls the navigator, the first-unused rule and the default name still have the shape the lane relies on, the navigator chain is still the dispatcher's own three hops, no site is patched at the confirm, one accepted request is one first-unused query, one default name, one confirm carrying a plain zeroed block and one String release with the current sequence as the source and the first unused slot as the destination, the settlement records both slots, and a null or wrongly typed navigator, a missing engine object, a full project, a self-inconsistent or absent current sequence, a missing CommandManager, a destination slot that does not name its own array position and a re-rooted editor each refuse without building or submitting anything; execute keeps the two sequences apart and hands the destination to the copy as the object written and the source as the object read, and the copy's own prologue and used-byte carry confirm that direction (native confirm, first-unused rule, default name and String release substituted)");
}
static void general_checks(void){
 MirrorState *saved=fixture;CommandState *csaved=command_state;fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"general state fixture");
 initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);component_global_dispatch=global_dispatch_fixture;
 uint32_t g=ptr(general_fixture_auto),e=ptr(general_fixture_editor),z=ptr(general_fixture_zoom),controls=ptr(general_fixture_controls),responder=ptr(general_fixture_responder),timeline=ptr(general_fixture_timeline);
 put(g,0x68ac48c);put(ptr(project_object)+0xaac,g);put(ptr(root_object)+0x388,controls);put(controls+0x78,z);put(controls+0xf0,responder);
 Context c=context();c.r[0]=g;call(&c,CH_AUTOMATION_OWNER);c.r[4]=g;c.r[2]=0;call(&c,CH_AUTOMATION_BIRTH);
 put(e,0x695395c);put(e+0x230,0x6953b1c);put(e+0x2c8,ptr(queue_object));put(e+0x2cc,7);put(e+0x37c,ptr(root_object));put(ptr(root_object)+0x834,e);
 c=context();c.r[4]=ptr(root_object)-0xd0;c.r[5]=e;call(&c,GL_EDITOR_PUBLISH);
 unsigned char *objects=calloc(1,0x2400);require(objects!=NULL,"native intent fixture objects");
 uint32_t a=ptr(recording_fixture_audio),seq=ptr(objects),metronome=ptr(objects+0x1c00);
 toggle_recorder=ptr(objects+0x500);toggle_properties=ptr(objects+0x700);toggle_click=ptr(objects+0xd00);toggle_sequence=ptr(objects+0x1100);
 put(ptr(root_object)+0x2e4,a);put(a+0x3bc,seq);put(a+0x3a4,timeline);put(timeline,0x69091b0);put(seq+0x400,timeline);put(responder+0x1f0,seq);put(seq+0x3ec,toggle_recorder);put(toggle_recorder,0x68a0318);put(toggle_recorder+0x98,toggle_properties);put(toggle_properties,0x68a0340);
 put(a+0x3c0,toggle_click);put(a+0x3b8,metronome);put(metronome,0x689febc);put(toggle_click,0x689fed4);put(toggle_click+0x60,metronome);put(toggle_click+0x64,seq);
 put(toggle_sequence,0x6934b18);put(toggle_sequence+0x400,0x6934b18);put(ptr(project_object)+0xe4,toggle_sequence);
 c=context();c.r[0]=toggle_properties;call(&c,CH_RECORD_OWNER);c.r[4]=toggle_properties;c.r[3]=0;call(&c,CH_RECORD_BIRTH);
 c=context();c.r[0]=toggle_click;call(&c,GL_CLICK_OWNER);c.r[4]=toggle_click;c.r[3]=0;call(&c,GL_CLICK_BIRTH);
 seed_fixture(0,0,0x3f000000,1);load_fixture();
 /* The six contiguous keys plus the two appended Tab operations. */
 require(!command_key_code(GLOBAL_SAVE)&&!command_key_code(GLOBAL_ZOOM_DOWN)&&!command_key_code(JOG_DATA)&&!command_key_modifiers(GLOBAL_KEY_ENTER),"the key table is never indexed outside the key operations");
 require(!command_key_symbol(GLOBAL_SAVE).keysym&&!command_key_symbol(JOG_DATA).keysym&&!command_key_symbol(GLOBAL_ZOOM_DOWN).text,"the keysym table is never indexed outside the key operations");
 /* The modifier word, the keysDown bitmap and the focused-component slot the
  * lane names are the ones the app's own keyboard handler bc1cfc names: decode
  * its pc-relative literals instead of restating the addresses. */
 const unsigned char *keyboard=guarded_bytes(0xbc1cfc,0xb6c),*press_body=guarded_bytes(0xbc18ec,0x378);
 const unsigned char *peer_body=guarded_bytes(0xb740bc,0x170),*peer_for=guarded_bytes(0xb74074,0x48);
 const unsigned char *desktop_body=guarded_bytes(0xb73474,0x70),*find_body=guarded_bytes(0xb9a904,0x14c);
 require(keyboard&&press_body&&peer_body&&peer_for&&desktop_body&&find_body&&guarded_bytes(0xb74600,0x24c)&&guarded_bytes(0xb899b8,0xa8),"peer keyboard handler, handleKeyPress, the peer lookup and the getPeerFor body it tail-calls, Desktop::getInstance, findComponentAt, the up/down bracket and the modifier notification guarded as whole native ranges");
 require(KEY_STATICS_RVA+KEY_MODIFIERS_OFFSET==key_image_word(keyboard,0xbc1cfc,0xbc2838,0xbc229c,0x10),"the modifier word is the one the handler hands to juce::KeyPress");
 require(KEY_DOWN_BITMAP_RVA==key_image_word(keyboard,0xbc1cfc,0xbc282c,0xbc21c8,0),"the keysDown bitmap is the one the handler clears on release");
 require(KEY_DOWN_BITMAP_RVA==key_image_word(keyboard,0xbc1cfc,0xbc2810,0xbc1e20,0),"and the one it sets before delivery");
 require(KEY_STATICS_RVA+KEY_FOCUS_OFFSET==key_image_word(press_body,0xbc18ec,0xbc1c40,0xbc18f8,0x50),"the focused-component slot is the one handleKeyPress itself reads");
 /* The corrected peer resolution, taken from the application's own two
  * functions rather than restated: the juce::Desktop singleton slot
  * Desktop::getInstance reads, and the desktopComponents list and count
  * Desktop::findComponentAt walks. */
 require(KEY_STATICS_RVA+KEY_DESKTOP_OFFSET==key_image_word(desktop_body,0xb73474,0xb734dc,0xb7347c,KEY_DESKTOP_OFFSET),"the Desktop singleton slot is the one Desktop::getInstance reads out of the same JUCE static table");
 {const uint32_t desktop_load=0xe5934044,count_load=0xe590403c,list_load=0xe5963034,flags_load=0xe5d03068,parent_load=0xe590000c,peer_tail=0xeaffffe5,component_load=0xe5902004;
  require(!memcmp(desktop_body+(0xb73480-0xb73474),&desktop_load,4),"Desktop::getInstance still loads the singleton from that slot and only constructs when it is null");
  require(!memcmp(find_body+(0xb9a908-0xb9a904),&count_load,4)&&!memcmp(find_body+(0xb9a960-0xb9a904),&list_load,4),"findComponentAt still takes the desktop component count from +0x3c and the list from +0x34");
  require(!memcmp(peer_body,&flags_load,4)&&!memcmp(peer_body+(0xb740c8-0xb740bc),&parent_load,4)&&!memcmp(peer_body+(0xb740d8-0xb740bc),&peer_tail,4),"Component::getPeer still tests the heavyweight flag at +0x68, walks parentComponent at +0xc and tail-calls getPeerFor, so a detached component yields no peer");
  require(!memcmp(peer_for+(0xb74098-0xb74074),&component_load,4),"getPeerFor still identifies a peer by the component at +4");}
 {const uint32_t modifier_store=0xe5823010,bracket=0xebfec895,notify=0xebff1dcf,deliver=0xebfffd8e,clear_bit=0xe1c22007,set_bit=0xe1822007,strip=0xe3c33070;
  require(!memcmp(keyboard+(0xbc23cc-0xbc1cfc),&modifier_store,4),"the handler still edits that modifier word in place for a modifier keysym");
  require(!memcmp(keyboard+(0xbc23a4-0xbc1cfc),&bracket,4),"it still brackets with handleKeyUpOrDown");
  require(!memcmp(keyboard+(0xbc2274-0xbc1cfc),&notify,4),"it still calls handleModifierKeysChange when that word changed");
  require(!memcmp(keyboard+(0xbc22ac-0xbc1cfc),&deliver,4),"it still delivers the press through handleKeyPress");
  require(!memcmp(keyboard+(0xbc1e1c-0xbc1cfc),&set_bit,4)&&!memcmp(keyboard+(0xbc21bc-0xbc1cfc),&clear_bit,4),"it still sets the keysDown bit on the way down and clears it on release");
  require(!memcmp(keyboard+(0xbc22a4-0xbc1cfc),&strip,4),"and still strips the mouse-button bits out of the modifiers it passes");}
 component_key_peer=key_peer_fixture;component_key_modifiers_changed=key_modifiers_fixture;
 component_key_up_down=key_up_down_fixture;component_key_press=key_press_fixture;
 static unsigned char key_peer_object[0x40] __attribute__((aligned(8)));
 /* The application's own resolution, modelled with real objects at the real
  * static slot: Desktop singleton -> desktopComponents[0] -> its peer, whose
  * component word names that same top-level component back. */
 uint32_t desktop=ptr(general_fixture_desktop),desktop_list=ptr(general_fixture_desktop_list),window=ptr(general_fixture_window);
 put(KEY_STATICS_RVA+KEY_DESKTOP_OFFSET,desktop);put(desktop+KEY_DESKTOP_LIST,desktop_list);put(desktop+KEY_DESKTOP_COUNT,1);put(desktop_list,window);
 general_fixture_window[KEY_COMPONENT_FLAGS]=KEY_COMPONENT_HEAVYWEIGHT;
 key_fixture_editor=e;key_fixture_top=window;key_fixture_peer=ptr(key_peer_object);
 put(key_fixture_peer,KEY_PEER_VTABLE_RVA);put(key_fixture_peer+KEY_PEER_COMPONENT,window);key_press_result=1;
 for(unsigned i=0;i<28;i++){
  unsigned op=i<3?CF_AUTOMATION:i<14?GLOBAL_SAVE+i-3:i<22?GLOBAL_PAGE_MAIN+i-14:i<26?GLOBAL_PLAY+i-22:i==26?GLOBAL_KEY_TAB:GLOBAL_KEY_BACKTAB,bits=i<3?i:0;
  MIRROR_COPY(out);require(copy_mirror(fixture,&out),"general initial snapshot");
  uint32_t target=op==CF_AUTOMATION?g:command_transport(op)?responder:op==GLOBAL_SAVE||command_page(op)||command_key(op)||command_history(op)?e:z,field=op==CF_AUTOMATION?out.automation.incarnation:0;
  CommandRequest r={.epoch=epoch,.bits=bits,.expires=120000,.reserved=op,.field_incarnation=field,.project_owner=out.project_owner,.global_owner=command_transport(op)?out.editor_owner:channel_owner_id(target)};
  unsigned copies=transport_queue_component_manager_count(0),destroys=transport_queue_component_manager_count(1),holder_count=word(e+0x2cc);
  /* A non-zero resting modifier word and a neighbour bit already set in the
   * same bitmap byte, so restoring is visible and cannot be a memset. The
   * focused-component slot alternates so both diagnostic values are settled. */
  unsigned char neighbour=0;uint32_t modifiers_before=0x1000u+2u*i;
  key_call_count=key_call_overflow=0;key_fixture_op=op;memset(key_press_triple,0,sizeof key_press_triple);
  put(key_modifier_address(),modifiers_before);
  put(KEY_STATICS_RVA+KEY_FOCUS_OFFSET,i&1?ptr(general_fixture_editor):0);
  if(command_key(op)){neighbour=(unsigned char)(0xa5u&~key_bitmap_mask(op));*(unsigned char*)(uintptr_t)key_bitmap_address(op)=neighbour;}
  unsigned at=submit_fixture(r),seq=atomic_load(&command_state->published);command_retire();
  if(command_transport(op)){
   require(transport_queue_component_pending()&&atomic_load(&command_state->slots[at].done)!=seq&&word(e+0x2cc)==holder_count+1,"transport transfers one Editor queue-holder reference and cannot complete on UI submission");
   require(transport_queue_component_manager_count(0)==copies+1&&transport_queue_component_manager_count(1)==destroys&&transport_queue_component_trivial()==1,"native queue copies capture and replaces source with empty manager");
   pthread_t audio;require(!pthread_create(&audio,NULL,transport_queue_runner,NULL)&&!pthread_join(audio,NULL),"transport executes on separate audio consumer thread");
   require(!transport_queue_component_pending()&&word(e+0x2cc)==holder_count&&transport_queue_component_manager_count(1)==destroys+1,"audio completion releases holder and destroys queued callable");command_retire();
  }
  require(atomic_load(&command_state->slots[at].done)==seq&&!atomic_load(&command_state->trace_error),"native global facade whole-operation completion");
  if(command_key(op)){
   unsigned shift=op==GLOBAL_KEY_BACKTAB,n=shift?6u:4u;
   static const unsigned plain[]={KEY_CALL_PEER,KEY_CALL_DOWN,KEY_CALL_PRESS,KEY_CALL_UP};
   static const unsigned shifted[]={KEY_CALL_PEER,KEY_CALL_MODIFIERS,KEY_CALL_DOWN,KEY_CALL_PRESS,KEY_CALL_UP,KEY_CALL_MODIFIERS};
   const unsigned *want=shift?shifted:plain;
   unsigned char mask=key_bitmap_mask(op);uint32_t held_modifiers=modifiers_before|(shift?COMMAND_KEY_SHIFT:0u);
   require(!key_call_overflow&&key_call_count==n,"one synthetic press makes exactly the peer keyboard calls the application's own handler makes");
   for(unsigned k=0;k<n;k++)require(key_calls[k].kind==want[k],"and makes them in the handler's own order: peer, shift change, key down, press, key up, shift restored");
   require(key_calls[0].bitmap==neighbour&&key_calls[0].modifiers==modifiers_before,"nothing is borrowed before the peer is qualified");
   for(unsigned k=shift?2u:1u;k<(shift?5u:4u);k++)
    require(key_calls[k].bitmap==(unsigned char)(neighbour|mask)&&key_calls[k].modifiers==held_modifiers,"the keysDown bit and the shift flag are set for the whole down/press/up bracket");
   if(shift)require(key_calls[1].modifiers==held_modifiers&&key_calls[1].bitmap==neighbour&&key_calls[5].modifiers==modifiers_before&&key_calls[5].bitmap==neighbour,"the shift change is told after the word is edited and again after it is put back");
   require(key_bitmap_byte(op)==neighbour&&word(key_modifier_address())==modifiers_before,"every borrowed word is put back, the neighbouring keysDown bits untouched");
   require(key_press_triple[0]==command_key_code(op)&&key_press_triple[1]==command_key_modifiers(op)&&key_press_triple[2]==command_key_symbol(op).text,"the delivered juce::KeyPress carries this operation's code, shift flag and text character");
   require(key_press_triple[2]==(op==GLOBAL_KEY_ENTER?0x0du:op==GLOBAL_KEY_CANCEL?0x1bu:op==GLOBAL_KEY_TAB||op==GLOBAL_KEY_BACKTAB?0x09u:0u),"Return, Escape and both Tabs carry their control character and the cursor keys carry none");
   /* The diagnostic the device reads back through command-client status: the
    * native result in bit 0, focused-component presence in bit 1. */
   CommandSlot *key_slot=command_state->slots+at;unsigned ends=0;
   for(unsigned lane=0;lane<COMMAND_LANES;lane++)for(unsigned j=0;j<atomic_load(&key_slot->lanes[lane].published);j++){
    CommandEvent ev;require(command_event_read(key_slot->lanes+lane,seq,j,&ev),"key settlement event read");
    if(ev.kind!=CE_GLOBAL_END)continue;
    ends++;require(ev.controller==op&&ev.bits==(key_press_result|((i&1)<<1)),"the settlement event carries the native result and the focused-component diagnostic");
   }
   require(ends==1,"one settlement event per synthetic press");
  }else require(!key_call_count,"a non-key global operation makes none of the peer keyboard calls");
  atomic_store(&fixture->heartbeat,i+1);require(copy_mirror(fixture,&out),"general completed snapshot");MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);bank_apply(&bank,&out,i+1);
  in.flights[at]=(InputFlight){.flight=seq,.field=op,.bits=bits,.expires=120000,.field_incarnation=field,.property=op==CF_AUTOMATION?g+0xc:0,.global_owner=r.global_owner,.global_epoch=epoch};
  input_drain(&in,&bank,&out,i+1);require(!in.error&&in.settled==1,"production global consumer accepts native whole-operation proof");command_retire();require(command_idle(command_state),"general facade slot reclaims");
 }
 /* The peer pin and the corrected resolution both fail closed. A peer whose
  * first word is a foreign vtable, a peer that names a different component, a
  * desktop with no component and a top-level component that is not on the
  * desktop are each refused at C_IDENTITY with nothing borrowed, no native
  * press and no completion. The first case is the one that fired on the device
  * with the old Editor-derived lookup. */
 for(unsigned bad=0;bad<4;bad++){
  MIRROR_COPY(out);require(copy_mirror(fixture,&out),"key refusal snapshot");
  CommandRequest r={.epoch=epoch,.expires=120000,.reserved=GLOBAL_KEY_TAB,.project_owner=out.project_owner,.global_owner=channel_owner_id(e)};
  unsigned char neighbour=(unsigned char)(0xa5u&~key_bitmap_mask(GLOBAL_KEY_TAB));uint32_t modifiers_before=0x3000u+bad;
  key_call_count=key_call_overflow=0;key_fixture_op=GLOBAL_KEY_TAB;
  put(key_modifier_address(),modifiers_before);*(unsigned char*)(uintptr_t)key_bitmap_address(GLOBAL_KEY_TAB)=neighbour;
  if(bad==0)put(key_fixture_peer,KEY_PEER_VTABLE_RVA+0x20);
  else if(bad==1)put(key_fixture_peer+KEY_PEER_COMPONENT,e);
  else if(bad==2)put(desktop+KEY_DESKTOP_COUNT,0);
  else general_fixture_window[KEY_COMPONENT_FLAGS]=0;
  unsigned at=submit_fixture(r),seq=atomic_load(&command_state->published);
  require(atomic_load(&command_state->slots[at].rejected)==C_IDENTITY,"an unqualified peer refuses the key operation at C_IDENTITY");
  require(atomic_load(&command_state->slots[at].dispatched)!=seq&&atomic_load(&command_state->slots[at].done)!=seq&&!atomic_load(&command_state->trace_error),"a refused key operation neither dispatches nor completes");
  require(!key_call_overflow&&key_call_count==(bad<2?1u:0u),"the refusal happens at the pin, and an unresolved desktop never reaches the peer lookup");
  require(key_bitmap_byte(GLOBAL_KEY_TAB)==neighbour&&word(key_modifier_address())==modifiers_before,"a refused key operation borrows no modifier word and no keysDown bit");
  put(key_fixture_peer,KEY_PEER_VTABLE_RVA);put(key_fixture_peer+KEY_PEER_COMPONENT,window);
  put(desktop+KEY_DESKTOP_COUNT,1);general_fixture_window[KEY_COMPONENT_FLAGS]=KEY_COMPONENT_HEAVYWEIGHT;
  command_retire();require(command_idle(command_state),"refused key operation reclaims its slot");
 }
 /* Failed submissions and retired callbacks must neither block the next
  * transport nor fabricate completion. The native queue is substituted. */
 CommandState *transport_saved=command_state;command_state=calloc(1,sizeof(*command_state));require(command_state!=NULL,"isolated transport cancellation process fixture");command_initialize();command_queues=ptr(queue_object);atomic_store(&command_state->alive,1);
 MIRROR_COPY(transport_snapshot);require(copy_mirror(fixture,&transport_snapshot),"transport cancellation identities");
 CommandRequest transport_request={.epoch=epoch,.expires=120000,.reserved=GLOBAL_PLAY,.project_owner=transport_snapshot.project_owner,.global_owner=transport_snapshot.editor_owner};
 unsigned transport_before=transport_calls,holder_before=word(e+0x2cc);
 transport_queue_component_fail(1);unsigned refused_at=submit_fixture(transport_request),refused_seq=atomic_load(&command_state->published);transport_queue_component_fail(0);
 require(atomic_load(&command_state->slots[refused_at].rejected)==C_NOT_READY&&!transport_queue_component_pending()&&word(e+0x2cc)==holder_before&&global_quiescent(),"failed queue submission releases holder and flight");
 transport_request.reserved=GLOBAL_STOP;unsigned stale_at=submit_fixture(transport_request),stale_seq=atomic_load(&command_state->published);
 require(transport_queue_component_pending()&&atomic_load(&command_state->slots[stale_at].dispatched)==stale_seq,"older rejected unreclaimed transport cannot block a later request");
 atomic_store(&fixture->epoch,epoch+1);pthread_t stale_audio;require(!pthread_create(&stale_audio,NULL,transport_queue_runner,NULL)&&!pthread_join(stale_audio,NULL),"retired transport callback drains");atomic_store(&fixture->epoch,epoch);
 require(transport_calls==transport_before&&global_quiescent()&&!atomic_load(active_request+stale_at)&&word(e+0x2cc)==holder_before&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error)&&atomic_load(&command_state->slots[stale_at].done)!=stale_seq,"stale epoch skips native transport without invented completion or trace failure");
 command_retire();require(atomic_load(&command_state->slots[refused_at].reclaimed)==refused_seq,"failed queue request reclaims normally");
 transport_request.reserved=GLOBAL_PLAY;unsigned close_at=submit_fixture(transport_request),close_seq=atomic_load(&command_state->published);
 require(transport_queue_component_pending(),"retired unresolved receipt cannot block later transport");command_close();require(!command_quiescent(),"queued transport holds observer close open");
 pthread_t close_audio;require(!pthread_create(&close_audio,NULL,transport_queue_runner,NULL)&&!pthread_join(close_audio,NULL),"closed transport callback drains");
 require(transport_calls==transport_before&&command_quiescent()&&word(e+0x2cc)==holder_before&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error)&&atomic_load(&command_state->slots[close_at].done)!=close_seq,"close skips native transport and drains without false completion");
 /* Destroy this closed fixture with unresolved receipts intact. */
 free(command_state);command_state=transport_saved;command_initialize();command_queues=ptr(queue_object);atomic_store(&observer_running,1);
 /* Intent tests use the real request publisher, UI drain, source hooks and
  * consumer receipts. Native facade bodies and downstream jobs are substituted. */
 unsigned tick=15;toggle_calls=toggle_fault=0;
 for(unsigned op=GLOBAL_RECORD_TOGGLE;op<=GLOBAL_LOOP_TOGGLE;op++)for(unsigned initial_value=0;initial_value<(op==GLOBAL_RECORD_TOGGLE?5u:2u);initial_value++){
  MIRROR_COPY(out);require(copy_mirror(fixture,&out),"intent copied identities");MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);bank_apply(&bank,&out,tick);
  /* Queue a rapid pair, then change actual state as a touchscreen would. The
   * stale copied bits are intentionally not refreshed before either request. */
  input_global_add(&in,&out,op,0,tick);input_global_add(&in,&out,op,0,tick);
  if(op==GLOBAL_RECORD_TOGGLE)put(toggle_properties+0x528,initial_value);
  else if(op==GLOBAL_CLICK_TOGGLE)*(unsigned char*)(uintptr_t)(toggle_click+0x34)=initial_value;
  else {put(ptr(project_object)+0xe4,toggle_sequence+0x400);*(unsigned char*)(uintptr_t)(toggle_sequence+0x638)=initial_value;}
  unsigned before=op==GLOBAL_RECORD_TOGGLE?(initial_value==1||initial_value==3):initial_value;
  for(unsigned press=0;press<2;press++){
   atomic_store(&fixture->heartbeat,tick);input_pump(&in,&bank,&out,tick);require(!in.error&&in.submitted==press+1,"intent request published");
   c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);command_retire();unsigned request_seq=atomic_load(&command_state->published),at=command_find(command_state,request_seq);require(at<COMMAND_SLOTS&&atomic_load(&command_state->slots[at].done)==request_seq,"matching configured commit and facade return complete intent");
   unsigned actual=op==GLOBAL_RECORD_TOGGLE?(word(toggle_properties+0x528)==1||word(toggle_properties+0x528)==3):op==GLOBAL_CLICK_TOGGLE?*(unsigned char*)(uintptr_t)(toggle_click+0x34):*(unsigned char*)(uintptr_t)(word(ptr(project_object)+0xe4)+0x238);
   require(actual==(press?before:!before),"each queued press inverts fresh native state, including touchscreen changes");
   atomic_store(&fixture->heartbeat,++tick);require(copy_mirror(fixture,&out),"intent source readback");input_drain(&in,&bank,&out,tick);require(!in.error&&in.settled==press+1,"real consumer accepts configured intent without downstream or effective-feedback completion");command_retire();require(command_idle(command_state),"intent receipt reclaimed");
  }
 }
 require(toggle_calls==18,"all mode/toggle pairs reached native facade once per press");
 /* Missing current Sequence refuses before native entry and releases its slot. */
 put(ptr(project_object)+0xe4,0);MIRROR_COPY(absent);require(copy_mirror(fixture,&absent),"absent Sequence snapshot");
 CommandRequest absent_request={.epoch=epoch,.expires=120000,.reserved=GLOBAL_LOOP_TOGGLE,.project_owner=absent.project_owner,.global_owner=absent.project_owner};
 unsigned absent_at=submit_fixture(absent_request),absent_seq=atomic_load(&command_state->published);command_retire();
 require(atomic_load(&command_state->slots[absent_at].rejected)==C_IDENTITY&&atomic_load(&command_state->slots[absent_at].reclaimed)==absent_seq&&command_idle(command_state),"no Sequence refuses without dispatch, receipt or stranded slot");
 sequence_duplicate_checks(e,a);
 /* No native commit cannot masquerade as intent completion; an invalidated
  * current link is rejected before a post-call Sequence read. */
 CommandState *healthy_commands=command_state;
 for(unsigned fault=1;fault<=2;fault++){
  /* Separate command-process fixture for each incomplete native invocation.
   * Production never resets or reclaims such an unproved invocation. */
  command_state=calloc(1,sizeof(*command_state));require(command_state!=NULL,"isolated failed-intent process fixture");command_initialize();command_queues=ptr(queue_object);atomic_store(&command_state->alive,1);
  toggle_fault=fault;put(ptr(project_object)+0xe4,toggle_sequence);MIRROR_COPY(out);require(copy_mirror(fixture,&out),"fault intent identities");
  CommandRequest r={.epoch=epoch,.expires=120000,.reserved=GLOBAL_LOOP_TOGGLE,.project_owner=out.project_owner,.global_owner=out.project_owner};unsigned at=submit_fixture(r);
  if(atomic_load(&command_state->slots[at].rejected)!=C_SOURCE_CHANGED||atomic_load(&command_state->slots[at].done)==atomic_load(&command_state->slots[at].published))fprintf(stderr,"intent fault=%u reject=%u done=%u seq=%u trace=%u\n",fault,atomic_load(&command_state->slots[at].rejected),atomic_load(&command_state->slots[at].done),atomic_load(&command_state->slots[at].published),atomic_load(&command_state->trace_error));
  require(atomic_load(&command_state->slots[at].rejected)==C_SOURCE_CHANGED&&atomic_load(&command_state->slots[at].done)!=atomic_load(&command_state->slots[at].published),"missing configured commit/current-link loss cannot complete intent");
  free(command_state);
 }
 command_state=healthy_commands;command_initialize();command_queues=ptr(queue_object);
 toggle_fault=0;put(ptr(project_object)+0xe4,0);
 /* Same consumer composition, source-owned native meter observation; demand
  * allocation/release is the explicitly substituted boundary in this harness. */
 uint32_t meter=ptr(program_objects[0])+0x2f4,old_meter=word(meter);put(meter,0x68ac3d4);put(meter+0x1c,0x3f000000);put(meter+0x20,0x3e800000);put(meter+0x34,1);
 request_meter_bank(0);c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);require(atomic_load(&fixture->meters.tokens)==1&&tracks[0].meter,"offscreen native meter demand enrolled on owner drain");
 c=context();c.r[7]=meter;call(&c,ME_STEREO);MeterCell *mc=meter_find(meter);require(mc&&atomic_load(&mc->lanes[0].left)==0x3f000000&&atomic_load(&mc->lanes[0].right)==0x3e800000,"native meter source lanes copy linear amplitude");
 atomic_store(&command_state->stop_requested,1);c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);require(!atomic_load(&fixture->meters.tokens)&&atomic_load(&fixture->meters.closed),"explicit source stop releases all UI-owner meter demand");
 observer_running=0;free(objects);put(meter,old_meter);put(ptr(root_object)+0x388,0);free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=csaved;
 puts("PASS Off/Read/Write whole native facade, Save/Zoom/focused key owner routing, every key operation delivered in the peer keyboard handler's own shape (image-derived modifier word, keysDown bit, focused-component slot and Desktop singleton slot; the peer resolved from the application's own desktop component list rather than the enrolled Editor and refused at C_IDENTITY for a foreign vtable, a mismatched component, an empty desktop list or a detached top-level component; shift change, up/down bracket, KeyPress triple with its text character, every borrowed word put back) with its focused-component diagnostic in the settlement event and no such call for a non-key operation, exact Editor Undo/Redo facade and rooted TransportControlsResponder Play/Stop targets, production settlement, fresh Record/Click/Loop intent pairs and configured commit/link rejection, offscreen meter source/demand closure (native method bodies and hardware substituted)");
}
/* Native bus Programs are absent from Project.ProgramPool. Exercise the same
 * current-owner resolver used by demand and real command dispatch, with copied
 * source/consumer settlement; native allocations and method bodies substituted. */
static void send_destination_checks(void){
 MirrorState *saved=fixture;CommandState *csaved=command_state;fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"send destination fixture");
 initial(4);command_initialize();observer_running=1;observer_image_bias=0;
 for(unsigned i=0;i<4;i++)seed_fixture(i,0,0x3f000000,1);
 uint32_t a=ptr(recording_fixture_audio),m=a+0x260,send[4]={ptr(program_objects[2]),ptr(program_objects[0]),ptr(program_objects[3]),ptr(program_objects[1])};put(ptr(root_object)+0x2e4,a);put(m,0x6898b34);
 Context c=context();c.r[0]=m;call(&c,CH_MIXER_BIRTH);load_fixture();for(unsigned i=0;i<4;i++)put(send[i],0x6931f70);put(m+0x78,ptr(send));put(m+0x7c,ptr(send)+16);put(m+0x80,ptr(send)+16);
 c=context();c.r[0]=ptr(queue_object)+4;send_destinations(&c);MIRROR_COPY(out);require(copy_mirror(fixture,&out),"actual source-reader send snapshot");
 for(unsigned i=0;i<4;i++)require(out.send_programs[i]==send[i]&&out.send_owners[i]==channel_owner_id(send[i]),"native Mixer order survives independent Track order");
 channel_owner_death(send[1]);require(copy_mirror(fixture,&out)&&!out.send_programs[1]&&out.send_programs[0],"retired destination cannot inherit a name from retained stale identity");
 send[1]=send[0];send_destinations(&c);require(copy_mirror(fixture,&out)&&!out.send_programs[0]&&!out.send_programs[3],"duplicate native membership clears destination mapping");
 observer_running=0;free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=csaved;
 puts("PASS existing UI membership capture -> actual copied send reader, ordinal reorder, owner death and duplicate invalidation; native object storage substituted");
}
static void bus_membership_checks(void){
 MirrorState *saved=fixture;CommandState *csaved=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"bus fixture state");
 void *tables=mmap((void*)0x6931000,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);require(tables!=MAP_FAILED,"bus method fixture pages");
 const uint32_t types[]={0x6931f70,0x6932150,0x6931bb0,0x6931d90};
 for(unsigned subtype=0;subtype<4;subtype++)for(unsigned trial=0;trial<6;trial++){
  unsigned category=subtype<3?subtype:2;
  memset(command_state,0,sizeof(*command_state));initial(2);command_initialize();atomic_store(&command_state->alive,1);observer_running=1;observer_image_bias=0;component_dispatch=fake_dispatch;dispatched_calls=0;defer_audio=0;
  uint32_t m=ptr(recording_fixture_audio)+0x260,p=ptr(program_objects[0]),registry[]={ptr(program_objects[1])},buses[3]={0};
  buses[category]=p;put(m,0x6898b34);Context c=context();c.r[0]=m;call(&c,CH_MIXER_BIRTH);
  for(unsigned j=0;j<3;j++){put(m+0x78+12*j,ptr(buses+j));put(m+0x7c+12*j,ptr(buses+j)+4);put(m+0x80+12*j,ptr(buses+j)+4);}
  put(ptr(program_pool)+0x1c,ptr(registry));put(ptr(program_pool)+0x20,ptr(registry)+4);put(ptr(program_pool)+0x24,ptr(registry)+4);
  put(p,types[subtype]);put(types[subtype]+0x28,0x250ba74);put(types[subtype]+0x78,0x1375c68);put(ptr(track_objects[0])+0x64c,7+category);
  for(unsigned j=0;j<2;j++){uint32_t n=ptr(program_objects[j])+0x2f4;put(n,0x68ac3d4);put(n+0x1c,0x3f000000);put(n+0x20,0x3e800000);put(n+0x34,1);seed_fixture(j,0,0x3f000000,1);}
  load_fixture();request_meter_bank(UINT32_MAX);
  if(trial==1){buses[category]=0;buses[(category+1)%3]=p;}
  if(trial==2)buses[category]=0;
  if(trial==3)channel_owner_death(m);
  c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);
  CommandRequest r=request(0x3f200000);probe_inject=trial==4?3:trial==5?4:0;unsigned at=submit_fixture(r);probe_inject=0;CommandSlot *slot=command_state->slots+at;
  if(trial>=4){
   require(!dispatched_calls&&atomic_load(&slot->rejected)==C_NATIVE_MEMBERSHIP&&!atomic_load(&slot->returned)&&!native_started[at],"lost bus membership or dead Mixer after resolve cannot reach native call");command_retire();require(command_idle(command_state),"late bus refusal reclaims only unstarted request");
   /* Fixture teardown after the deliberately injected fatal source failure;
    * this does not claim a live native close after owner destruction. */
   meter_release_all();continue;
  }
  if(trial){
   require(atomic_load(&fixture->meters.error)==ME_IDENTITY&&!atomic_load(&fixture->meters.tokens)&&!dispatched_calls&&atomic_load(&slot->rejected)==C_NATIVE_MEMBERSHIP,"wrong bus category, lost membership or dead Mixer reject demand and commands");
   command_retire();require(command_idle(command_state),"bus refusal drains without native settlement");
  }else{
   require(!atomic_load(&fixture->meters.error)&&atomic_load(&fixture->meters.tokens)==2&&tracks[0].meter&&tracks[1].meter,"mixed native bus and ProgramPool meters retain separate demand");
   require(dispatched_calls==1&&atomic_load(&slot->done)==1&&!atomic_load(&slot->rejected)&&!atomic_load(&command_state->trace_error),"bus command reaches exact native method and completes");command_retire();
   c=context();c.r[7]=p+0x2f4;call(&c,ME_STEREO);atomic_store(&fixture->heartbeat,1);MIRROR_COPY(out);require(copy_mirror(fixture,&out)&&out.tracks[0].meter.available,"bus source meter reaches production reader");
   MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);bank_apply(&bank,&out,1);
   in.flights[at]=(InputFlight){.flight=1,.field=CF_VOLUME,.bits=r.bits,.expires=120000,.before_revision=r.before_revision,.property=p+0x5b0,.target={r.epoch,r.serial,r.binding,r.incarnation,ptr(track_objects[0]),p,r.track_owner,r.program_owner}};
   input_drain(&in,&bank,&out,1);require(!in.error&&in.settled==1,"bus command actual source settles through production consumer");command_retire();require(command_idle(command_state),"bus settled command reclaims");
  }
  atomic_store(&command_state->stop_requested,1);c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);require(!atomic_load(&fixture->meters.tokens)&&atomic_load(&fixture->meters.closed),"bus demand releases completely on stop");
 }
 observer_running=0;munmap(tables,8192);memset(recording_fixture_audio,0,sizeof(recording_fixture_audio));free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=csaved;
 puts("PASS Return/Submix/Output/MasterOutput native-owner membership shared by meter and command admission, mixed ProgramPool demand, wrong-category/lost-member/dead-owner rejection, post-resolve membership/owner loss, and production source/consumer settlement (native objects, allocation and method bodies substituted)");
}
static void meter_interest_checks(void){
 MirrorState *saved=fixture;CommandState *csaved=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"meter interest fixture");
 initial(12);command_initialize();atomic_store(&command_state->alive,1);observer_running=1;
 for(unsigned i=0;i<12;i++){uint32_t n=ptr(program_objects[i])+0x2f4;put(n,0x68ac3d4);seed_fixture(i,0,0x3f000000,1);}
 load_fixture();Context c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);
 require(!atomic_load(&fixture->meters.tokens),"no controller interest creates no meter demand");
 request_meter_bank(0);call(&c,COMMAND_DRAIN);require(atomic_load(&fixture->meters.tokens)==8,"only first visible eight demand meters");
 unsigned subscription=meter_subscription;call(&c,COMMAND_DRAIN);require(subscription==meter_subscription,"unchanged owner drain does not rebuild subscriptions");
 request_meter_bank(8);call(&c,COMMAND_DRAIN);require(atomic_load(&fixture->meters.tokens)==4&&!tracks[0].meter&&tracks[8].meter,"bank move releases old and binds remaining four");
 atomic_store(&fixture->heartbeat,1001);call(&c,COMMAND_DRAIN);require(!atomic_load(&fixture->meters.tokens),"expired bridge lease releases demand");
 request_meter_bank(0);call(&c,COMMAND_DRAIN);require(atomic_load(&fixture->meters.tokens)==8,"fresh reconnect restores current bank");
 MIRROR_COPY(out);require(copy_mirror(fixture,&out),"disconnect source");MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);bank_apply(&bank,&out,1001);input_meter_interest(&in,&bank,&out,1001,0);call(&c,COMMAND_DRAIN);
 require(!atomic_load(&fixture->meters.tokens),"disconnect releases demand without waiting for source close");
 atomic_store(&command_state->stop_requested,1);call(&c,COMMAND_DRAIN);require(atomic_load(&fixture->meters.closed),"disabled interest still completes source close");
 observer_running=0;free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=csaved;
 puts("PASS production meter-interest reader/bank publisher: absent, eight-strip bank, bank switch, expiry, reconnect and closure (native allocation substituted)");
}
static void *registration_meter_source(void *unused){
 (void)unused;Context c=context();c.r[7]=ptr(program_objects[0])+0x2f4;call(&c,ME_STEREO);return NULL;
}
static void registration_overlap_checks(void){
 MirrorState *saved=fixture;CommandState *csaved=command_state;fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"registration overlap state");
 for(unsigned closing=0;closing<2;closing++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;atomic_store(&command_state->alive,1);
  uint32_t n=ptr(program_objects[0])+0x2f4;put(n,0x68ac3d4);put(n+0x1c,0x3f000000);put(n+0x20,0x3e800000);put(n+0x34,1);seed_fixture(0,0,0x3f000000,1);load_fixture();
  request_meter_bank(0);Context c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);MeterCell *m=meter_find(n);require(m&&atomic_load(&fixture->meters.tokens)==1,"actual meter admission before simultaneous source enrollment");
  atomic_store(&pause_at,20);atomic_store(&pause_reached,0);atomic_store(&pause_resume,0);pthread_t first,second;
  require(!pthread_create(&first,NULL,registration_meter_source,NULL),"first native meter source thread");
  while(!atomic_load_explicit(&pause_reached,memory_order_acquire)){struct timespec wait={0,1000000};nanosleep(&wait,NULL);}
  require(atomic_load(&enrolling)==1&&atomic_load(&lane_claims[1].busy)&&!atomic_load(&command_state->lane_tokens[1]),"reserved unpublished lane remains an enrollment flight");
  require(!pthread_create(&second,NULL,registration_meter_source,NULL)&&!pthread_join(second,NULL),"second simultaneous source completes while first registrant is paused");
  require(!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error)&&!atomic_load(&enrollment_fault)&&atomic_load(&command_state->lane_tokens[2])&&atomic_load(&m->lanes[2].revision)==2,"occupied reservation does not reject another meter source or share its lane");
  if(closing){atomic_store(&command_state->stop_requested,1);call(&c,COMMAND_DRAIN);command_close();require(!command_quiescent(),"close waits for unpublished reserved lane");}
  atomic_store(&pause_resume,1);require(!pthread_join(first,NULL),"first registrant leaves");atomic_store(&pause_at,0);
  require(atomic_load(&command_state->lane_tokens[1])!=atomic_load(&command_state->lane_tokens[2])&&atomic_load(&m->lanes[1].revision)==(closing?0:2)&&!atomic_load(&enrolling),"unique lanes hand off enrollment; close winner suppresses source publication");
  if(!closing){atomic_store(&command_state->stop_requested,1);call(&c,COMMAND_DRAIN);command_close();}
  require(command_quiescent()&&command_finalize(),"overlapping registration drains before terminal source closure");
  uint32_t before[COMMAND_LANES];for(unsigned i=0;i<COMMAND_LANES;i++)before[i]=atomic_load(&command_state->lane_tokens[i]);
  require(!pthread_create(&first,NULL,registration_meter_source,NULL)&&!pthread_join(first,NULL),"late unadmitted meter callback");
  for(unsigned i=0;i<COMMAND_LANES;i++)require(before[i]==atomic_load(&command_state->lane_tokens[i]),"closed lane registry immutable");
  require(!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error)&&!atomic_load(&fixture->error),"registration overlap and close retain zero source errors");
 }
 free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=csaved;
 puts("PASS concurrent first meter callbacks reserve distinct lanes without waiting on a paused registrant; SC enrollment handoff, close race and terminal registry immutability (native meter bodies/demand allocation substituted)");
}
static void retired_key_checks(void){
 MirrorState *saved=fixture;CommandState *saved_command=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"retired key fixture");
 for(unsigned family=0;family<2;family++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;atomic_store(&command_state->alive,1);
  seed_fixture(0,0,0x3f000000,1);if(family)channel_fixture(1);
  uint32_t property=ptr(program_objects[0])+(family?0x3d0:0x5b0);
  Context c=context();c.r[0]=c.r[5]=property;c.d[8]=0x3f100000;call(&c,MIRROR_PROPERTY_DESTROY);
  uint32_t revision=family?atomic_load(&channel_cell(property)->revision):atomic_load(&lookup(property)->revision);
  call(&c,M_FLOAT);call(&c,MIRROR_PROPERTY_DESTROY);
  require(!atomic_load(&command_state->error)&&(family?atomic_load(&channel_cell(property)->revision):atomic_load(&lookup(property)->revision))==revision,"retired late scalar and duplicate death preserve existing lifetime behavior");
  accepted[0].request.seq=1;accepted[0].request.reserved=family?CF_PAN:CF_VOLUME;accepted[0].property=property;atomic_store(active_request,1);
  call(&c,M_FLOAT);require(atomic_load(&command_state->trace_error)==C_TRACE_AMBIGUOUS,"active-request retired scalar retains prior zero-revision ambiguity");
  /* The trace latch now also records its call site in admission_detail, so
   * resetting fault state between phases must clear both. The assertion below
   * still proves no ENROLLMENT fault occurred in the phase that follows. */
  atomic_store(active_request,0);atomic_store(&command_state->trace_error,0);atomic_store(&command_state->admission_detail,0);
  local_lane=0;for(unsigned lane=0;lane<COMMAND_LANES;lane++){atomic_store(&lane_claims[lane].busy,1);atomic_store(&command_state->lane_tokens[lane],lane+1);}c.r[0]=c.r[5]=0x55555000;
  call(&c,M_FLOAT);call(&c,MIRROR_PROPERTY_DESTROY);
  require(!local_lane&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->admission_detail),"stable exact absence skips enrollment despite full lane registry");
  c.r[0]=c.r[5]=property;unsigned site=family?MIRROR_PROPERTY_DESTROY:M_FLOAT;call(&c,site);
  require(atomic_load(&command_state->error)==C_TRACE_LOSS&&atomic_load(&command_state->trace_error)==C_CAPACITY&&atomic_load(&command_state->admission_detail)==command_admission_detail(CA_LANE_GUARD,2,site),"retired known key keeps capacity admission and precise failure");
  command_close();require(command_finalize(),"retired admission failure closes normally");
 }
 free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=saved_command;
 puts("PASS exact unknown keys skip; retired volume/channel keys remain admitted, preserving late scalar/duplicate death and terminal capacity-admission behavior");
}
static void manual_checks(void){
 MirrorState *saved=fixture;CommandState *saved_command=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"manual lifecycle fixture");
 for(unsigned test=0;test<4;test++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;atomic_store(&command_state->alive,1);
  if(test<2){
   external_operation=2;external_result=0;atomic_store(&pause_at,test?13:8);atomic_store(&pause_reached,0);atomic_store(&pause_resume,0);
   pthread_t writer;require(!pthread_create(&writer,NULL,paused_external,NULL),"paused explicit stop writer");
   while(!atomic_load_explicit(&pause_reached,memory_order_acquire)){struct timespec d={0,1000000};nanosleep(&d,NULL);}
   command_close();require(command_finalize()==!test,"stop publication shares complete external flight closure");
   CommandState *before=malloc(sizeof(*before));require(before!=NULL,"stop snapshot");memcpy(before,command_state,sizeof(*before));
   atomic_store(&pause_resume,1);require(!pthread_join(writer,NULL),"stop writer leaves");
   if(!test)require(!external_result&&!memcmp(before,command_state,sizeof(*before)),"late explicit stop cannot mutate closed state");
   else require(external_result&&command_finalize()&&atomic_load(&command_state->closed)&&!atomic_load(&command_state->error),"admitted explicit stop finishes before immutable closure");
   free(before);atomic_store(&pause_at,0);
  }else if(test==2){
   atomic_store(&command_state->published,1);require(!command_request_stop(command_state)&&!atomic_load(&command_state->stop_requested),"pending request prevents premature producer stop");
   atomic_store(&command_state->consumed,1);atomic_store(&command_state->slots[0].settled,1);require(!command_request_stop(command_state),"settlement alone is not owner reclamation");
   atomic_store(&command_state->reclaimed,1);CommandSlot *q=command_state->slots;atomic_store(&q->processed,1);atomic_store(&q->returned,1);atomic_store(&q->done,1);atomic_store(&q->sealed,1);
   require(command_request_stop(command_state),"reclaimed frontier admits explicit stop");command_consume_tick(1000);require(command_finalize()&&!atomic_load(&command_state->error),"consumer observes explicit stop with clean closed footer");
  }else{
   require(command_duration_valid(0)&&command_tick_limit(0)==UINT32_MAX-1u&&!command_duration_valid(901),"manual duration and finite maximum explicit");
#if !WINDOW_SECONDS
   command_consume_tick(900001);require(atomic_load(&observer_running)&&atomic_load(&fixture->heartbeat)==900001,"manual producer stays active beyond former900s cutoff");
#endif
   command_consume_tick(UINT32_MAX);require(command_finalize()&&atomic_load(&command_state->error)==C_SOURCE_FAILURE,"elapsed clock exhaustion closes with explicit error, never wraps");
  }
 }
 free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=saved_command;
 puts("PASS manual stop admission ordering, pending/settled/reclaimed distinction, immutable closure and elapsed clock boundary; native calls substituted");
}
static void processor_lifecycle_checks(void){
 MirrorState *saved=fixture;CommandState *saved_command=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"processor lifecycle fixture");
 initial(1);command_initialize();observer_running=1;atomic_store(&command_state->alive,1);seed_fixture(0,0,0x3f000000,1);load_fixture();
 Context c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);require(!atomic_load(&command_state->processor_count),"disarmed existing drain adds no diagnostic records");
 require(processor_begin(command_state,atomic_load(&fixture->heartbeat),0,300000),"lifecycle discovery window");call(&c,COMMAND_DRAIN);
 uint32_t words[PROCESSOR_WORDS];require(processor_read(command_state,1,words)&&words[1]==PX_UI_DRAIN&&words[3]==token()&&words[6]==c.r[0]&&words[7]==c.r[0],"existing UI drain captures exact register key and enrolled thread");
 for(unsigned site=PX_REPLACE;site<=PX_PARAMETER_TREE;site++){
  c=context();c.r[0]=0x10050;c.r[1]=0x20000;c.r[2]=3;c.r[3]=0x30000;
  /* Native1a24884 stores entryr2 at W+70;1a24894 stores entryr3 at W+74.
   * Distinct captured W/Q/E/index values prevent mirroring the wrong key rule. */
  if(site==PX_WIDGET_CTOR){c.r[0]=0x15d1f37c;c.r[1]=0x70696e8;c.r[2]=0x94fff40;c.r[3]=5;}
  call(&c,site);
  unsigned sequence=site-PX_REPLACE+2;uint32_t key=site==PX_RETIRE?c.r[1]:site==PX_WIDGET_CTOR?0x94fff40:site==PX_UI_VECTOR?c.r[0]-0x50:c.r[0];
  require(processor_read(command_state,sequence,words)&&words[1]==site&&words[6]==key&&words[7]==c.r[0]&&words[8]==c.r[1]&&words[9]==c.r[2]&&words[10]==c.r[3]&&words[5]==c.lr,"lifecycle hooks preserve raw arguments/LR and explicit opaque normalization");
  if(site==PX_WIDGET_CTOR)require(words[6]==0x94fff40&&words[7]==0x15d1f37c&&words[8]==0x70696e8&&words[9]==0x94fff40&&words[10]==5,"native widget ctor ABI is W,Q,E,index; key is E, not index");
 }
 c=context();c.r[0]=0x12340000;c.r[1]=2;c.r[2]=1;c.r[3]=0x76540000;c.lr=0x235d660;
 unsigned enable_sequence=atomic_load(&command_state->processor_count)+1;call(&c,PX_INSERT_ENABLE);
 require(processor_read(command_state,enable_sequence,words)&&words[1]==PX_INSERT_ENABLE&&words[3]==token()&&words[5]==c.lr&&words[6]==c.r[0]&&words[7]==c.r[0]&&words[8]==2&&words[9]==1&&words[10]==c.r[3],"insert enable entry records opaque I/slot/bool/LR without native reads");
 unsigned count=atomic_load(&command_state->processor_count),enrolled=local_lane;local_lane=0;call(&c,PX_REPLACE);require(!local_lane&&atomic_load(&command_state->processor_count)==count,"diagnostic cannot enroll a new source lane");local_lane=enrolled;
 processor_end_window(command_state);c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);call(&c,PX_REPLACE);call(&c,PX_INSERT_ENABLE);require(atomic_load(&command_state->processor_count)==count&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error),"disarmed lifecycle/drain leaves diagnostic counters and musical health unchanged");
 free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=saved_command;
 puts("PASS actual observer lifecycle sites and reused UI drain: raw registers/keys/thread, disarmed silence and no enrollment (native contexts and model substituted)");
}
/* Substituted setter/queue scheduling, actual rooted resolution, production
 * service, event publication, callback accounting and input consumer. */
static unsigned mode_test_case;
static uint32_t mode_test_cells[2][4];
static void mode_setter_fixture(void *object,const int *selected){
 uint32_t k=ptr(object);unsigned mask=qlink_mode_jobs(word(k+0x5c),(unsigned)*selected);
 for(unsigned p=0;p<2;p++)if(mask&(1u<<p)){
  struct {Context c;uint32_t stack[16];} f={0};f.c=context();f.c.r[4]=k;f.c.r[7]=k+8;f.c.r[11]=source_sp(&f.c)+0x10;
  mode_job_point(&f.c,p?QM_PREPARE1:QM_PREPARE0);
  mode_test_cells[p][0]=p?0x11bcc70:0x11c2104;mode_test_cells[p][1]=p?0x11b8ea0:0x11b8e70;mode_test_cells[p][2]=k+8;mode_test_cells[p][3]=k;
  Context publish=context();publish.r[8]=source_sp(&f.c)+0x10;publish.r[7]=command_queues+0xfc;publish.r[9]=(k+8)|1u;publish.r[10]=ptr(mode_test_cells[p]);mode_job_point(&publish,JG_PUBLISH);
  f.c.r[0]=1;mode_job_point(&f.c,p?QM_RESULT1:QM_RESULT0);
 }
 put(k+0x5c,mode_test_case==0?8:mode_test_case==1?15:(unsigned)*selected);
}
static void mode_callbacks_fixture(unsigned mask){
 for(unsigned p=0;p<2;p++)if(mask&(1u<<p)){
  union {uint64_t align;unsigned char bytes[sizeof(Context)+32];} frame;
  Context *enter=(Context*)(frame.bytes+(p?16:32)),*done=(Context*)frame.bytes;
  *enter=context();enter->r[0]=ptr(mode_test_cells[p])+8;call(enter,p?QM_ENTER1:QM_ENTER0);
  *done=context();done->r[p?5:4]=ptr(mode_test_cells[p])+8;call(done,p?QM_DONE1:QM_DONE0);
 }
}
static void mode_acceptance_checks(void){
 MirrorState *saved=fixture;CommandState *saved_command=command_state;
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"mode fixture state");
 unsigned char *objects=calloc(1,0x3000);require(objects!=NULL,"rooted Q-Link mode fixture objects");
 uint32_t controls=ptr(objects),wrapper=controls+0x100,provider=controls+0x200,labels=controls+0x300,t=controls+0x400,parent=controls+0x600,k=controls+0x800,cap=controls+0xe00;
 uint32_t *entry=(uint32_t*)0x11bcd54;jump(&entry,ptr(mode_setter_fixture));__builtin___clear_cache((char*)0x11bcd54,(char*)entry);
 for(mode_test_case=0;mode_test_case<5;mode_test_case++){
  memset(command_state,0,sizeof(*command_state));initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);seed_fixture(0,0,0x3f000000,1);load_fixture();
  put(root+0x388,controls);put(root+0x540,wrapper);put(controls+0x30,provider);put(controls+0x38,labels);put(controls+0x2c,t);put(t,0x68962d8);put(wrapper,command_queues);put(wrapper+8,provider);put(provider+0x28,16);
  put(provider+0x2c,controls+0x1000);put(provider+0x30,controls+0x1900);put(provider+0x34,controls+0x2100);put(labels+0x10,16);put(labels+0x18,16);put(labels+0x1c,16);put(labels+0x14,controls+0x2200);
  put(root+0x750,parent);put(parent+0xa8,k);put(k,0x6897870);put(k+8,command_queues);put(k+0x524,cap);put(cap+0x28,13);put(k+0x5c,8);
  Context drain=context();drain.r[0]=command_queues+4;require(qlink_mode_sample(&drain,0)&&qlink_current.mode_valid,"actual rooted mode admission and eligible count");
  CommandRequest r={.epoch=epoch,.bits=mode_test_case==4?6:1,.expires=120000,.reserved=QLINK_MODE,.field_incarnation=qlink_mode_generation,.project_owner=channel_owner_id(project),.global_owner=k,.qlink_root=root};
  unsigned at=submit_fixture(r);CommandSlot *slot=command_state->slots+at;r=accepted[at].request;
  require(atomic_load(&slot->processed)==r.seq&&atomic_load(active_request+at)==r.seq&&!atomic_load(&slot->done)&&!atomic_load(&command_state->error),"setter returned but native mode callbacks remain owned");
  require(mode_accept_error[at]==(mode_test_case==0?C_SOURCE_CHANGED:mode_test_case==1?C_IDENTITY:C_OK),"synchronous mismatch and unavailable resolver fail acceptance");
  require((atomic_load(&slot->returned)==r.seq)==(mode_test_case>=2),"failed synchronous acceptance cannot publish successful return marker");
  if(mode_test_case>=2)put(k+0x5c,mode_test_case==3?15:3); /* later touchscreen supersession/unavailability */
  mode_callbacks_fixture(mode_expected[at]);require(atomic_load(&slot->done)==r.seq&&!atomic_load(&command_state->trace_error),"all published controller jobs drain even after acceptance failure");
  call(&drain,COMMAND_DRAIN);command_retire();
  MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);MIRROR_COPY(out);require(copy_mirror(fixture,&out),"mode consumer copied state");out.heartbeat=100;
  in.flights[at]=(InputFlight){.flight=r.seq,.field=QLINK_MODE,.expires=120000,.qlink_request=r};
  input_drain(&in,&bank,&out,100);
  if(mode_test_case<2){require(atomic_load(&command_state->error)==mode_accept_error[at]&&!atomic_load(&slot->sealed)&&!atomic_load(&slot->settled)&&!in.settled,"failed acceptance closes only after callbacks without seal or success");}
  else{
   require(!in.error&&in.settled==1&&in.flights[at].mode_accept.bits==accepted[at].controller&&in.flights[at].qlink_value.bits!=accepted[at].controller,"positive synchronous acceptance survives later different/unavailable observation");
   /* Independently corrupt the retained acceptance outcome to exercise the
    * actual consumer, not just producer error flags. */
   atomic_store(&slot->settled,0);MirrorInput bad={.commands=command_state};bad.flights[at]=(InputFlight){.flight=r.seq,.field=QLINK_MODE,.expires=120000,.qlink_request=r};
   for(unsigned lane=0;lane<COMMAND_LANES;lane++){CommandLane *l=slot->lanes+lane;for(unsigned j=0;j<atomic_load(&l->published);j++){CommandEvent e;require(command_event_read(l,r.seq,j,&e),"retained mode event");if(e.kind==CE_QM_ACCEPT){e.bits=8;command_event_store(l->events+j,&e);}}}
   input_drain(&bad,&bank,&out,100);require(bad.error==C_TRACE_AMBIGUOUS&&!bad.settled&&!atomic_load(&slot->settled),"consumer refuses mismatched synchronous acceptance despite closed callbacks");
  }
 }
 free(objects);free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=saved_command;
 puts("PASS mode acceptance: real rooted resolver/service/consumer; forced wrong or unavailable synchronous selection retains queued receipts then fails; accepted selection survives later supersession/unavailability; native setter/queues substituted");
}
/* Real AC lifecycle/source projection and MIDI-free Audio Monitor command.
 * Native choice mapper and GUI setter are the two explicit substitutions. */
static unsigned io_audio_mapper_fixture(void *mapper,unsigned row){(void)mapper;return row<2?row:row+14;}
static void io_audio_monitor_fixture(void *connector,void *unused,unsigned value){
 (void)unused;uint32_t p=word(ptr(connector)+0x30);Context c=context();call(&c,JG_PUBLISH);
 put(p+0x26f0,value);c.r[5]=p+0x26c8;c.r[7]=value;call(&c,IO_AUDIO_MONITOR_COMMIT);
 c=context();c.r[0]=ptr(connector);c.r[2]=0;call(&c,IO_AC_BIND);channel_write(channel_cell(project+0x78),0,NULL,0,0);
 memset(&io_current,0,sizeof(io_current));io_current.status=IO_OFF;atomic_store(&command_state->io_interest.enabled,0);call(&c,JG_PUBLISH);
}
/* Reproduce the lost outer bind with nested native-hook contexts. Objects and
 * callback scheduling are substituted; the production lifecycle owner runs. */
static void io_program_bind_checks(uint32_t key,uint32_t p,unsigned audio){
 unsigned char *object=calloc(1,0x700);require(object!=NULL,"nested connector fixture");uint32_t inner=ptr(object);
 unsigned birth=audio?IO_AC_BIRTH:IO_PC_BIRTH,bindsite=audio?IO_AC_BIND:IO_PC_BIND,boundsite=audio?IO_AC_BOUND:IO_PC_BOUND,death=audio?IO_AC_DEATH:IO_PC_DEATH,bytes=audio?232:64;
 put(key,audio?0x68a241c:0x68d87e4);put(key+0x30,p);put(inner,audio?0x68a241c:0x68d87e4);put(inner+0x30,p);
 Context c=context();c.r[0]=key;call(&c,birth);c.r[0]=inner;call(&c,birth);
 union {uint64_t align;unsigned char bytes[sizeof(Context)+232];} frames[IO_BIND_FRAMES+1];
 for(unsigned i=0;i<2;i++){Context *e=(Context*)(frames[i].bytes+bytes);*e=context();e->r[0]=i?inner:key;e->r[2]=p;call(e,bindsite);}
 c=context();c.r[audio?4:5]=key;call(&c,boundsite);require(!io_program_copy(key)->ready,"unmatched return cannot certify outer bind");
 for(unsigned i=2;i-->0;){Context *r=(Context*)frames[i].bytes;*r=context();r->r[audio?4:5]=i?inner:key;call(r,boundsite);}
 require(io_program_copy(key)->ready&&io_program_copy(inner)->ready,"nested same-family return preserves outer readiness");
 for(unsigned i=0;i<2;i++){Context *e=(Context*)(frames[i].bytes+bytes);*e=context();e->r[0]=key;e->r[2]=p;call(e,bindsite);}
 for(unsigned i=2;i-->0;){Context *r=(Context*)frames[i].bytes;*r=context();r->r[audio?4:5]=key;call(r,boundsite);}
 require(io_program_copy(key)->ready&&!io_program_binds[audio].depth,"same-key nested bind preserves newer completed generation");
 Context *e=(Context*)(frames[0].bytes+bytes),*r=(Context*)frames[0].bytes;*e=context();e->r[0]=key;e->r[2]=p;call(e,bindsite);
 /* Force the read/CAS interleaving at the production advance seam: the
  * foreign event runs after the owner's nonzero read, before its CAS. */
 unsigned observed=atomic_load(&io_program_copy(key)->generation);
 unsigned owner=command_owner;command_owner=owner+1;c=context();c.r[0]=key;c.r[2]=p;call(&c,bindsite);command_owner=owner;
 require(observed&&!io_program_advance(io_program_copy(key),observed)&&!io_program_copy(key)->generation,"intervening foreign poison defeats owner generation CAS");
 *r=context();r->r[audio?4:5]=key;call(r,boundsite);require(!io_program_copy(key)->ready,"foreign bind poisons outer readiness witness");
 *e=context();e->r[0]=key;e->r[2]=p;call(e,bindsite);*r=context();r->r[audio?4:5]=key;call(r,boundsite);require(!io_program_copy(key)->ready&&!io_program_copy(key)->generation,"UI bind cannot recover foreign lifetime poison");
 io_program_copy(key)->ready=1;require(!io_program_native(tracks,audio),"ready flag cannot bypass poisoned native connector admission");io_program_copy(key)->ready=0;
 c=context();c.r[0]=key;call(&c,death);call(&c,birth);
 *e=context();e->r[0]=key;e->r[2]=p;call(e,bindsite);*r=context();r->r[audio?4:5]=key;call(r,boundsite);require(io_program_copy(key)->ready,"new enrolled lifetime and bind recover foreign poison");
 *e=context();e->r[0]=key;e->r[2]=p;call(e,bindsite);
 c=context();c.r[0]=key;call(&c,death);call(&c,birth);io_program_copy(key)->binding=p;
 *r=context();r->r[audio?4:5]=key;call(r,boundsite);require(!io_program_copy(key)->ready,"retired/reused connector cannot accept old bind return");
 for(unsigned i=0;i<=IO_BIND_FRAMES;i++){e=(Context*)(frames[i].bytes+bytes);*e=context();e->r[0]=key;e->r[2]=p;call(e,bindsite);}
 for(unsigned i=IO_BIND_FRAMES+1;i-->0;){r=(Context*)frames[i].bytes;*r=context();r->r[audio?4:5]=key;call(r,boundsite);}
 require(!io_program_copy(key)->ready&&!io_program_binds[audio].depth,"overflow unwinds without stale readiness");
 e=(Context*)(frames[0].bytes+bytes);*e=context();e->r[0]=key;e->r[2]=p;call(e,bindsite);r=(Context*)frames[0].bytes;*r=context();r->r[audio?4:5]=key;call(r,boundsite);
 require(io_program_copy(key)->ready,"fresh bind recovers after bounded overflow");c=context();c.r[0]=inner;call(&c,death);free(object);
}
static float io_audio_output_seed_fixture(void *p,unsigned kind,unsigned parameter){(void)p;require(kind==0x101&&parameter==325,"Audio Out source getter ABI");return 1.0f;}
static unsigned io_audio_output_base_fixture(void *p){(void)p;return 1;}
static void io_audio_checks(void){
 require(mmap((void*)0x692f000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED,"AudioProgram method fixture page");put(0x692ff3c,0x1375c68);
 MirrorState *saved=fixture;CommandState *saved_command=command_state;fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"Audio I/O fixture state");
 unsigned char *objects=calloc(1,0x2400);require(objects!=NULL,"Audio I/O rooted connector fixtures");
 uint32_t outer=ptr(objects),pc=outer+0x100,ac=pc+0x12ec,mapper=outer+0x2000;
 uint32_t *entry=(uint32_t*)0x269741c;jump(&entry,ptr(io_audio_mapper_fixture));__builtin___clear_cache((char*)0x269741c,(char*)entry);
 entry=(uint32_t*)0x1e5d8b8;jump(&entry,ptr(io_audio_monitor_fixture));__builtin___clear_cache((char*)0x1e5d8b8,(char*)entry);
 initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);seed_fixture(0,0,0x3f000000,1);
 uint32_t p=ptr(program_objects[0]);put(p,0x692fec4);put(ptr(track_objects[0])+0x64c,6);load_fixture();
 Context c=context();
 ChannelCell *selection=channel_cell(project+0x78);require(selection!=NULL,"selected Track source");channel_write(selection,tracks[0].track,NULL,0,0);
 put(root+0x75c,outer);put(outer+8,pc);io_program_bind_checks(pc+0xd4c,p,0);io_program_bind_checks(ac,p,1);put(ac,0x68a241c);put(ac+0x30,p);put(ac+0x6fc,mapper);
 c=context();c.r[4]=p;c.d[16]=0;call(&c,IO_AUDIO_MONITOR_BIRTH);c.r[3]=0;call(&c,IO_AUDIO_IN_BIRTH);call(&c,IO_AUDIO_APPLY_BIRTH);
 c=context();c.r[0]=ac;call(&c,IO_AC_BIRTH);c.r[4]=ac;call(&c,IO_AC_EMPTY);require(io_program_copy(ac)!=NULL,"independent AudioProgramConnector birth enrolled");
 union {uint64_t alignment;unsigned char bytes[sizeof(Context)+232];} frame;
 Context *bind=(Context*)(frame.bytes+232),*bound=(Context*)frame.bytes;*bind=context();bind->r[0]=ac;bind->r[2]=p;call(bind,IO_AC_BIND);
 uint32_t labels[]={ptr("Input 1,2"),ptr("Input 3,4"),ptr("Input 5,6")};put(ac+0x61c,ptr(labels));put(ac+0x620,ptr(labels)+sizeof(labels));put(ac+0x624,ptr(labels)+sizeof(labels));
 c=context();c.r[5]=ac+0x60c;call(&c,IO_STRING_CATALOGUE);*bound=context();bound->r[4]=ac;call(bound,IO_AC_BOUND);require(io_program_copy(ac)->ready,"Audio constructor-empty-bind-catalogue-bound path ready");
 uint32_t output_labels[]={ptr("Out 1/2"),ptr("Out 3/4")},common=pc+0xd4c;put(common+0x544,ptr(output_labels));put(common+0x548,ptr(output_labels)+sizeof(output_labels));put(common+0x54c,ptr(output_labels)+sizeof(output_labels));
 c=context();c.r[5]=common+0x534;call(&c,IO_STRING_CATALOGUE);
 uint32_t description[6]={2,0,3,0,1,0x3f800000};channel_birth(p,p+0x348,CI_AUDIO_OUT,0,(const char*)description,24,1);
 entry=(uint32_t*)0x250b754;jump(&entry,ptr(io_audio_output_seed_fixture));__builtin___clear_cache((char*)0x250b754,(char*)entry);
 entry=(uint32_t*)0x26991b0;jump(&entry,ptr(io_audio_output_base_fixture));__builtin___clear_cache((char*)0x26991b0,(char*)entry);
 IOInterest *interest=&command_state->io_interest;atomic_store(&interest->revision,2);atomic_store(&interest->enabled,1);atomic_store(&interest->epoch,epoch);atomic_store(&interest->serial,tracks[0].serial);atomic_store(&interest->until,120000);
 Context drain=context();drain.r[0]=command_queues+4;io_service(&drain);
 require(io_current.fields[IO_MONITOR].status==IO_READY&&io_current.fields[IO_MONITOR].choice_count==4&&!strcmp(io_current.fields[IO_MONITOR].text,"Off")&&io_current.fields[IO_AUDIO_IN].status==IO_READY&&io_current.fields[IO_AUDIO_IN].choice_count==3&&!strcmp(io_current.fields[IO_AUDIO_IN].text,"Input 1,2"),"Audio Monitor and dynamic Audio In READY through actual service");
 require(io_current.fields[IO_AUDIO_OUT].status==IO_READY&&io_current.fields[IO_AUDIO_OUT].choice_count==2&&!strcmp(io_current.fields[IO_AUDIO_OUT].text,"Out 1/2"),"nested common bind exposes Audio Out on first selected-track service without reselection");
 MIRROR_COPY(out);require(copy_mirror(fixture,&out)&&out.io_available,"actual I/O copied reader");MirrorInput in={.commands=command_state};MirrorBank bank;bank_init(&bank);bank.assignment=BA_IO;bank_apply(&bank,&out,0);
 input_io_edit(&in,&bank,&out,IO_AUDIO_IN,2,0);input_io_submit(&in,&bank,&out,0);CommandRequest r;require(command_request_read(command_state->slots,1,&r)&&r.io_connector==ac&&r.io_field==IO_AUDIO_IN&&r.bits==2,"Audio In encoder intent uses actual AC/field/catalogue identity");
 Track *target=NULL;uint32_t connector=0;IOField actual={0};require(!io_resolve(&drain,&r,&target,&connector,&actual)&&connector==ac&&actual.value==0,"Audio In typed request admitted against actual source");
 /* This request was never dispatched; reset fixture ownership before testing
  * the separate Monitor setter/receipt. No native queued job is substituted. */
 atomic_store(&command_state->slots[0].published,0);atomic_store(&command_state->published,0);memset(&in,0,sizeof(in));in.commands=command_state;
 input_io_edit(&in,&bank,&out,IO_MONITOR,1,0);input_io_submit(&in,&bank,&out,0);call(&drain,COMMAND_DRAIN);command_retire();
 require(atomic_load(&command_state->slots[0].done)==1&&atomic_load(&command_state->slots[0].sealed)==1&&!atomic_load(&command_state->error),"Audio Monitor source commit and actual typed service seal");
 require(copy_mirror(fixture,&out),"Monitor readback copy");out.heartbeat=100;input_drain(&in,&bank,&out,100);require(!in.error&&in.settled==1&&in.flights[0].io_events[5].bits==1,"Audio Monitor actual input receipt consumer settles In");
 atomic_store(&interest->enabled,1);channel_write(selection,tracks[0].track,NULL,0,0);c=context();c.r[0]=ac;call(&c,IO_AC_DEATH);io_service(&drain);require(io_current.fields[IO_MONITOR].status==IO_UNAVAILABLE&&io_current.fields[IO_AUDIO_IN].status==IO_UNAVAILABLE,"AudioConnector death revokes both fields");
 free(objects);free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=saved_command;
 puts("PASS nested PC/AC bind ownership, read/CAS foreign poison, mismatched return, reuse and bounded overflow; first-selection Audio Out READY (native getter/base substituted); Audio I/O constructor/bind/source -> actual copied READY/intent/consumer and Monitor receipt after native-listener rebind/selection/page change; native mapper/GUI setter and rooted objects substituted; Audio In native dispatch and equipment audio unobserved");
}
/* Execute all three actual callback instructions at their pinned addresses.
 * Only its property setter/listener/queue backend is substituted. This catches
 * forwarding NULL as r1 even if a C callback-shaped mock would ignore it. */
static uint32_t io_monitor_expected,io_monitor_helper,io_monitor_queued[5];
static void *io_monitor_commit_stub;
static unsigned io_monitor_publication_fault;
static void io_monitor_setter_fixture(void *property,unsigned value){
 require(ptr(property)==io_monitor_expected&&value==3,"real Monitor callback forwards exact property and dereferenced Merge enum");
 Context c=context(),captured;c.r[0]=ptr(property);c.r[7]=value;c.r[3]=0;exercise(&c,(void*)0x1351008,&captured);
 require(word(ptr(property)+0x10)==value&&atomic_load(&channel_cell(ptr(property))->bits)==value,"actual native Monitor commit pair and generated gate publish retained source");
 c=context();call(&c,JG_PUBLISH); /* Other listener before our prepare. */
 c=context();c.r[4]=ptr(property)-4;c.r[5]=io_monitor_helper;c.r[9]=value;c.r[3]=0x245e634;call(&c,IO_MONITOR_PREPARE);uint32_t source=source_sp(&c);
 c.r[8]=source+64;call(&c,JG_PUBLISH); /* Unrelated while F is live. */
 io_monitor_queued[0]=0x245e634;io_monitor_queued[1]=0x245e5e8;io_monitor_queued[2]=io_monitor_helper;io_monitor_queued[3]=value;io_monitor_queued[4]=ptr(property)-4;
 if(io_monitor_publication_fault==1)io_monitor_queued[3]=value+1;
 c.r[7]=command_queues+0xfc;c.r[8]=source;c.r[9]=io_monitor_helper|1;c.r[10]=ptr(io_monitor_queued);call(&c,JG_PUBLISH);
 if(io_monitor_publication_fault==2)call(&c,JG_PUBLISH); /* Duplicate exact F. */
 c.r[8]=source+64;call(&c,JG_PUBLISH);
 c.r[0]=1;call(&c,IO_MONITOR_RESULT);
 /* Another property listener uses the same queue while the GUI setter is
  * still on stack. Its source callable is not our enrolled source frame. */
 c=context();c.r[8]=source;call(&c,JG_PUBLISH); /* Native stack-address reuse. */
}
static void io_monitor_dispatch_checks(void){
 MirrorState *saved=fixture;CommandState *saved_command=command_state;
 for(io_monitor_publication_fault=0;io_monitor_publication_fault<3;io_monitor_publication_fault++){
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"Monitor dispatch fixture state");
 unsigned char *objects=calloc(1,0x1600);require(objects!=NULL,"Monitor connector fixture");uint32_t outer=ptr(objects),pc=outer+0x100,connector=pc+0x2f4;io_monitor_helper=outer+0x1500;
 const unsigned char *callback=NULL;for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);i++)if(guards[i].address<=0x1e93ba0&&guards[i].address+guards[i].length>=0x1e93bac){callback=guards[i].bytes+0x1e93ba0-guards[i].address;break;}
 const uint32_t expected[]={0xe1a00001,0xe5921000,0xead2f4fe};require(callback&&!memcmp(callback,expected,sizeof(expected)),"pinned native Monitor MOV r0,r1; LDR r1,[r2]; tailbranch setter");memcpy((void*)0x1e93ba0,callback,12);__builtin___clear_cache((char*)0x1e93ba0,(char*)0x1e93bac);
 unsigned commit=0;while(commit<PATCH_COUNT&&hook_ids[commit]!=IO_MONITOR_COMMIT)commit++;require(commit<PATCH_COUNT&&anchor[commit]==0x1351008&&capture_after_pair[commit],"native Monitor store anchor and post-pair gate");
 io_monitor_commit_stub=mmap(NULL,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);require(io_monitor_commit_stub!=MAP_FAILED,"Monitor generated commit stub");build_stub(io_monitor_commit_stub,commit,0);uint32_t *commit_entry=(uint32_t*)0x1351008;jump(&commit_entry,ptr(io_monitor_commit_stub));jump(&commit_entry,ptr(capture_a));__builtin___clear_cache(io_monitor_commit_stub,(char*)io_monitor_commit_stub+4096);__builtin___clear_cache((char*)0x1351008,(char*)commit_entry);
 uint32_t *entry=(uint32_t*)0x1350fa8;jump(&entry,ptr(io_monitor_setter_fixture));__builtin___clear_cache((char*)0x1350fa8,(char*)entry);
 initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);seed_fixture(0,0,0x3f000000,1);load_fixture();Track *t=tracks;io_monitor_expected=t->track+0x400;
 channel_birth(t->track,io_monitor_expected,CI_MIDI_MONITOR,2,NULL,0,1);channel_birth(t->track,t->track+0x414,CI_MIDI_MONITOR_AUDIO,2,NULL,0,1);put(io_monitor_expected+0x10,2);put(t->track+0x418,io_monitor_helper);put(io_monitor_helper,command_queues);
 ChannelCell *selection=channel_cell(project+0x78);channel_write(selection,t->track,NULL,0,0);put(root+0x75c,outer);put(outer+8,pc);put(connector,0x68a2710);put(connector+0x30,t->track);put(connector+0xa34,pool);
 Context c=context();c.r[0]=connector;call(&c,IO_CONNECTOR_BIRTH);c.r[4]=connector;call(&c,IO_CONNECTOR_READY);
 union {uint64_t alignment;unsigned char bytes[sizeof(Context)+64];} frame;Context *bind=(Context*)(frame.bytes+64),*bound=(Context*)frame.bytes;*bind=context();bind->r[0]=connector;bind->r[2]=t->track;call(bind,IO_BIND_ENTER);*bound=context();bound->r[4]=connector;call(bound,IO_BIND_DONE);
 IOInterest *interest=&command_state->io_interest;atomic_store(&interest->revision,2);atomic_store(&interest->enabled,1);atomic_store(&interest->epoch,epoch);atomic_store(&interest->serial,t->serial);atomic_store(&interest->until,120000);
 Context drain=context();drain.r[0]=command_queues+4;io_service(&drain);MIRROR_COPY(out);require(copy_mirror(fixture,&out)&&out.io_available&&out.io.fields[0].status==IO_READY&&out.io.fields[0].value==2,"Monitor Auto copied source ready");MirrorBank bank;bank_init(&bank);bank.assignment=BA_IO;bank_apply(&bank,&out,0);MirrorInput in={.commands=command_state};input_io_edit(&in,&bank,&out,IO_MONITOR,1,0);input_io_submit(&in,&bank,&out,0);call(&drain,COMMAND_DRAIN);
 if(io_monitor_publication_fault){require(io_fault[0]==C_TRACE_AMBIGUOUS&&atomic_load(&command_state->error)==C_TRACE_AMBIGUOUS&&!atomic_load(&command_state->slots[0].returned),"wrong payload/duplicate exact prepared source remains ambiguous");goto release_monitor;}
 require(!atomic_load(&command_state->error)&&atomic_load(&command_state->slots[0].returned)==1&&!atomic_load(&command_state->slots[0].done)&&io_queue_result[0]==2,"actual typed Monitor dispatch executes native callback and retains queued obligation");
 c=context();c.r[0]=ptr(io_monitor_queued)+8;call(&c,IO_MONITOR_ENTER);c.r[2]=t->track+0x3fc;c.r[1]=3;call(&c,IO_MONITOR_AUDIO);c.r[2]=0;call(&c,IO_MONITOR_DONE);call(&drain,COMMAND_DRAIN);command_retire();require(copy_mirror(fixture,&out),"Monitor postcallback copy");out.heartbeat=100;input_drain(&in,&bank,&out,100);command_retire();require(!in.error&&in.settled==1&&command_idle(command_state)&&in.flights[0].io_events[5].bits==3,"real callback-forwarded Merge source completes and reclaims through actual consumer");
release_monitor:
 munmap(io_monitor_commit_stub,4096);free(objects);free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=saved_command;
 }
 puts("PASS typed Monitor executes native MOV/LDR/B and generated post-store source detour; unrelated listeners before/during/after expected queue and later F reuse ignored; exact-F wrong payload/duplicate rejected; actual consumer closes (listener/queue backend and audio scheduling substituted)");
}
/* Native GUI/AssignCommand bodies are substituted; production typed admission,
 * generated commit detour, source/Do receipts and consumer remain connected. */
static unsigned io_sync_test,io_sync_field,io_sync_commit;
static void io_sync_gui_fixture(void *connector,void *property,unsigned value){
 unsigned site=io_sync_field==IO_MIDI_IN_PORT?IO_INPUT_SET:io_sync_field==IO_MIDI_OUT_PORT?IO_OUTPUT_SET:IO_FILTER_SET;
 unsigned lr=site==IO_INPUT_SET?0x1e93c8c:site==IO_OUTPUT_SET?0x1e93d6c:0x1e93cfc;
 require(ptr(property)==accepted[0].property&&word(ptr(connector)+0x30)==accepted[0].track&&value==accepted[0].native_bits,"typed MIDI callback original connector/property/scalar");
 Context c=context(),post;call(&c,JG_PUBLISH); /* Native GUI listener, no I/O job. */
 c=context();c.r[0]=ptr(property);c.r[1]=site==IO_FILTER_SET?ptr(&value):value;c.lr=lr;call(&c,site);
 Context commit=context();commit.r[0]=commit.r[5]=ptr(property);commit.r[1]=commit.r[3]=commit.r[7]=value;
 exercise(&commit,(void*)(uintptr_t)anchor[io_sync_commit],&post);
 call(&c,JG_PUBLISH);call(&c,site+3);
 /* A native listener can invalidate choices, bind another selection and leave
  * the page before the initiating GUI callback returns. */
 ChannelCell *selection=channel_cell(project+0x78);channel_write(selection,tracks[1].track,NULL,0,0);
 c=context();c.r[0]=ptr(connector);c.r[2]=tracks[1].track;call(&c,IO_BIND_ENTER);put(ptr(connector)+0x30,tracks[1].track);
 memset(&io_current,0,sizeof(io_current));io_current.status=IO_OFF;atomic_store(&command_state->io_interest.enabled,0);call(&c,JG_PUBLISH);
 ChannelCell *source=channel_cell(ptr(property));
 if(io_sync_test==4)atomic_fetch_add(&source->revision,1);
 if(io_sync_test==5)channel_owner_death(accepted[0].track);
 if(io_sync_test==6){atomic_store(&source->live,0);channel_birth(accepted[0].track,ptr(property),CI_MIDI_OUT_PORT,value,NULL,0,1);}
}
static void io_sync_dispatch_checks(void){
 MirrorState *saved=fixture;CommandState *saved_command=command_state;
 const unsigned fields[]={IO_MIDI_IN_PORT,IO_MIDI_IN_CHANNEL,IO_MIDI_OUT_PORT,IO_MIDI_OUT_CHANNEL};
 const unsigned offsets[]={0x364,0x378,0x3a4,0x3d0},tags[]={CI_MIDI_IN_PORT,CI_MIDI_IN_CHANNEL,CI_MIDI_OUT_PORT,CI_MIDI_OUT_CHANNEL};
 const uint32_t functions[]={0x1e897c0,0x1e8943c,0x1e890b4,0x1e88d30};
 for(io_sync_test=0;io_sync_test<7;io_sync_test++){
  unsigned f=io_sync_test<4?io_sync_test:io_sync_test-4;io_sync_field=fields[f];unsigned port=!(f&1),initial_value=port?0x7ffffffd:1;
  fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));unsigned char *objects=calloc(1,0x1600);require(fixture&&command_state&&objects,"synchronous MIDI fixture");
  initial(2);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);seed_fixture(0,0,0x3f000000,1);seed_fixture(1,1,0x3f000000,1);put(ptr(track_objects[0])+0x64c,4);load_fixture();Track *t=tracks;
  uint32_t property=t->track+offsets[f],outer=ptr(objects),pc=outer+0x100,connector=pc+0x2f4;channel_birth(t->track,property,tags[f],initial_value,NULL,0,1);
  ChannelCell *selection=channel_cell(project+0x78);channel_write(selection,t->track,NULL,0,0);put(root+0x75c,outer);put(outer+8,pc);put(connector,0x68a2710);put(connector+0x30,t->track);put(connector+0xa34,pool);
  Context c=context();c.r[0]=connector;call(&c,IO_CONNECTOR_BIRTH);c.r[4]=connector;call(&c,IO_CONNECTOR_READY);
  union {uint64_t alignment;unsigned char bytes[sizeof(Context)+64];} frame;Context *bind=(Context*)(frame.bytes+64),*bound=(Context*)frame.bytes;*bind=context();bind->r[0]=connector;bind->r[2]=t->track;call(bind,IO_BIND_ENTER);*bound=context();bound->r[4]=connector;call(bound,IO_BIND_DONE);
  if(port){IOPortCatalogue *cat=io_connector_copy(connector)->ports+(f==2);atomic_store(&cat->revision,2);atomic_store(&cat->valid,1);atomic_store(&cat->count,2);atomic_store(&cat->rows[0].id,initial_value);atomic_store(&cat->rows[1].id,2072254911);atomic_store(&cat->rows[0].selectable,1);atomic_store(&cat->rows[1].selectable,1);}
  unsigned site=f==0?IO_IN_PORT_COMMIT:f==2?IO_OUT_PORT_COMMIT:IO_CHANNEL_COMMIT;io_sync_commit=0;while(io_sync_commit<PATCH_COUNT&&hook_ids[io_sync_commit]!=site)io_sync_commit++;require(io_sync_commit<PATCH_COUNT&&capture_after_pair[io_sync_commit],"actual MIDI commit post-store site");
  void *stub=mmap(NULL,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);require(stub!=MAP_FAILED,"MIDI commit gate");build_stub(stub,io_sync_commit,0);uint32_t *patch=(uint32_t*)(uintptr_t)anchor[io_sync_commit];jump(&patch,ptr(stub));jump(&patch,ptr(capture_a));__builtin___clear_cache(stub,(char*)stub+4096);__builtin___clear_cache((char*)(uintptr_t)anchor[io_sync_commit],(char*)patch);
  patch=(uint32_t*)(uintptr_t)functions[f];jump(&patch,ptr(io_sync_gui_fixture));__builtin___clear_cache((char*)(uintptr_t)functions[f],(char*)patch);
  IOInterest *interest=&command_state->io_interest;atomic_store(&interest->revision,2);atomic_store(&interest->enabled,1);atomic_store(&interest->epoch,epoch);atomic_store(&interest->serial,t->serial);atomic_store(&interest->until,120000);
  Context drain=context();drain.r[0]=command_queues+4;io_service(&drain);MIRROR_COPY(out);require(copy_mirror(fixture,&out)&&out.io.fields[io_sync_field].status==IO_READY,"actual four-MIDI copied source ready");MirrorBank bank;bank_init(&bank);bank.assignment=BA_IO;bank_apply(&bank,&out,0);MirrorInput in={.commands=command_state};input_io_edit(&in,&bank,&out,io_sync_field,1,0);input_io_submit(&in,&bank,&out,0);call(&drain,COMMAND_DRAIN);
  require(!io_fault[0]&&!atomic_load(&command_state->error)&&atomic_load(&command_state->slots[0].returned)==1,"sync native receipts survive unrelated publications and post-call rebind");
  if(io_sync_test==4){require(!atomic_load(&command_state->slots[0].done)&&observer_running,"odd sync source remains healthy pending");atomic_fetch_add(&channel_cell(property)->revision,1);call(&drain,COMMAND_DRAIN);}
  command_retire();require(copy_mirror(fixture,&out),"sync completion copy");out.heartbeat=100;input_drain(&in,&bank,&out,100);command_retire();require(!in.error&&in.settled==1&&command_idle(command_state)&&observer_running,"sync receipt consumer settles/reclaims after native rebind");require(io_sync_test<5?in.flights[0].io_events[5].revision>0:in.flights[0].io_events[5].revision==0,"retired/reused sync source unavailable, never positive");
  munmap(stub,4096);free(objects);free(fixture);free(command_state);fixture=saved;mirror_state=saved;command_state=saved_command;
 }
 puts("PASS four typed MIDI fields: generated native commit detours, exact SET/COMMIT/DONE, unrelated listeners and selection/catalogue/page change; odd-source retry and dead/reused unavailable settlement (GUI/Do bodies, catalogue and native objects substituted)");
}
/* Accepted native enqueue/history receipts are substituted here. The actual
 * completion resolver, queued callback hooks, source owner and input consumer
 * must survive selection/interest changes and reject a retired incarnation. */
static float io_output_getter_fixture(void *p,unsigned kind,unsigned parameter){(void)p;require(kind==0x101&&parameter==325,"Audio Out completion getter ABI");return 2.0f;}
static void io_completion_checks(void){
 MirrorState *saved=fixture;CommandState *saved_command=command_state;
 const unsigned fields[]={IO_MONITOR,IO_SEND_TO,IO_AUDIO_IN,IO_AUDIO_OUT,IO_MONITOR,IO_SEND_TO,IO_MONITOR,IO_SEND_TO};
 uint32_t *entry=(uint32_t*)0x250b754;jump(&entry,ptr(io_output_getter_fixture));__builtin___clear_cache((char*)0x250b754,(char*)entry);
 for(unsigned test=0;test<8;test++){
  fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"I/O completion fixture state");initial(2);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
  seed_fixture(0,0,0x3f000000,1);seed_fixture(1,1,0x3f000000,1);unsigned f=fields[test],program=f==IO_AUDIO_IN||f==IO_AUDIO_OUT;
  if(f==IO_AUDIO_IN){put(ptr(program_objects[0]),0x692fec4);put(ptr(track_objects[0])+0x64c,6);}load_fixture();
  Track *t=tracks;uint32_t property=program?t->program+(f==IO_AUDIO_IN?0x2784:0x348):t->track+(f==IO_SEND_TO?0x420:0x400),apply=program?t->program+(f==IO_AUDIO_IN?0x2798:0x370):t->track+(f==IO_SEND_TO?0x434:0x414);
  unsigned tag=f==IO_MONITOR?CI_MIDI_MONITOR:f==IO_SEND_TO?CI_SEND_TO:f==IO_AUDIO_IN?CI_AUDIO_IN:CI_AUDIO_OUT;uint32_t description[6]={2,0,0,0,0,0};
  channel_birth(program?t->program:t->track,property,tag,0,f==IO_AUDIO_OUT?(const char*)description:NULL,f==IO_AUDIO_OUT?24:0,1);channel_birth(program?t->program:t->track,apply,tag+1,0,f==IO_AUDIO_OUT?(const char*)description:NULL,f==IO_AUDIO_OUT?24:0,1);
  ChannelCell *ui=channel_cell(property),*audio=channel_cell(apply);require(ui&&audio,"original I/O source cells");unsigned before=atomic_load(&ui->revision),wanted=2;channel_write(ui,wanted,f==IO_AUDIO_OUT?(const char*)description:NULL,f==IO_AUDIO_OUT?24:0,0);
  CommandRequest r={.seq=1,.pid=command_state->pid,.start_lo=command_state->start_lo,.start_hi=command_state->start_hi,.origin_sec=command_state->origin_sec,.origin_nsec=command_state->origin_nsec,.epoch=epoch,.serial=t->serial,.binding=t->binding,.incarnation=t->incarnation,.bits=1,.expires=120000,.reserved=IO_PARAMETER,.track_owner=t->track_owner,.program_owner=t->program_owner,.field_incarnation=atomic_load(&ui->incarnation),.project_owner=channel_owner_id(project),.global_owner=property,.io_generation=1,.io_choice_generation=1,.io_field=f,.io_connector=0x12345000};
  require(command_publish_request(command_state,&r),"substituted admitted I/O request publication");accepted[0].request=r;accepted[0].track=t->track;accepted[0].program=t->program;accepted[0].property=property;accepted[0].controller=program?(f==IO_AUDIO_IN?1162:325):f;accepted[0].native_bits=wanted;if(program){float v=2;memcpy(&accepted[0].native_bits,&v,4);}native_started[0]=1;io_audio_incarnation[0]=atomic_load(&audio->incarnation);atomic_store(active_request,1);
  for(unsigned lane=0;lane<COMMAND_LANES;lane++)atomic_store(&command_state->slots[0].lanes[lane].sequence,1);
  Context c=context();io_event(&c,CE_DISPATCH,0,wanted,1);uint32_t payload[3]={0,0,0};
  if(program){uint32_t capture=ptr(payload);event(&c,CE_QUEUE,1,capture,t->program+0x40,t->program,0x101,accepted[0].controller,accepted[0].native_bits,0,epoch);event(&c,CE_BODY,1,capture,t->program+0x40,t->program,0x101,accepted[0].controller,accepted[0].native_bits,0,epoch);event(&c,CE_DONE,1,capture,t->program+0x40,t->program,0x101,accepted[0].controller,accepted[0].native_bits,0,epoch);atomic_store(&command_state->slots[0].done,1);atomic_store(active_request,0);}
  else{io_event(&c,CE_IO_SET,0,0,before);io_helper[0]=ptr(queue_object);payload[0]=io_helper[0];payload[1]=wanted;payload[2]=t->track+(f==IO_SEND_TO?0x41c:0x3fc);atomic_store(io_payload,ptr(payload));io_job_event(&c,CE_IO_QUEUE,0,wanted);io_job_event(&c,CE_IO_RESULT,0,1);}
  io_event(&c,CE_RETURN,0,wanted,1);atomic_store(&command_state->slots[0].dispatched,1);atomic_store(&command_state->slots[0].returned,1);atomic_store(&command_state->slots[0].processed,1);atomic_store(&command_state->consumed,1);
  /* The old selected connector is gone, page is off, but original source lives. */
  ChannelCell *selection=channel_cell(project+0x78);channel_write(selection,tracks[1].track,NULL,0,0);memset(&io_current,0,sizeof(io_current));io_current.status=IO_OFF;atomic_store(&command_state->io_interest.enabled,0);
  if(test==4||test==5){channel_owner_death(t->track);if(test==5)channel_owner_birth(t->track,CO_TRACK);}
  channel_write(audio,wanted,f==IO_AUDIO_OUT?(const char*)description:NULL,f==IO_AUDIO_OUT?24:0,0);
  if(!program){c=context();c.r[0]=ptr(payload);call(&c,f==IO_SEND_TO?IO_SEND_ENTER:IO_MONITOR_ENTER);c.r[2]=0;call(&c,f==IO_SEND_TO?IO_SEND_DONE:IO_MONITOR_DONE);}
  Context drain=context();drain.r[0]=command_queues+4;
  if(test<2){unsigned revision=atomic_load(&audio->revision);atomic_store(&audio->revision,revision+1);io_complete(&drain);command_retire();require(atomic_load(io_finished)==1&&!io_receipted(0)&&!atomic_load(&command_state->slots[0].done)&&!atomic_load(&command_state->slots[0].sealed)&&!atomic_load(&command_state->error)&&observer_running,"post-DONE secondary source publication leaves healthy pending obligation");atomic_store(&audio->revision,revision+2);}
  if(test>=6){atomic_store(&audio->live,0);if(test==7)channel_birth(t->track,apply,tag+1,wanted,NULL,0,1);}
  io_complete(&drain);command_retire();require(io_receipted(0)&&!atomic_load(&command_state->error)&&observer_running&&atomic_load(&command_state->slots[0].sealed)==1,"admitted I/O closes after selection/interest change without current connector");
  MirrorInput in={.commands=command_state};in.flights[0]=(InputFlight){.flight=1,.field=IO_PARAMETER,.expires=120000,.io_request=r};MIRROR_COPY(out);require(copy_mirror(fixture,&out),"I/O completion copied reader");out.heartbeat=100;MirrorBank bank;bank_init(&bank);input_drain(&in,&bank,&out,100);command_retire();require(!in.error&&in.settled==1&&command_idle(command_state),"actual I/O receipt consumer reclaims original queued obligation");require(test<4?in.flights[0].io_events[5].revision>0:in.flights[0].io_events[5].revision==0,"retired/reused original source never positively settles");
  free(fixture);free(command_state);
 }
 fixture=saved;mirror_state=saved;command_state=saved_command;
 puts("PASS I/O admitted Monitor/Send/Audio In/Out completion survives selection/rebind/interest off; post-DONE odd audio cell stays pending then stable retry settles; retired/reused owner or audio cell returns unavailable; native enqueue/history and getter bodies substituted, actual callback/completion/consumer exercised");
}
static void *foreign_create_fixture(void *unused){
 (void)unused;Context c=context();c.r[0]=ptr(root_object);c.r[1]=0;c.lr=0x2570bc4;
 call(&c,M_CREATE_ENTER);call(&c,M_CREATE_READY);return NULL;
}
static void create_source_checks(void){
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"blank source fixture");
 static uint32_t creator[4];
 union {uint64_t align;unsigned char bytes[sizeof(Context)+0x128];} frame;
 Context *entry=(Context*)(frame.bytes+0x128),*close=(Context*)frame.bytes;
 for(unsigned failure=0;failure<5;failure++){
  initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
  channel_fixture(1);seed_fixture(0,0,0x3f000000,1);
  creator[2]=ptr(root_object);put(ptr(root_object)+0x240,ptr(creator));
  *entry=context();entry->r[0]=ptr(root_object);entry->r[4]=ptr(creator);entry->lr=0x103ac80;
  call(entry,M_CREATE_ENTER);require(active()&&!root&&!ready&&!command_queues,"startup direct create does not enroll root or global queues");
  entry->lr=0x2570bc4;entry->r[1]=0;call(entry,M_CREATE_ENTER);require(active()&&!root&&!ready&&!command_queues,"automatic Engine callback with argument0 remains unavailable");
  entry->r[1]=1;call(entry,M_CREATE_ENTER);
  require(active()&&!ready&&creating.token&&root==ptr(root_object)&&command_queues==ptr(queue_object),"blank entry enrolls existing root owners without readiness");
  if(!failure){
   uint32_t admitted_token=creating.token,admitted_sp=creating.sp,admitted_epoch=epoch;pthread_t foreign;
   require(!pthread_create(&foreign,NULL,foreign_create_fixture,NULL)&&!pthread_join(foreign,NULL),"separate ARM thread runs unrelated automatic creation callbacks");
   require(active()&&observer_running&&!atomic_load(&command_violation)&&!ready&&creating.token==admitted_token&&creating.sp==admitted_sp&&epoch==admitted_epoch,"foreign automatic entry/completion preserves admitted flight and healthy unavailable source");
  }
  unsigned before_epoch=epoch;Context clear=context();clear.r[0]=project;call(&clear,M_PROJECT_CLEAR);
  require(active()&&creating.token&&epoch>before_epoch&&!ready,"native teardown invalidation preserves only exact create flight");
  union {uint64_t align;unsigned char bytes[sizeof(Context)+0x38];} add_frame;
  Context *add=(Context*)(add_frame.bytes+0x38),*commit=(Context*)add_frame.bytes;
  *add=context();add->r[0]=pool;add->r[2]=members[0];call(add,M_ADD_ENTER);
  *commit=context();commit->r[4]=pool;commit->r[5]=members[0];call(commit,M_ADD_COMMIT);call(commit,M_ADD_EXIT);
  require(active()&&!count&&!ready,"native construction add cannot publish early inventory");
  *close=context();close->r[4]=ptr(root_object);put(source_sp(close)+0x11c,0x2570bc4);
  if(failure==1)put(source_sp(close)+0x11c,0x2570b7c);
  if(failure==2)channel_owner_birth(project,CO_PROJECT);
  if(failure==3)put(root+0x24c,0);
  if(failure==4)put(pool+0x38,word(pool+0x34)+3);
  call(close,M_CREATE_READY);
  if(failure)require(!active()&&!ready&&!creating.token,"malformed close, reused Project, changed root or malformed inventory fails closed");
  else{MIRROR_COPY(out);require(active()&&ready&&!creating.token&&copy_mirror(fixture,&out)&&out.ready&&out.count==1&&out.tracks[0].bits==0x3f000000,"blank completion publishes actual copied consumer inventory and constructor value");}
 }
 free(fixture);free(command_state);fixture=NULL;command_state=NULL;
 puts("PASS blank creation actual source dispatch, foreign-thread automatic completion exclusion, teardown/add and copied inventory; native creator bodies/objects substituted, no native blank acceptance claim");
}
static void type_source_checks(void){
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"Type source fixture");
 initial(2);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);seed_fixture(0,0,0x3f000000,1);seed_fixture(1,0,0x3f000000,1);load_fixture();
 uint32_t cmd[24]={0},old=tracks[0].program,next=tracks[1].program,t=tracks[0].track,before=tracks[0].binding;
 cmd[0]=0x68f0940;cmd[0x2c/4]=project;cmd[0x34/4]=0;cmd[0x38/4]=3;cmd[0x44/4]=old;
 union {uint64_t align;unsigned char bytes[sizeof(Context)+0x70];} frame;
 Context *entry=(Context*)(frame.bytes+0x70),*close=(Context*)frame.bytes;
 *entry=context();entry->r[0]=ptr(cmd);entry->lr=0x146fcdc;call(entry,292);require(type_do.phase==1,"actual Type Do hook admits native history frame");
 struct {Context c;uint32_t stack[8];} replace={0};replace.c=context();replace.c.r[4]=t;replace.c.r[5]=next;replace.c.r[9]=old;call(&replace.c,M_REPLACE_FULL_PRE);
 put(t+0x648,next);put(t+0x64c,3);replace.c.r[3]=3;call(&replace.c,M_REPLACE_FULL_STORE);replace.stack[3]=old;call(&replace.c,M_REPLACE_NOTIFY);require(tracks[0].transition==3,"real replacement hook chain retains pending binding");
 *close=context();close->r[4]=ptr(cmd);close->r[0]=0;put(source_sp(close)+0x6c,0x146fcdc);call(close,293);
 require(active()&&!tracks[0].transition&&tracks[0].program==next&&tracks[0].binding>before&&tracks[0].kind==3,"Type Do completion refreshes real inventory without optional M_PROGRAM triplet");
 before=tracks[0].binding;cmd[0x44/4]=next;cmd[0x38/4]=4;*entry=context();entry->r[0]=ptr(cmd);entry->lr=0x146fcdc;call(entry,292);*close=context();close->r[4]=ptr(cmd);close->r[0]=3;put(source_sp(close)+0x6c,0x146fcdc);call(close,293);require(!type_do.phase&&tracks[0].binding==before,"native Type rejection does not invent binding");
 *entry=context();entry->r[0]=ptr(cmd);entry->lr=0x146fcdc;cmd[0x44/4]=old;call(entry,292);require(!type_do.phase,"old Program mismatch refuses source frame");
 /* Actual handler faults must close the source and clear TLS, including when
  * another valid frame arrives in the same epoch. Reset below substitutes a
  * new healthy observer between independent fault scenarios, never production recovery. */
 for(unsigned failure=0;failure<5;failure++){
  unsigned saved_serial=tracks[0].serial,saved_owner=tracks[0].track_owner;uint32_t saved_native=word(t+0x648);
  atomic_store(&fixture->alive,1);atomic_store(&fixture->error,0);cmd[0x44/4]=next;cmd[0x38/4]=3;
  *entry=context();entry->r[0]=ptr(cmd);entry->lr=0x146fcdc;call(entry,292);require(type_do.phase==1,"fault fixture admitted Type frame");
  if(failure==1)tracks[0].serial++;
  if(failure==2)tracks[0].track_owner++;
  if(failure==3)put(t+0x648,old);
  *close=context();close->r[4]=ptr(cmd)+(failure==0?4:0);close->r[0]=failure==4?2:0;put(source_sp(close)+0x6c,0x146fcdc);call(close,293);
  require(!type_do.phase&&!active()&&atomic_load(&fixture->error)==MV_LIFETIME,"malformed close or unproved success closes mirror and clears TLS");
  tracks[0].serial=saved_serial;tracks[0].track_owner=saved_owner;put(t+0x648,saved_native);
  *entry=context();entry->r[0]=ptr(cmd);entry->lr=0x146fcdc;call(entry,292);require(!type_do.phase&&!active(),"next valid same-epoch frame cannot silently revive failed source");
 }
 free(fixture);free(command_state);fixture=NULL;command_state=NULL;puts("PASS Type source hooks and inventory; native history/object bodies substituted, no native popup acceptance claim");
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--replay")){alarm(20);exercise_adjust=4096;replay_checks();return 0;}
 if(argc==2&&!strcmp(argv[1],"--create-source")){alarm(20);create_source_checks();return 0;}
 if(argc==2&&!strcmp(argv[1],"--type-source")){alarm(20);type_source_checks();return 0;}
 if(argc!=3)return 2;
 component_recording_dispatch=recording_fallback_fixture;
 alarm(30);retired_key_checks();manual_checks();exercise_adjust=4096;replay_checks();
 void *v=mmap((void*)0x6930000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);require(v!=(void*)-1,"fixture Drum vtable page");put(0x6930c28,0x250ba74);put(0x6930c78,0x1375c68);
 key_statics_reserve();
 fixture=command_file(argv[1],sizeof(MirrorState));command_state=command_file(argv[2],sizeof(CommandState));require(fixture!=MAP_FAILED&&command_state!=MAP_FAILED,"exclusive actual shared output files");
 pointer_device_checks();wheel_data_checks();focus_press_checks();processor_lifecycle_checks();repair_checks();external_close_checks();new_project_checks();heartbeat_checks();drain_scan_checks();snapshot_clear_checks();channel_checks();channel_reclaim_checks();channel_seqlock_checks();jog_checks();master_checks();recording_checks();general_checks();send_destination_checks();bus_membership_checks();meter_interest_checks();registration_overlap_checks();mode_acceptance_checks();io_audio_checks();io_monitor_dispatch_checks();io_sync_dispatch_checks();io_completion_checks();
 initial(1);command_state->magic=COMMAND_MAGIC;command_state->version=COMMAND_VERSION;command_state->bytes=sizeof(*command_state);command_state->capacity=COMMAND_SLOTS;command_state->pid=getpid();uint64_t start=command_process_start(getpid());command_state->start_lo=start;command_state->start_hi=start>>32;command_state->origin_sec=fixture->origin_sec;command_state->origin_nsec=fixture->origin_nsec;command_state->seconds=WINDOW_SECONDS;atomic_store(&command_state->alive,1);
 command_initialize();observer_running=1;observer_image_bias=0;component_dispatch=fake_dispatch;
 seed_fixture(0,0,0x3f000000,1);load_fixture();
 CommandRequest first=request(0x3f200000);defer_audio=1;unsigned n=submit_fixture(first);
 require(atomic_load(&command_state->slots[n].returned)==1&&!atomic_load(&command_state->slots[n].done)&&dispatched_calls==1,"native return is not source completion or reuse");
 Context drain=context();drain.r[0]=ptr(queue_object)+4;call(&drain,COMMAND_DRAIN);command_retire();require(!atomic_load(&command_state->reclaimed),"return alone cannot reclaim transaction");
 /* Hold the already admitted done hook after its done publication. */
 atomic_store(&pause_at,7);atomic_store(&pause_reached,0);atomic_store(&pause_resume,0);pthread_t audio;require(!pthread_create(&audio,NULL,fake_audio,NULL),"deferred source body thread");
 while(!atomic_load_explicit(&pause_reached,memory_order_acquire)){struct timespec delay={0,1000000};nanosleep(&delay,NULL);}
 call(&drain,COMMAND_DRAIN);command_retire();require(atomic_load(&command_state->slots[0].done)==1&&!atomic_load(active_request)&&!atomic_load(&command_state->slots[0].sealed)&&!atomic_load(&command_state->reclaimed),"late admitted old-sequence flight blocks seal and reuse");
 atomic_store_explicit(&pause_resume,1,memory_order_release);require(!pthread_join(audio,NULL),"retired old-sequence hook exits");atomic_store(&pause_at,0);
 call(&drain,COMMAND_DRAIN);command_retire();require(atomic_load(&command_state->slots[0].sealed)==1&&!atomic_load(&command_state->reclaimed),"owner seals evidence separately from client settlement");
 MIRROR_COPY(out);require(copy_mirror(fixture,&out)&&playable(out.tracks[0].vptr)&&out.tracks[0].bits==first.bits,"actual register commit is authoritative MMV7");
 atomic_store_explicit(&command_state->slots[0].settled,1,memory_order_release);call(&drain,COMMAND_DRAIN);command_retire();require(atomic_load(&command_state->reclaimed)==1,"sealed settled transaction becomes reusable");
 defer_audio=0;
 for(unsigned seq=2;seq<=32;seq++){
  CommandRequest r=request(seq&1?0x3f200000:0x3f100000);submit_fixture(r);
  require(atomic_load(&command_state->slots[0].done)==seq&&atomic_load(&command_state->slots[0].returned)==seq&&!atomic_load(&command_state->trace_error),"unique sequence source completion beyond sixteen requests");
  call(&drain,COMMAND_DRAIN);command_retire();require(atomic_load(&command_state->slots[0].sealed)==seq,"matching retired flights seal each generation");
  require(copy_mirror(fixture,&out)&&out.tracks[0].bits==r.bits,"each generation follows actual source value");
  CommandEvent copied;unsigned commits=0;
  for(unsigned lane=0;lane<COMMAND_LANES;lane++)for(unsigned i=0;i<atomic_load(&command_state->slots[0].lanes[lane].published);i++){
   require(command_event_read(command_state->slots[0].lanes+lane,seq,i,&copied)&&copied.request==seq,"reused atomic evidence belongs only to current sequence");commits+=copied.kind==CE_COMMIT;
   require(!command_event_read(command_state->slots[0].lanes+lane,seq-1,i,&copied),"old-sequence reader cannot certify new transaction");
  }
  require(commits==1,"one contained commit in sealed evidence");
  atomic_store_explicit(&command_state->slots[0].settled,seq,memory_order_release);call(&drain,COMMAND_DRAIN);command_retire();require(atomic_load(&command_state->reclaimed)==seq,"settlement releases exact sequence");
 }
 require(dispatched_calls==32&&active(),"one physical slot supports32 real component source transactions");
 CommandRequest stale=request(0x3f400000);Context c=context();c.r[0]=pool;c.r[1]=members[0];call(&c,M_REMOVE_ENTER);mutation.sp=source_sp(&c)+0x30;put(pool+0x38,ptr(members));c.r[5]=pool;c.r[4]=members[0];call(&c,M_REMOVE_COMMIT);call(&c,M_REMOVE_EXIT);
 submit_fixture(stale);require(atomic_load(&command_state->slots[0].processed)==33&&atomic_load(&command_state->slots[0].rejected)==C_IDENTITY&&dispatched_calls==32,"stale removed identity rejects without dispatch after reuse");
 atomic_store(lane_in_hook+1,1);command_retire();require(atomic_load(&command_state->reclaimed)==32,"rejected request cannot reclaim across admitted source flight");
 atomic_store(lane_in_hook+1,0);native_started[0]=33;command_retire();require(atomic_load(&command_state->reclaimed)==32,"post-native rejection cannot take refused-only reclamation");
 native_started[0]=0;command_retire();require(command_idle(command_state)&&atomic_load(&command_state->slots[0].reclaimed)==33,"never-dispatched stale rejection releases mailbox for next bridge start");
 require(atomic_load(&command_state->slots[0].returned)!=33&&atomic_load(&command_state->slots[0].done)!=33&&atomic_load(&command_state->slots[0].settled)!=33&&atomic_load(&command_state->slots[0].sealed)!=33,"rejected reclamation never certifies native settlement");
 command_close();require(command_quiescent(),"finite component source closure");require(command_finalize(),"actual production close finalization");
 puts("PASS CMD13 reusable atomic transaction:32 unique requests, late old-flight seal barrier, source settlement/reclaim, sequence reader rejection and stale removal; substituted native method/body on ARM pthread");return 0;
}
