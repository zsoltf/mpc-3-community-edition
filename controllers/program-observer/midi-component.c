/* Real retained-state/input/command/receipt consumer composition. Native objects,
 * name lookup, factory, queued bodies and scheduling are explicit substitutions. */
#define main command_suite_not_called
#include "command-component.c"
#undef main
#include "channel-wire.h"
static uint32_t midi_fixture_payload[32] __attribute__((aligned(8)));
static uint32_t midi_fixture_bits,midi_fixture_track;
static unsigned midi_fixture_auto,midi_fixture_calls,midi_fixture_duplicate,midi_fixture_no_set,midi_fixture_automation;
static uint32_t midi_lookup_fixture(uint32_t p,uint32_t name){
 require(p==ptr(pool_object)&&name>=ptr(track_objects[0])+0x468,"native name preflight ABI");
 return midi_fixture_duplicate?ptr(track_objects[0]):name-0x468;
}
static void *midi_apply_fixture(void *unused){
 (void)unused;union {uint64_t align;unsigned char bytes[sizeof(Context)+0x18];} frames;
 unsigned delta=midi_fixture_auto?0x18:8;Context *entry=(Context*)(frames.bytes+delta),*end=(Context*)frames.bytes;
 uint32_t n=ptr(midi_fixture_payload+2),t=midi_fixture_track;
 *entry=context();entry->r[0]=n;call(entry,midi_fixture_auto?MI_AUTO_ENTER:MI_ENTER);
 if(!midi_fixture_no_set){
  /* Automation Write armed: the application writes this Track level through its
   * own record path inside the command's window. Each injected call is
   * positively not this command's - another call site, another value, or a
   * repeat after this command's own call and commit were receipted. */
  Context f;
  if(midi_fixture_automation){
   f=context();f.r[0]=t+0x28c;f.lr=0x264d500;f.d[0]=midi_fixture_bits;call(&f,MI_SET);
   f=context();f.r[0]=t+0x28c;f.lr=0x264d478;f.d[0]=midi_fixture_bits^1u;call(&f,MI_SET);
  }
  Context c=context();c.r[0]=t+0x28c;c.lr=0x264d478;c.d[0]=midi_fixture_bits;call(&c,MI_SET);
  ChannelCell *v=channel_cell(t+0x28c);
  if(midi_fixture_automation){
   f=context();f.r[0]=t+0x28c;f.lr=0x264d478;f.d[0]=midi_fixture_bits;call(&f,MI_SET);
   f=context();f.r[5]=t+0x28c;f.d[8]=midi_fixture_bits^1u;call(&f,M_FLOAT);
  }
  if(atomic_load(&v->bits)!=midi_fixture_bits){c=context();c.r[5]=t+0x28c;c.d[8]=midi_fixture_bits;call(&c,M_FLOAT);}
  if(midi_fixture_automation){f=context();f.r[5]=t+0x28c;f.d[8]=midi_fixture_bits;call(&f,M_FLOAT);}
 }
 *end=context();end->r[4]=n;end->r[3]=word(n)+4;call(end,midi_fixture_auto?MI_AUTO_DONE:MI_DONE);return NULL;
}
/* Execute the complete position-independent native constructor, copied from
 * the pinned ELF guard. This prevents fixtures inventing an event discriminator
 * that the MPC never emits (the original fixture incorrectly used tag2). */
