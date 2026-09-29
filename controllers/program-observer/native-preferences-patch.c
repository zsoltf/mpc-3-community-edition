#define _GNU_SOURCE
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "native-preferences.h"
#include "native-preferences-pinned.h"

extern unsigned char gate_begin[],gate_end[],gate_handler[];
void native_preferences_set_state(NativePreferencesState *);
void native_preferences_configure_host(uint32_t);

static int exact_ranges(uint32_t bias){
 for(unsigned i=0;i<NATIVE_PREFERENCES_SITE_COUNT;i++)
  if(memcmp((void*)(uintptr_t)(bias+native_preferences_sites[i].address),native_preferences_sites[i].bytes,8))return 0;
 for(unsigned i=0;i<NATIVE_PREFERENCES_RANGE_COUNT;i++)
  if(memcmp((void*)(uintptr_t)(bias+native_preferences_ranges[i].address),native_preferences_ranges[i].bytes,native_preferences_ranges[i].length))return 0;
 for(unsigned i=0;i<NATIVE_PREFERENCES_POINTER_COUNT;i++)
  if(*(const uint32_t*)(uintptr_t)(bias+native_preferences_pointers[i].slot)!=bias+native_preferences_pointers[i].target)return 0;
 return 1;
}
static void jump(uint32_t **out,uint32_t target){*(*out)++=0xe51ff004u;*(*out)++=target;}

int native_preferences_initialize(void){
 int fd=open(NATIVE_PREFERENCES_STATE,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return 0;
 struct stat st={0};int ok=!posix_fallocate(fd,0,NATIVE_PREFERENCES_BYTES)&&!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size==NATIVE_PREFERENCES_BYTES;
 void *page=ok?mmap(NULL,NATIVE_PREFERENCES_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0):MAP_FAILED;close(fd);
 if(page==MAP_FAILED){unlink(NATIVE_PREFERENCES_STATE);return 0;}
 NativePreferencesState *state=page;memset(state,0,sizeof(*state));state->magic=NATIVE_PREFERENCES_MAGIC;state->version=NATIVE_PREFERENCES_VERSION;state->bytes=sizeof(*state);state->pid=(uint32_t)getpid();native_preferences_set_state(state);return 1;
}

int native_preferences_install(uint32_t bias,void **mapping){
 if(!mapping||!exact_ranges(bias))return 0;
 native_preferences_configure_host(bias);
 const size_t bytes=4096;unsigned char *code=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(code==MAP_FAILED)return 0;
 size_t gate=(size_t)(gate_end-gate_begin);unsigned char *cursor=code,*targets[NATIVE_PREFERENCES_SITE_COUNT];
 for(unsigned i=0;i<NATIVE_PREFERENCES_SITE_COUNT;i++){
  if((size_t)(cursor-code)+gate+16>bytes){munmap(code,bytes);return 0;}targets[i]=cursor;
  memcpy(cursor,gate_begin,gate);*(uint32_t*)(cursor+(gate_handler-gate_begin))=(uint32_t)(uintptr_t)&native_preferences_hook;*(uint32_t*)(cursor+(gate_handler-gate_begin)+4)=i;cursor+=gate;
  memcpy(cursor,native_preferences_sites[i].bytes,8);cursor+=8;
  uint32_t *out=(uint32_t*)cursor;jump(&out,bias+native_preferences_sites[i].address+8);cursor=(unsigned char*)out;
 }
 __builtin___clear_cache((char*)code,(char*)cursor);if(mprotect(code,bytes,PROT_READ|PROT_EXEC)){munmap(code,bytes);return 0;}
 uint32_t pages[NATIVE_PREFERENCES_SITE_COUNT]={};unsigned page_count=0;
 for(unsigned i=0;i<NATIVE_PREFERENCES_SITE_COUNT;i++){
  uint32_t page=(bias+native_preferences_sites[i].address)&~4095u;unsigned known=0;for(unsigned n=0;n<page_count;n++)known|=pages[n]==page;
  if(!known)pages[page_count++]=page;
 }
 unsigned writable=0;for(;writable<page_count;writable++)if(mprotect((void*)(uintptr_t)pages[writable],4096,PROT_READ|PROT_WRITE|PROT_EXEC))break;
 if(writable!=page_count){while(writable)mprotect((void*)(uintptr_t)pages[--writable],4096,PROT_READ|PROT_EXEC);munmap(code,bytes);return 0;}
 for(unsigned i=0;i<NATIVE_PREFERENCES_SITE_COUNT;i++){
  uint32_t *site=(uint32_t*)(uintptr_t)(bias+native_preferences_sites[i].address);jump(&site,(uint32_t)(uintptr_t)targets[i]);
  __builtin___clear_cache((char*)(uintptr_t)(bias+native_preferences_sites[i].address),(char*)(uintptr_t)(bias+native_preferences_sites[i].address+8));
 }
 for(unsigned i=0;i<page_count;i++)if(mprotect((void*)(uintptr_t)pages[i],4096,PROT_READ|PROT_EXEC))_exit(125);
 *mapping=code;return 1;
}
