/* Compact retained Instrument state, owned by the existing mirror producer.
 * Entries are never recycled during a producer lifetime; addresses are keys. */
#ifndef MPC_PAD_STATE_H
#define MPC_PAD_STATE_H
#define PAD_SLOTS 128u
#define PAD_OWNERS 32768u
#define PAD_HASH 65536u
#define PAD_PARENTS 512u
#define PAD_PARENT_HASH 1024u
#define PAD_PROBES 64u
enum {PF_VOLUME,PF_PAN,PF_MUTE,PF_SOLO,PF_SOLO_AUDIO,PF_COUNT};
enum {PD_OK,PD_CAPACITY,PD_OWNER,PD_LIFETIME,PD_MEMBERSHIP,PD_WRAP};
enum {PD_BIRTH=139,PD_COPY,PD_COMPLETE,PD_COPY_COMPLETE,PD_DEATH,PD_DELETE,PD_BIND,PD_BOUND,PD_SWAP,PD_SWAPPED,PD_PARENT_CHANGE,PD_EXECUTE};
typedef struct {_Atomic uint32_t revision,live,bits,updates,seed;} PadScalar;
typedef struct {
 _Atomic uint32_t address,incarnation,live,complete,parent,parent_owner;
 PadScalar fields[PF_COUNT];
} PadOwner;
typedef struct {_Atomic uint32_t generation,owner;} PadMembership;
typedef struct {
 _Atomic uint32_t address,owner,revision,live;
 PadMembership slots[PAD_SLOTS];
} PadParent;
typedef struct {
 _Atomic uint32_t error,used,parents_used;
 _Atomic uint32_t directory[PAD_HASH],parent_directory[PAD_PARENT_HASH];
 PadOwner owners[PAD_OWNERS];PadParent parents[PAD_PARENTS];
} PadState;
static inline unsigned pad_field(unsigned f){return f==CF_VOLUME?PF_VOLUME:f==CF_PAN?PF_PAN:f==CF_MUTE?PF_MUTE:f==CF_SOLO?PF_SOLO:f==CF_SOLO_AUDIO?PF_SOLO_AUDIO:PF_COUNT;}
static inline unsigned pad_offset(unsigned f){static const unsigned offsets[PF_COUNT]={0x574,0x394,0x14,0x4c,0x5c};return f<PF_COUNT?offsets[f]:0;}
static inline unsigned pad_controller(unsigned f){return f==CF_VOLUME?0x206:f==CF_PAN?0x205:f==CF_MUTE?0x20e:f==CF_SOLO?0x20f:0;}
#endif
