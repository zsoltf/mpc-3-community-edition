/* Public-only third-screen example using opaque toolkit objects. */
#include "third-screen.h"

namespace {

static void third_app_intent(void *owner, uint32_t intent) {
    MpclearnThirdApp *app = (MpclearnThirdApp *)owner;
    if (!app || !app->host || !app->callback) return;
    if (intent == MPCLEARN_THIRD_BACK &&
        !mpc_ui_app_request_close(app->host)) return;
    app->callback(app->callback_owner, intent);
}

static int third_app_open(void *owner, MpcUiAppHost *host,
                          MpcUiContext *context, MpcUiParent *parent,
                          MpcUiRect bounds) {
    MpclearnThirdApp *app = (MpclearnThirdApp *)owner;
    if (!app || !host || app->screen || app->host) return 0;
    app->host = host;
    app->screen = mpclearn_third_screen_open(
        context, parent, bounds, third_app_intent, app);
    if (!app->screen) app->host = nullptr;
    return app->screen != nullptr;
}

static int third_app_prepare_close(void *owner, int forced) {
    MpclearnThirdApp *app = (MpclearnThirdApp *)owner;
    (void)forced;
    return app && app->screen ? MPC_UI_APP_CLOSE_READY
                              : MPC_UI_APP_CLOSE_FAILED;
}

static int third_app_close(void *owner) {
    MpclearnThirdApp *app = (MpclearnThirdApp *)owner;
    if (!app || !app->screen) return 0;
    if (!mpclearn_third_screen_close(&app->screen)) return 0;
    app->host = nullptr;
    return 1;
}

const MpcUiAppCallbacks kThirdAppCallbacks = {
    sizeof(MpcUiAppCallbacks), third_app_open,
    third_app_prepare_close, third_app_close,
};

} // namespace

extern "C" MpclearnThirdScreen *mpclearn_third_screen_open(
    MpcUiContext *context, MpcUiParent *parent, MpcUiRect bounds,
    MpcUiIntentCallback callback, void *callback_owner) {
    if (!context || !parent || !callback) return nullptr;
    MpcUiScreen *screen = mpc_ui_screen_create(context, parent, bounds);
    if (!screen) return nullptr;

    const MpcUiButtonStyle title_style = {0xff202126u, 0xff202126u, 0xffffffffu};
    const MpcUiButtonStyle action_style = {0xff2f2f34u, 0xffd31145u, 0xffffffffu};
    const MpcUiGrid grid = {4, 2, 16, 16, 24, 24, 24, 24};
    MpcUiControl *title = mpc_ui_button_create(screen, "UI TOOLKIT EXAMPLE", 0,
                                                nullptr, nullptr);
    MpcUiControl *one = mpc_ui_button_create(screen, "ACTION ONE",
                                              MPCLEARN_THIRD_ACTION_ONE,
                                              callback, callback_owner);
    MpcUiControl *two = mpc_ui_button_create(screen, "ACTION TWO",
                                              MPCLEARN_THIRD_ACTION_TWO,
                                              callback, callback_owner);
    MpcUiControl *back = mpc_ui_button_create(screen, "BACK",
                                               MPCLEARN_THIRD_BACK,
                                               callback, callback_owner);
    if (!title || !one || !two || !back ||
        !mpc_ui_button_style(title, title_style) ||
        !mpc_ui_button_style(one, action_style) ||
        !mpc_ui_button_style(two, action_style) ||
        !mpc_ui_button_style(back, action_style) ||
        !mpc_ui_control_place_grid(title, grid, 0, 0, 1, 2) ||
        !mpc_ui_control_place_grid(one, grid, 1, 0, 1, 1) ||
        !mpc_ui_control_place_grid(two, grid, 1, 1, 1, 1) ||
        !mpc_ui_control_place_grid(back, grid, 3, 0, 1, 2) ||
        !mpc_ui_screen_show(screen)) {
        (void)mpc_ui_screen_close(&screen);
        return nullptr;
    }
    return (MpclearnThirdScreen *)screen;
}

extern "C" int mpclearn_third_screen_close(MpclearnThirdScreen **screen) {
    if (!screen || !*screen) return 0;
    return mpc_ui_screen_close((MpcUiScreen **)screen);
}

extern "C" int mpclearn_third_app_init(
    MpclearnThirdApp *app, MpcUiIntentCallback callback,
    void *callback_owner) {
    if (!app || !callback || app->screen || app->host) return 0;
    app->callback = callback;
    app->callback_owner = callback_owner;
    return 1;
}

extern "C" const MpcUiAppCallbacks *mpclearn_third_app_callbacks(void) {
    return &kThirdAppCallbacks;
}
