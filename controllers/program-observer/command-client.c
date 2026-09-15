#define _GNU_SOURCE
#define COMMAND_CLIENT
#include "mirror-motor-core.h"
#include "mirror-input-core.h"
#include "command-state.h"
#include "command-process.h"
#include <sys/file.h>
#include <limits.h>
#include <signal.h>
static int stopped;
static uint32_t command_now(uint32_t sec,uint32_t nsec){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return UINT32_MAX;int64_t n=((int64_t)t.tv_sec-sec)*1000+((int64_t)t.tv_nsec-nsec)/1000000;return n<0||n>=UINT32_MAX?UINT32_MAX:(uint32_t)n;}
static void *map_file(const char *path,size_t size,int writable,int *fd,struct stat *st){
 *fd=open(path,(writable?O_RDWR:O_RDONLY)|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(*fd<0)return MAP_FAILED;
 if(fstat(*fd,st)||!S_ISREG(st->st_mode)||st->st_size!=(off_t)size||st->st_uid!=geteuid()){close(*fd);*fd=-1;return MAP_FAILED;}
 void *p=mmap(NULL,size,PROT_READ|(writable?PROT_WRITE:0),MAP_SHARED,*fd,0);if(p==MAP_FAILED){close(*fd);*fd=-1;}return p;
}
static int file_current(const char *path,int fd,const struct stat *before){struct stat a,b;return !fstat(fd,&a)&&!lstat(path,&b)&&S_ISREG(b.st_mode)&&command_same_file(before,&a)&&command_same_file(before,&b);}
static int header(const CommandState *s){return s->magic==COMMAND_MAGIC&&s->version==COMMAND_VERSION&&s->bytes==sizeof(*s)&&s->capacity==COMMAND_SLOTS&&command_duration_valid(s->seconds)&&s->pid&&s->owner_token&&s->origin_nsec<1000000000;}
static void diagnostic_status(const CommandState *s){
 uint32_t packed=atomic_load_explicit(&s->admission_detail,memory_order_acquire),guard=(packed>>8)&255;
 printf(",\"navigation\":{\"watch\":%u,\"error\":%u,\"revision\":%u,\"owner\":%u,\"epoch\":%u,\"count\":%u,\"tick\":%u,\"quality\":\"causal_witness_fields_require_revision_read\"}",atomic_load(&s->navigation_watch),atomic_load(&s->navigation_error),atomic_load(&s->navigation_revision),atomic_load(&s->navigation_owner),atomic_load(&s->navigation_epoch),atomic_load(&s->navigation_count),atomic_load(&s->navigation_tick));
 static const char *names[]={"none","enrollment_increment","enrollment_decrement","lane_registration","constructor_owner","source_reentry"};
 printf(",\"admission\":{\"packed\":%u,\"category\":\"%s\",\"raw_result\":%u,\"site\":%u,\"thread_token\":null,\"lane\":null,\"quality\":\"coherent_failure_sample_no_thread_pair\"}",packed,guard<sizeof(names)/sizeof(*names)?names[guard]:"unknown",packed&255,packed>>16);
}
static void status(const CommandState *s){
 printf("{\"format\":\"CMD30\",\"pid\":%u,\"raw_alive\":%u,\"closed\":%u,\"error\":%u,\"trace_error\":%u,\"published\":%u,\"consumed\":%u,\"reclaimed\":%u,\"pending_count\":%u,\"capacity\":%u,\"call_return_is_ack\":false,\"history\":\"retained current generation per slot; totals are counts\",\"requests\":[",s->pid,atomic_load(&s->alive),atomic_load(&s->closed),atomic_load(&s->error),atomic_load(&s->trace_error),atomic_load(&s->published),atomic_load(&s->consumed),atomic_load(&s->reclaimed),command_pending(s),COMMAND_SLOTS);
 unsigned first=1;
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  const CommandSlot *slot=s->slots+at;unsigned seq=atomic_load_explicit(&slot->published,memory_order_acquire);CommandRequest r;
  if(!seq||!command_request_read(slot,seq,&r))continue;
  printf("%s{\"slot\":%u,\"seq\":%u,\"epoch\":%u,\"serial\":%u,\"binding\":%u,\"incarnation\":%u,\"global_owner\":%u,\"bits\":%u,\"field\":%u,\"processed\":%u,\"rejected\":%u,\"dispatched\":%u,\"call_returned\":%u,\"matching_body_completed\":%u,\"owner_call_completed\":%u,\"sealed\":%u,\"client_settled\":%u,\"reclaimed\":%u",first?"":",",at,seq,r.epoch,r.serial,r.binding,r.incarnation,r.global_owner,r.bits,r.reserved,atomic_load(&slot->processed)==seq,atomic_load(&slot->processed)==seq?atomic_load(&slot->rejected):0,atomic_load(&slot->dispatched)==seq,atomic_load(&slot->returned)==seq,(r.reserved!=CF_ARM&&r.reserved!=CF_SELECTION&&!command_global(r.reserved))&&atomic_load(&slot->done)==seq,(r.reserved==CF_ARM||r.reserved==CF_SELECTION||command_global(r.reserved))&&atomic_load(&slot->done)==seq,atomic_load(&slot->sealed)==seq,atomic_load(&slot->settled)==seq,atomic_load(&slot->reclaimed)==seq);printf(",\"pad_owner\":%u,\"pad_index\":%u,\"pad_generation\":%u",r.pad_owner,r.pad_index,r.pad_generation);if(command_toggle(r.reserved))printf(",\"native_intent_applied\":%s,\"downstream_jobs_complete\":false",atomic_load(&slot->done)==seq?"true":"false");if(command_qlink(r.reserved))printf(",\"qlink_index\":%u,\"qlink_generation\":%u,\"qlink_wrapper\":%u,\"qlink_provider\":%u,\"logical_slot_semantics\":true,\"downstream_target_pinned\":false",r.qlink_index,r.qlink_generation,r.qlink_wrapper,r.qlink_provider);putchar('}');first=0;
 }
 fputs("],\"events\":[",stdout);first=1;
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  const CommandSlot *slot=s->slots+at;unsigned seq=atomic_load_explicit(&slot->published,memory_order_acquire);
  for(unsigned lane=0;lane<COMMAND_LANES;lane++){
   const CommandLane *l=slot->lanes+lane;unsigned n=atomic_load_explicit(&l->published,memory_order_acquire);if(n>COMMAND_EVENTS)continue;
   for(unsigned j=0;j<n;j++){CommandEvent e;if(!command_event_read(l,seq,j,&e))break;
    printf("%s{\"slot\":%u,\"lane\":%u,\"index\":%u,\"kind\":%u,\"request\":%u,\"tick\":%u,\"token\":%u,\"sp\":%u,\"lr\":%u,\"capture\":%u,\"counter\":%u,\"program\":%u,\"arg_kind\":%u,\"controller\":%u,\"bits\":%u,\"incarnation\":%u,\"property\":%u,\"revision\":%u,\"field\":%u}",first?"":",",at,lane,j,e.kind,e.request,e.tick,e.token,e.sp,e.lr,e.capture,e.counter,e.program,e.arg_kind,e.controller,e.bits,e.incarnation,e.property,e.revision,e.reserved);first=0;
   }
  }
 }
 fputs("],\"snapshot_atomic\":false",stdout);printf(",\"new_project_intent\":%u",atomic_load(&s->new_project_intent));diagnostic_status(s);puts("}");
}