static void midi_native_event_install(void){
 void *p=mmap((void*)0x2402000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);require(p!=MAP_FAILED,"native MIDI event constructor page");
 unsigned found=0;
 for(unsigned i=0;i<sizeof(guards)/sizeof(guards[0]);i++)if(guards[i].address==0x240274c){require(guards[i].length==0x60,"complete native constructor length");memcpy((void*)0x240274c,guards[i].bytes,guards[i].length);found++;}
 require(found==1&&!mprotect(p,4096,PROT_READ|PROT_EXEC),"pinned native constructor installed read/execute");__builtin___clear_cache((char*)p,(char*)p+4096);
}
static void midi_dispatch_fixture(uint32_t f,uint32_t t,unsigned ctrl,uint32_t old,uint32_t engaged,uint32_t flag,float value){
 require(f==ptr(recording_fixture_factory)&&ctrl==7&&!old&&!engaged&&!flag,"GUI Track factory r0/r1/r2/r3, two stack words, s0");
 midi_fixture_calls++;midi_fixture_track=t;memcpy(&midi_fixture_bits,&value,4);memset(midi_fixture_payload,0,sizeof(midi_fixture_payload));
 uint32_t h=ptr(midi_fixture_payload),n=h+8,key=midi_fixture_auto?f+4:t+0x4d4,ev=n+(midi_fixture_auto?0x10:8);
 put(h,midi_fixture_auto?0x13cfa14:0x265415c);put(h+4,midi_fixture_auto?0x13d1950:0x2649de8);put(n,key);
 put(n+(midi_fixture_auto?0x48:0x40),t);if(midi_fixture_auto)put(n+8,f);
 ((void (*)(void*,unsigned,uint32_t,uint32_t,unsigned,unsigned,float))(uintptr_t)0x240274c)((void*)(uintptr_t)ev,7,0,0,0x100,0,value);
 require(word(ev+8)==1&&word(ev+0x10)==0x100&&word(ev+0x14)==midi_fixture_bits&&word(ev+0x18)==7,"actual native constructor emits tag1/controller7/kind256 and desired float");
 Context c=context();c.r[7]=ptr(queue_object)+0xfc;c.r[9]=key|1u;c.r[10]=h;call(&c,JG_PUBLISH);
 pthread_t thread;require(!pthread_create(&thread,NULL,midi_apply_fixture,NULL)&&!pthread_join(thread,NULL),"substituted native queue callback on separate source thread");
}
static void midi_seed(unsigned index){
 struct {Context c;uint32_t stack[8];} frame={0};uint32_t t=ptr(track_objects[index]);
 frame.c=context();frame.c.r[4]=t+0x114;frame.c.r[8]=t+0x28c;frame.c.r[2]=0x3f800000;frame.stack[0]=t;frame.stack[7]=0x26522b4;call(&frame.c,MI_VOLUME_BIRTH);
 put(t+0x2b4,0x7fc09999); /* The reader must not poll this storage. */
}
static void midi_setup(void){
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"bounded state");initial(3);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 for(unsigned i=0;i<3;i++)seed_fixture(i,0,0x3f000000,1);
 channel_fixture(3);put(ptr(program_objects[1]),0x6932338);put(ptr(program_objects[2]),0x6930a14);midi_seed(1);midi_seed(2);load_fixture();atomic_store(&fixture->heartbeat,10);
 component_midi_dispatch=midi_dispatch_fixture;component_midi_lookup=midi_lookup_fixture;
}
static void midi_command(unsigned pitch,int negative){
 unsigned now=atomic_load(&fixture->heartbeat);MIRROR_COPY(s);require(copy_mirror(fixture,&s),"source copy");MirrorBank bank;bank_init(&bank);bank_apply(&bank,&s,now);MirrorInput in={.commands=command_state};
 input_pitch(&in,&bank,1,pitch,now);input_pump(&in,&bank,&s,now);unsigned seq=atomic_load(&command_state->published),at=command_find(command_state,seq);
 require(in.submitted==1&&!in.error&&at<COMMAND_SLOTS,"MIDI fader publishes typed request");CommandRequest r;require(command_request_read(command_state->slots+at,seq,&r)&&r.reserved==CF_MIDI_VOLUME&&r.field_incarnation==s.tracks[1].fields[CF_MIDI_VOLUME].incarnation,"Track-owned volume field identity");
 Context c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);
 if(midi_fixture_duplicate){require(atomic_load(&command_state->slots[at].rejected)==C_NATIVE_MEMBERSHIP&&!atomic_load(&command_state->slots[at].dispatched),"duplicate name refuses before native factory");return;}
 if(atomic_load(&command_state->error)||atomic_load(&command_state->trace_error))fprintf(stderr,"MIDI native error=%u trace=%u rejected=%u\n",atomic_load(&command_state->error),atomic_load(&command_state->trace_error),atomic_load(&command_state->slots[at].rejected));
 require(!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error)&&atomic_load(&command_state->slots[at].done)==seq,"both native factory branches capture queue/set/commit/DONE");
 if(midi_fixture_automation){unsigned sets=0,commits=0;
  for(unsigned lane=0;lane<COMMAND_LANES;lane++){CommandLane *l=command_state->slots[at].lanes+lane;unsigned n=atomic_load(&l->published);
   for(unsigned i=0;i<n;i++){CommandEvent e;if(command_event_read(l,seq,i,&e)){sets+=e.kind==CE_MIDI_SET;commits+=e.kind==CE_MIDI_COMMIT;}}}
  require(sets==1&&commits==1,"armed automation: exactly one MIDI set and commit receipt beside the record path's own writes");}
 command_retire();
 atomic_store(&fixture->heartbeat,now+1);require(copy_mirror(fixture,&s),"post-body source copy");bank_apply(&bank,&s,now+1);
 if(negative){
  CommandState *missing=malloc(sizeof(*missing));require(missing!=NULL,"negative receipt copy");memcpy(missing,command_state,sizeof(*missing));unsigned removed=0;
  for(unsigned lane=0;lane<COMMAND_LANES;lane++){CommandLane *l=missing->slots[at].lanes+lane;unsigned n=atomic_load(&l->published);for(unsigned i=0;i<n;i++){CommandEvent e;require(command_event_read(l,seq,i,&e),"receipt read");if(e.kind!=CE_MIDI_COMMIT)continue;for(unsigned j=i+1;j<n;j++){CommandEvent next;require(command_event_read(l,seq,j,&next),"following receipt");command_event_store(l->events+j-1,&next);}atomic_store(&l->published,n-1);removed++;break;}}
  MirrorInput bad=in;bad.commands=missing;input_drain(&bad,&bank,&s,now+1);require(removed==1&&bad.error==C_TRACE_AMBIGUOUS&&!bad.settled,"missing required MIDI commit never acknowledged despite matching scalar and DONE");free(missing);
 }
 input_drain(&in,&bank,&s,now+1);if(in.error)fprintf(stderr,"MIDI consumer error=%u seen=%x phase=%u\n",in.error,in.flights[at].midi_seen,in.flights[at].phase);
 require(!in.error&&in.settled==1,"actual consumer settles exact MIDI receipt");command_retire();require(command_idle(command_state),"MIDI receipt reclamation waits settlement");
}
int main(void){
 alarm(30);void *v=mmap((void*)0x6930000,0x4000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);require(v!=MAP_FAILED,"fixture type pages");put(0x6930c28,0x250ba74);put(0x6930c78,0x1375c68);midi_native_event_install();midi_setup();
 MIRROR_COPY(s);require(copy_mirror(fixture,&s)&&s.tracks[1].available&&s.tracks[1].bits==0x3f800000&&!volume_capable(s.tracks[2].vptr),"default MIDI127 comes from ctor registers; CV remains unavailable");
 ChannelWire w=channel_wire(&s,s.tracks+1,CF_VOLUME);require(!memcmp(w.value,"127",3),"MIDI LCD uses integer127 not audio dB");
 require(!s.tracks[1].fields[CF_PAN].available&&!s.tracks[1].fields[CF_SEND1].available,"unsupported MIDI pan and sends unavailable");
 /* A downstream CC cache change has no captured-state property identity. */
 Context c=context();c.r[5]=ptr(program_objects[1])+0x700;c.d[8]=0;call(&c,M_FLOAT);copy_mirror(fixture,&s);require(s.tracks[1].bits==0x3f800000,"unrelated CC cache cannot replace retained Track level");
 for(unsigned branch=0;branch<2;branch++){midi_fixture_auto=branch;midi_command(0,1);midi_command(8192,1);midi_command(16383,1);}
 midi_fixture_no_set=1;midi_command(4000,0);midi_fixture_no_set=0;
 /* MIDI desires in the same7-bit value bucket do not enqueue redundant CCs. */
 copy_mirror(fixture,&s);MirrorBank bank;bank_init(&bank);bank_apply(&bank,&s,30);MirrorInput in={.commands=command_state};unsigned before=atomic_load(&command_state->published);input_pitch(&in,&bank,1,16382,30);input_pump(&in,&bank,&s,30);require(!in.submitted&&atomic_load(&command_state->published)==before,"same127 bucket does not reenqueue");
 input_pitch(&in,&bank,0,5000,30);input_pitch(&in,&bank,1,5000,30);input_pump(&in,&bank,&s,30);require(in.submitted==2&&!in.error,"audio and MIDI requests coexist independently");atomic_store(&fixture->heartbeat,30);c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);command_retire();atomic_store(&fixture->heartbeat,31);copy_mirror(fixture,&s);bank_apply(&bank,&s,31);input_drain(&in,&bank,&s,31);require(!in.error&&in.settled==2,"mixed audio/MIDI native routes retain separate receipts");command_retire();
 /* A retired Track property cannot feed motors or retarget an unsent gesture. */
 copy_mirror(fixture,&s);bank_apply(&bank,&s,32);MirrorInput stale={.commands=command_state};input_pitch(&stale,&bank,1,7000,32);unsigned old=s.tracks[1].fields[CF_MIDI_VOLUME].incarnation;
 c=context();c.r[0]=ptr(track_objects[1])+0x28c;call(&c,MIRROR_PROPERTY_DESTROY);copy_mirror(fixture,&s);require(!s.tracks[1].available,"retired Track level unavailable");midi_seed(1);copy_mirror(fixture,&s);require(s.tracks[1].available&&s.tracks[1].fields[CF_MIDI_VOLUME].incarnation!=old,"same-address property receives new retained incarnation");input_pump(&stale,&bank,&s,32);require(!stale.submitted,"old property gesture cannot target new incarnation");
 free(fixture);free(command_state);midi_setup();unsigned calls=midi_fixture_calls;midi_fixture_duplicate=1;midi_command(5000,0);require(midi_fixture_calls==calls,"name ambiguity never reaches the factory");
 /* Armed automation Write beside a MIDI Track level moved from the controller. */
 midi_fixture_duplicate=0;free(fixture);free(command_state);midi_setup();midi_fixture_automation=1;midi_command(9000,0);midi_fixture_automation=0;
 puts("PASS MIDI retained default/source, 0/midpoint/127 LCD and faders, ordinary plus automation receipts, missing-commit refusal, no-set completion, redundant CC admission, mixed audio/MIDI, duplicate-name preflight; pinned native event constructor executed; factory/callback bodies/lookup/scheduling and physical MIDI substituted");free(fixture);free(command_state);munmap((void*)0x2402000,4096);munmap(v,0x4000);return 0;
}
