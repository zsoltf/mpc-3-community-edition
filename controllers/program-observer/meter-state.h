#ifndef MPC_METER_STATE_H
#define MPC_METER_STATE_H
/* Source-lane envelope observations. Native peak/reset words are not an atomic
 * stereo transaction; these records preserve each source's completed copy. */
#define METER_CELLS 2048u
#define METER_LANES 8u
#define METER_HASH 4096u
enum {ME_OK,ME_CAPACITY,ME_IDENTITY,ME_VALUE,ME_CLOSE};
enum {ME_STEREO=107,ME_MONO,ME_DECAY,ME_DEATH,ME_DELETE,ME_TRACK};
#define METER_BANK 8u
typedef struct {
 _Atomic uint32_t revision,enabled,epoch,until,count;
 _Atomic uint32_t serial[METER_BANK],track_owner[METER_BANK],program_owner[METER_BANK];
} MeterInterest;
typedef struct {
 uint32_t revision,enabled,epoch,until,count;
 uint32_t serial[METER_BANK],track_owner[METER_BANK],program_owner[METER_BANK];
} MeterDemand;
static inline int meter_demand_read(const MeterInterest *s,MeterDemand *d){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 d->revision=rev;d->enabled=atomic_load_explicit(&s->enabled,memory_order_relaxed);d->epoch=atomic_load_explicit(&s->epoch,memory_order_relaxed);d->until=atomic_load_explicit(&s->until,memory_order_relaxed);d->count=atomic_load_explicit(&s->count,memory_order_relaxed);
 if(d->count>METER_BANK)return 0;
 for(unsigned i=0;i<d->count;i++){d->serial[i]=atomic_load_explicit(s->serial+i,memory_order_relaxed);d->track_owner[i]=atomic_load_explicit(s->track_owner+i,memory_order_relaxed);d->program_owner[i]=atomic_load_explicit(s->program_owner+i,memory_order_relaxed);}
 atomic_thread_fence(memory_order_acquire);return rev==atomic_load_explicit(&s->revision,memory_order_relaxed);
}
typedef struct {_Atomic uint32_t revision,subscription,epoch,tick,token,left,right,enabled,site;} MeterSample;
typedef struct {
 _Atomic uint32_t address,owner,owner_kind,live,subscription,epoch;
 MeterSample lanes[METER_LANES];
} MeterCell;
typedef struct {
 _Atomic uint32_t used,error,master,tokens,closing,closed;
 _Atomic uint32_t directory[METER_HASH];
 MeterCell cells[METER_CELLS];
} MeterState;
#endif