static unsigned global_name(const char *name){
 static const char *names[]={"save","zoom-in","zoom-out","zoom-up","zoom-down","enter","cancel","left","up","right","down","record-toggle","click-toggle","loop-toggle","main","arrange","clip","track-mix","pad-mix","track-edit","sample-edit","step-sequencer"};
 if(!strcmp(name,"automation"))return CF_AUTOMATION;
 if(!strcmp(name,"track-type"))return GLOBAL_TRACK_TYPE;
 if(!strcmp(name,"track-new"))return GLOBAL_TRACK_NEW;
 if(!strcmp(name,"play"))return GLOBAL_PLAY;
 if(!strcmp(name,"stop"))return GLOBAL_STOP;
 if(!strcmp(name,"undo"))return GLOBAL_UNDO;
 if(!strcmp(name,"redo"))return GLOBAL_REDO;
 for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++)if(!strcmp(name,names[i]))return GLOBAL_SAVE+i;
 return UINT_MAX;
}
static int prepare(const char *path,const char *output,const char *value_text,const char *index_text,const char *field_text){
 CommandRequest r={0};unsigned field=CF_VOLUME;static const char *fields[]={"volume","pan","mute","solo","arm","name","color","select","effective_mute","solo_audio","send1","send2","send3","send4"};if(field_text&&global_name(field_text)!=UINT_MAX)field=global_name(field_text);else if(field_text&&!strcmp(field_text,"master"))field=CF_MASTER;else if(field_text&&!strcmp(field_text,"bars"))field=JOG_BAR;else if(field_text&&!strcmp(field_text,"beats"))field=JOG_BEAT;else if(field_text&&!strcmp(field_text,"pulses"))field=JOG_PULSE;else if(field_text){for(field=0;field<sizeof(fields)/sizeof(fields[0])&&strcmp(field_text,fields[field]);field++);if(!command_supported(field))return 2;}
 char *end;errno=0;int32_t delta=0;float value=0;if(command_jog(field)){long n=strtol(value_text,&end,10);if(errno||*end||n < -JOG_DELTA_LIMIT||n>JOG_DELTA_LIMIT||!n||index_text)return 2;delta=(int32_t)n;}else{value=strtof(value_text,&end);if(errno||*end||!(value>=0&&value<=(field==CF_AUTOMATION?2:1)))return 2;}
 unsigned index=UINT_MAX;if(index_text){errno=0;unsigned long n=strtoul(index_text,&end,10);if(errno||*end||n<1||n>MIRROR_TRACKS)return 2;index=(unsigned)n-1;}
 int fd=-1;struct stat st;const MirrorState *s=map_file(path,sizeof(*s),0,&fd,&st);if(s==MAP_FAILED)return 1;int result=1;
 if(s->magic!=MIRROR_MAGIC||s->version!=MIRROR_VERSION||s->bytes!=sizeof(*s)||s->capacity!=MIRROR_CELLS||!s->pid)goto done;
 uint64_t start=command_process_start(s->pid);long hz=sysconf(_SC_CLK_TCK);if(!start||hz<=0||(uint64_t)s->origin_sec*(unsigned long)hz+(uint64_t)s->origin_nsec*(unsigned long)hz/1000000000<start||!command_process_exact(s->pid))goto done;
 CopiedMirror copy;uint32_t now;if(!copy_mirror(s,&copy)||!copy.ready||!copy.alive||copy.error)goto done;
 now=command_now(s->origin_sec,s->origin_nsec);if(now==UINT32_MAX||now<copy.heartbeat||now-copy.heartbeat>=1000||!file_current(path,fd,&st)||command_process_start(s->pid)!=start)goto done;
 if(command_jog(field)){
  if(now>UINT32_MAX-120000)goto done;
  r=(CommandRequest){.pid=s->pid,.start_lo=(uint32_t)start,.start_hi=(uint32_t)(start>>32),.origin_sec=s->origin_sec,.origin_nsec=s->origin_nsec,.epoch=copy.epoch,.bits=(uint32_t)delta,.created=now,.expires=now+120000,.reserved=field,.project_owner=copy.project_owner};goto write_request;
 }
 if(command_global(field)){
  if(index_text||now>UINT32_MAX-120000||value!=(unsigned)value||(field!=CF_AUTOMATION&&value!=0))goto done;
  unsigned owner=input_global_owner(&copy,field);
  if(!owner||(field==GLOBAL_TRACK_TYPE&&!copy.selected_serial)||(field==CF_AUTOMATION&&!copy.automation.available))goto done;
  r=(CommandRequest){.pid=s->pid,.start_lo=(uint32_t)start,.start_hi=(uint32_t)(start>>32),.origin_sec=s->origin_sec,.origin_nsec=s->origin_nsec,.epoch=copy.epoch,.bits=(unsigned)value,.created=now,.expires=now+120000,.reserved=field,.field_incarnation=input_global_field(&copy,field),.project_owner=copy.project_owner,.global_owner=owner};goto write_request;
 }
 if(field==CF_MASTER){
  if(index_text||now>UINT32_MAX-120000||!copy.master.available)goto done;
  uint32_t bits;memcpy(&bits,&value,4);r=(CommandRequest){.pid=s->pid,.start_lo=(uint32_t)start,.start_hi=(uint32_t)(start>>32),.origin_sec=s->origin_sec,.origin_nsec=s->origin_nsec,.epoch=copy.epoch,.bits=bits,.created=now,.expires=now+120000,.before_bits=copy.master.bits,.before_revision=copy.master.revision,.reserved=CF_MASTER,.field_incarnation=copy.master.incarnation,.project_owner=copy.project_owner,.global_owner=copy.master.owner_incarnation};goto write_request;
 }
 if(index==UINT_MAX){for(unsigned i=0;i<copy.count;i++)if(copy.tracks[i].vptr==0x6930c00){index=i;break;}}
 if(index>=copy.count||now>UINT32_MAX-120000)goto done;
 CopiedTrack *t=copy.tracks+index;if(field==CF_VOLUME&&midi_volume(t->vptr))field=CF_MIDI_VOLUME;CopiedField volume;const CopiedField *f=copied_field(&copy,t,command_source_field(field),&volume);if(!f||!f->available)goto done;uint32_t bits;if(command_float(field))memcpy(&bits,&value,4);else{if(value!=0&&value!=1)goto done;bits=(uint32_t)value;}if(field==CF_SELECTION?f->bits==t->track:bits==f->bits)goto done;
 r=(CommandRequest){0,s->pid,(uint32_t)start,(uint32_t)(start>>32),s->origin_sec,s->origin_nsec,copy.epoch,t->serial,t->binding,t->incarnation,bits,now,now+120000,f->bits,f->revision,field,t->track_owner,t->program_owner,field?f->incarnation:0,copy.project_owner,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
write_request:;
 int out=open(output,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(out<0)goto done;int ok=write(out,&r,sizeof(r))==(ssize_t)sizeof(r);close(out);if(!ok)goto done;
 if(command_global(field))printf("{\"prepared\":true,\"operation\":%u,\"owner\":%u,\"epoch\":%u,\"bits\":%u}\n",r.reserved,r.global_owner,r.epoch,r.bits);
 else if(command_jog(field))printf("{\"prepared\":true,\"pid\":%u,\"epoch\":%u,\"operation\":%u,\"signed_steps\":%d,\"expires\":%u}\n",r.pid,r.epoch,r.reserved,(int32_t)r.bits,r.expires);
 else if(field==CF_MASTER)printf("{\"prepared\":true,\"pid\":%u,\"epoch\":%u,\"global_mixer_owner\":%u,\"field_incarnation\":%u,\"bits\":%u,\"expires\":%u}\n",r.pid,r.epoch,r.global_owner,r.field_incarnation,r.bits,r.expires);
 else printf("{\"prepared\":true,\"pid\":%u,\"master_index\":%u,\"epoch\":%u,\"serial\":%u,\"binding\":%u,\"incarnation\":%u,\"bits\":%u,\"expires\":%u}\n",r.pid,index+1,r.epoch,r.serial,r.binding,r.incarnation,r.bits,r.expires);
 result=0;
done:munmap((void*)s,sizeof(*s));close(fd);return result;
}
static int submit(const char *path,const char *request_path){
 CommandRequest r;int input=open(request_path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);struct stat rs;if(input<0)return 1;int ok=!fstat(input,&rs)&&S_ISREG(rs.st_mode)&&rs.st_size==sizeof(r)&&read(input,&r,sizeof(r))==sizeof(r);close(input);if(!ok||r.seq||!command_operation(r.reserved))return 1;
 int fd=-1;struct stat st;CommandState *s=map_file(path,sizeof(*s),1,&fd,&st);if(s==MAP_FAILED)return 1;int result=1;
 if(flock(fd,LOCK_EX|LOCK_NB)||!header(s)||!atomic_load_explicit(&s->alive,memory_order_acquire)||atomic_load(&s->closed)||atomic_load(&s->error)||atomic_load(&s->trace_error))goto done;
 uint64_t start=((uint64_t)s->start_hi<<32)|s->start_lo;if(!start||command_process_start(s->pid)!=start||!command_process_exact(s->pid)||!file_current(path,fd,&st))goto done;
 if(r.pid!=s->pid||r.start_lo!=s->start_lo||r.start_hi!=s->start_hi||r.origin_sec!=s->origin_sec||r.origin_nsec!=s->origin_nsec)goto done;
 unsigned n=atomic_load_explicit(&s->published,memory_order_acquire);if(n>=COMMAND_SEQUENCE_LAST||!command_idle(s))goto done;
 unsigned pending=command_pending(s);if(pending)goto done;
 if(!atomic_load_explicit(&s->alive,memory_order_acquire)||command_process_start(s->pid)!=start||!file_current(path,fd,&st))goto done;
 unsigned at=command_free(s);r.seq=n+1;if(!command_publish_request_at(s,&r,at))goto done;
 uint32_t began_wait=command_now(s->origin_sec,s->origin_nsec);
 while(!stopped&&atomic_load_explicit(&s->alive,memory_order_acquire)&&atomic_load_explicit(&s->slots[at].processed,memory_order_acquire)!=r.seq){
  uint32_t now=command_now(s->origin_sec,s->origin_nsec);if(now==UINT32_MAX||now-began_wait>=10000)break;struct timespec delay={0,10000000};nanosleep(&delay,NULL);
 }
 status(s);result=atomic_load_explicit(&s->slots[at].processed,memory_order_acquire)==r.seq?0:3;
 if(result==3)fputs("No processing observation before local wait ended; timeout is not a command reply.\n",stderr);
done:munmap(s,sizeof(*s));close(fd);return result;
}
/* Same observation contract as the bridge: transient copied-snapshot
 * contention retries, while actual stopped/error/stale/future source fails. */
static int settlement_snapshot(const MirrorState *m,CopiedMirror *s,uint32_t *now,const char **reason){
 int copied=copy_mirror(m,s);uint32_t heartbeat=atomic_load_explicit(&m->heartbeat,memory_order_acquire);
 *now=command_now(m->origin_sec,m->origin_nsec); /* after publication reads */
 if(*now==UINT32_MAX){*reason="same-origin clock invalid";return -1;}
 if(!atomic_load_explicit(&m->alive,memory_order_acquire)||atomic_load_explicit(&m->error,memory_order_acquire)||*now<heartbeat||*now-heartbeat>=1000){*reason="source stopped, failed or stale";return -1;}
 if(!copied)return 0;
 if(!s->alive||s->error||*now<s->heartbeat||*now-s->heartbeat>=1000){*reason="copied source stopped, failed or stale";return -1;}
 return 1;
}
static int settle(const char *command_path,const char *mirror_path){
 int fd=-1,mfd=-1,result=1;struct stat st,mst;CommandState *c=map_file(command_path,sizeof(*c),1,&fd,&st);if(c==MAP_FAILED)return 1;
 const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&mfd,&mst);if(m==MAP_FAILED)goto done;
 if(flock(fd,LOCK_EX|LOCK_NB)||!header(c)||m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m)||m->pid!=c->pid||m->origin_sec!=c->origin_sec||m->origin_nsec!=c->origin_nsec)goto unmap;
 uint64_t start=((uint64_t)c->start_hi<<32)|c->start_lo;if(!start||command_process_start(c->pid)!=start||!command_process_exact(c->pid))goto unmap;
 if(command_idle(c)){status(c);result=0;goto unmap;}
 MirrorInput in={.commands=c};MirrorBank bank;bank_init(&bank);CopiedMirror s;
 uint32_t began=command_now(c->origin_sec,c->origin_nsec);int bound=0;const char *reason="interrupted";
 while(!stopped){
  if(!file_current(command_path,fd,&st)||!file_current(mirror_path,mfd,&mst)||command_process_start(c->pid)!=start||!command_process_file(c->pid,0)){reason="source file or process identity changed";break;}
  uint32_t now;int copied=settlement_snapshot(m,&s,&now,&reason);
  if(copied<0)break;
  if(now<began){reason="same-origin clock invalid";break;}
  if(now-began>10000){reason=copied?"bounded settlement wait ended":"bounded snapshot contention wait ended";break;}
  if(!copied){struct timespec delay={0,10000000};nanosleep(&delay,NULL);continue;}
  if(!bound){
   for(unsigned at=0;at<COMMAND_SLOTS;at++){
    const CommandSlot *q=c->slots+at;unsigned seq=atomic_load_explicit(&q->published,memory_order_acquire);CommandRequest r;
    if(!seq||atomic_load_explicit(&q->reclaimed,memory_order_acquire)==seq)continue;
    if(!command_request_read(q,seq,&r)||!command_operation(r.reserved))goto unmap;
    if(r.reserved==CF_MASTER){
     if(!s.master.available||s.master.owner_incarnation!=r.global_owner||s.master.incarnation!=r.field_incarnation)goto unmap;
     InputFlight *tx=in.flights+at;memset(tx,0,sizeof(*tx));tx->flight=seq;tx->field=CF_MASTER;tx->bits=r.bits;tx->expires=r.expires;tx->global_owner=r.global_owner;tx->global_epoch=r.epoch;tx->field_incarnation=r.field_incarnation;tx->property=s.master.property;tx->before_revision=r.before_revision;tx->acknowledged=atomic_load(&q->settled)==seq;continue;
    }
    if(command_global(r.reserved)){
     InputFlight *tx=in.flights+at;memset(tx,0,sizeof(*tx));tx->flight=seq;tx->field=r.reserved;tx->bits=r.bits;tx->expires=r.expires;tx->global_epoch=r.epoch;tx->global_owner=r.global_owner;tx->field_incarnation=r.field_incarnation;tx->property=r.reserved==CF_AUTOMATION?s.automation.property:0;tx->acknowledged=atomic_load(&q->settled)==seq;continue;
    }
    if(command_jog(r.reserved)){
     InputFlight *tx=in.flights+at;memset(tx,0,sizeof(*tx));tx->flight=seq;tx->field=r.reserved;tx->bits=r.bits;tx->expires=r.expires;tx->target.epoch=r.epoch;tx->acknowledged=atomic_load_explicit(&q->settled,memory_order_acquire)==seq;continue;
    }
    if(command_io(r.reserved)){InputFlight *tx=in.flights+at;memset(tx,0,sizeof(*tx));tx->flight=seq;tx->field=r.reserved;tx->bits=r.bits;tx->expires=r.expires;tx->io_request=r;tx->acknowledged=atomic_load(&q->settled)==seq;continue;}
    if(command_qlink(r.reserved)||command_qlink_mode(r.reserved)){InputFlight *tx=in.flights+at;memset(tx,0,sizeof(*tx));tx->flight=seq;tx->field=r.reserved;tx->bits=r.bits;tx->expires=r.expires;tx->qlink_request=r;tx->acknowledged=atomic_load(&q->settled)==seq;continue;}
    const CopiedTrack *t=NULL;for(unsigned i=0;i<s.count;i++)if(s.tracks[i].serial==r.serial)t=s.tracks+i;
    if(!t||s.epoch!=r.epoch||s.project_owner!=r.project_owner||t->binding!=r.binding||t->incarnation!=r.incarnation||t->track_owner!=r.track_owner||t->program_owner!=r.program_owner)goto unmap;
    if(command_effect(r.reserved)||command_chooser(r.reserved)){InputFlight *tx=in.flights+at;*tx=(InputFlight){.flight=seq,.bits=r.bits,.expires=r.expires,.field=r.reserved,.field_incarnation=r.field_incarnation,.effect_request=r,.acknowledged=atomic_load(&q->settled)==seq};tx->target=(MotorIdentity){r.epoch,r.serial,r.binding,r.incarnation,t->track,t->program,r.track_owner,r.program_owner,0,0,0};continue;}
    CopiedField v;const CopiedField *f=copied_field(&s,t,command_source_field(r.reserved),&v);if(!f||!f->available||(r.reserved&&f->incarnation!=r.field_incarnation))goto unmap;
    InputFlight *tx=in.flights+at;*tx=(InputFlight){.flight=seq,.bits=r.bits,.expires=r.expires,.before_revision=r.before_revision,.field=r.reserved,.field_incarnation=r.field_incarnation,.property=f->property,.acknowledged=atomic_load_explicit(&q->settled,memory_order_acquire)==seq};
    tx->target=(MotorIdentity){r.epoch,r.serial,r.binding,r.incarnation,t->track,t->program,r.track_owner,r.program_owner,r.pad_owner,r.pad_index,r.pad_generation};
   }
   bound=1;
  }
  bank_apply(&bank,&s,now);input_pump(&in,&bank,&s,now);
  if(in.error){reason="source settlement proof rejected";break;}
  if(input_drained(&in)){result=0;break;}
  struct timespec delay={0,10000000};nanosleep(&delay,NULL);
 }
 status(c);if(result)fprintf(stderr,"No certified source settlement; timeout/return never acknowledges. error=%u reason=%s\n",in.error,reason);
unmap:munmap((void*)m,sizeof(*m));close(mfd);
done:munmap(c,sizeof(*c));close(fd);return result;
}
/* File allocation precedes header publication. This probe only recognizes
 * zero/unpublished headers; a known mismatch, terminal flag or live peer fails. */
