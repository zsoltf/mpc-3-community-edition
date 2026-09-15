/* Included by observer.c only in mirror mode; exact startup validation and
 * installer remain shared with the diagnostic. Runtime has no finite queue. */
static void *consume(void *unused){
 (void)unused;
 while(atomic_load_explicit(&observer_running,memory_order_acquire)){
  uint32_t now=elapsed();
  if(now==UINT32_MAX){mirror_stop(MV_SETUP);break;}
  atomic_store_explicit(&mirror_state->heartbeat,now,memory_order_release);
  struct timespec delay={0,10000000};nanosleep(&delay,NULL);
 }
 mirror_stop(0);return NULL;
}
static void child_inert(void){
 /* MAP_SHARED must not be modified by an inherited child. */
 mirror_state=NULL;atomic_store_explicit(&observer_running,0,memory_order_release);started=0;
}
__attribute__((constructor)) static void startup(void){
 const char *preload=getenv("LD_PRELOAD");if(!preload||strcmp(preload,OBSERVER_LIBRARY))return;
 if(unsetenv("LD_PRELOAD"))return;
 unsigned long hw=getauxval(AT_HWCAP);
 if(getauxval(AT_PAGESZ)!=4096||(hw&(HWCAP_NEON|HWCAP_VFPD32))!=(HWCAP_NEON|HWCAP_VFPD32))return;
 if(!exact_file()||!only_thread())return;
 dl_iterate_phdr(image,NULL);if(found!=1||!load_bias||(load_bias&4095))return;
 int fd=open(OBSERVER_LOG,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return;
 struct stat st;int allocation=posix_fallocate(fd,0,sizeof(MirrorState));
 if(allocation||fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);return;}
 void *state=mmap(NULL,sizeof(MirrorState),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);
 if(state==MAP_FAILED)return;
 memset(state,0,sizeof(MirrorState));mirror_state=state;channel_initialize();
 mirror_state->magic=MIRROR_MAGIC;mirror_state->version=MIRROR_VERSION;mirror_state->bytes=sizeof(MirrorState);mirror_state->pid=(uint32_t)getpid();mirror_state->capacity=MIRROR_CELLS;
 if(clock_gettime(CLOCK_MONOTONIC,&began)||began.tv_sec<0||(uint64_t)began.tv_sec>UINT32_MAX){mirror_stop(MV_SETUP);return;}
 mirror_state->origin_sec=(uint32_t)began.tv_sec;mirror_state->origin_nsec=(uint32_t)began.tv_nsec;
 void *stack=mmap(NULL,CONSUMER_STACK+4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0),*gates=NULL;
 pthread_attr_t attr;int attr_ok=0;
 if(stack==MAP_FAILED)goto failure;
 if(pthread_attr_init(&attr))goto failure;
 attr_ok=1;
 if(mprotect((char*)stack+4096,CONSUMER_STACK,PROT_READ|PROT_WRITE)||pthread_attr_setstack(&attr,(char*)stack+4096,CONSUMER_STACK))goto failure;
 if(pthread_atfork(NULL,NULL,child_inert)||!only_thread())goto failure;
 observer_image_bias=load_bias;
 if(!install_patches(load_bias,-1,&gates))goto failure;
 atomic_store_explicit(&observer_running,1,memory_order_release);
 atomic_store_explicit(&mirror_state->alive,1,memory_order_release);
 if(pthread_create(&consumer,&attr,consume,NULL))goto failure;
 pthread_attr_destroy(&attr);started=1;return;
failure:
 if(attr_ok)pthread_attr_destroy(&attr);
 atomic_store_explicit(&observer_running,0,memory_order_release);mirror_stop(MV_SETUP);
}
__attribute__((destructor)) static void finish(void){
 if(started){atomic_store_explicit(&observer_running,0,memory_order_release);pthread_join(consumer,NULL);}
 /* No observer cell/gate/mapping reclamation until process exit. */
}
