#ifndef MPCLEARN_GLOBAL_MIDI_LEARN_SCREEN_H
#define MPCLEARN_GLOBAL_MIDI_LEARN_SCREEN_H

#include "global-midi-learn-groups.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS 16u
#define MPCLEARN_GLOBAL_LEARN_NO_SELECTION MPC_UI_NO_SELECTION
#define MPCLEARN_GLOBAL_LEARN_TEXT_MAX 64u

enum MpclearnGlobalLearnIntent {
    MPCLEARN_GLOBAL_LEARN_GROUP_1 = 0x100u,
    MPCLEARN_GLOBAL_LEARN_TARGET_1 = 0x200u,
    MPCLEARN_GLOBAL_LEARN_BEGIN = 0x300u,
    MPCLEARN_GLOBAL_LEARN_CANCEL = 0x301u,
    MPCLEARN_GLOBAL_LEARN_BACK = 0x302u,
    MPCLEARN_GLOBAL_LEARN_NEW = 0x303u,
    MPCLEARN_GLOBAL_LEARN_COPY = 0x304u,
    MPCLEARN_GLOBAL_LEARN_LOAD = 0x305u,
    MPCLEARN_GLOBAL_LEARN_EXPORT = 0x306u,
    MPCLEARN_GLOBAL_LEARN_CLEAR = 0x307u,
    MPCLEARN_GLOBAL_LEARN_PREVIOUS_FILE = 0x308u,
    MPCLEARN_GLOBAL_LEARN_NEXT_FILE = 0x309u,
    MPCLEARN_GLOBAL_LEARN_TYPE = 0x30au,
    MPCLEARN_GLOBAL_LEARN_CHANNEL_DOWN = 0x30bu,
    MPCLEARN_GLOBAL_LEARN_CHANNEL_UP = 0x30cu,
    MPCLEARN_GLOBAL_LEARN_DATA_DOWN = 0x30du,
    MPCLEARN_GLOBAL_LEARN_DATA_UP = 0x30eu,
    MPCLEARN_GLOBAL_LEARN_MODE = 0x30fu,
    MPCLEARN_GLOBAL_LEARN_REVERSE = 0x310u,
    MPCLEARN_GLOBAL_LEARN_APPLY = 0x311u,
};

typedef struct {
    const char *target_text[MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS];
    const char *selected_target_text;
    const char *assignment_text;
    const char *status_text;
    const char *mapping_text;
    const char *edit_type_text;
    const char *edit_channel_text;
    const char *edit_data_text;
    const char *edit_mode_text;
    const char *edit_reverse_text;
    uint32_t target_count;
    uint32_t selected_group;
    uint32_t selected_target_row;
    uint32_t can_learn;
    uint32_t learning;
    uint32_t can_new;
    uint32_t can_copy;
    uint32_t can_load;
    uint32_t can_export;
    uint32_t can_clear;
    uint32_t can_previous_file;
    uint32_t can_next_file;
    uint32_t can_edit;
    uint32_t can_edit_data;
    uint32_t can_apply;
} MpclearnGlobalLearnView;

typedef struct {
    MpcUiScreen *screen;
    MpcUiList *groups;
    MpcUiList *targets;
    MpcUiControl *title;
    MpcUiControl *mapping_previous;
    MpcUiControl *mapping;
    MpcUiControl *mapping_next;
    MpcUiControl *file_new;
    MpcUiControl *file_copy;
    MpcUiControl *file_load;
    MpcUiControl *file_export;
    MpcUiControl *edit_type;
    MpcUiControl *edit_channel_down;
    MpcUiControl *edit_channel;
    MpcUiControl *edit_channel_up;
    MpcUiControl *edit_data_down;
    MpcUiControl *edit_data;
    MpcUiControl *edit_data_up;
    MpcUiControl *edit_mode;
    MpcUiControl *edit_reverse;
    MpcUiControl *edit_apply;
    MpcUiControl *selected_target;
    MpcUiControl *assignment;
    MpcUiControl *status;
    MpcUiControl *clear_assignment;
    MpcUiControl *learn;
    MpcUiControl *cancel;
    MpcUiControl *back;
    MpcUiIntentCallback callback;
    void *callback_owner;
    uint32_t target_count;
    char target_text[MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS]
                    [MPCLEARN_GLOBAL_LEARN_TEXT_MAX];
} MpclearnGlobalLearnScreen;

int mpclearn_global_learn_screen_open(
    MpclearnGlobalLearnScreen *screen, MpcUiContext *context,
    MpcUiParent *parent, MpcUiRect bounds, MpcUiIntentCallback callback,
    void *callback_owner);
int mpclearn_global_learn_screen_update(
    MpclearnGlobalLearnScreen *screen, const MpclearnGlobalLearnView *view);
int mpclearn_global_learn_screen_close(MpclearnGlobalLearnScreen *screen);

#ifdef __cplusplus
}
#endif

#endif