static int initializing_file(const char *path,size_t size,uint32_t magic,unsigned version,int command){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);struct stat st;uint32_t words[32]={0};int ok=0;
 if(fd<0)return 0;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=geteuid()||(st.st_size&&st.st_size!=(off_t)size))goto done;
 if(st.st_size&&pread(fd,words,sizeof(words),0)!=sizeof(words))goto done;
 if((words[0]&&words[0]!=magic)||(words[1]&&words[1]!=version)||(words[2]&&words[2]!=size))goto done;
 if(command){if(words[offsetof(CommandState,alive)/4]||words[offsetof(CommandState,error)/4]||words[offsetof(CommandState,trace_error)/4]||words[offsetof(CommandState,closed)/4]||words[offsetof(CommandState,startup)/4]>=2)goto done;}
 else if(words[offsetof(MirrorState,alive)/4]||words[offsetof(MirrorState,error)/4])goto done;
 ok=1;
done:close(fd);return ok;
}
/* Session control shares the command writer lock and admission. Stop cannot
 * overtake a client still certifying an already published transaction. */
static int session(const char *command_path,const char *mirror_path,int stop){
 int fd=-1,mfd=-1,result=1;struct stat st,mst;
 if(!stop){struct stat a,b;
  if(lstat(command_path,&a)||lstat(mirror_path,&b)||!S_ISREG(a.st_mode)||!S_ISREG(b.st_mode)||a.st_uid!=geteuid()||b.st_uid!=geteuid())return 1;
  if((!a.st_size||!b.st_size)&&initializing_file(command_path,sizeof(CommandState),COMMAND_MAGIC,COMMAND_VERSION,1)&&initializing_file(mirror_path,sizeof(MirrorState),MIRROR_MAGIC,MIRROR_VERSION,0)){puts("{\"state\":\"initializing\",\"healthy\":false}");return 5;}
 }
 CommandState *c=map_file(command_path,sizeof(*c),stop,&fd,&st);if(c==MAP_FAILED)return 1;
 const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&mfd,&mst);if(m==MAP_FAILED)goto done;
 unsigned startup=atomic_load_explicit(&c->startup,memory_order_acquire);
 if(!stop&&startup<2&&!atomic_load(&c->error)&&!atomic_load(&c->trace_error)&&!atomic_load(&c->closed)&&!atomic_load(&m->error)&&!atomic_load(&c->published)){
  if((c->magic&&c->magic!=COMMAND_MAGIC)||(c->version&&c->version!=COMMAND_VERSION)||(c->bytes&&c->bytes!=sizeof(*c))||(m->magic&&m->magic!=MIRROR_MAGIC)||(m->version&&m->version!=MIRROR_VERSION)||(m->bytes&&m->bytes!=sizeof(*m)))goto unmap;
  if((!startup&&!atomic_load(&c->alive))||(startup==1&&header(c)&&m->pid==c->pid)){puts("{\"state\":\"initializing\",\"healthy\":false}");result=5;goto unmap;}
 }
 if(!header(c)||!command_duration_valid(c->seconds)||m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m)||m->pid!=c->pid||m->origin_sec!=c->origin_sec||m->origin_nsec!=c->origin_nsec)goto unmap;
 uint64_t start=((uint64_t)c->start_hi<<32)|c->start_lo;
 int process=start&&command_process_start(c->pid)==start&&command_process_file(c->pid,stop)&&command_process_start(c->pid)==start;
 CopiedMirror copy;int stable=copy_mirror(m,&copy);uint32_t now=command_now(c->origin_sec,c->origin_nsec);
 int current=file_current(command_path,fd,&st)&&file_current(mirror_path,mfd,&mst)&&command_process_start(c->pid)==start;
 int fresh=stable&&now!=UINT32_MAX&&now>=copy.heartbeat&&now-copy.heartbeat<1000;
 int healthy=startup==2&&process&&current&&fresh&&copy.alive&&!copy.error&&atomic_load(&c->alive)&&!atomic_load(&c->error)&&!atomic_load(&c->trace_error)&&!atomic_load(&c->closed);
 if(stop){
  if(!process||!current||flock(fd,LOCK_EX|LOCK_NB))goto unmap;
  if(!atomic_load(&c->closed)){
   if(!healthy||(!atomic_load(&c->stop_requested)&&!command_request_stop(c)))goto unmap;
   uint32_t began=now;
   while(!atomic_load_explicit(&c->closed,memory_order_acquire)){
    now=command_now(c->origin_sec,c->origin_nsec);
    if(now==UINT32_MAX||now<began||now-began>=10000||!file_current(command_path,fd,&st)||command_process_start(c->pid)!=start)goto unmap;
    struct timespec delay={0,10000000};nanosleep(&delay,NULL);
   }
  }
  result=!atomic_load(&c->error)&&!atomic_load(&c->trace_error)&&!atomic_load(&m->error)?0:1;
 }else result=healthy?(atomic_load(&c->new_project_intent)?6:copy.ready?0:3):atomic_load(&c->closed)?4:1;
 printf("{\"format\":\"CMD30\",\"pid\":%u,\"process_current\":%s,\"fresh\":%s,\"healthy\":%s,\"state\":\"%s\",\"manual\":%s,\"closed\":%u,\"error\":%u,\"trace_error\":%u,\"mirror_error\":%u,\"published\":%u,\"reclaimed\":%u,\"pending\":%s,\"elapsed_ms\":%u,\"clock_limit_ms\":%u,\"volume_cells\":%u,\"volume_capacity\":%u,\"owners_used\":%u,\"owners_capacity\":%u,\"fields_used\":%u,\"fields_capacity\":%u",c->pid,process?"true":"false",fresh?"true":"false",healthy&&!stop?"true":"false",atomic_load(&c->closed)?"closed":healthy?(copy.ready?"ready":"awaiting_project"):"unavailable",c->seconds?"false":"true",atomic_load(&c->closed),atomic_load(&c->error),atomic_load(&c->trace_error),atomic_load(&m->error),atomic_load(&c->published),atomic_load(&c->reclaimed),!command_idle(c)?"true":"false",now,command_tick_limit(c->seconds),atomic_load(&m->allocated),MIRROR_CELLS,atomic_load(&m->channel.owners_used),CHANNEL_OWNERS,atomic_load(&m->channel.fields_used),CHANNEL_FIELDS);printf(",\"meter_error\":%u,\"meter_tokens\":%u,\"meter_cells\":%u,\"meter_capacity\":%u",atomic_load(&m->meters.error),atomic_load(&m->meters.tokens),atomic_load(&m->meters.used),METER_CELLS);printf(",\"pad_error\":%u,\"pad_owners\":%u,\"pad_capacity\":%u,\"pad_parents\":%u",atomic_load(&m->pads.error),atomic_load(&m->pads.used),PAD_OWNERS,atomic_load(&m->pads.parents_used));printf(",\"new_project_intent\":%u",atomic_load(&c->new_project_intent));diagnostic_status(c);printf(",\"effects\":");effects_json(&copy);printf(",\"io\":");io_json(&copy);puts("}");
