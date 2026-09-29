/* Public grouped assignment editor. The native mapping owner supplies the view. */
#include "global-midi-learn-screen.h"

namespace {

constexpr MpcUiButtonStyle kHeading = {
    0xff202126u, 0xff202126u, 0xffffffffu};
constexpr MpcUiButtonStyle kList = {
    0xff24252au, 0xffd31145u, 0xffffffffu};
constexpr MpcUiButtonStyle kAction = {
    0xff2f2f34u, 0xffd31145u, 0xffffffffu};
constexpr MpcUiButtonStyle kCancel = {
    0xffd31145u, 0xffd31145u, 0xffffffffu};

static int copy_text(char *out, uint32_t bytes, const char *text) {
    if (!out || bytes < 2u || !text) return 0;
    for (uint32_t index = 0; index + 1u < bytes; ++index) {
        out[index] = text[index];
        if (!text[index]) return index != 0u;
        out[index + 1u] = 0;
    }
    return 0;
}

static const char *group_text(void *, uint32_t row) {
    const MpclearnGlobalLearnGroup *group = mpclearn_global_learn_group(row);
    return group ? group->name : nullptr;
}

static const char *target_text(void *owner, uint32_t row) {
    auto *screen = (MpclearnGlobalLearnScreen *)owner;
    return screen && row < screen->target_count ? screen->target_text[row]
                                                : nullptr;
}

static void group_selected(void *owner, uint32_t row) {
    auto *screen = (MpclearnGlobalLearnScreen *)owner;
    if (screen && screen->callback && row < MPCLEARN_GLOBAL_LEARN_GROUP_COUNT)
        screen->callback(screen->callback_owner,
                         MPCLEARN_GLOBAL_LEARN_GROUP_1 + row);
}

static void target_selected(void *owner, uint32_t row) {
    auto *screen = (MpclearnGlobalLearnScreen *)owner;
    if (screen && screen->callback && row < screen->target_count)
        screen->callback(screen->callback_owner,
                         MPCLEARN_GLOBAL_LEARN_TARGET_1 + row);
}

static int place(MpclearnGlobalLearnScreen *screen, MpcUiRect bounds) {
    MpclearnGlobalLearnLayout layout = {};
    if (!mpclearn_global_learn_layout(bounds, &layout)) return 0;
    const MpcUiRect detail = layout.detail;
    return mpc_ui_control_place(screen->title, layout.title) &&
           mpc_ui_control_place(screen->mapping_previous,
                                layout.mapping_previous) &&
           mpc_ui_control_place(screen->mapping, layout.mapping) &&
           mpc_ui_control_place(screen->mapping_next, layout.mapping_next) &&
           mpc_ui_control_place(screen->file_new, layout.file_new) &&
           mpc_ui_control_place(screen->file_copy, layout.file_copy) &&
           mpc_ui_control_place(screen->file_load, layout.file_load) &&
           mpc_ui_control_place(screen->file_export, layout.file_export) &&
           mpc_ui_list_place(screen->groups, layout.groups) &&
           mpc_ui_list_place(screen->targets, layout.targets) &&
           mpc_ui_control_place(screen->selected_target,
                                {detail.x, detail.y, detail.width, 48}) &&
           mpc_ui_control_place(screen->assignment,
                                {detail.x, detail.y + 56, detail.width, 48}) &&
           mpc_ui_control_place(screen->edit_type, layout.edit_type) &&
           mpc_ui_control_place(screen->edit_mode, layout.edit_mode) &&
           mpc_ui_control_place(screen->edit_channel_down,
                                layout.edit_channel_down) &&
           mpc_ui_control_place(screen->edit_channel, layout.edit_channel) &&
           mpc_ui_control_place(screen->edit_channel_up,
                                layout.edit_channel_up) &&
           mpc_ui_control_place(screen->edit_reverse, layout.edit_reverse) &&
           mpc_ui_control_place(screen->edit_data_down,
                                layout.edit_data_down) &&
           mpc_ui_control_place(screen->edit_data, layout.edit_data) &&
           mpc_ui_control_place(screen->edit_data_up, layout.edit_data_up) &&
           mpc_ui_control_place(screen->edit_apply, layout.edit_apply) &&
           mpc_ui_control_place(screen->status, layout.edit_status) &&
           mpc_ui_control_place(screen->clear_assignment,
                                layout.clear_assignment) &&
           mpc_ui_control_place(screen->learn, layout.learn) &&
           mpc_ui_control_place(screen->cancel, layout.learn) &&
           mpc_ui_control_place(screen->back, layout.back);
}

static int style(MpclearnGlobalLearnScreen *screen) {
    return mpc_ui_button_style(screen->title, kHeading) &&
           mpc_ui_button_style(screen->mapping_previous, kAction) &&
           mpc_ui_button_style(screen->mapping, kHeading) &&
           mpc_ui_button_style(screen->mapping_next, kAction) &&
           mpc_ui_button_style(screen->file_new, kAction) &&
           mpc_ui_button_style(screen->file_copy, kAction) &&
           mpc_ui_button_style(screen->file_load, kAction) &&
           mpc_ui_button_style(screen->file_export, kAction) &&
           mpc_ui_button_style(screen->edit_type, kAction) &&
           mpc_ui_button_style(screen->edit_mode, kAction) &&
           mpc_ui_button_style(screen->edit_channel_down, kAction) &&
           mpc_ui_button_style(screen->edit_channel, kHeading) &&
           mpc_ui_button_style(screen->edit_channel_up, kAction) &&
           mpc_ui_button_style(screen->edit_reverse, kAction) &&
           mpc_ui_button_style(screen->edit_data_down, kAction) &&
           mpc_ui_button_style(screen->edit_data, kHeading) &&
           mpc_ui_button_style(screen->edit_data_up, kAction) &&
           mpc_ui_button_style(screen->edit_apply, kAction) &&
           mpc_ui_list_style(screen->groups, kList) &&
           mpc_ui_list_style(screen->targets, kList) &&
           mpc_ui_button_style(screen->selected_target, kHeading) &&
           mpc_ui_button_style(screen->assignment, kHeading) &&
           mpc_ui_button_style(screen->status, kHeading) &&
           mpc_ui_button_style(screen->clear_assignment, kCancel) &&
           mpc_ui_button_style(screen->learn, kAction) &&
           mpc_ui_button_style(screen->cancel, kCancel) &&
           mpc_ui_button_style(screen->back, kAction);
}

} // namespace

