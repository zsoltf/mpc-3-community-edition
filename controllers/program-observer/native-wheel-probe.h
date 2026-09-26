#ifndef NATIVE_WHEEL_PROBE_H
#define NATIVE_WHEEL_PROBE_H
#include <stdint.h>
#include <stdatomic.h>
#define NWP_MAGIC 0x3150574eu
#define NWP_VERSION 1u
#define NWP_CAPACITY 128u
#define NWP_SECONDS 180u
#define NWP_SITE 302u
enum {NWP_OPEN=0,NWP_CAP,NWP_EXPIRED,NWP_CLOCK,NWP_UNAVAILABLE};
enum {NWP_OWNER=1u,NWP_CALLEE=2u,NWP_CONTROLLER=4u,NWP_FOCUS=8u,NWP_GRID=16u,NWP_VISIBLE=32u};
typedef struct {
 uint32_t sequence,sec,nsec,r[4],thread,owner,heartbeat,flags;
 uint32_t controller_vptr,focused,focus_vptr,focus_flags;
 uint32_t running,in_hook,error,trace_error;
} NativeWheelProbeEvent;
typedef struct {
 uint32_t magic,version,bytes,capacity,seconds,start_sec,start_nsec;
 _Atomic uint32_t published,closed,busy_loss,reservation_loss;
 NativeWheelProbeEvent events[NWP_CAPACITY];
} NativeWheelProbe;
_Static_assert(sizeof(NativeWheelProbeEvent)==76,"fixed probe event");
_Static_assert(sizeof(NativeWheelProbe)==9772,"fixed probe object");
/* Reader output envelope, followed by exactly one NativeWheelProbe. */
typedef struct {
 uint32_t magic,version,pid,start_lo,start_hi,bias,rva,bytes,sec,nsec;
 uint32_t device_major,device_minor,inode_lo,inode_hi;
 char observer_sha[64];
} NativeWheelProbeRead;
#endif