unmap:munmap((void*)m,sizeof(*m));close(mfd);
done:munmap(c,sizeof(*c));close(fd);return result;
}
/* A native intent is consumable only for the exact dead producer recorded by
 * the session waiter. This changes no request, receipt or closure frontier. */
static int new_project(const char *path,const char *pid_text,const char *tick_text,int consume){
 char *end;errno=0;unsigned long pid=strtoul(pid_text,&end,10);if(errno||*end||pid<2||pid>UINT32_MAX)return 2;
 errno=0;unsigned long long tick=strtoull(tick_text,&end,10);if(errno||*end||!tick)return 2;
 int fd=-1,result=1;struct stat st;CommandState *c=map_file(path,sizeof(*c),consume,&fd,&st);if(c==MAP_FAILED)return 1;
 if(!header(c)||c->pid!=pid||(((uint64_t)c->start_hi<<32)|c->start_lo)!=tick||atomic_load(&c->stop_requested)||!file_current(path,fd,&st))goto done;
 if(consume){
  if(command_process_start(c->pid)==tick||flock(fd,LOCK_EX|LOCK_NB)||!file_current(path,fd,&st))goto done;
  unsigned expected=1;if(!atomic_compare_exchange_strong(&c->new_project_intent,&expected,2))goto done;
 }else if(atomic_load_explicit(&c->new_project_intent,memory_order_acquire)!=1)goto done;
 printf("new_project_%s pid=%u start_tick=%llu epoch=%u tick=%u\n",consume?"consumed":"accepted",c->pid,tick,atomic_load(&c->new_project_epoch),atomic_load(&c->new_project_tick));result=0;
done:munmap(c,sizeof(*c));close(fd);return result;
}
static int processor_watch(const char *path,const char *key_arg,const char *seconds_arg){
 char *end;unsigned long seconds=strtoul(seconds_arg,&end,10);if(!*seconds_arg||*end||seconds<1||seconds>300)return 2;
 unsigned long key=0;if(strcmp(key_arg,"discover")){key=strtoul(key_arg,&end,0);if(!*key_arg||*end||key<4096||key>UINT32_MAX||(key&3))return 2;}
 int fd=-1;struct stat st;CommandState *s=map_file(path,sizeof(*s),1,&fd,&st);if(s==MAP_FAILED)return 1;int result=1;
 uint32_t now=command_now(s->origin_sec,s->origin_nsec);uint64_t start=((uint64_t)s->start_hi<<32)|s->start_lo;
 if(!header(s)||!start||command_process_start(s->pid)!=start||!command_process_exact(s->pid)||!file_current(path,fd,&st)||now>UINT32_MAX-(uint32_t)seconds*1000u||!command_writer_enter(s))goto done;
 if(!processor_begin(s,now,(uint32_t)key,(uint32_t)seconds*1000)){command_writer_leave(s);goto done;}
 uint32_t generation=atomic_load(&s->processor_generation);command_writer_leave(s);result=0;
 printf("Passive processor window CMD30 generation=%u key=%u seconds=%lu; rolling register-only, enrolled threads, lifetime unqualified\n",generation,(uint32_t)key,seconds);
done:munmap(s,sizeof(*s));close(fd);return result;
}
static int processor_end(const char *path){
 int fd=-1,result=1;struct stat st;CommandState *s=map_file(path,sizeof(*s),1,&fd,&st);if(s==MAP_FAILED)return 1;
 uint64_t start=((uint64_t)s->start_hi<<32)|s->start_lo;
 if(header(s)&&start&&command_process_start(s->pid)==start&&command_process_exact(s->pid)&&file_current(path,fd,&st)&&command_writer_enter(s)){
  processor_end_window(s);command_writer_leave(s);result=0;
  puts("Processor watch admission ended; admitted getters may finish, retained records and musical state unchanged");
 }
 munmap(s,sizeof(*s));close(fd);return result;
}
static void processor_print(FILE *out,uint32_t sequence,const uint32_t w[PROCESSOR_WORDS]){
 fprintf(out,"{\"type\":\"record\",\"sequence\":%u,\"generation\":%u,\"site\":%u,\"tick\":%u,\"token\":%u,\"sp\":%u,\"lr\":%u,\"key\":%u,\"r0\":%u,\"r1\":%u,\"r2\":%u,\"r3\":%u,\"s0_bits\":%u,\"paired_getter\":%u}",sequence,w[0],w[1],w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9],w[10],w[11],w[12]);
}
static void processor_metadata(FILE *out,const CommandState *s){
 uint32_t count=atomic_load(&s->processor_count),loss=0;uint64_t drops=0;for(unsigned i=0;i<COMMAND_LANES;i++){loss|=atomic_load(s->processor_loss+i);drops+=atomic_load(s->processor_dropped+i);}
 fprintf(out,"{\"type\":\"status\",\"format\":\"CMD30\",\"quality\":\"passive_registers_lifetime_unqualified_enrolled_threads_only\",\"metadata_quality\":\"independent_atomic_counters\",\"generation\":%u,\"until\":%u,\"loss_flags\":%u,\"writer_or_frame_drops\":%llu,\"open_getters\":%u,\"reserved\":%u,\"retained_from\":%u,\"retention_is_not_stream_loss\":true}",atomic_load(&s->processor_generation),atomic_load(&s->processor_until),loss,(unsigned long long)drops,processor_open_total(s),count,count>PROCESSOR_OBSERVATIONS?count-PROCESSOR_OBSERVATIONS+1:1);
}
static int processor_status(const char *path){
 int fd=-1;struct stat st;CommandState *s=map_file(path,sizeof(*s),0,&fd,&st);
 if(s==MAP_FAILED){fputs("processor diagnostics require a matched CMD30 file\n",stderr);return 1;}
 if(!header(s)){munmap(s,sizeof(*s));close(fd);fputs("processor diagnostics require CMD30\n",stderr);return 1;}
 fputs("{\"metadata\":",stdout);processor_metadata(stdout,s);fputs(",\"events\":[",stdout);
 unsigned count=atomic_load(&s->processor_count),first=1,begin=count>PROCESSOR_OBSERVATIONS?count-PROCESSOR_OBSERVATIONS:0;
 for(unsigned at=begin;at<count;at++){uint32_t w[PROCESSOR_WORDS];if(!processor_read(s,at+1,w))continue;if(!first)fputc(',',stdout);processor_print(stdout,at+1,w);first=0;}
 puts("]}");munmap(s,sizeof(*s));close(fd);return ferror(stdout)?1:0;
}
typedef struct {uint32_t floor,seen[PROCESSOR_OBSERVATIONS];} ProcessorCursor;
static void processor_gap(FILE *out,uint32_t first,uint32_t last,const char *reason){if(first<=last)fprintf(out,"{\"type\":\"gap\",\"first\":%u,\"last\":%u,\"reason\":\"%s\"}\n",first,last,reason);}
/* Sequences may complete out of order. Emit each observed record once; only
 * report a missing ticket when it leaves retention or at collector shutdown.
 * A gap says this collector did not observe it, not that native code omitted it. */
