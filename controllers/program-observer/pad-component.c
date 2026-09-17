/* Real capture/consumer/command composition. Native objects, factory, queues,
 * setter bodies and their scheduling are deliberately substituted. */
#define main command_suite_not_called
#include "command-component.c"
#undef main
#include "channel-wire.h"
static unsigned char instruments[PAD_SLOTS+4][0x1490] __attribute__((aligned(8)));
static uint32_t pad_vector[PAD_SLOTS];
static uint32_t pad_payload[32],pad_desired,pad_code,pad_slot;
static unsigned swap_on_submit,ordinary_pad;
static void instrument_seed(uint32_t i,int copy){
 struct {Context c;uint32_t stack[128];} f={0};
 Context c=context();c.r[0]=i;c.r[1]=copy?ptr(instruments[0]):ptr(program_objects[0]);c.r[2]=ptr(program_objects[0]);call(&c,copy?PD_COPY:PD_BIRTH);
 f.c=context();f.c.r[4]=i;
 if(copy){f.stack[0x8c/4]=0x2370710;f.stack[0xf4/4]=0x2370a7c;f.stack[0x10c/4]=0x2435c28;f.stack[0xf8/4]=i;f.c.r[7]=i+0x574;f.c.r[3]=0x3f000000;call(&f.c,MIRROR_SEED_COPY);f.c.r[7]=0x3f000000;call(&f.c,CH_PAN_COPY);}
 else{f.stack[0xb4/4]=0x237034c;f.stack[0x114/4]=0x23705f8;f.stack[0x13c/4]=0x243727c;f.stack[0x12c/4]=i;f.c.r[5]=i+0x574;f.c.d[8]=0x3f000000;call(&f.c,MIRROR_SEED_NORMAL);f.c.d[16]=0x3f000000;call(&f.c,CH_PAN_BIRTH);}
 memset(f.stack,0,sizeof(f.stack));f.c.r[4]=i+0xc;f.c.r[2]=f.c.r[3]=f.c.r[6]=0;
 if(copy){f.stack[0x8c/4]=0x236faf4;f.stack[0x11c/4]=0x2370710;f.stack[0x184/4]=0x2370a7c;f.stack[0x19c/4]=0x2435c28;f.stack[0x188/4]=i;}
 else{f.stack[0x8c/4]=0x236f090;f.stack[0x144/4]=0x237034c;f.stack[0x1a4/4]=0x23705f8;f.stack[0x1cc/4]=0x243727c;f.stack[0x1bc/4]=i;}
 call(&f.c,copy?CH_MUTE_COPY:CH_MUTE_BIRTH);call(&f.c,copy?CH_SOLO_COPY:CH_SOLO_BIRTH);call(&f.c,copy?CH_SOLO_AUDIO_COPY:CH_SOLO_AUDIO_BIRTH);
 put(i,0x692f650);put(i+0x13b0,ptr(program_objects[0]));put(i+0x13d8,0);
 /* Poison native scalar storage: only captured register seeds may be exposed. */
 put(i+0x59c,0x7fc01234);put(i+0x3bc,0x7fc01234);
 c=context();c.r[4]=i;call(&c,copy?PD_COPY_COMPLETE:PD_COMPLETE);
 require(!atomic_load(&fixture->pads.error),"Instrument exact normal/copy register frames enroll all five fields");
}
static void instrument_bind(unsigned slot,uint32_t instrument){
 union {uint64_t align;unsigned char bytes[sizeof(Context)+0x58];} frames;
 Context *entry=(Context*)(frames.bytes+0x58),*end=(Context*)frames.bytes;
 *entry=context();entry->r[0]=ptr(program_objects[0]);entry->r[1]=slot;entry->r[2]=instrument;call(entry,PD_BIND);
 pad_vector[slot]=instrument;if(instrument)put(instrument+0x12dc,slot);
 *end=context();call(end,PD_BOUND);require(!atomic_load(&fixture->pads.error),"binding closes exact entry/return and copied child generation");
}
static void instrument_swap(unsigned a,unsigned b){
 union {uint64_t align;unsigned char bytes[sizeof(Context)+0x20];} frames;
 Context *entry=(Context*)(frames.bytes+0x20),*end=(Context*)frames.bytes;
 *entry=context();entry->r[0]=ptr(program_objects[0]);entry->r[1]=a;entry->r[2]=b;call(entry,PD_SWAP);
 uint32_t before=pad_vector[a];pad_vector[a]=pad_vector[b];pad_vector[b]=before;
 MIRROR_COPY(s);require(copy_mirror(fixture,&s)&&!s.pads[a].available&&!s.pads[b].available,"swap hides both slots before either direct native vector write can be consumed");
 instrument_bind(a,pad_vector[a]);instrument_bind(b,pad_vector[b]);
 *end=context();call(end,PD_SWAPPED);require(!atomic_load(&fixture->pads.error),"nested binds publish atomically at outer swap completion");
}
static unsigned pad_foreign;
/* This point is admitted for every pad execute made while a command call is
 * open on this thread, so the application executing other pads reaches it.
 * Every call here is positively not this command's execute: a receiver that is
 * not a pad slot, another bound pad slot of the same Program, another pad
 * field, another value, or, after the real execute, a repeat. */
