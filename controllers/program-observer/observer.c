#define _GNU_SOURCE
#include "observer.h"
#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/auxv.h>
#include <asm/hwcap.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "sha256.h"
#include "config.h"
extern _Atomic uint32_t observer_error,observer_published,observer_tick,observer_armed;
extern Record observer_records[RECORD_COUNT];
int install_patches(uint32_t,int,void**);
static const char digest[]="bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426";
static const unsigned char buildid[20]={0x83,0x71,0x19,0x9f,0x94,0x5e,0xcc,0x40,0xf7,0x9c,0xb1,0x9c,0x17,0xff,0x5d,0xe7,0x28,0x10,0xa9,0x1c};
static uint32_t load_bias;static int found,started;static pthread_t consumer;
#ifndef VOLUME_MIRROR
static int logfd=-1;
#endif
static struct timespec began;
static uint32_t elapsed(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return UINT32_MAX;int64_t n=(t.tv_sec-began.tv_sec)*1000+(t.tv_nsec-began.tv_nsec)/1000000;return n<0?UINT32_MAX:(uint32_t)n;}
static int image(struct dl_phdr_info *i,size_t size,void *unused){
 (void)size;(void)unused;if(i->dlpi_name && *i->dlpi_name)return 0;
 int note=0,load=0;
 for(unsigned j=0;j<i->dlpi_phnum;j++){
  const ElfW(Phdr)*p=i->dlpi_phdr+j;
  if(p->p_type==PT_LOAD && p->p_offset==0 && p->p_vaddr==0 && (p->p_flags&(PF_R|PF_X))==(PF_R|PF_X))load++;
  if(p->p_type==PT_NOTE && p->p_memsz<=4096){
   const unsigned char *s=(const unsigned char*)(i->dlpi_addr+p->p_vaddr),*end=s+p->p_memsz;
   while(s+12<=end){ElfW(Nhdr) h;memcpy(&h,s,12);s+=12;unsigned n=(h.n_namesz+3)&~3u,d=(h.n_descsz+3)&~3u;if(n>4096||d>4096||s+n+d>end)break;
    if(h.n_type==NT_GNU_BUILD_ID && h.n_namesz==4 && h.n_descsz==20 && !memcmp(s,"GNU",4)&&!memcmp(s+n,buildid,20))note++;
    s+=n+d;
   }
  }
 }
 if(load==1&&note==1){load_bias=(uint32_t)i->dlpi_addr;found++;}return 0;
}
static int exact_file(void){
 char path[128];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);if(n<0)return 0;path[n]=0;if(strcmp(path,"/usr/bin/MPC"))return 0;
 int fd=open("/proc/self/exe",O_RDONLY|O_CLOEXEC);if(fd<0)return 0;struct stat before={0},after={0};int ok=!fstat(fd,&before)&&S_ISREG(before.st_mode)&&before.st_size==112222004;
 Sha sha;sha_init(&sha);unsigned char bytes[65536];off_t total=0;
 while(ok){ssize_t got=read(fd,bytes,sizeof(bytes));if(got<0){ok=0;break;}if(!got)break;total+=got;if(total>112222004){ok=0;break;}sha_add(&sha,bytes,(size_t)got);}
 if(fstat(fd,&after)||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||before.st_size!=after.st_size||before.st_mtim.tv_sec!=after.st_mtim.tv_sec||before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec)ok=0;
 close(fd);unsigned char hash[32];sha_end(&sha,hash);char hex[65];for(unsigned j=0;j<32;j++)snprintf(hex+2*j,3,"%02x",hash[j]);return ok&&total==112222004&&!strcmp(hex,digest);
}
static int only_thread(void){DIR *d=opendir("/proc/self/task");if(!d)return 0;unsigned n=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]>='0'&&e->d_name[0]<='9')n++;closedir(d);return n==1;}
#ifdef VOLUME_MIRROR
#if defined(MIRROR_COMMAND)
#include "command-runtime.inc"
#elif defined(UI_WITNESS)
#include "ui-witness-runtime.inc"
#else
#include "mirror-runtime.inc"
#endif
#else
/* Bounded record-sized writes to an exclusive regular local file. Kernel I/O
 * latency is not made realtime by O_NONBLOCK; root owns the process timeout. */
