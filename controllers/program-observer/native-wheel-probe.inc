/* Temporary event-only observation. No input ownership or native mutation. */
#include <errno.h>
#include <time.h>
#include "native-wheel-probe.h"
__attribute__((used,visibility("hidden"))) NativeWheelProbe native_wheel_probe;
static AdmissionStorage native_wheel_probe_guard __attribute__((aligned(2048)));
#ifdef COMMAND_COMPONENT
static int (*component_probe_clock)(struct timespec *);
#endif
static int native_wheel_probe_clock(struct timespec *t){
#ifdef COMMAND_COMPONENT
 if(component_probe_clock)return component_probe_clock(t);
#endif
 return clock_gettime(CLOCK_MONOTONIC,t);
}
static void native_wheel_probe_initialize(void){
 NativeWheelProbe *p=&native_wheel_probe;
 memset(p,0,sizeof(*p));atomic_store(&native_wheel_probe_guard.busy,0);
 p->magic=NWP_MAGIC;p->version=NWP_VERSION;p->bytes=sizeof(*p);p->capacity=NWP_CAPACITY;p->seconds=NWP_SECONDS;
 struct timespec t;
 if(native_wheel_probe_clock(&t)||t.tv_sec<0||(uint64_t)t.tv_sec>UINT32_MAX){atomic_store(&p->closed,NWP_CLOCK);return;}
 p->start_sec=(uint32_t)t.tv_sec;p->start_nsec=(uint32_t)t.tv_nsec;
}
static void native_wheel_probe_capture(const Context *c){
 NativeWheelProbe *p=&native_wheel_probe;
 if(atomic_load_explicit(&p->closed,memory_order_acquire)||p->magic!=NWP_MAGIC)return;
 unsigned admission=claim_result(&native_wheel_probe_guard.busy);
 if(admission){atomic_store_explicit(admission==2?&p->busy_loss:&p->reservation_loss,1,memory_order_release);return;}
 int saved_errno=errno;
 if(atomic_load_explicit(&p->closed,memory_order_relaxed))goto out;
 struct timespec t;
 if(native_wheel_probe_clock(&t)||t.tv_sec<0||(uint64_t)t.tv_sec>UINT32_MAX){atomic_store_explicit(&p->closed,NWP_CLOCK,memory_order_release);goto out;}
 int64_t elapsed_ns=((int64_t)t.tv_sec-p->start_sec)*1000000000ll+t.tv_nsec-p->start_nsec;
 if(elapsed_ns<0){atomic_store_explicit(&p->closed,NWP_CLOCK,memory_order_release);goto out;}
 if(elapsed_ns>=(int64_t)NWP_SECONDS*1000000000ll){atomic_store_explicit(&p->closed,NWP_EXPIRED,memory_order_release);goto out;}
 unsigned at=atomic_load_explicit(&p->published,memory_order_relaxed);
 if(at>=NWP_CAPACITY){atomic_store_explicit(&p->closed,NWP_CAP,memory_order_release);goto out;}
 NativeWheelProbeEvent e={0};e.sequence=at+1;e.sec=(uint32_t)t.tv_sec;e.nsec=(uint32_t)t.tv_nsec;
 for(unsigned i=0;i<4;i++)e.r[i]=c->r[i];
 e.thread=token();e.owner=command_owner;
 e.running=atomic_load_explicit(&observer_running,memory_order_relaxed);
 e.in_hook=atomic_load_explicit(&command_in_hook,memory_order_relaxed);
 e.heartbeat=mirror_state?atomic_load_explicit(&mirror_state->heartbeat,memory_order_relaxed):UINT32_MAX;
 e.error=command_state?atomic_load_explicit(&command_state->error,memory_order_relaxed):UINT32_MAX;
 e.trace_error=command_state?atomic_load_explicit(&command_state->trace_error,memory_order_relaxed):UINT32_MAX;
 if(e.thread==e.owner)e.flags|=NWP_OWNER;
 if(c->r[3]==observer_image_bias+WHEEL_ROTATE_RVA)e.flags|=NWP_CALLEE;
 /* Foreign calls remain evidence, but never authorize a native pointer read.
  * This is shallow owner-thread focus qualification, not an item inventory. */
 if((e.flags&(NWP_OWNER|NWP_CALLEE))==(NWP_OWNER|NWP_CALLEE)&&e.running&&
    pointer(c->r[0])&&c->r[0]<=UINT32_MAX-(WHEEL_FOCUSED_OFFSET+4u)&&
    word(observer_image_bias+FOCUS_SLOT_RVA)==c->r[0]){
  e.controller_vptr=word(c->r[0]);
  for(unsigned i=0;i<4;i++)if(e.controller_vptr==observer_image_bias+wheel_focus_vtables[i])e.flags|=NWP_CONTROLLER;
  if(e.flags&NWP_CONTROLLER){
   e.focused=word(c->r[0]+WHEEL_FOCUSED_OFFSET);
   if(pointer(e.focused)&&e.focused<=UINT32_MAX-(WHEEL_FLAGS_OFFSET+4u)){
    e.focus_vptr=word(e.focused);e.focus_flags=word(e.focused+WHEEL_FLAGS_OFFSET);e.flags|=NWP_FOCUS;
    if(e.focus_vptr==observer_image_bias+NOTE_GRID_VTABLE)e.flags|=NWP_GRID;
    if(e.focus_flags&WHEEL_VISIBLE_FLAG)e.flags|=NWP_VISIBLE;
   }
  }
 }
 p->events[at]=e;atomic_store_explicit(&p->published,at+1,memory_order_release);
 if(at+1==NWP_CAPACITY)atomic_store_explicit(&p->closed,NWP_CAP,memory_order_release);
out:
 errno=saved_errno;release(&native_wheel_probe_guard.busy);
}
