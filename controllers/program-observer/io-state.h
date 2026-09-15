/* Selected-track routing projection of the existing native field/catalogue
 * owner. Values and option IDs are copied data, never callable handles. */
#ifndef MPC_IO_STATE_H
#define MPC_IO_STATE_H
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>
#define IO_FIELDS 8u
#define IO_TEXT 64u
#define IO_FRESH_MS 1000u
enum {IO_MONITOR,IO_MIDI_IN_PORT,IO_MIDI_IN_CHANNEL,IO_SEND_TO,IO_AUDIO_OUT,IO_MIDI_OUT_PORT,IO_MIDI_OUT_CHANNEL,IO_AUDIO_IN};
enum {IO_OFF,IO_LOADING,IO_READY,IO_NOT_APPLICABLE,IO_UNAVAILABLE,IO_INVALID};
typedef struct {
 uint32_t status,property,incarnation,revision,value,tick,choice_generation,choice_count,choice_index,connector;
 char text[IO_TEXT];
} IOField;
typedef struct {
 uint32_t demand,epoch,generation,serial,binding,incarnation,track_owner,program_owner,connector,program_connector,audio_connector,status,tick;
 IOField fields[IO_FIELDS];
} IOCopy;
#define IO_WORDS (sizeof(IOCopy)/4u)
typedef struct {_Atomic uint32_t revision,invalid,error;_Atomic uint32_t words[IO_WORDS];} IOState;
typedef struct {_Atomic uint32_t revision,enabled,epoch,serial,until;} IOInterest;
typedef struct {uint32_t revision,enabled,epoch,serial,until;} IODemand;
static inline int io_demand_read(const IOInterest *s,IODemand *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 *out=(IODemand){rev,atomic_load(&s->enabled),atomic_load(&s->epoch),atomic_load(&s->serial),atomic_load(&s->until)};
 atomic_thread_fence(memory_order_acquire);return rev==atomic_load_explicit(&s->revision,memory_order_relaxed);
}
static inline int io_copy_read(const IOState *s,IOCopy *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 if(atomic_load_explicit(s->words+offsetof(IOCopy,status)/4,memory_order_relaxed)==IO_OFF){memset(out,0,sizeof(*out));atomic_thread_fence(memory_order_acquire);return rev==atomic_load_explicit(&s->revision,memory_order_relaxed);}
 uint32_t words[IO_WORDS];for(unsigned i=0;i<IO_WORDS;i++)words[i]=atomic_load_explicit(s->words+i,memory_order_relaxed);memcpy(out,words,sizeof(*out));
 atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load_explicit(&s->revision,memory_order_relaxed))return 0;
 if(atomic_load(&s->invalid)||atomic_load(&s->error)){out->status=IO_INVALID;return 0;}return 1;
}
static inline const char *io_field_name(unsigned field){
 static const char *names[]={"Monitor","MIDI In Port","MIDI In Channel","Send To","Audio Out","MIDI Out Port","MIDI Out Channel","Audio In"};return field<IO_FIELDS?names[field]:"Unknown";
}
#endif
