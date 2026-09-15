#ifndef MPC_COMMAND_PROCESS_H
#define MPC_COMMAND_PROCESS_H
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef COMMAND_CLIENT
#include "sha256.h"
#endif
static uint64_t command_process_start(unsigned pid){
 char path[64],data[4096];snprintf(path,sizeof(path),"/proc/%u/stat",pid);int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return 0;
 ssize_t n=read(fd,data,sizeof(data)-1);close(fd);if(n<=0)return 0;data[n]=0;char *end=strrchr(data,')');if(!end||end[1]!=' ')return 0;
 char *save=NULL,*part=strtok_r(end+2," ",&save);for(unsigned field=3;part;field++,part=strtok_r(NULL," ",&save))if(field==22){char *tail;errno=0;unsigned long long value=strtoull(part,&tail,10);return !errno&&!*tail?value:0;}return 0;
}
#ifdef COMMAND_CLIENT
static int command_same_file(const struct stat *a,const struct stat *b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size;}
/* Read-only polling checks process/file identity without rereading the binary.
 * Full admission and mutations still require the pinned content hash. */
static int command_process_file(unsigned pid,int full_hash){
 char path[64],target[128];snprintf(path,sizeof(path),"/proc/%u/exe",pid);ssize_t n=readlink(path,target,sizeof(target)-1);if(n<0)return 0;target[n]=0;if(strcmp(target,"/usr/bin/MPC"))return 0;
 int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return 0;struct stat before,after,installed,live;
 int ok=!fstat(fd,&before)&&S_ISREG(before.st_mode)&&before.st_size==112222004&&!stat("/usr/bin/MPC",&installed)&&S_ISREG(installed.st_mode)&&command_same_file(&before,&installed);
 if(ok&&full_hash){
  Sha sha;sha_init(&sha);unsigned char bytes[65536],sum[32];size_t total=0;
  while(ok){ssize_t got=read(fd,bytes,sizeof(bytes));if(got<0){ok=0;break;}if(!got)break;total+=(size_t)got;if(total>112222004){ok=0;break;}sha_add(&sha,bytes,(size_t)got);}
  sha_end(&sha,sum);char hex[65];for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",sum[i]);
  ok=ok&&total==112222004&&!strcmp(hex,"bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426");
 }
 ok=ok&&!fstat(fd,&after)&&command_same_file(&before,&after)&&!stat(path,&live)&&S_ISREG(live.st_mode)&&command_same_file(&before,&live)&&!stat("/usr/bin/MPC",&installed)&&S_ISREG(installed.st_mode)&&command_same_file(&before,&installed);
 n=readlink(path,target,sizeof(target)-1);if(n<0)ok=0;else{target[n]=0;if(strcmp(target,"/usr/bin/MPC"))ok=0;}
 close(fd);return ok;
}
static int command_process_exact(unsigned pid){return command_process_file(pid,1);}
#endif
#endif