static int output(const void *p,size_t n){return write(logfd,p,n)==(ssize_t)n;}
/* Only the consumer opens this exact one-shot setup marker. No MPC object read. */
static int arm_marker(void){
 int fd=open(OBSERVER_ARM_PATH,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0)return errno==ENOENT?0:-1;
 struct stat st={0},again={0};char token[5];
 int ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size==4&&read(fd,token,5)==4&&!memcmp(token,"ARM\n",4);
 if(fstat(fd,&again)||st.st_ino!=again.st_ino||st.st_dev!=again.st_dev||again.st_size!=4)ok=0;
 close(fd);return ok?1:-1;
}
/* Zero is a valid first millisecond, never a sentinel. A separate flag keeps
 * the one-second close budget intact even when publication races the first drain. */
static int close_expired(uint32_t now,int *closing,uint32_t *closed_at){
 if(!*closing){*closing=1;*closed_at=now;}
 return now-*closed_at>=1000;
}
static void *consume(void *unused){
 (void)unused;unsigned read_at=0;uint32_t closed_at=0,last_arm_check=0,last_heartbeat=0;int closing=0;
 for(;;){
  uint32_t now=elapsed();atomic_store_explicit(&observer_tick,now,memory_order_relaxed);
  if(now==UINT32_MAX){atomic_store(&observer_error,CLOCK_ERROR);atomic_store(&observer_running,0);}
  if(atomic_load(&observer_running)&&!atomic_load(&observer_armed)&&now-last_arm_check>=100){
   last_arm_check=now;int request=arm_marker();
   if(request<0){atomic_store(&observer_error,IO);atomic_store(&observer_running,0);}
   else if(request)observer_hook(NULL,ARMED);
  }
  if(atomic_load(&observer_running)&&now-last_heartbeat>=200){last_heartbeat=now;observer_hook(NULL,HEARTBEAT);}
  if(now>=WINDOW_SECONDS*1000u){
#ifdef MODEL_CAPTURE
   /* Model transitions need an explicit finite-close verdict before footer. */
   if(atomic_load_explicit(&observer_running,memory_order_acquire))observer_hook(NULL,MODEL_FINITE_CLOSE);
#endif
   atomic_store_explicit(&observer_running,0,memory_order_release);
  }
  unsigned published=atomic_load_explicit(&observer_published,memory_order_acquire);
  if(published>RECORD_COUNT){atomic_store(&observer_error,ORDER);atomic_store(&observer_running,0);published=read_at;}
  while(read_at<published){if(!output(observer_records+read_at,sizeof(Record))){atomic_store(&observer_error,IO);atomic_store(&observer_running,0);break;}read_at++;}
  if(!atomic_load_explicit(&observer_running,memory_order_acquire)){
   int expired=close_expired(now,&closing,&closed_at);
   if(!atomic_load_explicit(&observer_busy,memory_order_acquire)){
    if(read_at==atomic_load_explicit(&observer_published,memory_order_acquire)||atomic_load(&observer_error)==IO)break;
   }
   if(expired){atomic_store(&observer_error,CLOSING_WRITER);break;}
  }
  struct timespec delay={0,10000000};nanosleep(&delay,NULL);
 }
 Record end={read_at,255,0,0,0,atomic_load(&observer_error),atomic_load(&observer_published),elapsed()};
 (void)output(&end,sizeof(end));close(logfd);logfd=-1;return NULL;
}
/* fork inherits mappings but not the consumer thread; its copy must stay inert.
 * No model dereference and no attempt to unload executable detours in the child. */
