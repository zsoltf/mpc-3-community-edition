/* Actual CMD13 file/identity/consume path; synthetic producer header, real
 * short-lived child PID/start and shared file. No native MPC or device. */
#define main command_client_entry_not_called
#include "command-client.c"
#undef main
#include <sys/wait.h>
static void need(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 int fd=open(argv[1],O_CREAT|O_EXCL|O_RDWR,0600);need(fd>=0&&!ftruncate(fd,sizeof(CommandState)),"exclusive command fixture");
 CommandState *c=mmap(NULL,sizeof(*c),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);need(c!=MAP_FAILED,"fixture mapping");
 pid_t child=fork();need(child>=0,"actual child");if(!child){for(;;)pause();}
 uint64_t tick=command_process_start(child);need(tick!=0,"actual start time");
 c->magic=COMMAND_MAGIC;c->version=COMMAND_VERSION;c->bytes=sizeof(*c);c->capacity=COMMAND_SLOTS;c->owner_token=1;c->pid=child;c->start_lo=tick;c->start_hi=tick>>32;
 atomic_store(&c->new_project_intent,1);atomic_store(&c->published,1);atomic_store(&c->slots[0].published,1);atomic_store(&c->slots[0].returned,1);
 char pid[32],start[32];snprintf(pid,sizeof(pid),"%u",child);snprintf(start,sizeof(start),"%llu",(unsigned long long)tick);
 need(!new_project(argv[1],pid,start,0)&&new_project(argv[1],pid,start,1)==1,"live intent readable but never consumable");
 need(new_project(argv[1],pid,"1",0)==1,"stale start does not match");kill(child,SIGTERM);int status;need(waitpid(child,&status,0)==child,"actual child wait");
 atomic_store(&c->stop_requested,1);need(new_project(argv[1],pid,start,1)==1,"explicit source stop excludes consume");atomic_store(&c->stop_requested,0);
 atomic_store(&c->new_project_intent,3);need(new_project(argv[1],pid,start,1)==1,"other native shutdown excludes consume");atomic_store(&c->new_project_intent,1);
 CommandState *before=malloc(sizeof(*before));need(before!=NULL,"byte snapshot");memcpy(before,c,sizeof(*before));atomic_store(&before->new_project_intent,2);
 need(!new_project(argv[1],pid,start,1)&&!memcmp(before,c,sizeof(*c)),"dead exact intent consumes once without rewriting incomplete request or closure");
 need(new_project(argv[1],pid,start,1)==1,"consumed marker never reused");
 free(before);munmap(c,sizeof(*c));close(fd);unlink(argv[1]);puts("PASS actual intent client: exact PID/start, live/dead/stop/shutdown guards, one consume and otherwise byte-unchanged incomplete receipts; producer header substituted");return 0;
}
