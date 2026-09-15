/* Observer-owned coalesced volume state. Native addresses are opaque keys only.
 * Fixed cells are never reclaimed during a producer run. No record queue. */
#ifndef MPC_MIRROR_STATE_H
#define MPC_MIRROR_STATE_H
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include "channel-state.h"
#include "meter-state.h"
#include "pad-state.h"
#include "effects-state.h"
#include "qlink-state.h"
#include "io-state.h"
#define MIRROR_MAGIC 0x41564d4du /* MMV17: selected effects and optional Q-Link UI projection */
#define MIRROR_VERSION 17u
#define MIRROR_CELLS 2048u
#define MIRROR_HASH 4096u
#define MIRROR_PROBES 32u
#define MIRROR_TRACKS 128u
#define MIRROR_BANK 8u
#define MIRROR_SEED_NORMAL 43u
#define MIRROR_SEED_COPY 44u
#define MIRROR_PROPERTY_DESTROY 45u
enum {MV_OK,MV_CAPACITY,MV_PUBLICATION_BUSY,MV_PUBLICATION_LOSS,MV_LIFETIME,
 MV_TOPOLOGY,MV_ROOT,MV_LOAD,MV_WRAP,MV_SETUP};
typedef struct {
 _Atomic uint32_t guard,revision,live,property,program,incarnation,bits,updates,seed;
 unsigned char padding[2048-9*4];
} MirrorCell;
_Static_assert(sizeof(MirrorCell)==2048,"independent ARM exclusive granules");
typedef struct {
 _Atomic uint32_t serial,track,program,kind,incarnation,binding,vptr,track_owner,program_owner,meter;
} MirrorBinding;
#define MIRROR_POSITION_LANES 8u
#include "general-state.h"
typedef struct {_Atomic uint32_t revision,epoch,tick,token,bar,beat,clock;} MirrorPosition;
typedef struct {
 uint32_t magic,version,bytes,pid,origin_sec,origin_nsec,capacity,reserved;
 _Atomic uint32_t alive,error,heartbeat,allocated;
 unsigned char pad0[2048-48];
 _Atomic uint32_t birth_guard;
 unsigned char pad1[2048-4];
 _Atomic uint32_t topology_guard,revision,epoch,ready,count,generation;
 MirrorBinding tracks[MIRROR_TRACKS];
 unsigned char pad2[8192-24-MIRROR_TRACKS*sizeof(MirrorBinding)];
 _Atomic uint32_t directory[MIRROR_HASH];
 MirrorCell cells[MIRROR_CELLS];
 ChannelState channel;
 _Atomic uint32_t sequencer,position_error;
 MirrorPosition position[MIRROR_POSITION_LANES];
 MeterState meters;
 GeneralState general;
 PadState pads;
 EffectsState effects;
 QLinkState qlinks;
 IOState io;
} MirrorState;
_Static_assert(offsetof(MirrorState,cells)%2048==0,"cell alignment");
extern MirrorState *mirror_state;
void mirror_stop(unsigned);
void channel_initialize(void);
#endif
