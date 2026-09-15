/* Actual diagnostic writer/readback/collector with substituted native registers,
 * clock and enrolled-thread identities. No MPC object or native getter called. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdatomic.h>
#define PROCESSOR_CAS_TEST
static _Atomic uint32_t *cas_test_address;
static unsigned cas_test_failures,cas_test_calls;
static int processor_test_cas(_Atomic uint32_t *address,uint32_t *expected,uint32_t desired){
 (void)desired;if(address!=cas_test_address)return 0;cas_test_calls++;
 if(!cas_test_failures)return 0;
 cas_test_failures--;*expected=atomic_load(address);return 1;
}
#define main diagnostic_client_entry_not_called
#include "command-client.c"
#undef main
#include "observer.h"
#include <pthread.h>
#include <sched.h>
CommandState *command_state;
MirrorState *mirror_state;
static __thread unsigned test_lane;
static uint32_t token(void){return 100+test_lane;}
static uint32_t source_sp(Context *c){return c->r[12];}
#include "processor-observation.inc"
static void need(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static void reset(void){
 memset(command_state,0,sizeof(*command_state));memset(mirror_state,0,sizeof(*mirror_state));processor_get_depth=processor_get_overflow=0;test_lane=0;
 command_state->magic=COMMAND_MAGIC;command_state->version=COMMAND_VERSION;command_state->bytes=sizeof(*command_state);command_state->capacity=COMMAND_SLOTS;command_state->pid=getpid();command_state->owner_token=1;
 for(unsigned i=0;i<COMMAND_LANES;i++)atomic_store(command_state->lane_tokens+i,100+i);
 atomic_store(&mirror_state->heartbeat,100);need(processor_begin(command_state,100,0,300000),"start 300-second window");
}
static Context context(uint32_t key,uint32_t sp){Context c={0};c.r[0]=key;c.r[1]=7;c.r[3]=0x1025;c.r[12]=sp;c.d[0]=0x3f000000;return c;}
static void observe(Context *c,unsigned site){if(processor_observation_wanted(site))processor_observe(c,site,test_lane);}
static void frame_checks(void){
 reset();Context c=context(0x10000,0x8000);observe(&c,PX_GET);need(processor_open_total(command_state)==1,"getter admitted");
 atomic_store(&mirror_state->heartbeat,300101);need(!processor_observation_active()&&!processor_begin(command_state,300101,0,300000),"expiry does not permit generation over open getter");
 Context nested=context(0x20000,0x7f00);observe(&nested,PX_GET);need(processor_open_total(command_state)==2,"nested expired getter retains structural bookkeeping");nested.r[12]-=40;observe(&nested,PX_GET_DONE);c.r[12]-=40;observe(&c,PX_GET_DONE);
 uint32_t w[PROCESSOR_WORDS];need(!processor_open_total(command_state)&&atomic_load(&command_state->processor_count)==2&&processor_read(command_state,2,w)&&w[12]==1&&w[2]==300101,"admitted getter closes and records after expiry; expired nested call emits no record");need(processor_begin(command_state,300101,0x10000,300000),"new generation after complete getter closure");
 c=context(0x10000,0x8000);observe(&c,PX_GET);Context notify=context(0x10004,0x9000);observe(&notify,PX_NOTIFY);Context other=context(0x20004,0x9000);observe(&other,PX_NOTIFY);Context listener=context(0x10050,0x9000);observe(&listener,PX_LISTENER);observe(&c,PX_SET_ALT);other.r[0]=0x40000;observe(&other,PX_FACADE);c.r[12]-=40;observe(&c,PX_GET_DONE);
 need(atomic_load(&command_state->processor_count)==7&&processor_read(command_state,4,w)&&w[6]==0x10000&&w[7]==0x10004&&w[10]==0x1025,"E+4 notify filter joins E getter while preserving raw r0/r3; foreign E and facade excluded");
 reset();for(unsigned i=0;i<6;i++){c=context(0x10000,0x8000-i*100);observe(&c,PX_GET);}need(processor_open_total(command_state)==6,"bounded stack overflow remains counted");for(unsigned i=6;i;i--){c=context(0,0x8000-(i-1)*100-40);observe(&c,PX_GET_DONE);}need(!processor_open_total(command_state)&&!processor_get_depth&&!processor_get_overflow,"overflow returns cannot pop admitted outer frames early");
 reset();atomic_store(&command_state->processor_count,UINT32_MAX-2);c=context(0x10000,0x8000);observe(&c,PX_GET);c.r[12]-=40;observe(&c,PX_GET_DONE);need(!processor_open_total(command_state)&&(atomic_load(command_state->processor_loss)&PX_LOSS_SEQUENCE),"sequence capacity does not strand admitted getter");
 reset();c=context(0x10000,0x8000);observe(&c,PX_GET);processor_end_window(command_state);need(!atomic_load(&command_state->processor_until)&&processor_open_total(command_state)==1,"diagnostic disarm preserves admitted getter");c.r[12]-=40;observe(&c,PX_GET_DONE);need(!processor_open_total(command_state)&&atomic_load(&command_state->processor_count)==2&&!atomic_load(&command_state->closed)&&!atomic_load(&command_state->external_stop),"disarmed watch drains tail without stopping musical state");
 reset();unsigned generation=processor_enter(command_state,0);need(generation==2,"source enters generation");need(!processor_begin(command_state,300101,0,300000),"generation cannot cross active publisher");processor_leave(command_state,0);need(processor_begin(command_state,300101,0,300000),"closed source admits later generation");
 puts("PASS actual getter bookkeeping: expiry/nested/overflow/capacity closure, generation barriers, E/E+4 owner filtering and raw r3 (native context/enrollment/time substituted)");
}
static void rolling_checks(void){
 reset();FILE *out=tmpfile();need(out!=NULL,"collector sink");ProcessorCursor cursor={0};
 for(unsigned n=1;n<=1536;n++){Context c=context(0x10000,0x8000);c.r[3]=n;observe(&c,PX_SET);if(n%64==0)processor_collect(out,command_state,&cursor,0);}
 processor_collect(out,command_state,&cursor,1);need(atomic_load(&command_state->processor_until)==300100&&!atomic_load(command_state->processor_loss),">512 records roll without terminating capture or declaring stream loss");
 uint32_t w[PROCESSOR_WORDS];need(!processor_read(command_state,1,w)&&processor_read(command_state,1536,w)&&w[10]==1536,"only exact retained sequence can be read");
 rewind(out);char line[1024];unsigned records=0,gaps=0;while(fgets(line,sizeof(line),out)){records+=strstr(line,"\"type\":\"record\"")!=NULL;gaps+=strstr(line,"\"type\":\"gap\"")!=NULL;}need(records==1536&&!gaps,"stream retains every polled record with no false overwrite-loss report");fclose(out);
 out=tmpfile();need(out!=NULL,"late collector");memset(&cursor,0,sizeof(cursor));processor_collect(out,command_state,&cursor,1);rewind(out);need(fgets(line,sizeof(line),out)&&strstr(line,"\"first\":1,\"last\":1024")&&strstr(line,"not_observed_before_retention_advanced"),"late collector explicitly reports its unobserved range");fclose(out);
 reset();out=tmpfile();need(out!=NULL,"missed-full-turn collector");memset(&cursor,0,sizeof(cursor));
 for(unsigned n=1;n<=2048;n++){Context step=context(0x10000,0x8000);step.r[3]=n;observe(&step,PX_SET);if(n==512||n==1024)processor_collect(out,command_state,&cursor,0);}
 need(cursor.floor==513,"second complete poll advances collector floor before later wrap");processor_collect(out,command_state,&cursor,1);rewind(out);records=gaps=0;
 while(fgets(line,sizeof(line),out)){records+=strstr(line,"\"type\":\"record\"")!=NULL;if(strstr(line,"\"type\":\"gap\"")){gaps++;need(strstr(line,"\"first\":1025,\"last\":1536")!=NULL,"missed full turn excludes all previously collected tickets");fputs(line,stdout);}}
 need(records==1536&&gaps==1,"two collected turns then producer2048 yields only gap1025..1536");fclose(out);
 puts("PASS actual collector polls512/1024 then2048: only unobserved1025..1536 reported; already collected1..1024 retained");
 reset();atomic_store(&command_state->processor_events[0].guard,1);Context c=context(0x10000,0x8000);observe(&c,PX_SET);need(atomic_load(command_state->processor_dropped)==1&&(atomic_load(command_state->processor_loss)&PX_LOSS_WRITER)&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error),"contended ring drops only diagnostic record, never musical state");atomic_store(&command_state->processor_events[0].guard,0);
 puts("PASS rolling writer/real collector:1536 events, stable retained reads, no false stream gaps, explicit late-reader gap and writer contention loss");
}
static void cell_claim_checks(void){
 uint32_t words[PROCESSOR_WORDS]={0},readback[PROCESSOR_WORDS];
 for(unsigned failures=1;failures<=4;failures++){
  reset();cas_test_address=&command_state->processor_events[0].guard;cas_test_failures=failures;cas_test_calls=0;
  int ok=processor_publish(command_state,0,words);
  need(cas_test_calls==(failures<4?failures+1:4),"cell exclusive retries have exact four-attempt bound");
  need(ok==(failures<4)&&processor_read(command_state,1,readback)==ok,"spurious free-cell failure recovers only within bound");
  need(atomic_load(command_state->processor_dropped)==(unsigned)!ok&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error),"retry exhaustion loses only diagnostic record");
 }
 reset();cas_test_address=&command_state->processor_events[0].guard;cas_test_failures=0;cas_test_calls=0;atomic_store(cas_test_address,1);
 need(!processor_publish(command_state,0,words)&&cas_test_calls==1&&atomic_load(cas_test_address)==1,"busy cell rejects immediately and preserves existing owner");
 cas_test_address=NULL;reset();atomic_store(&command_state->processor_events[0].sequence,513);
 need(!processor_publish(command_state,0,words)&&atomic_load(&command_state->processor_events[0].sequence)==513&&!atomic_load(&command_state->processor_events[0].guard),"older ticket cannot overwrite newer cell after claim");
 puts("PASS actual publisher injected lost-exclusive1..4: bounded recovery/exhaustion, immediate busy rejection and newer-ticket protection (CAS failures substituted)");
}
static pthread_barrier_t start_barrier;static _Atomic unsigned finished;
static void *producer(void *arg){
 unsigned lane=(unsigned)(uintptr_t)arg;test_lane=lane;pthread_barrier_wait(&start_barrier);
 for(unsigned n=0;n<10000;n++){
  uint32_t generation=processor_enter(command_state,lane);if(!generation)continue;
  uint32_t w[PROCESSOR_WORDS],value=lane*10000+n;w[0]=generation;for(unsigned i=1;i<PROCESSOR_WORDS;i++)w[i]=value+i*0x100000;
  (void)processor_publish(command_state,lane,w);processor_leave(command_state,lane);if(n%64==0)sched_yield();
 }
 atomic_fetch_add(&finished,1);return NULL;
}
static void concurrent_checks(void){
 reset();pthread_t threads[4];atomic_store(&finished,0);need(!pthread_barrier_init(&start_barrier,NULL,5),"producer barrier");for(unsigned i=0;i<4;i++)need(!pthread_create(threads+i,NULL,producer,(void*)(uintptr_t)i),"producer thread");pthread_barrier_wait(&start_barrier);
 unsigned read=0;do{uint32_t count=atomic_load(&command_state->processor_count),first=count>PROCESSOR_OBSERVATIONS?count-PROCESSOR_OBSERVATIONS:0;
  for(uint32_t n=first;n<count;n++){uint32_t w[PROCESSOR_WORDS];if(!processor_read(command_state,n+1,w))continue;need(w[0]==2,"record generation coherent");for(unsigned i=2;i<PROCESSOR_WORDS;i++)need(w[i]-w[1]==(i-1)*0x100000,"no mixed record while concurrent writers wrap");read++;}
 }while(atomic_load(&finished)<4);
 for(unsigned i=0;i<4;i++)need(!pthread_join(threads[i],NULL),"producer joined");
 pthread_barrier_destroy(&start_barrier);need(read>0&&atomic_load(&command_state->processor_count)>512,"concurrent wrap and read exercised");
 puts("PASS four actual pthread producers plus concurrent atomic reader through wrapping production cells; no mixed accepted record (native events substituted)");
}
static void output_checks(void){
 reset();Context c=context(0x10000,0x8000);observe(&c,PX_SET);void (*old_pipe)(int)=signal(SIGPIPE,SIG_IGN);
 for(unsigned broken=0;broken<2;broken++){
  int fds[2];need(!pipe(fds),"output pipe");int flags=fcntl(fds[1],F_GETFL);
  if(broken)close(fds[0]);else{need(!fcntl(fds[1],F_SETFL,flags|O_NONBLOCK),"prepare full pipe");char bytes[4096]={0};while(write(fds[1],bytes,sizeof(bytes))>0){}need(errno==EAGAIN,"pipe filled");need(!fcntl(fds[1],F_SETFL,flags),"restore blocking pipe before production setup");}
  FILE *out=fdopen(fds[1],"w");need(out!=NULL,"output FILE");int before;need(processor_output_begin(out,&before),"production nonblocking output setup");ProcessorCursor cursor={0};alarm(2);processor_collect(out,command_state,&cursor,0);alarm(0);need(ferror(out),"blocked/broken output fails promptly rather than waiting");fclose(out);if(!broken)close(fds[0]);
 }
 signal(SIGPIPE,old_pipe);puts("PASS actual collector stdout path fails on full/broken pipes without waiting (real pipes; native stream admission not substituted here)");
}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 command_state=calloc(1,sizeof(*command_state));mirror_state=calloc(1,sizeof(*mirror_state));need(command_state&&mirror_state,"diagnostic fixture state");frame_checks();rolling_checks();cell_claim_checks();concurrent_checks();output_checks();
 reset();int fd=open(argv[1],O_CREAT|O_EXCL|O_RDWR,0600);need(fd>=0&&!ftruncate(fd,sizeof(*command_state)),"actual diagnostic file");need(write(fd,command_state,sizeof(*command_state))==(ssize_t)sizeof(*command_state),"diagnostic bytes");close(fd);
 need(!processor_status(argv[1]),"actual client stable CMD30 file readback");fd=open(argv[1],O_RDWR);uint32_t version=25;need(pwrite(fd,&version,4,4)==4,"prior version fixture");close(fd);need(processor_status(argv[1])==1&&processor_stream(argv[1],"1")==1,"CMD25 rejected by status and streaming client");unlink(argv[1]);free(command_state);free(mirror_state);puts("PASS exact CMD30 client file readback and prior CMD25 rejection; native MPC process identity and live stream remain untested");return 0;
}