/* Setup-only fields: epoch=stage, generation=direct error code. No callback I/O. */
static void setup_footer(unsigned stage,int code){
 atomic_store(&observer_running,0);atomic_store(&observer_error,SETUP);
 Record end={0,255,stage,(uint32_t)code,0,SETUP,0,0};
 (void)output(&end,sizeof(end));close(logfd);logfd=-1;
}
static void child_inert(void){
 atomic_store_explicit(&observer_running,0,memory_order_release);
 atomic_store_explicit(&observer_pair,0,memory_order_release);started=0;
 if(logfd>=0){close(logfd);logfd=-1;}
}
__attribute__((constructor)) static void startup(void){
 /* Consume only this exact one-off preload entry, before MPC can exec children.
  * Never remove or rewrite an unrelated preload list. Other entry forms inert. */
 const char *preload=getenv("LD_PRELOAD");
 if(!preload||strcmp(preload,OBSERVER_LIBRARY))return;
 if(unsetenv("LD_PRELOAD"))return;
 /* Refuse unsupported entry identity before creating output or patching. */
 unsigned long hw=getauxval(AT_HWCAP);
 if(getauxval(AT_PAGESZ)!=4096 || (hw&(HWCAP_NEON|HWCAP_VFPD32))!=(HWCAP_NEON|HWCAP_VFPD32))return;
 if(!exact_file()||!only_thread())return;
 dl_iterate_phdr(image,NULL);if(found!=1||!load_bias||(load_bias&4095))return;
 struct stat marker_stat;if(!lstat(OBSERVER_ARM_PATH,&marker_stat)||errno!=ENOENT)return;
 logfd=open(OBSERVER_LOG,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK,0600);if(logfd<0)return;
 struct stat st;if(fstat(logfd,&st)||!S_ISREG(st.st_mode)){close(logfd);logfd=-1;return;}
 if(clock_gettime(CLOCK_MONOTONIC,&began)||began.tv_sec<0||(uint64_t)began.tv_sec>UINT32_MAX){close(logfd);logfd=-1;return;}
 uint32_t header[32]={STREAM_MAGIC,STREAM_VERSION,(uint32_t)getpid(),load_bias,RECORD_COUNT,WINDOW_SECONDS,sizeof(Record),CONSUMER_STACK};
 memcpy(header+8,digest,64);memcpy(header+24,buildid,20);header[29]=(uint32_t)began.tv_sec;header[30]=(uint32_t)began.tv_nsec;
 if(!output(header,sizeof(header))){close(logfd);logfd=-1;return;}
 void *stack=mmap(NULL,CONSUMER_STACK+4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0),*gates=NULL;
 pthread_attr_t attr;int attr_ok=0,code=0;unsigned stage=SETUP_MMAP;
 if(stack==MAP_FAILED){code=errno;goto setup_failed;}
 stage=SETUP_ATTR;code=pthread_attr_init(&attr);if(code)goto setup_failed;attr_ok=1;
 stage=SETUP_PROTECT;if(mprotect((char*)stack+4096,CONSUMER_STACK,PROT_READ|PROT_WRITE)){code=errno;goto setup_failed;}
 stage=SETUP_STACK;code=pthread_attr_setstack(&attr,(char*)stack+4096,CONSUMER_STACK);if(code)goto setup_failed;
 stage=SETUP_ATFORK;code=pthread_atfork(NULL,NULL,child_inert);if(code)goto setup_failed;
 stage=SETUP_THREADS;if(!only_thread())goto setup_failed;
 observer_image_bias=load_bias;
 stage=SETUP_PATCHES;if(!install_patches(load_bias,-1,&gates))goto setup_failed;
 stage=SETUP_CREATE;atomic_store_explicit(&observer_running,1,memory_order_release);
 code=pthread_create(&consumer,&attr,consume,NULL);if(code)goto setup_failed;
 pthread_attr_destroy(&attr);
 started=1;return;
setup_failed:
 if(attr_ok)pthread_attr_destroy(&attr);
 setup_footer(stage,code);/* Installed detours, if any, stay mapped and inert. */
}
__attribute__((destructor)) static void finish(void){
 if(started){atomic_store_explicit(&observer_running,0,memory_order_release);pthread_join(consumer,NULL);}
 /* All storage/gates/stack intentionally stay mapped until process exit. */
}

#endif /* finite diagnostic / retained mirror */
