#ifndef MPC_GENERAL_STATE_H
#define MPC_GENERAL_STATE_H
typedef struct {_Atomic uint32_t revision,owner,epoch,tick,mode,mixed,count,generation;} AutomationSample;
typedef struct {
 _Atomic uint32_t automation_generation,automation_error,editor_root,editor,editor_owner,zoom,zoom_owner;
 _Atomic uint32_t sends_revision,sends_epoch,sends_mixer,sends_programs[4],sends_owners[4];
 AutomationSample automation[MIRROR_POSITION_LANES];
} GeneralState;
#endif
