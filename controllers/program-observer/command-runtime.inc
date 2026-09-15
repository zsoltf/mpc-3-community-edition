#include "command-state.h"
#include "command-process.h"
static void *command_file(const char *path,size_t size){
 int fd=open(path,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return MAP_FAILED;
 struct stat st;void *result=MAP_FAILED;
 if(!posix_fallocate(fd,0,size)&&!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size==(off_t)size)result=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
 close(fd);return result;
}
/* Called only after stop and all source/external admissions have closed. An
 * accepted publication stranded by finite stop is explicitly incomplete. */
static int command_finalize(void){
 if(!command_quiescent())return 0;
 command_retire(); /* terminal pre-call refusals can release storage after closure */
 unsigned diagnostic=command_enrollment_detail();if(diagnostic)atomic_store_explicit(&command_state->admission_detail,diagnostic,memory_order_release);
 unsigned enroll=command_enrollment_error();if(enroll)atomic_store(&command_state->error,enroll);
 unsigned violation=atomic_load_explicit(&command_violation,memory_order_seq_cst);if(violation)atomic_store(&command_state->error,violation&65535);
 int incomplete=!command_idle(command_state);
 if(atomic_load(&mirror_state->meters.tokens)){atomic_store(&mirror_state->meters.error,ME_CLOSE);incomplete=1;}
 if(incomplete&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error))atomic_store(&command_state->error,C_CLOSED);
 atomic_store_explicit(&command_state->alive,0,memory_order_release);mirror_stop(0);atomic_store_explicit(&command_state->closed,1,memory_order_release);return 1;
}
static void command_consume_tick(uint32_t now){
 static unsigned meter_close_polls;
 unsigned orderly=0;
 if(now==UINT32_MAX){atomic_store(&command_state->error,C_SOURCE_FAILURE);command_close();}
  else atomic_store_explicit(&mirror_state->heartbeat,now,memory_order_release);
#if WINDOW_SECONDS
  if(now>=WINDOW_SECONDS*1000u)orderly=1;
#endif
  if(atomic_load(&command_state->error)||atomic_load(&mirror_state->error))command_close();
  if(atomic_load_explicit(&command_state->stop_requested,memory_order_acquire)){
   uint32_t n=atomic_load_explicit(&command_state->stop_sequence,memory_order_relaxed);
   if(n!=atomic_load(&command_state->published)||!command_idle(command_state))atomic_store(&command_state->error,C_CLOSED);
   orderly=1;
  }
  if(orderly){
   if(command_meter_closing())command_close();
   else if(++meter_close_polls>=200){atomic_store(&mirror_state->meters.error,ME_CLOSE);atomic_store(&command_state->error,C_CLOSED);command_close();}
  }
  if(atomic_load_explicit(&observer_running,memory_order_seq_cst))command_retire();
}
static void *consume(void *unused){
 (void)unused;unsigned closing_polls=0;
 for(;;){
  if(atomic_load_explicit(&command_state->startup,memory_order_acquire)==1){struct timespec init_wait={0,1000000};nanosleep(&init_wait,NULL);continue;}
  uint32_t now=elapsed();command_consume_tick(now);
  if(!atomic_load_explicit(&observer_running,memory_order_seq_cst)){
   if(command_finalize())return NULL;
   if(++closing_polls>=100){
    /* Bounded wait expired: inhibit output, but do not claim immutable closed
     * state while an admitted source writer still exists. */
    unsigned diagnostic=command_enrollment_detail();if(diagnostic)atomic_store_explicit(&command_state->admission_detail,diagnostic,memory_order_release);
    atomic_store(&command_state->error,C_SOURCE_FAILURE);atomic_store(&command_state->alive,0);mirror_stop(0);return NULL;
   }
  }
  struct timespec delay={0,10000000};nanosleep(&delay,NULL);
 }
}
static void child_inert(void){command_state=NULL;mirror_state=NULL;atomic_store(&observer_running,0);started=0;}
__attribute__((constructor)) static void startup(void){
 const char *preload=getenv("LD_PRELOAD");if(!preload||strcmp(preload,OBSERVER_LIBRARY))return;if(unsetenv("LD_PRELOAD"))return;
 unsigned long hw=getauxval(AT_HWCAP);if(getauxval(AT_PAGESZ)!=4096||(hw&(HWCAP_NEON|HWCAP_VFPD32))!=(HWCAP_NEON|HWCAP_VFPD32)||!exact_file()||!only_thread())return;
 dl_iterate_phdr(image,NULL);if(found!=1||!load_bias||(load_bias&4095))return;
 uint64_t process_start=command_process_start(getpid());if(!process_start)return;
 MirrorState *state=command_file(OBSERVER_LOG,sizeof(MirrorState));if(state==MAP_FAILED)return;mirror_state=state;memset(state,0,sizeof(*state));
 CommandState *commands=command_file(COMMAND_PATH,sizeof(CommandState));if(commands==MAP_FAILED)return;command_state=commands;memset(commands,0,sizeof(*commands));
 if(clock_gettime(CLOCK_MONOTONIC,&began)||began.tv_sec<0||(uint64_t)began.tv_sec>UINT32_MAX)goto failure;
 state->magic=MIRROR_MAGIC;state->version=MIRROR_VERSION;state->bytes=sizeof(*state);state->pid=getpid();state->origin_sec=began.tv_sec;state->origin_nsec=began.tv_nsec;state->capacity=MIRROR_CELLS;
 commands->magic=COMMAND_MAGIC;commands->version=COMMAND_VERSION;commands->bytes=sizeof(*commands);commands->pid=getpid();commands->start_lo=process_start;commands->start_hi=process_start>>32;commands->origin_sec=began.tv_sec;commands->origin_nsec=began.tv_nsec;commands->capacity=COMMAND_SLOTS;commands->seconds=WINDOW_SECONDS;
 command_initialize();atomic_store_explicit(&commands->startup,1,memory_order_release);
 void *stack=mmap(NULL,CONSUMER_STACK+4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0),*gates=NULL;pthread_attr_t attr;int attr_ok=0;
 if(stack==MAP_FAILED)goto failure;
 if(pthread_attr_init(&attr))goto failure;
 attr_ok=1;
 if(mprotect((char*)stack+4096,CONSUMER_STACK,PROT_READ|PROT_WRITE)||pthread_attr_setstack(&attr,(char*)stack+4096,CONSUMER_STACK)||pthread_atfork(NULL,NULL,child_inert)||!only_thread())goto setup_failure;
 observer_image_bias=load_bias;if(!install_patches(load_bias,-1,&gates))goto setup_failure;
 atomic_store_explicit(&observer_running,1,memory_order_seq_cst);atomic_store_explicit(&state->alive,1,memory_order_release);atomic_store_explicit(&commands->alive,1,memory_order_release);
 if(pthread_create(&consumer,&attr,consume,NULL))goto setup_failure;
 pthread_attr_destroy(&attr);started=1;atomic_store_explicit(&commands->startup,2,memory_order_release);return;
setup_failure:if(attr_ok)pthread_attr_destroy(&attr);
failure:command_close();atomic_store(&command_state->error,C_SOURCE_FAILURE);atomic_store(&command_state->alive,0);mirror_stop(MV_SETUP);atomic_store_explicit(&command_state->startup,3,memory_order_release);
}
__attribute__((destructor)) static void finish(void){if(started){command_close();pthread_join(consumer,NULL);}}
