#ifndef MPCLEARN_GLOBAL_MIDI_LEARN_GROUPS_H
#define MPCLEARN_GLOBAL_MIDI_LEARN_GROUPS_H

#include "../include/mpclearn-ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MPCLEARN_GLOBAL_LEARN_TARGET_COUNT 120u
#define MPCLEARN_GLOBAL_LEARN_GROUP_COUNT 14u

typedef struct {
    const char *name;
    uint32_t first_target;
    uint32_t target_count;
} MpclearnGlobalLearnGroup;

typedef struct {
    MpcUiRect title;
    MpcUiRect mapping_previous;
    MpcUiRect mapping;
    MpcUiRect mapping_next;
    MpcUiRect file_new;
    MpcUiRect file_copy;
    MpcUiRect file_load;
    MpcUiRect file_export;
    MpcUiRect groups;
    MpcUiRect targets;
    MpcUiRect detail;
    MpcUiRect edit_type;
    MpcUiRect edit_mode;
    MpcUiRect edit_channel_down;
    MpcUiRect edit_channel;
    MpcUiRect edit_channel_up;
    MpcUiRect edit_reverse;
    MpcUiRect edit_data_down;
    MpcUiRect edit_data;
    MpcUiRect edit_data_up;
    MpcUiRect edit_apply;
    MpcUiRect edit_status;
    MpcUiRect clear_assignment;
    MpcUiRect learn;
    MpcUiRect back;
} MpclearnGlobalLearnLayout;

const MpclearnGlobalLearnGroup *mpclearn_global_learn_group(uint32_t group);
int mpclearn_global_learn_target_location(uint32_t target, uint32_t *group,
                                          uint32_t *index_in_group);
int mpclearn_global_learn_layout(MpcUiRect bounds,
                                 MpclearnGlobalLearnLayout *layout);

#ifdef __cplusplus
}
#endif

#endif
