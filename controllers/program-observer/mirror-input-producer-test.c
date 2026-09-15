/* Actual CMD12/MMV7 producer composition; native MPC method/body substituted by
 * the existing command component. Separate client process uses the real files. */
#define main prior_command_component_main
#include "command-component.c"
#undef main
#include <sys/wait.h>
/* QEMU user mode synthesizes self /proc/stat with starttime=0. A helper
 * reading the parent gets the actual kernel identity also seen by our client.
 * This fixture accommodation does not change production process validation. */
static uint64_t fixture_start(void){
 int pipefd[2];require(!pipe(pipefd),"process identity pipe");pid_t parent=getpid(),helper=fork();require(helper>=0,"process identity reader");
 if(!helper){close(pipefd[0]);uint64_t start=command_process_start(parent);int ok=write(pipefd[1],&start,sizeof(start))==sizeof(start);close(pipefd[1]);_exit(ok?0:1);}
 close(pipefd[1]);uint64_t start=0;ssize_t n=read(pipefd[0],&start,sizeof(start));close(pipefd[0]);int status;require(waitpid(helper,&status,0)==helper&&WIFEXITED(status)&&!WEXITSTATUS(status)&&n==sizeof(start)&&start,"actual parent starttime");return start;
}
int main(int argc,char **argv){
 if(argc!=4)return 2;
 alarm(30);
 require(mmap((void*)0x6930000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=MAP_FAILED,"fixture method page");put(0x6930c28,0x250ba74);put(0x6930c78,0x1375c68);
 fixture=command_file(argv[1],sizeof(MirrorState));command_state=command_file(argv[2],sizeof(CommandState));require(fixture!=MAP_FAILED&&command_state!=MAP_FAILED,"exclusive shared integration files");
 initial(12);command_state->magic=COMMAND_MAGIC;command_state->version=COMMAND_VERSION;command_state->bytes=sizeof(*command_state);command_state->capacity=COMMAND_SLOTS;command_state->pid=getpid();uint64_t start=fixture_start();
 command_state->start_lo=start;command_state->start_hi=start>>32;command_state->origin_sec=fixture->origin_sec;command_state->origin_nsec=fixture->origin_nsec;command_state->seconds=WINDOW_SECONDS;atomic_store(&command_state->alive,1);
 command_initialize();observer_running=1;observer_image_bias=0;component_dispatch=fake_dispatch;for(unsigned i=0;i<12;i++)seed_fixture(i,0,0x3f000000,1);load_fixture();
 pid_t child=fork();require(child>=0,"separate input process");
 if(!child){execl(argv[3],argv[3],argv[1],argv[2],NULL);_exit(127);}
 int status=0;
 for(;;){
  pid_t result=waitpid(child,&status,WNOHANG);require(result>=0,"wait input process");if(result)break;
  struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
  uint32_t tick=(now.tv_sec-fixture->origin_sec)*1000+((int64_t)now.tv_nsec-fixture->origin_nsec)/1000000;
  atomic_store_explicit(&fixture->heartbeat,tick,memory_order_release);
  Context c=context();c.r[0]=ptr(queue_object)+4;call(&c,COMMAND_DRAIN);command_retire();
  struct timespec delay={0,1000000};nanosleep(&delay,NULL);
 }
 require(WIFEXITED(status)&&WEXITSTATUS(status)==0,"actual input client process succeeds");
 require(dispatched_calls==20&&atomic_load(&command_state->slots[0].done)==20&&atomic_load(&command_state->reclaimed)==20&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error),"twenty held requests reached substituted native queue/body/commit/done");
 Context final=context();final.r[5]=ptr(program_objects[0])+0x5b0;final.d[8]=0x3f000000;call(&final,M_FLOAT);
 command_close();require(command_quiescent(),"composition closes after source completion");require(command_finalize(),"actual composition finalization");
 puts("PASS separate ARM input process -> actual CMD12 reusable publication/producer -> substituted native method and queued audio pthread -> MMV7 -> settled motor encoding");return 0;
}