extern "C" int mpclearn_global_learn_screen_open(
    MpclearnGlobalLearnScreen *view, MpcUiContext *context,
    MpcUiParent *parent, MpcUiRect bounds, MpcUiIntentCallback callback,
    void *callback_owner) {
    if (!view || !context || !parent || !callback || view->screen) return 0;
    *view = {};
    view->callback = callback;
    view->callback_owner = callback_owner;
    view->screen = mpc_ui_screen_create(context, parent, bounds);
    if (!view->screen) return 0;
    view->title = mpc_ui_button_create(
        view->screen, "GLOBAL MIDI LEARN", 0, nullptr, nullptr);
    view->mapping_previous = mpc_ui_button_create(
        view->screen, "<", MPCLEARN_GLOBAL_LEARN_PREVIOUS_FILE,
        callback, callback_owner);
    view->mapping = mpc_ui_button_create(
        view->screen, "CURRENT MAPPING", 0, nullptr, nullptr);
    view->mapping_next = mpc_ui_button_create(
        view->screen, ">", MPCLEARN_GLOBAL_LEARN_NEXT_FILE,
        callback, callback_owner);
    view->file_new = mpc_ui_button_create(
        view->screen, "NEW", MPCLEARN_GLOBAL_LEARN_NEW,
        callback, callback_owner);
    view->file_copy = mpc_ui_button_create(
        view->screen, "COPY", MPCLEARN_GLOBAL_LEARN_COPY,
        callback, callback_owner);
    view->file_load = mpc_ui_button_create(
        view->screen, "IMPORT", MPCLEARN_GLOBAL_LEARN_LOAD,
        callback, callback_owner);
    view->file_export = mpc_ui_button_create(
        view->screen, "EXPORT", MPCLEARN_GLOBAL_LEARN_EXPORT,
        callback, callback_owner);
    view->edit_type = mpc_ui_button_create(
        view->screen, "TYPE: CC", MPCLEARN_GLOBAL_LEARN_TYPE,
        callback, callback_owner);
    view->edit_mode = mpc_ui_button_create(
        view->screen, "MODE: ABS CC", MPCLEARN_GLOBAL_LEARN_MODE,
        callback, callback_owner);
    view->edit_channel_down = mpc_ui_button_create(
        view->screen, "-", MPCLEARN_GLOBAL_LEARN_CHANNEL_DOWN,
        callback, callback_owner);
    view->edit_channel = mpc_ui_button_create(
        view->screen, "CHANNEL: 1", 0, nullptr, nullptr);
    view->edit_channel_up = mpc_ui_button_create(
        view->screen, "+", MPCLEARN_GLOBAL_LEARN_CHANNEL_UP,
        callback, callback_owner);
    view->edit_reverse = mpc_ui_button_create(
        view->screen, "REVERSE: OFF", MPCLEARN_GLOBAL_LEARN_REVERSE,
        callback, callback_owner);
    view->edit_data_down = mpc_ui_button_create(
        view->screen, "-", MPCLEARN_GLOBAL_LEARN_DATA_DOWN,
        callback, callback_owner);
    view->edit_data = mpc_ui_button_create(
        view->screen, "NUMBER: 0", 0, nullptr, nullptr);
    view->edit_data_up = mpc_ui_button_create(
        view->screen, "+", MPCLEARN_GLOBAL_LEARN_DATA_UP,
        callback, callback_owner);
    view->edit_apply = mpc_ui_button_create(
        view->screen, "APPLY", MPCLEARN_GLOBAL_LEARN_APPLY,
        callback, callback_owner);
    view->groups = mpc_ui_list_create(
        view->screen, "FUNCTION GROUPS", MPCLEARN_GLOBAL_LEARN_GROUP_COUNT,
        52, group_text, group_selected, view);
    view->targets = mpc_ui_list_create(
        view->screen, "TARGETS", 0, 52, target_text, target_selected, view);
    view->selected_target = mpc_ui_button_create(
        view->screen, "SELECT A TARGET", 0, nullptr, nullptr);
    view->assignment = mpc_ui_button_create(
        view->screen, "ASSIGNMENT: UNASSIGNED", 0, nullptr, nullptr);
    view->status = mpc_ui_button_create(
        view->screen, "SELECT A TARGET TO LEARN", 0, nullptr, nullptr);
    view->clear_assignment = mpc_ui_button_create(
        view->screen, "CLEAR ASSIGNMENT", MPCLEARN_GLOBAL_LEARN_CLEAR,
        callback, callback_owner);
    view->learn = mpc_ui_button_create(
        view->screen, "LEARN", MPCLEARN_GLOBAL_LEARN_BEGIN,
        callback, callback_owner);
    view->cancel = mpc_ui_button_create(
        view->screen, "CANCEL LEARN", MPCLEARN_GLOBAL_LEARN_CANCEL,
        callback, callback_owner);
    view->back = mpc_ui_button_create(
        view->screen, "BACK", MPCLEARN_GLOBAL_LEARN_BACK,
        callback, callback_owner);
    if (!view->title || !view->mapping_previous || !view->mapping ||
        !view->mapping_next || !view->file_new ||
        !view->file_copy || !view->file_load || !view->file_export ||
        !view->edit_type || !view->edit_mode ||
        !view->edit_channel_down || !view->edit_channel ||
        !view->edit_channel_up || !view->edit_reverse ||
        !view->edit_data_down || !view->edit_data ||
        !view->edit_data_up || !view->edit_apply ||
        !view->groups || !view->targets ||
        !view->selected_target || !view->assignment || !view->status ||
        !view->clear_assignment || !view->learn || !view->cancel ||
        !view->back || !style(view) ||
        !place(view, bounds) ||
        !mpc_ui_list_update(view->groups,
                            MPCLEARN_GLOBAL_LEARN_GROUP_COUNT, 0) ||
        !mpc_ui_control_visible(view->cancel, 0) ||
        !mpc_ui_control_visible(view->learn, 0) ||
        !mpc_ui_control_visible(view->clear_assignment, 0) ||
        !mpc_ui_control_visible(view->file_new, 0) ||
        !mpc_ui_control_visible(view->file_copy, 0) ||
        !mpc_ui_control_visible(view->file_load, 0) ||
        !mpc_ui_control_visible(view->file_export, 0) ||
        !mpc_ui_control_visible(view->edit_type, 0) ||
        !mpc_ui_control_visible(view->edit_mode, 0) ||
        !mpc_ui_control_visible(view->edit_channel_down, 0) ||
        !mpc_ui_control_visible(view->edit_channel, 0) ||
        !mpc_ui_control_visible(view->edit_channel_up, 0) ||
        !mpc_ui_control_visible(view->edit_reverse, 0) ||
        !mpc_ui_control_visible(view->edit_data_down, 0) ||
        !mpc_ui_control_visible(view->edit_data, 0) ||
        !mpc_ui_control_visible(view->edit_data_up, 0) ||
        !mpc_ui_control_visible(view->edit_apply, 0) ||
        !mpc_ui_control_visible(view->mapping_previous, 0) ||
        !mpc_ui_control_visible(view->mapping_next, 0) ||
        !mpc_ui_screen_show(view->screen)) {
        (void)mpclearn_global_learn_screen_close(view);
        return 0;
    }
    return 1;
}

