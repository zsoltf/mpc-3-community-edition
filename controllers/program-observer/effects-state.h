/* Selected-page processor state. The observer publishes copied words; native
 * addresses below are identity keys, never callable handles for consumers. */
#ifndef MPC_EFFECTS_STATE_H
#define MPC_EFFECTS_STATE_H
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#define EFFECT_SLOTS 4u
#define EFFECT_PAGE 8u
#define EFFECT_TEXT 32u
#define EFFECT_LIST UINT32_MAX
#define EFFECT_MAX_PARAMETERS 4096u
#define EFFECT_MAX_CATALOGUE_PAGES 64u
#define EFFECT_MAX_PRESENTATION (EFFECT_MAX_PARAMETERS+EFFECT_MAX_CATALOGUE_PAGES*EFFECT_PAGE)
#define EFFECT_FRESH_MS 1000u
enum {EF_OFF,EF_LOADING,EF_READY,EF_EMPTY,EF_UNSUPPORTED,EF_INVALID,EF_FOREIGN,EF_REENTRANT,EF_INVENTORY};
typedef struct {uint32_t valid,uid,instrument;char name[EFFECT_TEXT],format[EFFECT_TEXT],manufacturer[EFFECT_TEXT],descriptive[EFFECT_TEXT];} EffectDescription;
typedef struct {uint32_t key,ap,generation,count,status,presentation_count,layout,enable_bits,enable_revision,enable_tick,enable_valid;char name[EFFECT_TEXT];EffectDescription description;} EffectSlot;
typedef struct {uint32_t index,status,tick,text_tick,revision,bits,minimum,maximum,default_bits,steps,automatable,position,default_valid;char name[EFFECT_TEXT],text[EFFECT_TEXT];} EffectParameter;
enum {EC_NONE,EC_OPEN,EC_LOADING,EC_LOADED,EC_CANCELLED,EC_UNAVAILABLE};
typedef struct {uint32_t request,status,epoch,serial,track_owner,program_owner,slot,generation,key,ap,tick;} EffectChooser;
typedef struct {
 EffectChooser chooser;
 uint32_t demand,epoch,serial,track_owner,program_owner,inserts,generation,slot,page,status,tick,reads;
 uint32_t page_count,tab,subtab,section,section_count;char page_name[EFFECT_TEXT];
 EffectSlot slots[EFFECT_SLOTS];EffectParameter parameters[EFFECT_PAGE];
} EffectsCopy;
#define EFFECT_WORDS (sizeof(EffectsCopy)/4u)
typedef struct {
 _Atomic uint32_t revision,invalid,error;
 _Atomic uint32_t words[EFFECT_WORDS];
} EffectsState;
typedef struct {_Atomic uint32_t revision,enabled,epoch,serial,slot,page,until;} EffectsInterest;
typedef struct {uint32_t revision,enabled,epoch,serial,slot,page,until;} EffectsDemand;
static inline int effects_demand_read(const EffectsInterest *s,EffectsDemand *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 *out=(EffectsDemand){rev,atomic_load(&s->enabled),atomic_load(&s->epoch),atomic_load(&s->serial),atomic_load(&s->slot),atomic_load(&s->page),atomic_load(&s->until)};
 atomic_thread_fence(memory_order_acquire);return rev==atomic_load_explicit(&s->revision,memory_order_relaxed);
}
static inline int effects_copy_read(const EffectsState *s,EffectsCopy *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 uint32_t words[EFFECT_WORDS];for(unsigned i=0;i<EFFECT_WORDS;i++)words[i]=atomic_load_explicit(s->words+i,memory_order_relaxed);memcpy(out,words,sizeof(*out));
 atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load_explicit(&s->revision,memory_order_relaxed))return 0;
 if(atomic_load(&s->invalid)||atomic_load(&s->error)){out->status=EF_INVALID;return 0;}return 1;
}
#endif
