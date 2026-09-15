#include "ui-witness-state.h"
static int ui_fd=-1;
static int ui_write(const void *data,size_t bytes){return write(ui_fd,data,bytes)==(ssize_t)bytes;}
/* Read the sticky violation after stop with SC ordering. Optional payload can
 * be absent; it never delays or determines a required ownership failure. */
static UiRecord ui_footer_record(uint32_t read_at,uint32_t now){
 uint32_t terminal=atomic_load_explicit(&ui_terminal,memory_order_seq_cst);
 uint32_t model_error=atomic_load_explicit(&mirror_state->error,memory_order_acquire),error=atomic_load_explicit(&ui_error,memory_order_acquire);
 if(terminal)error=terminal&255;else if(!error)error=model_error;
 UiRecord end={.seq=read_at,.kind=255,.site=error,.tick=now,.token=atomic_load(&ui_published),.lr=model_error,.sp=atomic_load_explicit(&ui_gap,memory_order_acquire),.r0=terminal};
 ui_footer_optional(&end);return end;
}
static void *consume(void *unused){
 (void)unused;uint32_t read_at=0,closed_at=0;int closing=0;
 for(;;){
  uint32_t now=elapsed();atomic_store_explicit(&ui_tick,now,memory_order_relaxed);
  if(now==UINT32_MAX){atomic_store(&ui_error,UI_CLOCK);atomic_store(&observer_running,0);}
  if(now>=WINDOW_SECONDS*1000u&&atomic_load_explicit(&observer_running,memory_order_acquire))observer_hook(NULL,UI_CLOSE);
  uint32_t published=atomic_load_explicit(&ui_published,memory_order_acquire);
  if(published>UI_RECORD_COUNT){atomic_store(&ui_error,UI_OVERFLOW);atomic_store(&observer_running,0);published=read_at;}
  while(read_at<published){if(!ui_write(ui_records+read_at,sizeof(UiRecord))){atomic_store(&ui_error,UI_IO);atomic_store(&observer_running,0);break;}read_at++;}
  if(!atomic_load_explicit(&observer_running,memory_order_acquire)){
   if(!closing){closed_at=now;closing=1;}
   if(ui_quiescent()&&read_at==atomic_load_explicit(&ui_published,memory_order_acquire))break;
   if(atomic_load(&ui_error)==UI_IO)break;
   if(now-closed_at>=1000){atomic_store(&ui_error,UI_CLOSING);break;}
  }
  struct timespec delay={0,10000000};nanosleep(&delay,NULL);
 }
 UiRecord end=ui_footer_record(read_at,elapsed());(void)ui_write(&end,sizeof(end));close(ui_fd);ui_fd=-1;return NULL;
}
static void child_inert(void){atomic_store(&observer_running,0);mirror_state=NULL;started=0;if(ui_fd>=0){close(ui_fd);ui_fd=-1;}}
__attribute__((constructor)) static void startup(void){
 const char *preload=getenv("LD_PRELOAD");if(!preload||strcmp(preload,OBSERVER_LIBRARY))return;if(unsetenv("LD_PRELOAD"))return;
 unsigned long hw=getauxval(AT_HWCAP);if(getauxval(AT_PAGESZ)!=4096||(hw&(HWCAP_NEON|HWCAP_VFPD32))!=(HWCAP_NEON|HWCAP_VFPD32)||!exact_file()||!only_thread())return;
 dl_iterate_phdr(image,NULL);if(found!=1||!load_bias||(load_bias&4095))return;
 ui_fd=open(OBSERVER_LOG,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK,0600);if(ui_fd<0)return;
 struct stat st;if(fstat(ui_fd,&st)||!S_ISREG(st.st_mode)){close(ui_fd);ui_fd=-1;return;}
 ui_initialize();
 if(clock_gettime(CLOCK_MONOTONIC,&began)||began.tv_sec<0||(uint64_t)began.tv_sec>UINT32_MAX){close(ui_fd);ui_fd=-1;return;}
 uint32_t header[32]={UI_MAGIC,UI_VERSION,(uint32_t)getpid(),load_bias,UI_RECORD_COUNT,WINDOW_SECONDS,sizeof(UiRecord),CONSUMER_STACK};memcpy(header+8,digest,64);memcpy(header+24,buildid,20);header[29]=(uint32_t)began.tv_sec;header[30]=(uint32_t)began.tv_nsec;header[31]=ui_owner_tp;
 if(!ui_write(header,sizeof(header))){close(ui_fd);ui_fd=-1;return;}
 void *stack=mmap(NULL,CONSUMER_STACK+4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0),*gates=NULL;pthread_attr_t attr;int attr_ok=0,code=0;
 if(stack==MAP_FAILED){code=errno;goto failure;}
 code=pthread_attr_init(&attr);if(code)goto failure;attr_ok=1;
 if(mprotect((char*)stack+4096,CONSUMER_STACK,PROT_READ|PROT_WRITE)){code=errno;goto failure;}
 code=pthread_attr_setstack(&attr,(char*)stack+4096,CONSUMER_STACK);if(code)goto failure;
 code=pthread_atfork(NULL,NULL,child_inert);if(code)goto failure;
 if(!only_thread())goto failure;
 observer_image_bias=load_bias;
 if(!install_patches(load_bias,-1,&gates))goto failure;
 atomic_store_explicit(&observer_running,1,memory_order_release);
 code=pthread_create(&consumer,&attr,consume,NULL);if(code)goto failure;
 pthread_attr_destroy(&attr);started=1;return;
failure:
 if(attr_ok)pthread_attr_destroy(&attr);
 atomic_store(&observer_running,0);atomic_store(&ui_error,MV_SETUP);
 UiRecord end={.kind=255,.site=MV_SETUP,.lr=(uint32_t)code};ui_footer_optional(&end);(void)ui_write(&end,sizeof(end));close(ui_fd);ui_fd=-1;
}
__attribute__((destructor)) static void finish(void){if(started){observer_hook(NULL,UI_CLOSE);pthread_join(consumer,NULL);}}