static void processor_collect(FILE *out,const CommandState *s,ProcessorCursor *cursor,int final){
 uint32_t count=atomic_load(&s->processor_count),low=count>PROCESSOR_OBSERVATIONS?count-PROCESSOR_OBSERVATIONS+1:1;
 if(!cursor->floor)cursor->floor=1;
 if(cursor->floor<low){
  uint32_t end=low-1,scan=end-cursor->floor>=PROCESSOR_OBSERVATIONS?cursor->floor+PROCESSOR_OBSERVATIONS-1:end,first=0;
  for(uint32_t seq=cursor->floor;seq<=scan;seq++){
   if(cursor->seen[(seq-1)%PROCESSOR_OBSERVATIONS]!=seq){if(!first)first=seq;}
   else if(first){processor_gap(out,first,seq-1,"not_observed_before_retention_advanced");first=0;}
  }
  if(first)processor_gap(out,first,end,"not_observed_before_retention_advanced");
  else if(scan<end)processor_gap(out,scan+1,end,"not_observed_before_retention_advanced");
  cursor->floor=low;
 }
 for(uint32_t seq=low;seq<=count;seq++){
  unsigned slot=(seq-1)%PROCESSOR_OBSERVATIONS;uint32_t words[PROCESSOR_WORDS];
  if(cursor->seen[slot]==seq)continue;
  if(processor_read(s,seq,words)){processor_print(out,seq,words);fputc('\n',out);if(ferror(out))return;cursor->seen[slot]=seq;}
 }
 if(final){uint32_t first=0;for(uint32_t seq=low;seq<=count;seq++){
  if(cursor->seen[(seq-1)%PROCESSOR_OBSERVATIONS]!=seq){if(!first)first=seq;}
  else if(first){processor_gap(out,first,seq-1,"not_observed_at_collector_end");first=0;}
 }if(first)processor_gap(out,first,count,"not_observed_at_collector_end");processor_metadata(out,s);fputc('\n',out);}
}
/* SSH backpressure is a failed collection, never an unbounded wait. Unbuffered
 * stdio has no pending payload to flush after restoring descriptor flags. */
static int processor_output_begin(FILE *out,int *flags){
 int fd=fileno(out);*flags=fcntl(fd,F_GETFL);if(*flags<0||setvbuf(out,NULL,_IONBF,0))return 0;
 return !fcntl(fd,F_SETFL,*flags|O_NONBLOCK);
}
static volatile sig_atomic_t processor_stream_stop;
static void processor_stream_signal(int sig){(void)sig;processor_stream_stop=1;}
static int processor_stream(const char *path,const char *seconds_arg){
 char *end;unsigned long seconds=strtoul(seconds_arg,&end,10);if(!*seconds_arg||*end||seconds<1||seconds>300)return 2;
 int fd=-1,result=1;struct stat st;CommandState *s=map_file(path,sizeof(*s),0,&fd,&st);if(s==MAP_FAILED){fputs("processor-stream requires a matched CMD30 file\n",stderr);return 1;}
 uint64_t start=((uint64_t)s->start_hi<<32)|s->start_lo;uint32_t began=command_now(s->origin_sec,s->origin_nsec);
 if(!header(s)||!start||began==UINT32_MAX||command_process_start(s->pid)!=start||!command_process_exact(s->pid)||!file_current(path,fd,&st))goto done;
 int output_flags;if(!processor_output_begin(stdout,&output_flags))goto done;
 ProcessorCursor cursor={0};processor_stream_stop=0;void (*old_int)(int)=signal(SIGINT,processor_stream_signal),(*old_term)(int)=signal(SIGTERM,processor_stream_signal),(*old_pipe)(int)=signal(SIGPIPE,SIG_IGN);
 result=0;
 while(!processor_stream_stop){
  uint32_t now=command_now(s->origin_sec,s->origin_nsec);
  if(now==UINT32_MAX||now<began||!file_current(path,fd,&st)||command_process_start(s->pid)!=start||!command_process_file(s->pid,0)){result=1;break;}
  processor_collect(stdout,s,&cursor,0);if(ferror(stdout)||fflush(stdout)){result=1;break;}
  if(now-began>=seconds*1000u||!atomic_load(&s->alive)||atomic_load(&s->closed))break;
  struct timespec pause={0,50000000};nanosleep(&pause,NULL);
 }
 if(!ferror(stdout)){processor_collect(stdout,s,&cursor,1);if(fflush(stdout))result=1;}
 if(fcntl(fileno(stdout),F_SETFL,output_flags))result=1;
 signal(SIGINT,old_int);signal(SIGTERM,old_term);signal(SIGPIPE,old_pipe);
 done:munmap(s,sizeof(*s));close(fd);return result;
}

/* Explicit operator interest uses the same exclusive writer and copied source.
 * No native call or alternative parameter owner is introduced by this CLI. */
