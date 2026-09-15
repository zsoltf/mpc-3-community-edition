/* Container-only process identity test. Real Linux proc/stat/exec and file hash;
 * /usr/bin/MPC is a disposable, padded test executable, not native MPC. */
#define _GNU_SOURCE
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
static uint64_t read_bytes;
static ssize_t counted_read(int fd,void *data,size_t size){ssize_t n=read(fd,data,size);if(n>0)read_bytes+=(uint64_t)n;return n;}
#define read counted_read
#define COMMAND_CLIENT
#include "command-process.h"
#undef read
static void need(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static pid_t launch(void){
 int pipefd[2];need(!pipe(pipefd),"pipe");pid_t pid=fork();need(pid>=0,"fork");
 if(!pid){close(pipefd[0]);char fd[32];snprintf(fd,sizeof(fd),"%d",pipefd[1]);execl("/usr/bin/MPC","MPC","hold",fd,(char*)0);_exit(2);}
 close(pipefd[1]);char ready;need(read(pipefd[0],&ready,1)==1,"child exec ready");close(pipefd[0]);return pid;
}
static void finish(pid_t pid){need(!kill(pid,SIGTERM),"child stop");int status;need(waitpid(pid,&status,0)==pid,"exact child wait");}
int main(int argc,char **argv){
 if(argc==3&&!strcmp(argv[1],"hold")){int fd=atoi(argv[2]);if(write(fd,"1",1)!=1)return 2;close(fd);for(;;)pause();}
 need(!access("/.dockerenv",F_OK)&&geteuid()==0,"disposable root container only");
 int source=open("/proc/self/exe",O_RDONLY),target=open("/usr/bin/MPC",O_CREAT|O_EXCL|O_RDWR,0700);need(source>=0&&target>=0,"exclusive test executable");
 char bytes[65536];ssize_t n;while((n=read(source,bytes,sizeof(bytes)))>0)need(write(target,bytes,(size_t)n)==n,"copy test executable");need(n==0&&!ftruncate(target,112222004),"pinned-size substitute");close(source);close(target);
 pid_t pid=launch();uint64_t start=command_process_start(pid);need(start&&start==command_process_start(pid),"actual PID start identity");
 read_bytes=0;need(command_process_file(pid,0)&&read_bytes==0,"read-only identity accepts exact path/inode/size without reading executable content");
 read_bytes=0;need(!command_process_exact(pid)&&read_bytes==112222004,"full admission still hashes all bytes and rejects non-pinned content");
 need(!command_process_file(getpid(),0),"different executable path rejected");
 need(!rename("/usr/bin/MPC","/usr/bin/MPC.old"),"rename fixture executable");need(!command_process_file(pid,0),"renamed or replaced installed executable rejected");
 finish(pid);need(!command_process_start(pid)&&!command_process_file(pid,0),"departed PID rejected");need(!rename("/usr/bin/MPC.old","/usr/bin/MPC"),"restore fixture path");
 target=open("/usr/bin/MPC",O_RDWR);need(target>=0&&!ftruncate(target,112222003),"wrong-size executable fixture");close(target);pid=launch();read_bytes=0;need(!command_process_file(pid,0)&&!read_bytes,"wrong executable size rejected without hashing");finish(pid);need(!unlink("/usr/bin/MPC"),"fixture executable cleanup");
 puts("PASS real Linux process identity: exact path/installed inode/size and PID lifecycle; read-only zero content bytes, full mode112222004 bytes and wrong-hash rejection; native MPC replaced by padded test executable");return 0;
}
