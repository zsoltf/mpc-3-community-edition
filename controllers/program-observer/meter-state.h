#ifndef MPC_METER_STATE_H
#define MPC_METER_STATE_H
/* Source-lane envelope observations. Native peak/reset words are not an atomic
 * stereo transaction; these records preserve each source's completed copy. */
#define METER_CELLS 2048u
#define METER_LANES 8u
#define METER_HASH 4096u
enum {ME_OK,ME_CAPACITY,ME_IDENTITY,ME_VALUE,ME_CLOSE};
enum {ME_STEREO=107,ME_MONO,ME_DECAY,ME_DEATH,ME_DELETE,ME_TRACK};
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