static int effects_interest_cli(const char *command_path,const char *mirror_path,const char *slot_arg,const char *page_arg,const char *seconds_arg){
 char *end;unsigned long page=strtoul(page_arg,&end,10);if(!*page_arg||*end||page>=EFFECT_MAX_PRESENTATION/EFFECT_PAGE)return 2;
 unsigned long seconds=strtoul(seconds_arg,&end,10);if(!*seconds_arg||*end||seconds<1||seconds>300)return 2;
 unsigned enabled=strcmp(slot_arg,"off")!=0,slot=EFFECT_LIST;
 if(enabled&&strcmp(slot_arg,"list")){unsigned long n=strtoul(slot_arg,&end,10);if(!*slot_arg||*end||n>=EFFECT_SLOTS)return 2;slot=(unsigned)n;}
 if(slot==EFFECT_LIST&&page)return 2;
 int fd=-1,mfd=-1,result=1;const char *reason="command mapping unavailable";struct stat st,mst;CommandState *c=map_file(command_path,sizeof(*c),1,&fd,&st);if(c==MAP_FAILED){fprintf(stderr,"Effects interest: %s\n",reason);return 1;}
 reason="mirror mapping unavailable";const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&mfd,&mst);if(m==MAP_FAILED)goto done;
 reason="command writer already owned";if(flock(fd,LOCK_EX|LOCK_NB))goto unmap;
 reason="command/mirror header or source mismatch";if(!header(c)||m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m)||m->pid!=c->pid||m->origin_sec!=c->origin_sec||m->origin_nsec!=c->origin_nsec)goto unmap;
 uint64_t start=((uint64_t)c->start_hi<<32)|c->start_lo;CopiedMirror copy;uint32_t now;
 reason="MPC process identity or executable hash rejected";if(!start||command_process_start(c->pid)!=start||!command_process_exact(c->pid))goto unmap;
 /* Full executable qualification is slow. Copy publication and sample its
  * clock afterward, just as the settlement consumer does. */
 reason="copied source contended";if(settlement_snapshot(m,&copy,&now,&reason)!=1)goto unmap;
 reason="source file or process identity changed";if(!file_current(command_path,fd,&st)||!file_current(mirror_path,mfd,&mst)||command_process_start(c->pid)!=start)goto unmap;
 reason="selected project source unavailable";if(enabled&&(!copy.ready||!copy.selection.available||!copy.selected_serial))goto unmap;
 reason="interest lease exceeds clock range";if(now>UINT32_MAX-seconds*1000)goto unmap;
 reason="New Project transition in progress";if(atomic_load(&c->new_project_intent))goto unmap;
 unsigned rev=atomic_load(&c->effects_interest.revision);reason="interest revision or command admission unavailable";if((rev&1)||rev>UINT32_MAX-2||!command_writer_enter(c))goto unmap;
 EffectsInterest *d=&c->effects_interest;atomic_store(&d->revision,rev+1);atomic_store(&d->enabled,enabled);atomic_store(&d->epoch,copy.epoch);atomic_store(&d->serial,copy.selected_serial);atomic_store(&d->slot,slot);atomic_store(&d->page,(unsigned)page);atomic_store(&d->until,enabled?now+(unsigned)seconds*1000:0);atomic_store_explicit(&d->revision,rev+2,memory_order_release);command_writer_leave(c);result=0;
 printf("Effects interest enabled=%u serial=%u slot=%u page=%lu seconds=%lu; observer UI owner supplies state\n",enabled,copy.selected_serial,slot,page,seconds);
 unmap:munmap((void*)m,sizeof(*m));close(mfd);
 done:if(result)fprintf(stderr,"Effects interest: %s\n",reason);munmap(c,sizeof(*c));close(fd);return result;
}
static int qlink_interest_cli(const char *command_path,const char *mirror_path,const char *mode_arg,const char *seconds_arg){
 char *end;unsigned long seconds=strtoul(seconds_arg,&end,10);if(!*seconds_arg||*end||seconds<1||seconds>300)return 2;
 if(strcmp(mode_arg,"on")&&strcmp(mode_arg,"off"))return 2;
 unsigned enabled=!strcmp(mode_arg,"on");
 int fd=-1,mfd=-1,result=1;const char *reason="command mapping unavailable";struct stat st,mst;CommandState *c=map_file(command_path,sizeof(*c),1,&fd,&st);if(c==MAP_FAILED){fprintf(stderr,"Q-Link interest: %s\n",reason);return 1;}
 reason="mirror mapping unavailable";const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&mfd,&mst);if(m==MAP_FAILED)goto done;
 reason="command writer already owned";if(flock(fd,LOCK_EX|LOCK_NB))goto unmap;
 reason="command/mirror header or source mismatch";if(!header(c)||m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m)||m->pid!=c->pid||m->origin_sec!=c->origin_sec||m->origin_nsec!=c->origin_nsec)goto unmap;
 uint64_t start=((uint64_t)c->start_hi<<32)|c->start_lo;CopiedMirror copy;uint32_t now;
 reason="MPC process identity or executable hash rejected";if(!start||command_process_start(c->pid)!=start||!command_process_exact(c->pid))goto unmap;
 /* Full executable qualification is slow. Copy publication and sample its
  * clock afterward, just as the settlement consumer does. */
 reason="copied source contended";if(settlement_snapshot(m,&copy,&now,&reason)!=1)goto unmap;
 reason="source file or process identity changed";if(!file_current(command_path,fd,&st)||!file_current(mirror_path,mfd,&mst)||command_process_start(c->pid)!=start)goto unmap;
 reason="project source unavailable";if(enabled&&!copy.ready)goto unmap;
 reason="interest lease exceeds clock range";if(now>UINT32_MAX-seconds*1000)goto unmap;
 reason="New Project transition in progress";if(atomic_load(&c->new_project_intent))goto unmap;
 unsigned rev=atomic_load(&c->qlink_interest.revision);reason="interest revision or command admission unavailable";if((rev&1)||rev>UINT32_MAX-2||!command_writer_enter(c))goto unmap;
 QLinkInterest *d=&c->qlink_interest;atomic_store(&d->revision,rev+1);atomic_store(&d->enabled,enabled);atomic_store(&d->epoch,copy.epoch);atomic_store(&d->page,2);atomic_store(&d->until,enabled?now+(unsigned)seconds*1000:0);atomic_store_explicit(&d->revision,rev+2,memory_order_release);command_writer_leave(c);result=0;
 printf("Q-Link interest enabled=%u epoch=%u seconds=%lu; copied UI cache only, assignment and target settlement unqualified\n",enabled,copy.epoch,seconds);
 unmap:munmap((void*)m,sizeof(*m));close(mfd);
 done:if(result)fprintf(stderr,"Q-Link interest: %s\n",reason);munmap(c,sizeof(*c));close(fd);return result;
}
static int io_interest_cli(const char *command_path,const char *mirror_path,const char *mode_arg,const char *seconds_arg){
 char *end;unsigned long seconds=strtoul(seconds_arg,&end,10);if(!*seconds_arg||*end||seconds<1||seconds>300)return 2;
 if(strcmp(mode_arg,"on")&&strcmp(mode_arg,"off"))return 2;
 unsigned enabled=!strcmp(mode_arg,"on");
 int fd=-1,mfd=-1,result=1;const char *reason="command mapping unavailable";struct stat st,mst;CommandState *c=map_file(command_path,sizeof(*c),1,&fd,&st);if(c==MAP_FAILED){fprintf(stderr,"I/O interest: %s\n",reason);return 1;}
 reason="mirror mapping unavailable";const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&mfd,&mst);if(m==MAP_FAILED)goto done;
 reason="command writer already owned";if(flock(fd,LOCK_EX|LOCK_NB))goto unmap;
 reason="command/mirror header or source mismatch";if(!header(c)||m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m)||m->pid!=c->pid||m->origin_sec!=c->origin_sec||m->origin_nsec!=c->origin_nsec)goto unmap;
 uint64_t start=((uint64_t)c->start_hi<<32)|c->start_lo;CopiedMirror copy;uint32_t now;
 reason="MPC process identity or executable hash rejected";if(!start||command_process_start(c->pid)!=start||!command_process_exact(c->pid))goto unmap;
 /* Full executable qualification is slow. Copy publication and sample its
  * clock afterward, just as the settlement consumer does. */
 reason="copied source contended";if(settlement_snapshot(m,&copy,&now,&reason)!=1)goto unmap;
 reason="source file or process identity changed";if(!file_current(command_path,fd,&st)||!file_current(mirror_path,mfd,&mst)||command_process_start(c->pid)!=start)goto unmap;
 reason="project source unavailable";if(enabled&&(!copy.ready||!copy.selection.available||!copy.selected_serial))goto unmap;
 reason="interest lease exceeds clock range";if(now>UINT32_MAX-seconds*1000)goto unmap;
 reason="New Project transition in progress";if(atomic_load(&c->new_project_intent))goto unmap;
 unsigned rev=atomic_load(&c->io_interest.revision);reason="interest revision or command admission unavailable";if((rev&1)||rev>UINT32_MAX-2||!command_writer_enter(c))goto unmap;
 IOInterest *d=&c->io_interest;atomic_store(&d->revision,rev+1);atomic_store(&d->enabled,enabled);atomic_store(&d->epoch,copy.epoch);atomic_store(&d->serial,copy.selected_serial);atomic_store(&d->until,enabled?now+(unsigned)seconds*1000:0);atomic_store_explicit(&d->revision,rev+2,memory_order_release);command_writer_leave(c);result=0;
 printf("I/O interest enabled=%u epoch=%u seconds=%lu; native configuration source; equipment routing is a separate boundary\n",enabled,copy.epoch,seconds);
 unmap:munmap((void*)m,sizeof(*m));close(mfd);
 done:if(result)fprintf(stderr,"I/O interest: %s\n",reason);munmap(c,sizeof(*c));close(fd);return result;
}
static int effects_prepare_kind(const char *mirror_path,const char *output,const char *strip_arg,const char *value_arg,unsigned enable){
 char *end;unsigned long strip=strtoul(strip_arg,&end,10);if(!*strip_arg||*end||strip>=(enable?EFFECT_SLOTS:EFFECT_PAGE))return 2;
 float value=strtof(value_arg,&end);if(!*value_arg||*end||!isfinite(value)||value<0||value>1||(enable&&value!=0&&value!=1))return 2;
 int fd=-1,result=1;const char *reason="mirror mapping unavailable";struct stat st;const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&fd,&st);if(m==MAP_FAILED){fprintf(stderr,"Effects prepare: %s\n",reason);return 1;}
 CopiedMirror s;uint32_t now;reason="mirror header mismatch";if(m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m))goto done;
 uint64_t start=command_process_start(m->pid);reason="MPC process identity or executable hash rejected";if(!start||!command_process_exact(m->pid))goto done;
 reason="copied source contended";if(settlement_snapshot(m,&s,&now,&reason)!=1)goto done;
 reason="source file or process identity changed";if(!file_current(mirror_path,fd,&st)||command_process_start(m->pid)!=start)goto done;
 reason="Effects parameter page unavailable";if(!s.ready||!s.effects_available||s.effects.status!=EF_READY||(enable?s.effects.slot!=EFFECT_LIST:s.effects.slot>=EFFECT_SLOTS))goto done;
 reason="request lease exceeds clock range";if(now>UINT32_MAX-120000)goto done;
 const EffectParameter *p=s.effects.parameters+strip;const EffectSlot *e=s.effects.slots+(enable?(unsigned)strip:s.effects.slot);const CopiedTrack *t=NULL;for(unsigned i=0;i<s.count;i++)if(s.tracks[i].serial==s.selected_serial)t=s.tracks+i;
 reason="parameter or selected track unavailable/stale";if(!t||s.effects.serial!=t->serial||e->status!=EF_READY||(enable?(!e->enable_valid||e->enable_tick>now||now-e->enable_tick>=EFFECT_FRESH_MS):(p->status!=EF_READY||p->tick>now||now-p->tick>=EFFECT_FRESH_MS||p->index>=e->count)))goto done;
 uint32_t bits;memcpy(&bits,&value,4);if(enable&&bits==e->enable_bits){reason="insert already has requested enabled state";goto done;}CommandRequest r={.pid=m->pid,.start_lo=(uint32_t)start,.start_hi=(uint32_t)(start>>32),.origin_sec=m->origin_sec,.origin_nsec=m->origin_nsec,.epoch=s.epoch,.serial=t->serial,.binding=t->binding,.incarnation=t->incarnation,.bits=bits,.created=now,.expires=now+120000,.before_bits=enable?e->enable_bits:p->bits,.before_revision=enable?e->enable_revision:p->revision,.reserved=enable?EFFECT_ENABLE:EFFECT_PARAMETER,.track_owner=t->track_owner,.program_owner=t->program_owner,.field_incarnation=s.effects.generation,.project_owner=s.project_owner,.effect_generation=s.effects.generation,.effect_slot=enable?(unsigned)strip:s.effects.slot,.effect_index=enable?0:p->index,.effect_key=e->key,.effect_ap=e->ap,.effect_position=enable?0:p->position};

 reason="exclusive request file creation failed";int out=open(output,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(out<0)goto done;reason="request file write/sync/close failed";result=write(out,&r,sizeof(r))==sizeof(r)&&!fsync(out)?0:1;if(close(out))result=1;
 done:if(result)fprintf(stderr,"Effects prepare: %s\n",reason);munmap((void*)m,sizeof(*m));close(fd);return result;
}
static int qlink_prepare_cli(const char *mirror_path,const char *output,const char *index_arg,const char *value_arg){
 int mode=!strcmp(index_arg,"mode");
 char *end;unsigned long index=mode?0:strtoul(index_arg,&end,10);if(!mode&&(!*index_arg||*end||index>=QLINK_SLOTS))return 2;
 float value=0;int32_t direction=0;if(mode){long n=strtol(value_arg,&end,10);if(!*value_arg||*end||n<-32||n>32||!n)return 2;direction=(int32_t)n;}else{value=strtof(value_arg,&end);if(!*value_arg||*end||!isfinite(value)||value<0||value>1)return 2;}
 int fd=-1,result=1;const char *reason="mirror mapping unavailable";struct stat st;const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&fd,&st);if(m==MAP_FAILED){fprintf(stderr,"Q-Link prepare: %s\n",reason);return 1;}
 CopiedMirror s;uint32_t now;reason="mirror header mismatch";if(m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m))goto done;
 uint64_t start=command_process_start(m->pid);reason="MPC process identity or executable hash rejected";if(!start||!command_process_exact(m->pid))goto done;
 reason="copied source contended";if(settlement_snapshot(m,&s,&now,&reason)!=1)goto done;
 reason="source file or process identity changed";if(!file_current(mirror_path,fd,&st)||command_process_start(m->pid)!=start)goto done;
 reason="Q-Link UI sample unavailable";if(!s.ready||!s.project_owner||!s.qlinks_available)goto done;
 const QLinkCopy *q=&s.qlinks;const QLinkSlot *v=q->slots+index;
 reason="Q-Link slot sample stale or gesture already active";if(mode?(!q->mode_valid||q->mode_tick>now||now-q->mode_tick>=QLINK_FRESH_MS):(!v->sampled||v->index!=index||v->tick>now||now-v->tick>=QLINK_FRESH_MS||v->gesture_activity))goto done;
 reason="request lease exceeds clock range";if(now>UINT32_MAX-120000)goto done;
 uint32_t bits;memcpy(&bits,&value,4);CommandRequest r={.pid=m->pid,.start_lo=(uint32_t)start,.start_hi=(uint32_t)(start>>32),.origin_sec=m->origin_sec,.origin_nsec=m->origin_nsec,.epoch=s.epoch,.bits=bits,.created=now,.expires=now+120000,.before_bits=v->bits,.before_revision=v->revision,.reserved=QLINK_VALUE,.field_incarnation=q->generation,.project_owner=s.project_owner,.qlink_root=q->root,.qlink_wrapper=q->wrapper,.qlink_provider=q->provider,.qlink_generation=q->generation,.qlink_index=(uint32_t)index};
 if(mode){r.reserved=QLINK_MODE;r.bits=(uint32_t)direction;r.global_owner=q->mode_controller;r.field_incarnation=q->mode_generation;r.before_bits=q->mode_id;r.before_revision=q->mode_revision;r.qlink_wrapper=r.qlink_provider=r.qlink_generation=r.qlink_index=0;}
 reason="exclusive request file creation failed";int out=open(output,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(out<0)goto done;reason="request file write/sync/close failed";result=write(out,&r,sizeof(r))==sizeof(r)&&!fsync(out)?0:1;if(close(out))result=1;
 done:if(result)fprintf(stderr,"Q-Link prepare: %s\n",reason);munmap((void*)m,sizeof(*m));close(fd);return result;
}