static void pad_foreign_executes(unsigned after){
 unsigned other=pad_slot?pad_slot-1:pad_slot+1;Context f;
 if(after){f=context();f.r[0]=pad_vector[pad_slot];f.r[1]=pad_code;f.d[0]=pad_desired;call(&f,PD_EXECUTE);return;}
 f=context();f.r[0]=ptr(program_objects[0]);f.r[1]=pad_code;f.d[0]=pad_desired;call(&f,PD_EXECUTE);
 if(pad_vector[other]){f=context();f.r[0]=pad_vector[other];f.r[1]=pad_code;f.d[0]=pad_desired;call(&f,PD_EXECUTE);}
 f=context();f.r[0]=pad_vector[pad_slot];f.r[1]=pad_code^1u;f.d[0]=pad_desired;call(&f,PD_EXECUTE);
 f=context();f.r[0]=pad_vector[pad_slot];f.r[1]=pad_code;f.d[0]=pad_desired^1u;call(&f,PD_EXECUTE);
}
static void pad_set(void){
 if(pad_foreign){pad_foreign_executes(0);require(!atomic_load(&command_state->trace_error),"another pad's execute inside this pad command's window must not poison it");}
 Context c=context();c.r[0]=pad_vector[pad_slot];c.r[1]=pad_code;c.d[0]=pad_desired;call(&c,PD_EXECUTE);
 if(pad_foreign){pad_foreign_executes(1);require(!atomic_load(&command_state->trace_error),"a repeated pad execute after this command's own was receipted must not poison it");}
 unsigned field=pad_code==0x206?CF_VOLUME:pad_code==0x205?CF_PAN:pad_code==0x20e?CF_MUTE:CF_SOLO_AUDIO;
 c=context();c.r[5]=pad_vector[pad_slot]+pad_offset(pad_field(field));c.d[8]=pad_desired;c.r[7]=pad_desired?1:0;call(&c,field==CF_MUTE?M_MUTE:field==CF_SOLO_AUDIO?CH_BOOL_COMMIT:M_FLOAT);
}
static void *pad_apply(void *unused){
 (void)unused;union {uint64_t align;unsigned char bytes[sizeof(Context)+0x18];} frames;
 Context *entry=(Context*)(frames.bytes+0x18),*end=(Context*)frames.bytes;
 uint32_t n=ptr(pad_payload+2),d=ptr(program_objects[0]);
 *entry=context();entry->r[0]=n;call(entry,RC_ENTER);
 Context c=context();c.r[0]=d;c.r[2]=n+0x18;c.lr=0x1c04b94;call(&c,RC_APPLY);
 pad_set();
 *end=context();end->r[4]=n;call(end,RC_DONE);return NULL;
}
static void *pad_ordinary_apply(void *unused){
 (void)unused;union {uint64_t align;unsigned char bytes[sizeof(Context)+8];} frames;
 Context *entry=(Context*)(frames.bytes+8),*body=(Context*)frames.bytes;
 /* 13ed238 ordinary history -> 13f9980/1477a08 -> native250ba74
  * publishes [D+40,D,index,controller,s0] for25178b8 ->25175cc.
  * History/queue scheduling is substituted; these are their exact hook ABI. */
 *entry=context();entry->r[0]=ptr(pad_payload);call(entry,M_QUEUE_ENTER);
 *body=context();body->lr=0x25178d4;body->r[0]=pad_payload[1];body->r[1]=pad_payload[2];body->r[2]=pad_payload[3];body->d[0]=pad_payload[4];call(body,M_COMMAND_BODY);
 pad_set();
 *body=context();body->r[4]=ptr(pad_payload);body->r[3]=pad_payload[0];call(body,M_QUEUE_DONE);return NULL;
}
static void pad_dispatch(uint32_t f,uint32_t d,unsigned index,unsigned controller,float value){
 memcpy(&pad_desired,&value,4);pad_code=controller;pad_slot=index;
 require(f==ptr(recording_fixture_factory)&&d==ptr(program_objects[0])&&index<PAD_SLOTS&&controller>=0x205,"native GUI factory receives parent, pad index and native controller code");
 memset(pad_payload,0,sizeof(pad_payload));
 if(ordinary_pad){
  pad_payload[0]=d+0x40;pad_payload[1]=d;pad_payload[2]=index;pad_payload[3]=controller;pad_payload[4]=pad_desired;
 }else{
  uint32_t n=ptr(pad_payload+2);
  pad_payload[0]=0x13cf830;pad_payload[1]=0x13d18b0;put(n,f+4);put(n+8,f);put(n+0xc,index);put(n+0x10,controller);put(n+0x14,d);put(n+0x20,2);put(n+0x28,index);put(n+0x2c,pad_desired);put(n+0x30,controller);put(n+0x50,ptr(track_objects[0]));
  Context c=context();c.r[7]=ptr(queue_object)+0xfc;c.r[9]=(f+4)|1u;c.r[10]=ptr(pad_payload);call(&c,JG_PUBLISH);
 }
 if(swap_on_submit)instrument_swap(0,1);
 pthread_t thread;require(!pthread_create(&thread,NULL,ordinary_pad?pad_ordinary_apply:pad_apply,NULL)&&!pthread_join(thread,NULL),"native queued pad body runs on separate source thread (body substituted)");
 /* UI Solo is owner-thread feedback, separate from the audio setter.
  * Emulated here so repeated path tests can toggle both ways. */
 if(controller==0x20f){Context c=context();c.r[5]=pad_vector[index]+pad_offset(PF_SOLO);c.r[7]=pad_desired?1:0;call(&c,M_BOOL);}
}
static void pad_setup(void){
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"bounded mirror and command state");initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 seed_fixture(0,0,0x3f000000,1);load_fixture();
 put(ptr(program_objects[0])+0x1fec,ptr(pad_vector));put(ptr(program_objects[0])+0x1ff0,ptr(pad_vector)+sizeof(pad_vector));put(ptr(program_objects[0])+0x1ff4,ptr(pad_vector)+sizeof(pad_vector));
 for(unsigned i=0;i<PAD_SLOTS;i++){instrument_seed(ptr(instruments[i]),i%2);instrument_bind(i,ptr(instruments[i]));}
 Context c=context();c.r[5]=ptr(project_object)+0x78;c.r[7]=ptr(track_objects[0]);call(&c,CH_SELECTION_COMMIT);atomic_store(&fixture->heartbeat,10);component_recording_dispatch=pad_dispatch;
}
static void pad_command(unsigned field,int replacement){
 unsigned now=atomic_load(&fixture->heartbeat);
 MIRROR_COPY(s);require(copy_mirror(fixture,&s),"copied pad inventory");MirrorBank bank;bank_init(&bank);bank_view(&bank,BV_DRUM_PADS,now);bank_apply(&bank,&s,now);MirrorInput in={.commands=command_state};
 if(field==CF_VOLUME)input_pitch(&in,&bank,0,12000,now);else input_control(&in,&bank,&s,0,field,12,field==CF_MUTE||field==CF_SOLO,now);
 input_pump(&in,&bank,&s,now);unsigned seq=atomic_load(&command_state->published),at=command_find(command_state,seq);require(at<COMMAND_SLOTS&&!in.error,"production pad input publishes pointer-free command");
 CommandRequest r;require(command_request_read(command_state->slots+at,seq,&r)&&r.pad_owner&&r.pad_index==0&&r.reserved==field,"request separates parent binding, slot and submitted Instrument");
 swap_on_submit=replacement==1;Context c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);swap_on_submit=0;
 require(!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error)&&atomic_load(&command_state->slots[at].done)==seq,"exact GUI queue/actual child setter/source commit completes");
 command_retire();
 unsigned executed_owner=atomic_load(&recording_pad_owner[at]);
 if(replacement>=2){instrument_swap(0,1);if(replacement==3){uint32_t executed=atomic_load(&fixture->pads.owners[executed_owner-1].address);c=context();c.r[0]=executed;call(&c,PD_DEATH);}}
 atomic_store(&fixture->heartbeat,now+1);require(copy_mirror(fixture,&s),"new source feedback");bank_apply(&bank,&s,now+1);
 if(ordinary_pad&&field==CF_PAN&&!replacement){
  /* Exact consumer over this emitted receipt, with only the source commit
   * removed. DONE and a matching scalar are not substitutes for that event. */
  CommandState *missing=malloc(sizeof(*missing));require(missing!=NULL,"negative receipt copy");memcpy(missing,command_state,sizeof(*missing));
  unsigned removed=0;
  for(unsigned lane=0;lane<COMMAND_LANES;lane++){
   CommandLane *l=missing->slots[at].lanes+lane;unsigned n=atomic_load(&l->published);
   for(unsigned i=0;i<n;i++){CommandEvent e;require(command_event_read(l,seq,i,&e),"copied receipt event");if(e.kind!=CE_COMMIT)continue;
    for(unsigned j=i+1;j<n;j++){CommandEvent next;require(command_event_read(l,seq,j,&next),"following receipt event");command_event_store(l->events+j-1,&next);}
    atomic_store(&l->published,n-1);removed++;break;
   }
  }
  MirrorInput rejected=in;rejected.commands=missing;input_drain(&rejected,&bank,&s,now+1);
  require(removed==1&&rejected.error==C_TRACE_AMBIGUOUS&&!rejected.settled&&atomic_load(&missing->slots[at].settled)!=seq,"ordinary DONE without required source commit cannot settle");free(missing);
 }
 input_drain(&in,&bank,&s,now+1);
 if(in.error)fprintf(stderr,"pad field=%u error=%u recordseen=%x padseen=%u\n",field,in.error,in.flights[at].recording_seen,in.flights[at].pad_seen);
 require(!in.error&&in.settled==1,"real consumer settles from exact execution receiver");
 require(ordinary_pad?in.flights[at].phase==4&&!in.flights[at].recording_seen:!in.flights[at].phase&&in.flights[at].recording_seen,"ordinary history and recording receipts stay distinct");
 require(in.flights[at].pad_execution.bits==executed_owner&&(replacement!=1||r.pad_owner!=executed_owner)&&(replacement<2||s.pads[0].pad_owner!=executed_owner),"slot swap records actual Instrument instead of certifying old Instrument");
 command_retire();require(command_idle(command_state),"pad transaction reclaimed only after all source hooks and settlement");
}
static void pad_mixed_controls(void){
 unsigned now=atomic_load(&fixture->heartbeat);MIRROR_COPY(s);copy_mirror(fixture,&s);MirrorBank b;bank_init(&b);bank_view(&b,BV_DRUM_PADS,0);bank_apply(&b,&s,1);MirrorInput in={.commands=command_state};
 for(unsigned i=0;i<8;i++){input_pitch(&in,&b,i,2000+300*i,now);input_pitch(&in,&b,i,(ordinary_pad?6000:5000)+400*i,now);input_control(&in,&b,&s,i,CF_PAN,3,0,now);}
 input_pump(&in,&b,&s,now);require(!in.error&&input_pending(&in)==16,"eight faders and eight pans independently publish coalesced desires");
 Context c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);command_retire();atomic_store(&fixture->heartbeat,now+1);copy_mirror(fixture,&s);bank_apply(&b,&s,now+1);input_drain(&in,&b,&s,now+1);
 require(!in.error&&in.settled==16,"all sixteen pad controls settle through real command/capture/consumer composition");command_retire();require(command_idle(command_state),"mixed pad controls drain all slots");
 /* An already-submitted old Instrument still conflicts with its slot's new
  * occupant, while another pad and the parent Track volume remain independent. */
 CommandRequest a={.reserved=CF_VOLUME,.program_owner=1,.pad_owner=1,.pad_index=3,.pad_generation=1},d=a;d.pad_owner=2;d.pad_generation=2;require(command_conflict(&a,&d),"replacement cannot overtake old request in same pad slot");d.pad_index=4;require(!command_conflict(&a,&d),"different pad slots are independent");d.pad_owner=d.pad_index=d.pad_generation=0;require(!command_conflict(&a,&d),"pad and parent track volume do not conflict");
}
int main(void){
 alarm(30);void *v=mmap((void*)0x6930000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);require(v!=MAP_FAILED,"fixture native type page");put(0x6930c28,0x250ba74);put(0x6930c78,0x1375c68);pad_setup();
 /* Only the pad rows are poisoned. A copy destination must be zero at first
  * use (MIRROR_COPY), but the pad region is exempt from that rule because
  * copy_pad memsets a whole CopiedTrack before writing it, so no byte of a pad
  * row is ever inherited. That is what lets this check still say what it says:
  * a normal mixer copy does not inspect or clear the 128 pad rows. */
 MIRROR_COPY(lazy);memset(lazy.pads,0xa5,sizeof(lazy.pads));require(copy_mirror_view(fixture,&lazy,0)&&!lazy.pad_count&&lazy.pads[0].serial==0xa5a5a5a5,"normal mixer does not inspect or clear 128 pad rows");
 MIRROR_COPY(s);require(copy_mirror(fixture,&s)&&s.pad_count==128&&s.pads[127].available&&s.pads[127].bits==0x3f000000,"all untouched offscreen pad values come from constructor register capture despite poisoned native memory");
 MirrorBank bank;bank_init(&bank);bank_view(&bank,BV_DRUM_PADS,0);bank_apply(&bank,&s,1);bank.offset=120;bank_apply(&bank,&s,1);ChannelWire w=channel_wire(&s,s.pads+127,CF_VOLUME);require(!memcmp(w.name,"H16",3)&&!s.pads[127].fields[CF_NAME].available&&!s.pads[127].fields[CF_COLOR].available&&!w.led[3],"native pad labels, unsupported metadata/selection absent");
 require(bank.strips[7].pad_index==127&&bank_due(&bank,7,10)==8192,"last bank selects offscreen H16 motor feedback");
 pad_command(CF_VOLUME,1);pad_command(CF_PAN,0);pad_command(CF_MUTE,0);pad_command(CF_SOLO,0);pad_mixed_controls();pad_command(CF_PAN,2);
 /* The application executing other pads while this pad command is in flight. */
 pad_foreign=1;pad_command(CF_MUTE,0);pad_command(CF_SOLO,0);pad_foreign=0;
 ordinary_pad=1;
 pad_command(CF_PAN,0);pad_command(CF_VOLUME,1);pad_command(CF_MUTE,0);pad_command(CF_SOLO,0);pad_command(CF_PAN,2);
 pad_foreign=1;pad_command(CF_MUTE,0);pad_command(CF_SOLO,0);pad_foreign=0;
 /* Sixteen concurrent ordinary pad controls used to produce OTHER_COMMIT
  * with no queue receipt, freezing the bridge on its first adjustment. */
 pad_mixed_controls();
 ordinary_pad=0;
 /* Source invalidation must not retarget an unsent held gesture. */
 require(copy_mirror(fixture,&s),"post-command copy");bank_init(&bank);bank_view(&bank,BV_DRUM_PADS,0);bank_apply(&bank,&s,1);MirrorInput in={.commands=command_state};input_touch(&in,&bank,&s,0,1,10);input_pitch(&in,&bank,0,3000,10);instrument_swap(0,1);copy_mirror(fixture,&s);bank_apply(&bank,&s,11);input_sync(&in,&bank);require(!in.desires[0][CF_VOLUME].pending,"unsent gesture discarded on membership generation change");
 pad_command(CF_VOLUME,3);
 /* Dead historical overlapping addresses must not mask a new live field. */
 uint32_t old=ptr(instruments[128]),fresh=old+0x1e0;instrument_seed(old,0);Context c=context();c.r[0]=old;call(&c,PD_DEATH);instrument_seed(fresh,0);c=context();c.r[5]=fresh+0x394;c.d[8]=0x3e800000;call(&c,M_FLOAT);PadOwner *p=pad_owner(fresh);require(atomic_load(&p->fields[PF_PAN].bits)==0x3e800000,"dead volume-key overlap cannot mask new live pan commit");
 unsigned used=atomic_load(&fixture->pads.used);atomic_store(&fixture->pads.used,PAD_OWNERS);c=context();c.r[0]=ptr(instruments[130]);c.r[1]=ptr(program_objects[0]);call(&c,PD_BIRTH);require(atomic_load(&fixture->pads.error)==PD_CAPACITY,"retained incarnation exhaustion explicit");copy_mirror(fixture,&s);require(!s.pads[0].available&&s.tracks[0].available,"pad capacity failure cannot expose stale pad state or destroy existing Track source");atomic_store(&fixture->pads.used,used);
 puts("PASS compact pad capture/copy/128-slot view, parent generations, ordinary history and recording GUI volume/pan/mute/solo receipts, execution-time slot replacement, source motor values and bounded failure; native bodies, scheduling, objects and physical MCU substituted");free(fixture);free(command_state);return 0;
}
