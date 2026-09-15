/* Native Q-Link UI projection. Keys are comparison data, never borrowed objects. */
#ifndef MPC_QLINK_STATE_H
#define MPC_QLINK_STATE_H
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>
#define QLINK_SLOTS 16u
#define QLINK_TEXT 64u
#define QLINK_FRESH_MS 1500u
enum {QL_OFF,QL_LOADING,QL_SAMPLED,QL_UNAVAILABLE,QL_INVALID,QL_FOREIGN,QL_REENTRANT};
typedef struct {
 uint32_t index,tick,revision,bits,sampled,normalized_valid,kind,gesture_activity,name_status,text_status;
 uint32_t refresh_revision,refresh_tick,refresh_raw,refresh_kind,refresh_matches[8];
 uint32_t raw_signals[3];
 char name[QLINK_TEXT],text[QLINK_TEXT];
} QLinkSlot;
typedef struct {
 uint32_t demand,epoch,generation,status,tick,reads,root,controls,queue,wrapper,provider,labels,mode,sources,descriptors,cache,strings;
 uint32_t mode_controller,mode_generation,mode_revision,mode_id,mode_count,mode_tick,mode_valid;
 uint32_t raw_signals[3];
 QLinkSlot slots[QLINK_SLOTS];
} QLinkCopy;
#define QLINK_WORDS (sizeof(QLinkCopy)/4u)
static inline const char *qlink_mode_name(unsigned id){
 static const char *names[]={"Screen","Project1","Project2","Track","Pad Scene","Pad Param","Track FX","MIDI","Volume","Pan","Send1","Send2","Send3","Send4","Step Seq"};return id<15?names[id]:"Unknown";
}
static inline unsigned qlink_mode_jobs(unsigned before,unsigned after){return before==after?0:after==0||after==14?3:2;}
typedef struct {_Atomic uint32_t revision,invalid,error;_Atomic uint32_t words[QLINK_WORDS];} QLinkState;
typedef struct {_Atomic uint32_t revision,enabled,epoch,until,page;} QLinkInterest;
typedef struct {uint32_t revision,enabled,epoch,until,page;} QLinkDemand;
static inline int qlink_demand_read(const QLinkInterest *s,QLinkDemand *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 *out=(QLinkDemand){rev,atomic_load(&s->enabled),atomic_load(&s->epoch),atomic_load(&s->until),atomic_load(&s->page)};
 atomic_thread_fence(memory_order_acquire);return rev==atomic_load_explicit(&s->revision,memory_order_relaxed);
}
static inline int qlink_copy_read(const QLinkState *s,QLinkCopy *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 /* Ordinary bridge snapshots avoid copying the dormant diagnostic projection. */
 if(atomic_load_explicit(s->words+offsetof(QLinkCopy,status)/4,memory_order_relaxed)==QL_OFF){
  memset(out,0,sizeof(*out));atomic_thread_fence(memory_order_acquire);return rev==atomic_load_explicit(&s->revision,memory_order_relaxed);
 }
 uint32_t words[QLINK_WORDS];for(unsigned i=0;i<QLINK_WORDS;i++)words[i]=atomic_load_explicit(s->words+i,memory_order_relaxed);memcpy(out,words,sizeof(*out));
 atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load_explicit(&s->revision,memory_order_relaxed))return 0;
 if(atomic_load(&s->invalid)||atomic_load(&s->error)){out->status=atomic_load(&s->error)?atomic_load(&s->error):QL_INVALID;return 0;}return 1;
}
#endif