static int effects_prepare_cli(const char *mirror_path,const char *output,const char *strip_arg,const char *value_arg){return effects_prepare_kind(mirror_path,output,strip_arg,value_arg,0);}

static int effects_chooser_cli(const char *mirror_path,const char *output,const char *slot_arg,const char *action){
 char *end;unsigned long slot=strtoul(slot_arg,&end,10);if(!*slot_arg||*end||slot>=EFFECT_SLOTS||(strcmp(action,"add")&&strcmp(action,"replace")))return 2;
 unsigned replace=!strcmp(action,"replace");int fd=-1,result=1;const char *reason="mirror mapping unavailable";struct stat st;const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&fd,&st);if(m==MAP_FAILED){fprintf(stderr,"Chooser prepare: %s\n",reason);return 1;}
 CopiedMirror s;uint32_t now;reason="mirror header mismatch";if(m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m))goto done;
 uint64_t start=command_process_start(m->pid);reason="MPC process identity or executable hash rejected";if(!start||!command_process_exact(m->pid))goto done;
 reason="copied source contended";if(settlement_snapshot(m,&s,&now,&reason)!=1)goto done;
 reason="source file or process identity changed";if(!file_current(mirror_path,fd,&st)||command_process_start(m->pid)!=start)goto done;
 reason="selected Effects page unavailable";if(!s.ready||!s.effects_available||s.effects.status!=EF_READY||!s.selection.available||s.effects.serial!=s.selected_serial||(replace?s.effects.slot!=slot:s.effects.slot!=EFFECT_LIST)||now>UINT32_MAX-120000)goto done;
 const EffectSlot *e=s.effects.slots+slot;const CopiedTrack *t=NULL;for(unsigned i=0;i<s.count;i++)if(s.tracks[i].serial==s.selected_serial)t=s.tracks+i;
 reason="slot source does not match add/replace action";if(!t||(replace?(e->status!=EF_READY||!e->key||!e->ap):(e->status!=EF_EMPTY||e->key||e->ap)))goto done;
 CommandRequest r={.pid=m->pid,.start_lo=(uint32_t)start,.start_hi=(uint32_t)(start>>32),.origin_sec=m->origin_sec,.origin_nsec=m->origin_nsec,.epoch=s.epoch,.serial=t->serial,.binding=t->binding,.incarnation=t->incarnation,.bits=replace,.created=now,.expires=now+120000,.reserved=EFFECT_CHOOSER,.track_owner=t->track_owner,.program_owner=t->program_owner,.field_incarnation=s.effects.generation,.project_owner=s.project_owner,.effect_generation=s.effects.generation,.effect_slot=(unsigned)slot,.effect_key=e->key,.effect_ap=e->ap,.effect_position=s.effects.page};
 reason="exclusive request file creation failed";int out=open(output,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(out<0)goto done;reason="request file write/sync/close failed";result=write(out,&r,sizeof(r))==sizeof(r)&&!fsync(out)?0:1;if(close(out))result=1;
 done:if(result)fprintf(stderr,"Chooser prepare: %s\n",reason);munmap((void*)m,sizeof(*m));close(fd);return result;
}

