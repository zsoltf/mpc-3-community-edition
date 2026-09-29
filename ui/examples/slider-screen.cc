/* Public-only Slider example. The consumer remains the value owner. */
#include "slider-screen.h"

extern "C" void *memset(void *, int, __SIZE_TYPE__) noexcept;

extern "C" int mpclearn_slider_screen_open(
    MpclearnSliderScreen *state, MpcUiContext *context, MpcUiParent *parent,
    MpcUiRect bounds, double initial_value,
    MpcUiIntentCallback action_callback,
    MpcUiSliderCallback slider_callback, void *callback_owner) {
    if (!state || state->screen || !context || !parent || !action_callback ||
        !slider_callback) return 0;
    MpcUiScreen *screen = mpc_ui_screen_create(context, parent, bounds);
    if (!screen) return 0;

    const MpcUiLabelStyle title_style = {
        0xff202126u, 0xffffffffu, 36};
    const MpcUiButtonStyle action_style = {
        0xff2f2f34u, 0xffd31145u, 0xffffffffu};
    const MpcUiGrid slider_grid = {4, 1, 16, 0, 24, 72, 24, 72};
    MpcUiControl *title = mpc_ui_label_create(screen, "NATIVE SLIDER");
    MpcUiSlider *slider = mpc_ui_slider_create(
        screen, 0.0, 127.0, 1.0, initial_value,
        MPC_UI_SLIDER_HORIZONTAL, MPCLEARN_SLIDER_VALUE,
        slider_callback, callback_owner);
    MpcUiControl *back = mpc_ui_button_create(
        screen, "BACK", MPCLEARN_SLIDER_BACK,
        action_callback, callback_owner);
    if (!title || !slider || !back ||
        !mpc_ui_label_style(title, title_style) ||
        !mpc_ui_button_style(back, action_style) ||
        !mpc_ui_control_place(title, {24, 24, bounds.width - 48, 72}) ||
        !mpc_ui_slider_place_grid(slider, slider_grid, 1, 0, 1, 1) ||
        !mpc_ui_control_place(back,
                              {24, bounds.height - 96,
                               bounds.width - 48, 72}) ||
        !mpc_ui_screen_show(screen)) {
        (void)mpc_ui_screen_close(&screen);
        return 0;
    }
    state->screen = screen;
    state->title = title;
    state->slider = slider;
    state->back = back;
    return 1;
}

extern "C" int mpclearn_slider_screen_set_title(
    MpclearnSliderScreen *state, const char *text) {
    return state && state->screen && state->title
               ? mpc_ui_label_text(state->title, text)
               : 0;
}

extern "C" int mpclearn_slider_screen_set_value(
    MpclearnSliderScreen *state, double value) {
    return state && state->screen && state->slider
               ? mpc_ui_slider_set_value(state->slider, value)
               : 0;
}

extern "C" int mpclearn_slider_screen_close(MpclearnSliderScreen *state) {
    if (!state || !state->screen) return 0;
    if (!mpc_ui_screen_close(&state->screen)) return 0;
    memset(state, 0, sizeof(*state));
    return 1;
}
