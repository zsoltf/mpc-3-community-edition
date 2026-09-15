#define _GNU_SOURCE
#include "observer.h"
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "pinned.h"
#define STUB_BYTES 512u
#define STUB_MAPPING_BYTES ((PATCH_COUNT*STUB_BYTES+4095u)&~4095u)
#if defined(MODEL_CAPTURE) || defined(VOLUME_MIRROR)
#include "model-patch.inc"
#else
static const unsigned patch_kind[PATCH_COUNT]={OWNER,BINDER,INITIAL,CHANGED,DISCONNECT,PROJECT,PROJECT,METADATA};
static void jump(uint32_t **p,uint32_t to){*(*p)++=0xe51ff004u;*(*p)++=to;}
static void mov32(uint32_t **p,unsigned reg,uint32_t n){*(*p)++=0xe3000000u|(reg<<12)|((n&0xf000)<<4)|(n&0xfff);*(*p)++=0xe3400000u|(reg<<12)|((n&0xf0000000)>>12)|((n>>16)&0xfff);}
static size_t build_stub(unsigned char *dest,unsigned site,uint32_t bias){
 unsigned kind=patch_kind[site];unsigned char *origin=dest;
 /* Initial VLDR must execute before capture: record the exact delivered s0,
  * not a second unsynchronized read of Property+28. Both displaced operations
  * preserve flags and execute exactly once on matched and filtered paths. */
 if(kind==INITIAL){memcpy(dest,expected[site],8);dest+=8;}
 unsigned char *start=dest;
 if(kind==CHANGED || kind==INITIAL || kind==BINDER || kind==DISCONNECT){
  size_t n=(size_t)(fast_end-fast_begin);memcpy(dest,fast_begin,n);
  *(uint32_t*)(dest+(fast_running-fast_begin))=(uint32_t)(uintptr_t)&observer_running;
  *(uint32_t*)(dest+(fast_filter-fast_begin))=(uint32_t)(uintptr_t)((kind==BINDER || kind==DISCONNECT)?observer_wrappers:observer_filter);
  if(kind==BINDER || kind==DISCONNECT)*(uint32_t*)(dest+(fast_target-fast_begin))=0xe59d3000; /* saved original r0 */
  dest+=n;
 }
 size_t n=(size_t)(gate_end-gate_begin);memcpy(dest,gate_begin,n);
 *(uint32_t*)(dest+(gate_handler-gate_begin))=(uint32_t)(uintptr_t)&observer_hook;
 *(uint32_t*)(dest+(gate_handler-gate_begin)+4)=kind;dest+=n;
 if(kind==CHANGED || kind==INITIAL || kind==BINDER || kind==DISCONNECT)*(uint32_t*)(start+(fast_skip-fast_begin))=(uint32_t)(uintptr_t)dest;
 uint32_t *out=(uint32_t*)dest;
 if(site==P_PROJECT_FALLBACK){mov32(&out,3,project_literal);*out++=0xe16d42f4;}
 else if(kind!=INITIAL){memcpy(out,expected[site],8);out+=2;}
 jump(&out,bias+anchor[site]+8);
 return (unsigned char*)out-origin;
}
#endif
/* Called only in pre-main single-thread startup. Consumer starts afterward.
 * Test-only fail_after exercises this exact transaction's rollback. */
int install_patches(uint32_t bias,int fail_after,void **mapping){
 for(unsigned i=0;i<sizeof(guards)/sizeof(guards[0]);i++)if(memcmp((void*)(uintptr_t)(bias+guards[i].address),guards[i].bytes,guards[i].length))return 0;
 for(unsigned i=0;i<PATCH_COUNT;i++)if(memcmp((void*)(uintptr_t)(bias+anchor[i]),expected[i],8))return 0;
 unsigned char *code=mmap(NULL,STUB_MAPPING_BYTES,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(code==MAP_FAILED)return 0;
 for(unsigned i=0;i<PATCH_COUNT;i++)if(build_stub(code+i*STUB_BYTES,i,bias)>STUB_BYTES){munmap(code,STUB_MAPPING_BYTES);return 0;}
 __builtin___clear_cache((char*)code,(char*)code+STUB_MAPPING_BYTES);
 if(mprotect(code,STUB_MAPPING_BYTES,PROT_READ|PROT_EXEC)){munmap(code,STUB_MAPPING_BYTES);return 0;}
 uint32_t pages[PATCH_COUNT];unsigned count=0,changed=0;
 for(unsigned i=0;i<PATCH_COUNT;i++){uint32_t page=(bias+anchor[i])&~4095u;unsigned j;for(j=0;j<count;j++)if(pages[j]==page)break;if(j==count)pages[count++]=page;}
 for(unsigned i=0;i<count;i++)if(mprotect((void*)(uintptr_t)pages[i],4096,PROT_READ|PROT_WRITE|PROT_EXEC))goto rollback;
 for(unsigned i=0;i<PATCH_COUNT;i++){
  if(fail_after==(int)i)goto rollback;
  uint32_t *p=(uint32_t*)(uintptr_t)(bias+anchor[i]);jump(&p,(uint32_t)(uintptr_t)(code+i*STUB_BYTES));changed++;
  __builtin___clear_cache((char*)(uintptr_t)(bias+anchor[i]),(char*)(uintptr_t)(bias+anchor[i]+8));
 }
 for(unsigned i=0;i<count;i++)if(mprotect((void*)(uintptr_t)pages[i],4096,PROT_READ|PROT_EXEC))goto rollback;
 *mapping=code;return 1;
rollback:
 /* A rollback protection failure must stop startup, never leave partial hooks. */
 for(unsigned i=0;i<count;i++)if(mprotect((void*)(uintptr_t)pages[i],4096,PROT_READ|PROT_WRITE|PROT_EXEC))_exit(125);
 for(unsigned i=0;i<changed;i++){memcpy((void*)(uintptr_t)(bias+anchor[i]),expected[i],8);__builtin___clear_cache((char*)(uintptr_t)(bias+anchor[i]),(char*)(uintptr_t)(bias+anchor[i]+8));}
 for(unsigned i=0;i<count;i++)if(mprotect((void*)(uintptr_t)pages[i],4096,PROT_READ|PROT_EXEC))_exit(125);
 munmap(code,STUB_MAPPING_BYTES);return 0;
}
