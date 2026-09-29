/* Public-only combined app example. Its value remains consumer-owned. */
#include "toolkit-example.h"

extern "C" void *memset(void *, int, __SIZE_TYPE__) noexcept;

namespace {

static void example_action(void *owner, uint32_t intent) {
    MpclearnToolkitExampleApp *app = (MpclearnToolkitExampleApp *)owner;
    if (!app || !app->host || !app->action_callback) return;
    if (intent == MPCLEARN_TOOLKIT_EXAMPLE_BACK &&
        !mpc_ui_app_request_close(app->host)) return;
    app->action_callback(app->callback_owner, intent);
}

static void example_slider(void *owner, uint32_t intent, uint32_t event,
                           double value) {
    MpclearnToolkitExampleApp *app = (MpclearnToolkitExampleApp *)owner;
    if (!app || !app->host || !app->slider_callback) return;
    app->slider_callback(app->callback_owner, intent, event, value);
}

static int example_open(void *owner, MpcUiAppHost *host,
                        MpcUiContext *context, MpcUiParent *parent,
                        MpcUiRect bounds) {
    MpclearnToolkitExampleApp *app = (MpclearnToolkitExampleApp *)owner;
    if (!app || !host || app->host || app->view.screen) return 0;
    app->host = host;
    MpcUiScreen *screen = mpc_ui_screen_create(context, parent, bounds);
    if (!screen) { app->host = nullptr; return 0; }

    const MpcUiLabelStyle title_style = {
        0xff202126u, 0xffffffffu, 36};
    const MpcUiButtonStyle action_style = {
        0xff2f2f34u, 0xffd31145u, 0xffffffffu};
    const int content_width = bounds.width - 48;
    const int action_width = (content_width - 24) / 2;
    MpcUiControl *title = mpc_ui_label_create(screen, "UI TOOLKIT EXAMPLE");
    MpcUiSlider *slider = mpc_ui_slider_create(
        screen, 0.0, 127.0, 1.0, app->initial_value,
        MPC_UI_SLIDER_HORIZONTAL, MPCLEARN_TOOLKIT_EXAMPLE_SLIDER,
        example_slider, app);
    MpcUiControl *one = mpc_ui_button_create(
        screen, "ACTION ONE", MPCLEARN_TOOLKIT_EXAMPLE_ACTION_ONE,
        example_action, app);
    MpcUiControl *two = mpc_ui_button_create(
        screen, "ACTION TWO", MPCLEARN_TOOLKIT_EXAMPLE_ACTION_TWO,
        example_action, app);
    MpcUiControl *back = mpc_ui_button_create(
        screen, "BACK", MPCLEARN_TOOLKIT_EXAMPLE_BACK,
        example_action, app);
    if (!title || !slider || !one || !two || !back ||
        !mpc_ui_label_style(title, title_style) ||
        !mpc_ui_button_style(one, action_style) ||
        !mpc_ui_button_style(two, action_style) ||
        !mpc_ui_button_style(back, action_style) ||
        !mpc_ui_control_place(title, {24, 24, content_width, 64}) ||
        !mpc_ui_slider_place(slider, {24, 112, content_width, 96}) ||
        !mpc_ui_control_place(one, {24, 232, action_width, 96}) ||
        !mpc_ui_control_place(two,
                              {48 + action_width, 232, action_width, 96}) ||
        !mpc_ui_control_place(back,
                              {24, bounds.height - 96, content_width, 72}) ||
        !mpc_ui_screen_show(screen)) {
        (void)mpc_ui_screen_close(&screen);
        app->host = nullptr;
        return 0;
    }
    app->view = {screen, title, slider, one, two, back};
    return 1;
}

static int example_prepare_close(void *owner, int forced) {
    MpclearnToolkitExampleApp *app = (MpclearnToolkitExampleApp *)owner;
    (void)forced;
    return app && app->view.screen ? MPC_UI_APP_CLOSE_READY
                                   : MPC_UI_APP_CLOSE_FAILED;
}

static int example_close(void *owner) {
    MpclearnToolkitExampleApp *app = (MpclearnToolkitExampleApp *)owner;
    if (!app || !app->view.screen) return 0;
    if (!mpc_ui_screen_close(&app->view.screen)) return 0;
    MpcUiIntentCallback action = app->action_callback;
    MpcUiSliderCallback slider = app->slider_callback;
    void *callback_owner = app->callback_owner;
    double initial_value = app->initial_value;
    memset(&app->view, 0, sizeof(app->view));
    app->host = nullptr;
    app->action_callback = action;
    app->slider_callback = slider;
    app->callback_owner = callback_owner;
    app->initial_value = initial_value;
    return 1;
}

const MpcUiAppCallbacks kExampleCallbacks = {
    sizeof(MpcUiAppCallbacks), example_open,
    example_prepare_close, example_close,
};

} // namespace

extern "C" int mpclearn_toolkit_example_app_init(
    MpclearnToolkitExampleApp *app, double initial_value,
    MpcUiIntentCallback action_callback,
    MpcUiSliderCallback slider_callback, void *callback_owner) {
    if (!app || app->view.screen || app->host || !action_callback ||
        !slider_callback || initial_value < 0.0 || initial_value > 127.0)
        return 0;
    app->action_callback = action_callback;
    app->slider_callback = slider_callback;
    app->callback_owner = callback_owner;
    app->initial_value = initial_value;
    return 1;
}

extern "C" const MpcUiAppCallbacks *
mpclearn_toolkit_example_app_callbacks(void) {
    return &kExampleCallbacks;
}

extern "C" int mpclearn_toolkit_example_set_title(
    MpclearnToolkitExampleApp *app, const char *text) {
    return app && app->view.screen && app->view.title
               ? mpc_ui_label_text(app->view.title, text)
               : 0;
}

extern "C" int mpclearn_toolkit_example_set_value(
    MpclearnToolkitExampleApp *app, double value) {
    return app && app->view.screen && app->view.slider
               ? mpc_ui_slider_set_value(app->view.slider, value)
               : 0;
}