static int io_prepare_cli(const char *mirror_path,const char *output,const char *field_arg,const char *delta_arg){
 char *end;unsigned long field=strtoul(field_arg,&end,10);if(!*field_arg||*end||field>=IO_FIELDS)return 2;
 long delta=strtol(delta_arg,&end,10);if(!*delta_arg||*end||!delta||delta<-32||delta>32)return 2;
 int fd=-1,result=1;const char *reason="mirror mapping unavailable";struct stat st;const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&fd,&st);if(m==MAP_FAILED){fprintf(stderr,"I/O prepare: %s\n",reason);return 1;}
 CopiedMirror s;uint32_t now;reason="mirror header mismatch";if(m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m))goto done;
 uint64_t start=command_process_start(m->pid);reason="MPC process identity or executable hash rejected";if(!start||!command_process_exact(m->pid))goto done;
 reason="copied source contended";if(settlement_snapshot(m,&s,&now,&reason)!=1)goto done;
 reason="source file or process identity changed";if(!file_current(mirror_path,fd,&st)||command_process_start(m->pid)!=start)goto done;
 reason="selected I/O source unavailable";if(!s.ready||!s.io_available||!s.project_owner)goto done;
 const IOCopy *q=&s.io;const IOField *f=q->fields+field;reason="I/O field or native choices unavailable";
 if(f->status!=IO_READY||!f->connector||!f->incarnation||!f->property||!f->choice_generation||!f->choice_count||f->tick>now||now-f->tick>=IO_FRESH_MS)goto done;
 reason="request lease exceeds clock range";if(now>UINT32_MAX-120000)goto done;
 CommandRequest r={.pid=m->pid,.start_lo=(uint32_t)start,.start_hi=(uint32_t)(start>>32),.origin_sec=m->origin_sec,.origin_nsec=m->origin_nsec,.epoch=s.epoch,.serial=q->serial,.binding=q->binding,.incarnation=q->incarnation,.bits=(uint32_t)(int32_t)delta,.created=now,.expires=now+120000,.before_bits=f->value,.before_revision=f->revision,.reserved=IO_PARAMETER,.track_owner=q->track_owner,.program_owner=q->program_owner,.project_owner=s.project_owner,.field_incarnation=f->incarnation,.global_owner=f->property,.io_generation=q->generation,.io_choice_generation=f->choice_generation,.io_field=field,.io_connector=f->connector};
 reason="exclusive request file creation failed";int out=open(output,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(out<0)goto done;reason="request file write/sync/close failed";result=write(out,&r,sizeof(r))==sizeof(r)&&!fsync(out)?0:1;if(close(out))result=1;
 done:if(result)fprintf(stderr,"I/O prepare: %s\n",reason);munmap((void*)m,sizeof(*m));close(fd);return result;
}

int main(int argc,char **argv){
 if(argc==6&&!strcmp(argv[1],"io-interest"))return io_interest_cli(argv[2],argv[3],argv[4],argv[5]);
 if(argc==6&&!strcmp(argv[1],"effects-chooser"))return effects_chooser_cli(argv[2],argv[3],argv[4],argv[5]);
 if(argc==6&&!strcmp(argv[1],"io-prepare"))return io_prepare_cli(argv[2],argv[3],argv[4],argv[5]);
 if(argc==5&&!strcmp(argv[1],"qlink-mode"))return qlink_prepare_cli(argv[2],argv[3],"mode",argv[4]);
 if(argc==6&&!strcmp(argv[1],"qlink-prepare"))return qlink_prepare_cli(argv[2],argv[3],argv[4],argv[5]);
 if(argc==6&&!strcmp(argv[1],"qlink-interest"))return qlink_interest_cli(argv[2],argv[3],argv[4],argv[5]);
 if(argc==7&&!strcmp(argv[1],"effects-interest"))return effects_interest_cli(argv[2],argv[3],argv[4],argv[5],argv[6]);
 if(argc==6&&!strcmp(argv[1],"effects-enable"))return effects_prepare_kind(argv[2],argv[3],argv[4],argv[5],1);
 if(argc==6&&!strcmp(argv[1],"effects-prepare"))return effects_prepare_cli(argv[2],argv[3],argv[4],argv[5]);
 if(argc==5&&!strcmp(argv[1],"processor-watch"))return processor_watch(argv[2],argv[3],argv[4]);
 if(argc==3&&!strcmp(argv[1],"processor-status"))return processor_status(argv[2]);
 if(argc==4&&!strcmp(argv[1],"processor-stream"))return processor_stream(argv[2],argv[3]);
 if(argc==3&&!strcmp(argv[1],"processor-end"))return processor_end(argv[2]);
 if((argc==5||argc==6)&&!strcmp(argv[1],"new-project-intent")&&(argc==5||!strcmp(argv[5],"consume")))return new_project(argv[2],argv[3],argv[4],argc==6);
 if((argc==5||argc==6)&&!strcmp(argv[1],"global"))return prepare(argv[2],argv[3],argc==6?argv[5]:"0",NULL,argv[4]);
 if(argc==5&&!strcmp(argv[1],"master"))return prepare(argv[2],argv[3],argv[4],NULL,"master");
 if(argc==6&&!strcmp(argv[1],"jog"))return prepare(argv[2],argv[3],argv[4],NULL,argv[5]);
 if(argc==4&&!strcmp(argv[1],"session-status"))return session(argv[2],argv[3],0);
 if(argc==4&&!strcmp(argv[1],"stop"))return session(argv[2],argv[3],1);
 if(argc>=2&&!strcmp(argv[1],"prepare")&&(argc>=5&&argc<=7))return prepare(argv[2],argv[3],argv[4],argc>=6?argv[5]:NULL,argc==7?argv[6]:NULL);
 if(argc==4&&!strcmp(argv[1],"submit"))return submit(argv[2],argv[3]);
 if(argc==4&&!strcmp(argv[1],"settle"))return settle(argv[2],argv[3]);
 if(argc==3&&!strcmp(argv[1],"status")){int fd=-1;struct stat st;CommandState *s=map_file(argv[2],sizeof(*s),0,&fd,&st);if(s==MAP_FAILED)return 1;int ok=header(s);if(ok)status(s);munmap(s,sizeof(*s));close(fd);return ok?0:1;}
 fputs("command-client effects-chooser /volume.state /absent-request.bin SLOT(0..3) add|replace\ncommand-client io-interest /command.state /volume.state on|off SECONDS(1..300)\ncommand-client io-prepare /volume.state /absent-request.bin FIELD(0..7) SIGNED_STEPS(-32..32, nonzero)\ncommand-client qlink-mode /volume.state /absent-request.bin SIGNED_STEPS(-32..32, nonzero)\ncommand-client qlink-prepare /volume.state /absent-request.bin INDEX(0..15) VALUE(0..1)\ncommand-client qlink-interest /command.state /volume.state on|off SECONDS(1..300)\ncommand-client effects-interest /command.state /volume.state off|list|SLOT(0..3) PAGE(0..575) SECONDS(1..300)\ncommand-client effects-prepare /volume.state /absent-request.bin STRIP(0..7) VALUE(0..1)\ncommand-client effects-enable /volume.state /absent-request.bin SLOT(0..3) ENABLED(0|1)\ncommand-client processor-watch /command.state discover|KEY 1..300\ncommand-client processor-stream /command.state 1..300 (NDJSON stdout; backpressure fails)\ncommand-client processor-status /command.state\ncommand-client processor-end /command.state\ncommand-client global /volume.state /absent-request.bin automation MODE(0..2)|save|zoom-in|zoom-out|zoom-up|zoom-down|enter|cancel|left|up|right|down|record-toggle|click-toggle|loop-toggle\ncommand-client master /volume.state /absent-request.bin VALUE\ncommand-client jog /volume.state /absent-request.bin SIGNED_STEPS bars|beats|pulses\ncommand-client prepare /volume.state /absent-request.bin VALUE [MASTER_INDEX [volume|pan|mute|solo|arm|select|send1|send2|send3|send4]]\ncommand-client submit /command.state /request.bin\ncommand-client settle /command.state /volume.state\ncommand-client status /command.state\ncommand-client session-status /command.state /volume.state\ncommand-client stop /command.state /volume.state\n",stderr);return 2;
}