extern "C" int mpclearn_global_learn_screen_update(
    MpclearnGlobalLearnScreen *screen, const MpclearnGlobalLearnView *view) {
    if (!screen || !screen->screen || !view ||
        view->selected_group >= MPCLEARN_GLOBAL_LEARN_GROUP_COUNT ||
        view->target_count > MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS ||
        (view->selected_target_row != MPCLEARN_GLOBAL_LEARN_NO_SELECTION &&
         view->selected_target_row >= view->target_count) ||
        !view->selected_target_text || !view->assignment_text ||
        !view->status_text || !view->mapping_text ||
        !view->edit_type_text || !view->edit_channel_text ||
        !view->edit_data_text || !view->edit_mode_text ||
        !view->edit_reverse_text) return 0;
    for (uint32_t row = 0; row < view->target_count; ++row)
        if (!copy_text(screen->target_text[row],
                       MPCLEARN_GLOBAL_LEARN_TEXT_MAX,
                       view->target_text[row])) return 0;
    screen->target_count = view->target_count;
    if (!mpc_ui_button_text(screen->selected_target,
                            view->selected_target_text) ||
        !mpc_ui_button_text(screen->mapping, view->mapping_text) ||
        !mpc_ui_button_text(screen->assignment, view->assignment_text) ||
        !mpc_ui_button_text(screen->status, view->status_text) ||
        !mpc_ui_button_text(screen->edit_type, view->edit_type_text) ||
        !mpc_ui_button_text(screen->edit_channel, view->edit_channel_text) ||
        !mpc_ui_button_text(screen->edit_data, view->edit_data_text) ||
        !mpc_ui_button_text(screen->edit_mode, view->edit_mode_text) ||
        !mpc_ui_button_text(screen->edit_reverse, view->edit_reverse_text) ||
        !mpc_ui_list_update(screen->groups,
                            MPCLEARN_GLOBAL_LEARN_GROUP_COUNT,
                            view->selected_group) ||
        !mpc_ui_list_update(screen->targets, view->target_count,
                            view->selected_target_row) ||
        !mpc_ui_control_visible(screen->learn,
                                view->can_learn && !view->learning) ||
        !mpc_ui_control_visible(screen->cancel,
                                view->can_learn && view->learning) ||
        !mpc_ui_control_visible(screen->clear_assignment,
                                view->can_clear && !view->learning) ||
        !mpc_ui_control_visible(screen->file_new,
                                view->can_new && !view->learning) ||
        !mpc_ui_control_visible(screen->file_copy,
                                view->can_copy && !view->learning) ||
        !mpc_ui_control_visible(screen->file_load,
                                view->can_load && !view->learning) ||
        !mpc_ui_control_visible(screen->file_export,
                                view->can_export && !view->learning) ||
        !mpc_ui_control_visible(screen->mapping_previous,
                                view->can_previous_file && !view->learning) ||
        !mpc_ui_control_visible(screen->mapping_next,
                                view->can_next_file && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_type,
                                view->can_edit && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_mode,
                                view->can_edit && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_channel_down,
                                view->can_edit && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_channel,
                                view->can_edit && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_channel_up,
                                view->can_edit && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_reverse,
                                view->can_edit && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_data_down,
                                view->can_edit_data && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_data,
                                view->can_edit_data && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_data_up,
                                view->can_edit_data && !view->learning) ||
        !mpc_ui_control_visible(screen->edit_apply,
                                view->can_apply && !view->learning))
        return 0;
    return 1;
}

extern "C" int mpclearn_global_learn_screen_close(
    MpclearnGlobalLearnScreen *screen) {
    if (!screen || !screen->screen) return 0;
    if (!mpc_ui_screen_close(&screen->screen)) return 0;
    *screen = {};
    return 1;
}
