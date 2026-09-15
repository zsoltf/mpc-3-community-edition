/* Finite read-only queue-owner witness, not MMV1 state or command input. */
#ifndef MPC_UI_WITNESS_STATE_H
#define MPC_UI_WITNESS_STATE_H
#define UI_MAGIC 0x31574955u /* UIW1 */
#define UI_VERSION 3u
#define UI_RECORD_COUNT 16384u
#define UI_DRAIN 46u
#define UI_CLOSE 201u
enum {UI_OVERFLOW=64,UI_ROOT_UNKNOWN,UI_CLOSING,UI_IO,UI_CLOCK,UI_WRONG_OWNER,UI_REENTRY};
typedef struct {
 uint32_t seq,kind,site,tick,token,lr,sp,r0;
 uint32_t engine,queues,expected,owner,epoch,generation,ready,depth;
 uint32_t count,root_generation,flags,r1,transitions,idle,error,model_error;
} UiRecord;
_Static_assert(sizeof(UiRecord)==96,"UIW1 copied wire record");
extern UiRecord ui_records[UI_RECORD_COUNT];
extern _Atomic uint32_t ui_published,ui_tick,ui_error,ui_gap,ui_terminal,ui_in_hook;
extern uint32_t ui_owner_tp;
extern AdmissionStorage ui_admission;
void ui_initialize(void);
int ui_quiescent(void);
void ui_footer_optional(UiRecord*);
#endif
