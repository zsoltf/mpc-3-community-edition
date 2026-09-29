/* ARM composition test. Host widgets and parent adapter are explicit fixtures. */
#include "../private/mpclearn-ui-private.h"
#include "../private/mpc-3.9.1-profile.h"
#include "../examples/third-screen.h"
#include "../examples/slider-screen.h"
#include "../examples/toolkit-example.h"
#include "../screens/global-midi-learn-groups.h"
#include "../screens/global-midi-learn-screen.h"

using size_t = __SIZE_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;
extern "C" void *malloc(size_t);
extern "C" void free(void *);
extern "C" void *memset(void *, int, size_t) noexcept;
extern "C" void *memcpy(void *, const void *, size_t) noexcept;
extern "C" int strcmp(const char *, const char *);
extern "C" int printf(const char *, ...);
extern "C" void exit(int);
extern "C" long syscall(long, ...) noexcept;

static unsigned allocations, frees, button_ctors, button_dtors, colour_calls;
static unsigned list_ctors, list_dtors, list_updates, list_selections;
static unsigned list_viewport_gets, list_drag_enables, list_drag_disables;
static unsigned list_background_colours;
static unsigned label_ctors, label_dtors, label_text_updates;
static unsigned slider_ctors, slider_dtors, slider_adds, slider_removes;
static unsigned slider_silent_updates, slider_user_events;
static unsigned repaint_calls, toggle_calls, bounds_calls, visible_calls;
static unsigned keep_front_calls;
static unsigned attach_calls, detach_calls, focus_calls, callback_calls;
static unsigned host_add_calls, host_remove_calls, paint_calls;
static unsigned fail_detach, fail_focus;
static int host_add_relationship = 1, host_remove_relationship = 1;
static uint32_t intents[8];
static unsigned char base_button_vtable[212];
static unsigned char base_list_model_vtable[68];
static void *paint_button;
static uint32_t paint_fill, paint_text;
static int paint_width, paint_height;
static const char *paint_label;

static void require(bool value, const char *message) {
    if (!value) { printf("FAIL %s\n", message); exit(1); }
}

static void *fixture_allocate(uint32_t bytes) {
    void *value = malloc(bytes);
    if (value) { memset(value, 0, bytes); allocations++; }
    return value;
}
static void fixture_delete(void *value, uint32_t) { if (value) { frees++; free(value); } }
static void fixture_string_ctor(void *storage, const char *text) { *(const char **)storage = text; }
static void fixture_string_dtor(void *) {}
static void fixture_set_colour(void *, int colour, uint32_t) {
    if (colour == MPC_UI_LIST_BACKGROUND_COLOUR_ID) {
        list_background_colours++;
        return;
    }
    if (colour == MPC_UI_LABEL_BACKGROUND_COLOUR_ID ||
        colour == MPC_UI_LABEL_TEXT_COLOUR_ID) {
        colour_calls++;
        return;
    }
    require(colour >= 0x01000100 && colour <= 0x01000103, "qualified button colour id");
    colour_calls++;
}
static void fixture_button_ctor(void *button, const void *name) {
    const char *text = *(const char *const *)name;
    require(text != nullptr, "named TextButton construction");
    *(void **)button = base_button_vtable + 8;
    *(const char **)((unsigned char *)button + 0xa8) = text;
    button_ctors++;
}
static void fixture_button_dtor(void *button) {
    require(*(void **)button == base_button_vtable + 8,
            "base TextButton vptr restored before complete destructor");
    button_dtors++;
}
static void fixture_label_ctor(void *label, const void *name,
                               const void *text) {
    require(label && *(const char *const *)name && *(const char *const *)text,
            "named native Label construction");
    *(const char **)((unsigned char *)label + 0x20) =
        *(const char *const *)text;
    label_ctors++;
}
static void fixture_label_dtor(void *label) {
    require(label != nullptr, "native Label complete destructor");
    label_dtors++;
}
static void fixture_label_set_text(void *label, const void *text,
                                   int notification) {
    require(label && *(const char *const *)text && notification == 0,
            "native Label silent owner text refresh");
    *(const char **)((unsigned char *)label + 0x20) =
        *(const char *const *)text;
    label_text_updates++;
}
static void fixture_label_set_justification(void *label, int justification) {
    require(label && justification == 36,
            "native Label bounded justification");
}
static void fixture_component_set_intercepts_mouse(void *component,
                                                   int self, int children) {
    require(component && self == 0 && children == 0,
            "native Label passes touch to the owning screen");
}
static void fixture_list_ctor(void *list, const void *name, void *model) {
    require(list && *(const char *const *)name && model,
            "named ListBox construction");
    *(void **)((unsigned char *)list + 0x78) = model;
    *(int *)((unsigned char *)list + 0xa4) = -1;
    list_ctors++;
}
static void fixture_list_dtor(void *list) {
    require(list && !*(void **)((unsigned char *)list + 0x78),
            "ListBox model cleared before complete destructor");
    list_dtors++;
}
static void fixture_list_set_model(void *list, void *model) {
    *(void **)((unsigned char *)list + 0x78) = model;
}
static void fixture_list_update_content(void *list) {
    void *model = *(void **)((unsigned char *)list + 0x78);
    require(model && ((int (*)(void *))(*(void ***)model)[2])(model) >= 0,
            "ListBox content reads presentation count");
    list_updates++;
}
static void fixture_list_set_row_height(void *, int height) {
    require(height > 0 && height <= 256, "bounded ListBox row height");
}
static void fixture_list_select_row(void *list, int row, int dont_scroll,
                                    int deselect_others) {
    require((dont_scroll == 0 || dont_scroll == 1) && deselect_others == 1,
            "qualified ListBox selectRow flags");
    int *selected = (int *)((unsigned char *)list + 0xa4);
    if (*selected == row) return;
    *selected = row;
    void *model = *(void **)((unsigned char *)list + 0x78);
    if (model) ((void (*)(void *, int))(*(void ***)model)[8])(model, row);
    list_selections++;
}
static void *fixture_list_get_viewport(void *list) {
    require(list, "ListBox owns a viewport");
    list_viewport_gets++;
    return list;
}
static void fixture_viewport_set_drag(void *viewport, int enabled,
                                      float threshold) {
    require(viewport && threshold == MPC_UI_LIST_DRAG_THRESHOLD,
            "qualified viewport drag threshold");
    if (enabled) list_drag_enables++;
    else list_drag_disables++;
}
static void fixture_slider_ctor(void *slider) {
    require(slider, "native Slider construction");
    slider_ctors++;
}
static void fixture_slider_dtor(void *slider) {
    require(slider && !*(void **)((unsigned char *)slider + 0x28),
            "Slider listener removed before complete destructor");
    slider_dtors++;
}
static void fixture_slider_set_style(void *slider, int style) {
    require(slider && (style == 0 || style == 1),
            "qualified native Slider style");
    *(int *)((unsigned char *)slider + 0x30) = style;
}
static void fixture_slider_set_range(void *slider, double minimum,
                                     double maximum, double interval) {
    require(slider && minimum < maximum && interval >= 0.0,
            "bounded native Slider range");
}
static void fixture_slider_set_text_box(void *slider, int position,
                                        int read_only, int width, int height) {
    require(slider && position == 0 && read_only == 1 && width == 0 &&
                height == 0,
            "native Slider hides its text box");
}
static void fixture_slider_add_listener(void *slider, void *listener) {
    require(slider && listener &&
                !*(void **)((unsigned char *)slider + 0x28),
            "Slider borrows one toolkit listener");
    *(void **)((unsigned char *)slider + 0x28) = listener;
    slider_adds++;
}
static void fixture_slider_remove_listener(void *slider, void *listener) {
    require(slider && listener &&
                *(void **)((unsigned char *)slider + 0x28) == listener,
            "Slider removes its borrowed listener");
    *(void **)((unsigned char *)slider + 0x28) = nullptr;
    slider_removes++;
}
static double fixture_slider_get_value(void *slider) {
    return *(double *)((unsigned char *)slider + 0x20);
}
static void fixture_slider_set_value(void *slider, double value,
                                     int notification) {
    require(slider && notification == 0,
            "owner Slider refresh is silent");
    *(double *)((unsigned char *)slider + 0x20) = value;
    slider_silent_updates++;
}
struct Attached {
    void *object;
    int bounds[4];
    int visible;
};
struct ParentFixture {
    Attached children[64];
    unsigned count;
    int focused;
    int detach_reentered;
};
static ParentFixture *parent_fixture;
static ParentFixture component_fixture;
static unsigned char component_parent_object[32];

static Attached *attached(void *object) {
    for (unsigned i = 0; i < parent_fixture->count; ++i)
        if (parent_fixture->children[i].object == object) return &parent_fixture->children[i];
    for (unsigned i = 0; i < component_fixture.count; ++i)
        if (component_fixture.children[i].object == object)
            return &component_fixture.children[i];
    return nullptr;
}
static void fixture_host_add(void *parent, void *child, int index) {
    require(parent == component_parent_object && index == -1 &&
            component_fixture.count < 8, "faithful void Component add");
    host_add_calls++;
    if (!host_add_relationship) return;
    component_fixture.children[component_fixture.count++].object = child;
    *(void **)((unsigned char *)child + 0x0c) = parent;
}
static void fixture_host_remove(void *parent, void *child) {
    require(parent == component_parent_object && attached(child),
            "faithful void Component remove");
    host_remove_calls++;
    if (!host_remove_relationship) return;
    Attached *entry = attached(child);
    entry->object = nullptr;
    *(void **)((unsigned char *)child + 0x0c) = nullptr;
}
static void fixture_set_bounds(void *object, int x, int y, int width, int height) {
    Attached *child = attached(object); require(child != nullptr, "bounds after adapter attach");
    child->bounds[0] = x; child->bounds[1] = y;
    child->bounds[2] = width; child->bounds[3] = height; bounds_calls++;
}
static void fixture_set_visible(void *object, int visible) {
    Attached *child = attached(object); require(child != nullptr, "visibility after adapter attach");
    child->visible = visible != 0; visible_calls++;
}
static void fixture_set_always_on_top(void *object, int enabled) {
    require(object && enabled == 1, "qualified always-on-top request");
    *(uint32_t *)((unsigned char *)object + 0x68) |= 0x100u;
    keep_front_calls++;
}
static void fixture_set_toggle(void *, int selected, int notification) {
    require((selected == 0 || selected == 1) && notification == 0,
            "selection never emits a click notification");
    toggle_calls++;
}
static void fixture_repaint(void *) { repaint_calls++; }
static void fixture_graphics_fill_all(void *graphics, uint32_t colour) {
    require(graphics == &paint_calls, "qualified Graphics fixture");
    paint_fill = colour;
}
static void fixture_graphics_set_colour(void *graphics, uint32_t colour) {
    require(graphics == &paint_calls, "qualified Graphics colour fixture");
    paint_text = colour;
}
static void fixture_graphics_set_font(void *graphics, float height) {
    require(graphics == &paint_calls && height == 22.0f,
            "qualified Graphics font fixture");
}
static void fixture_graphics_draw_fitted_text(
    void *graphics, const void *text, int x, int y, int width, int height,
    int justification, int lines, float scale) {
    require(graphics == &paint_calls && paint_button &&
            *(const char *const *)text != nullptr && x == 8 && y == 0 &&
            justification == 36 && lines == 1 && scale == 0.0f,
            "qualified fitted label fixture");
    paint_width = width;
    paint_height = height;
    paint_label = *(const char *const *)text;
    paint_calls++;
}

static int fixture_attach(void *owner, void *child) {
    ParentFixture *parent = (ParentFixture *)owner;
    require(parent == parent_fixture && parent->count < 64, "adapter-owned bounded attach");
    parent->children[parent->count++].object = child;
    attach_calls++;
    return 1;
}
static void click(void *button) {
    void **address_point = *(void ***)button;
    auto callback = (void (*)(void *))address_point[46];
    require(callback != nullptr, "copied clicked slot");
    callback(button);
}
static void paint(void *button, int highlighted, int down) {
    void **address_point = *(void ***)button;
    auto callback = (void (*)(void *, void *, int, int))address_point[48];
    require(callback != nullptr, "copied full-button paint slot");
    paint_button = button;
    callback(button, &paint_calls, highlighted, down);
    paint_button = nullptr;
}
static void select_list(void *list, int row) {
    fixture_list_select_row(list, row, 0, 1);
}
static void paint_list(void *list, int row, int selected, int width, int height) {
    void *model = *(void **)((unsigned char *)list + 0x78);
    require(model, "live borrowed ListBox presentation model");
    auto callback = (void (*)(void *, int, void *, int, int, int))
        (*(void ***)model)[3];
    require(callback, "copied ListBox paint slot");
    paint_button = list;
    callback(model, row, &paint_calls, width, height, selected);
    paint_button = nullptr;
}
static void gesture_slider(void *slider, double value) {
    void *listener = *(void **)((unsigned char *)slider + 0x28);
    require(listener, "live borrowed Slider listener");
    void **slots = *(void ***)listener;
    auto begin = (void (*)(void *, void *))slots[3];
    auto changed = (void (*)(void *, void *))slots[2];
    auto end = (void (*)(void *, void *))slots[4];
    require(begin && changed && end, "Slider listener event slots");
    begin(listener, slider);
    *(double *)((unsigned char *)slider + 0x20) = value;
    changed(listener, slider);
    end(listener, slider);
    slider_user_events += 3;
}
static int fixture_detach(void *owner, void *child) {
    ParentFixture *parent = (ParentFixture *)owner;
    require(parent == parent_fixture && attached(child), "adapter-owned detach of attached child");
    if (!parent->detach_reentered) {
        parent->detach_reentered = 1;
        unsigned before = callback_calls;
        click(child);
        require(callback_calls == before, "bindings invalidated before detach reentry");
    }
    if (fail_detach) { fail_detach--; return 0; }
    Attached *entry = attached(child); entry->object = nullptr;
    detach_calls++;
    return 1;
}
static int fixture_focus(void *owner, int active) {
    ParentFixture *parent = (ParentFixture *)owner;
    require(parent == parent_fixture && (active == 0 || active == 1), "adapter-owned focus");
    if (fail_focus && !--fail_focus) return 0;
    if (active) parent->focused++;
    else { require(parent->focused > 0, "balanced adapter focus"); parent->focused--; }
    focus_calls++; return 1;
}
struct IntentOwner { MpclearnThirdApp app; };
static void intent(void *opaque, uint32_t value) {
    require(callback_calls < 8, "bounded intents");
    intents[callback_calls++] = value;
    require(opaque != nullptr, "public app intent owner");
}

struct InvalidateOnClick {
    MpcUiScreen **screen;
    unsigned calls;
    int close_result;
};
static void invalidate_on_click(void *opaque, uint32_t) {
    InvalidateOnClick *owner = (InvalidateOnClick *)opaque;
    owner->calls++;
    require(mpc_ui_private_screen_invalidate(*owner->screen),
            "forced owner can invalidate a non-consumer screen in callback");
    owner->close_result = mpc_ui_screen_close(owner->screen);
}

struct FailedApp { MpclearnThirdScreen *screen; };
static void ignored_intent(void *, uint32_t) {}
static int failed_app_open(void *opaque, MpcUiAppHost *, MpcUiContext *context,
                           MpcUiParent *parent, MpcUiRect bounds) {
    FailedApp *app = (FailedApp *)opaque;
    app->screen = mpclearn_third_screen_open(
        context, parent, bounds, ignored_intent, app);
    return 0;
}
static int failed_app_prepare(void *, int) {
    return MPC_UI_APP_CLOSE_READY;
}
static int failed_app_close(void *opaque) {
    FailedApp *app = (FailedApp *)opaque;
    return app && app->screen && mpclearn_third_screen_close(&app->screen);
}
static const MpcUiAppCallbacks failed_app_callbacks = {
    sizeof(MpcUiAppCallbacks), failed_app_open,
    failed_app_prepare, failed_app_close,
};

struct PendingApp { MpclearnThirdScreen *screen; unsigned prepares; };
static int pending_app_open(void *opaque, MpcUiAppHost *, MpcUiContext *context,
                            MpcUiParent *parent, MpcUiRect bounds) {
    PendingApp *app = (PendingApp *)opaque;
    app->screen = mpclearn_third_screen_open(
        context, parent, bounds, ignored_intent, app);
    return app->screen != nullptr;
}
static int pending_app_prepare(void *opaque, int forced) {
    PendingApp *app = (PendingApp *)opaque;
    if (forced) return MPC_UI_APP_CLOSE_READY;
    return app->prepares++ ? MPC_UI_APP_CLOSE_READY
                           : MPC_UI_APP_CLOSE_PENDING;
}
static int pending_app_close(void *opaque) {
    PendingApp *app = (PendingApp *)opaque;
    return app && app->screen && mpclearn_third_screen_close(&app->screen);
}
static const MpcUiAppCallbacks pending_app_callbacks = {
    sizeof(MpcUiAppCallbacks), pending_app_open,
    pending_app_prepare, pending_app_close,
};

struct MultiScreenApp {
    MpcUiAppHost *host;
    MpcUiScreen *shown;
    MpcUiScreen *shown_second;
    MpcUiScreen *hidden;
    MpcUiControl *shown_button;
    MpcUiControl *shown_second_button;
    MpcUiControl *hidden_button;
    int callback_suspend_result;
};
static void multi_screen_intent(void *opaque, uint32_t) {
    MultiScreenApp *app = (MultiScreenApp *)opaque;
    app->callback_suspend_result = mpc_ui_private_app_host_suspend(app->host);
}
static int multi_screen_open(void *opaque, MpcUiAppHost *host,
                             MpcUiContext *context, MpcUiParent *parent,
                             MpcUiRect bounds) {
    MultiScreenApp *app = (MultiScreenApp *)opaque;
    app->host = host;
    app->shown = mpc_ui_screen_create(context, parent, bounds);
    app->shown_second = mpc_ui_screen_create(context, parent, bounds);
    app->hidden = mpc_ui_screen_create(context, parent, bounds);
    app->shown_button = app->shown ? mpc_ui_button_create(
        app->shown, "SHOWN", 1, multi_screen_intent, app) : nullptr;
    app->hidden_button = app->hidden ? mpc_ui_button_create(
        app->hidden, "HIDDEN", 2, multi_screen_intent, app) : nullptr;
    app->shown_second_button = app->shown_second ? mpc_ui_button_create(
        app->shown_second, "SHOWN TWO", 3, multi_screen_intent, app) : nullptr;
    if (!app->shown || !app->shown_second || !app->hidden ||
        !app->shown_button || !app->shown_second_button || !app->hidden_button ||
        !mpc_ui_control_place(app->shown_button, {0, 0, 120, 60}) ||
        !mpc_ui_control_place(app->shown_second_button, {120, 0, 120, 60}) ||
        !mpc_ui_control_place(app->hidden_button, {0, 0, 120, 60}) ||
        !mpc_ui_screen_show(app->shown) ||
        !mpc_ui_screen_show(app->shown_second) ||
        !mpc_ui_screen_show(app->hidden) ||
        !mpc_ui_screen_hide(app->hidden)) return 0;
    return 1;
}
static int multi_screen_prepare(void *, int) {
    return MPC_UI_APP_CLOSE_READY;
}
static int multi_screen_close(void *opaque) {
    MultiScreenApp *app = (MultiScreenApp *)opaque;
    int ok = 1;
    if (app->hidden && !mpc_ui_screen_close(&app->hidden)) ok = 0;
    if (app->shown_second && !mpc_ui_screen_close(&app->shown_second)) ok = 0;
    if (app->shown && !mpc_ui_screen_close(&app->shown)) ok = 0;
    if (ok) {
        app->shown_button = nullptr;
        app->shown_second_button = nullptr;
        app->hidden_button = nullptr;
        app->host = nullptr;
    }
    return ok;
}
static const MpcUiAppCallbacks multi_screen_callbacks = {
    sizeof(MpcUiAppCallbacks), multi_screen_open,
    multi_screen_prepare, multi_screen_close,
};

struct GlobalIntentOwner { uint32_t values[32]; unsigned count; };
static void global_intent(void *opaque, uint32_t value) {
    GlobalIntentOwner *owner = (GlobalIntentOwner *)opaque;
    require(owner && owner->count < 32, "bounded assignment intents");
    owner->values[owner->count++] = value;
}

struct SliderOwner {
    uint32_t actions[4];
    uint32_t events[8];
    double values[8];
    unsigned action_count;
    unsigned event_count;
    MpclearnSliderScreen *screen;
    int close_result;
};
static void slider_action(void *opaque, uint32_t intent_value) {
    SliderOwner *owner = (SliderOwner *)opaque;
    require(owner && owner->action_count < 4, "bounded Slider actions");
    owner->actions[owner->action_count++] = intent_value;
    if (intent_value == MPCLEARN_SLIDER_BACK)
        owner->close_result = mpclearn_slider_screen_close(owner->screen);
}
static void slider_event(void *opaque, uint32_t intent_value,
                         uint32_t event, double value) {
    SliderOwner *owner = (SliderOwner *)opaque;
    require(owner && intent_value == MPCLEARN_SLIDER_VALUE &&
                owner->event_count < 8,
            "consumer-defined Slider intent");
    owner->events[owner->event_count] = event;
    owner->values[owner->event_count] = value;
    owner->event_count++;
}

struct ToolkitOwner {
    MpclearnToolkitExampleApp app;
    uint32_t actions[4];
    uint32_t events[8];
    unsigned action_count;
    unsigned event_count;
};
static void toolkit_action(void *opaque, uint32_t intent_value) {
    ToolkitOwner *owner = (ToolkitOwner *)opaque;
    require(owner && owner->action_count < 4, "bounded toolkit actions");
    owner->actions[owner->action_count++] = intent_value;
}
static void toolkit_slider(void *opaque, uint32_t intent_value,
                           uint32_t event, double) {
    ToolkitOwner *owner = (ToolkitOwner *)opaque;
    require(owner && intent_value == MPCLEARN_TOOLKIT_EXAMPLE_SLIDER &&
                owner->event_count < 8,
            "combined example Slider intent");
    owner->events[owner->event_count++] = event;
}

static void require_bounds(unsigned index, int x, int y, int width, int height) {
    Attached &child = parent_fixture->children[index];
    require(child.bounds[0] == x && child.bounds[1] == y &&
            child.bounds[2] == width && child.bounds[3] == height,
            "parent-local grid bounds");
}

int main() {
    MpcUiPrivateHost host = {
        fixture_allocate, fixture_delete, fixture_string_ctor, fixture_string_dtor,
        fixture_set_colour, fixture_button_ctor, fixture_button_dtor,
        fixture_label_ctor, fixture_label_dtor, fixture_label_set_text,
        fixture_label_set_justification,
        fixture_component_set_intercepts_mouse,
        fixture_list_ctor, fixture_list_dtor, fixture_list_set_model,
        fixture_list_update_content, fixture_list_set_row_height,
        fixture_list_select_row,
        fixture_list_get_viewport, fixture_viewport_set_drag,
        fixture_slider_ctor, fixture_slider_dtor,
        fixture_slider_set_style, fixture_slider_set_range,
        fixture_slider_set_text_box, fixture_slider_add_listener,
        fixture_slider_remove_listener, fixture_slider_get_value,
        fixture_slider_set_value,
        fixture_host_add, fixture_host_remove, fixture_set_bounds,
        fixture_set_visible, fixture_set_always_on_top, fixture_set_toggle,
        fixture_repaint,
        fixture_graphics_set_colour, fixture_graphics_set_font,
        fixture_graphics_fill_all, fixture_graphics_draw_fitted_text,
        base_button_vtable, base_list_model_vtable,
    };
    uint32_t thread = (uint32_t)syscall(224);
    require(mpc_ui_private_context_for_component(&host, thread + 1) == nullptr,
            "context creation requires current admitted UI thread");
    MpcUiContext *context = mpc_ui_private_context_for_component(&host, thread);
    require(context != nullptr && mpc_ui_context_error(context) == MPC_UI_OK,
            "per-instance admitted context fixture");
    ParentFixture fixture = {}; parent_fixture = &fixture;
    MpcUiParent *parent = mpc_ui_private_parent_create(
        context, &fixture, fixture_attach, fixture_detach, fixture_focus);
    require(parent != nullptr, "opaque adapter parent");
    require(!mpc_ui_private_context_destroy(&context) &&
            mpc_ui_context_error(context) == MPC_UI_LIFETIME_ORDER,
            "context cannot outlive parent ownership");
    mpc_ui_context_clear_error(context);

    MpcUiRect bounds = {20, 30, 800, 480};
    IntentOwner intent_owner = {};
    MpcUiAppHost *app_host = mpc_ui_private_app_host_create(
        context, parent, bounds);
    require(app_host &&
            mpclearn_third_app_init(&intent_owner.app, intent, &intent_owner) &&
            mpc_ui_app_register(app_host, mpclearn_third_app_callbacks(),
                                &intent_owner.app) &&
            !mpc_ui_app_register(app_host, mpclearn_third_app_callbacks(),
                                 &intent_owner.app) &&
            mpc_ui_app_open(app_host),
            "one public app consumer opens through opaque admitted host");
    mpc_ui_context_clear_error(context);
    MpclearnThirdScreen *screen = intent_owner.app.screen;
    require(screen != nullptr && fixture.count == 4 && attach_calls == 4 &&
            bounds_calls == 4 && fixture.focused == 1,
            "public-only third screen composed through toolkit");
    require_bounds(0, 44, 54, 752, 96);
    require_bounds(1, 44, 166, 368, 96);
    require_bounds(2, 428, 166, 368, 96);
    require_bounds(3, 44, 390, 752, 96);
    require(colour_calls == 16 && repaint_calls == 4,
            "stock widget colour painting configured through public API");
    for (unsigned i = 0; i < 4; ++i) require(fixture.children[i].visible, "screen show");
    paint(fixture.children[0].object, 0, 0);
    require(paint_fill == 0xff202126u && paint_text == 0xffffffffu &&
            paint_width == 736 && paint_height == 96,
            "full-button title paint uses stored normal style and bounds");
    paint(fixture.children[1].object, 1, 0);
    require(paint_fill == 0xffd31145u && paint_text == 0xffffffffu &&
            paint_width == 352 && paint_height == 96,
            "highlight paint uses stored selected style and fitted label bounds");
    click(fixture.children[1].object);
    click(fixture.children[2].object);
    click(fixture.children[3].object);
    require(callback_calls == 3 && intents[0] == MPCLEARN_THIRD_ACTION_ONE &&
            intents[1] == MPCLEARN_THIRD_ACTION_TWO && intents[2] == MPCLEARN_THIRD_BACK,
            "instance bindings publish only declared intents");
    require(intent_owner.app.screen == screen && mpc_ui_app_is_open(app_host) &&
            mpc_ui_app_is_closing(app_host) &&
            mpc_ui_context_error(context) == MPC_UI_OK,
            "Back requests deferred close without destroying its callback stack");
    require(!mpc_ui_private_parent_destroy(&parent), "parent cannot precede screen teardown");
    mpc_ui_context_clear_error(context);

    fail_detach = 1;
    require(!mpc_ui_private_app_host_drain(app_host) &&
            intent_owner.app.screen == screen && mpc_ui_app_is_open(app_host) &&
            mpc_ui_context_error(context) == MPC_UI_DETACH_FAILED,
            "failed detach retains app consumer and screen ownership for retry");
    require(callback_calls == 3 && button_dtors == 0,
            "failed detach cannot destroy or deliver reentrant intent");
    mpc_ui_context_clear_error(context);
    require(mpc_ui_private_app_host_drain(app_host) &&
            !intent_owner.app.screen && !mpc_ui_app_is_open(app_host) &&
            mpc_ui_app_unregister(app_host) &&
            mpc_ui_private_app_host_destroy(&app_host, 0) && !app_host,
            "post-callback retry closes, unregisters and releases public app host");
    require(detach_calls == 4 && button_dtors == 4 && fixture.focused == 0,
            "all controls detached before base destruction");

    MpcUiScreen *selection = mpc_ui_screen_create(context, parent, {0, 0, 120, 80});
    MpcUiControl *choice = mpc_ui_button_create(selection, "SELECTED", 0, nullptr, nullptr);
    require(selection && choice &&
            mpc_ui_button_style(choice, {0xff000000u, 0xffffffffu, 0xffffffffu}) &&
            mpc_ui_control_place(choice, {10, 10, 100, 60}) &&
            mpc_ui_button_selected(choice, 1) && mpc_ui_screen_show(selection) &&
            mpc_ui_control_visible(choice, 0) &&
            mpc_ui_control_visible(choice, 1) &&
            mpc_ui_screen_hide(selection),
            "public selection, control visibility, absolute layout and show/hide path");
    paint(fixture.children[4].object, 0, 0);
    require(paint_fill == 0xffffffffu && paint_text == 0xffffffffu &&
            paint_width == 84 && paint_height == 60,
            "selected state drives the full-button paint boundary");
    require(mpc_ui_screen_close(&selection), "selected screen teardown");
    require(toggle_calls == 1 && attach_calls == 5 && detach_calls == 5 &&
            button_dtors == 5, "selected host widget owns one complete lifetime");

    MpclearnSliderScreen slider_screen = {};
    SliderOwner slider_owner = {};
    slider_owner.screen = &slider_screen;
    unsigned callbacks_before_slider = callback_calls;
    require(mpclearn_slider_screen_open(
                &slider_screen, context, parent, bounds, 63.0,
                slider_action, slider_event, &slider_owner) &&
            slider_screen.screen && slider_screen.title &&
            slider_screen.slider && slider_screen.back,
            "public-only native Label and Slider example opens");
    require_bounds(5, 44, 54, 752, 72);
    require_bounds(6, 92, 166, 656, 96);
    require_bounds(7, 44, 414, 752, 72);
    require(mpclearn_slider_screen_set_title(&slider_screen,
                                              "OWNER VALUE: 64") &&
            mpclearn_slider_screen_set_value(&slider_screen, 64.0) &&
            mpc_ui_control_visible(slider_screen.title, 0) &&
            mpc_ui_control_visible(slider_screen.title, 1) &&
            mpc_ui_slider_visible(slider_screen.slider, 0) &&
            mpc_ui_slider_visible(slider_screen.slider, 1) &&
            slider_owner.event_count == 0,
            "Label and Slider owner refresh and visibility are silent");
    void *slider_object =
        mpc_ui_private_slider_host_object(slider_screen.slider);
    require(slider_object == fixture.children[6].object,
            "opaque Slider retains its admitted native object privately");
    gesture_slider(slider_object, 80.0);
    require(slider_owner.event_count == 3 &&
            slider_owner.events[0] == MPC_UI_SLIDER_DRAG_STARTED &&
            slider_owner.events[1] == MPC_UI_SLIDER_VALUE_CHANGED &&
            slider_owner.events[2] == MPC_UI_SLIDER_DRAG_ENDED &&
            slider_owner.values[0] == 64.0 &&
            slider_owner.values[1] == 80.0 &&
            slider_owner.values[2] == 80.0,
            "Slider publishes consumer intent with value and gesture lifecycle");
    click(fixture.children[7].object);
    require(slider_owner.action_count == 1 &&
            slider_owner.actions[0] == MPCLEARN_SLIDER_BACK &&
            !slider_owner.close_result && slider_screen.screen &&
            mpc_ui_context_error(context) == MPC_UI_CALLBACK_ACTIVE,
            "Slider example cannot close inside its active Back callback");
    mpc_ui_context_clear_error(context);
    require(mpclearn_slider_screen_close(&slider_screen) &&
                !slider_screen.screen &&
                callback_calls == callbacks_before_slider &&
                slider_ctors == 1 && slider_dtors == 1 &&
                slider_adds == 1 && slider_removes == 1 &&
                label_ctors == 1 && label_dtors == 1 &&
                label_text_updates == 1 && slider_user_events == 3,
            "Label/Slider example removes listener, detaches and balances lifetime");

    ToolkitOwner toolkit_owner = {};
    unsigned toolkit_base = fixture.count;
    MpcUiAppHost *toolkit_host = mpc_ui_private_app_host_create(
        context, parent, bounds);
    require(toolkit_host &&
            mpclearn_toolkit_example_app_init(
                &toolkit_owner.app, 64.0, toolkit_action,
                toolkit_slider, &toolkit_owner) &&
            mpc_ui_app_register(
                toolkit_host, mpclearn_toolkit_example_app_callbacks(),
                &toolkit_owner.app) &&
            mpc_ui_app_open(toolkit_host) &&
            toolkit_owner.app.view.screen && fixture.count == toolkit_base + 5,
            "combined public-only app opens through opaque host");
    require_bounds(toolkit_base, 44, 54, 752, 64);
    require_bounds(toolkit_base + 1, 44, 142, 752, 96);
    require_bounds(toolkit_base + 2, 44, 262, 364, 96);
    require_bounds(toolkit_base + 3, 432, 262, 364, 96);
    require_bounds(toolkit_base + 4, 44, 414, 752, 72);
    require(mpclearn_toolkit_example_set_title(
                &toolkit_owner.app, "OWNER REFRESH") &&
            mpclearn_toolkit_example_set_value(&toolkit_owner.app, 32.0) &&
            toolkit_owner.action_count == 0 && toolkit_owner.event_count == 0,
            "combined app owner Label and Slider refresh is silent");
    click(fixture.children[toolkit_base + 2].object);
    click(fixture.children[toolkit_base + 3].object);
    gesture_slider(fixture.children[toolkit_base + 1].object, 80.0);
    click(fixture.children[toolkit_base + 4].object);
    require(toolkit_owner.action_count == 3 &&
            toolkit_owner.actions[0] == MPCLEARN_TOOLKIT_EXAMPLE_ACTION_ONE &&
            toolkit_owner.actions[1] == MPCLEARN_TOOLKIT_EXAMPLE_ACTION_TWO &&
            toolkit_owner.actions[2] == MPCLEARN_TOOLKIT_EXAMPLE_BACK &&
            toolkit_owner.event_count == 3 &&
            mpc_ui_app_is_closing(toolkit_host),
            "combined app publishes actions, Slider gesture and deferred Back");
    require(mpc_ui_private_app_host_drain(toolkit_host) &&
            !toolkit_owner.app.view.screen &&
            mpc_ui_app_unregister(toolkit_host) &&
            mpc_ui_private_app_host_destroy(&toolkit_host, 0) && !toolkit_host,
            "combined app closes after callback and releases opaque host");

    uint32_t expected_target = 0;
    for (uint32_t group = 0; group < MPCLEARN_GLOBAL_LEARN_GROUP_COUNT;
         ++group) {
        const MpclearnGlobalLearnGroup *entry =
            mpclearn_global_learn_group(group);
        require(entry && entry->name && entry->name[0] &&
                entry->first_target == expected_target &&
                entry->target_count != 0,
                "functional target groups are contiguous and named");
        for (uint32_t index = 0; index < entry->target_count; ++index) {
            uint32_t found_group = 99, found_index = 99;
            require(mpclearn_global_learn_target_location(
                        entry->first_target + index, &found_group,
                        &found_index) &&
                    found_group == group && found_index == index,
                    "target resolves to its functional group and row");
        }
        expected_target += entry->target_count;
    }
    require(expected_target == MPCLEARN_GLOBAL_LEARN_TARGET_COUNT &&
            !mpclearn_global_learn_group(MPCLEARN_GLOBAL_LEARN_GROUP_COUNT),
            "functional groups cover exactly the 120 owner targets");
    uint32_t untouched_group = 91, untouched_index = 92;
    require(!mpclearn_global_learn_target_location(
                MPCLEARN_GLOBAL_LEARN_TARGET_COUNT, &untouched_group,
                &untouched_index) &&
            untouched_group == 91 && untouched_index == 92,
            "out-of-range targets do not fabricate a group");
    MpclearnGlobalLearnLayout grouped_layout = {};
    require(mpclearn_global_learn_layout(
                {0, 0, 1280, 690}, &grouped_layout) &&
            grouped_layout.title.x == 32 &&
            grouped_layout.mapping_previous.x == 296 &&
            grouped_layout.mapping.x == 380 &&
            grouped_layout.mapping_next.x == 644 &&
            grouped_layout.file_new.x == 732 &&
            grouped_layout.file_export.x == 1128 &&
            grouped_layout.groups.x == 32 && grouped_layout.groups.width == 248 &&
            grouped_layout.targets.x == 296 && grouped_layout.targets.width == 420 &&
            grouped_layout.detail.x == 732 && grouped_layout.detail.width == 516 &&
            grouped_layout.learn.y == 578 && grouped_layout.back.x == 1028 &&
            !mpclearn_global_learn_layout({0, 0, 800, 480}, &grouped_layout),
            "three-column assignment layout reserves group, target and detail areas");

    GlobalIntentOwner global_owner = {};
    MpclearnGlobalLearnScreen global = {};
    require(mpclearn_global_learn_screen_open(
                &global, context, parent, {0, 0, 1280, 690},
                global_intent, &global_owner),
            "public assignment editor opens through toolkit");
    const char *rows[MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS] = {
        "025 Q-LINK 1", "026 Q-LINK 2", "027 Q-LINK 3"};
    MpclearnGlobalLearnView global_view = {};
    global_view.target_text[0] = rows[0];
    global_view.target_text[1] = rows[1];
    global_view.target_text[2] = rows[2];
    global_view.selected_target_text = "026 Q-LINK 2";
    global_view.assignment_text = "ASSIGNMENT: CH1 CC14 ABS";
    global_view.status_text = "READY TO LEARN";
    global_view.mapping_text = "CURRENT: TEST COPY";
    global_view.edit_type_text = "TYPE: CC";
    global_view.edit_channel_text = "CHANNEL: 1";
    global_view.edit_data_text = "NUMBER: 14";
    global_view.edit_mode_text = "MODE: ABS CC";
    global_view.edit_reverse_text = "REVERSE: OFF";
    global_view.target_count = 3;
    global_view.selected_group = 2;
    global_view.selected_target_row = 1;
    global_view.can_learn = 1;
    global_view.can_new = 1;
    global_view.can_copy = 1;
    global_view.can_load = 1;
    global_view.can_export = 1;
    global_view.can_clear = 1;
    global_view.can_previous_file = 1;
    global_view.can_next_file = 1;
    global_view.can_edit = 1;
    global_view.can_edit_data = 1;
    global_view.can_apply = 1;
    unsigned repaint_before_update = repaint_calls;
    require(mpclearn_global_learn_screen_update(&global, &global_view),
            "group, target names, selected detail and Learn update");
    require(repaint_calls >= repaint_before_update + 2,
            "same-count list updates invalidate both native list paints");
    require(global_owner.count == 0,
            "programmatic list selection suppresses consumer intent");
    paint_list(mpc_ui_private_list_host_object(global.targets), 1, 1, 420, 52);
    require(paint_fill == 0xffd31145u &&
            paint_label && !strcmp(paint_label, rows[1]),
            "selected native target row uses accent paint");
    select_list(mpc_ui_private_list_host_object(global.groups), 3);
    select_list(mpc_ui_private_list_host_object(global.targets), 2);
    click(mpc_ui_private_control_host_object(global.file_new));
    click(mpc_ui_private_control_host_object(global.file_copy));
    click(mpc_ui_private_control_host_object(global.file_load));
    click(mpc_ui_private_control_host_object(global.file_export));
    click(mpc_ui_private_control_host_object(global.clear_assignment));
    click(mpc_ui_private_control_host_object(global.mapping_previous));
    click(mpc_ui_private_control_host_object(global.mapping_next));
    click(mpc_ui_private_control_host_object(global.edit_type));
    click(mpc_ui_private_control_host_object(global.edit_channel_down));
    click(mpc_ui_private_control_host_object(global.edit_channel_up));
    click(mpc_ui_private_control_host_object(global.edit_data_down));
    click(mpc_ui_private_control_host_object(global.edit_data_up));
    click(mpc_ui_private_control_host_object(global.edit_mode));
    click(mpc_ui_private_control_host_object(global.edit_reverse));
    click(mpc_ui_private_control_host_object(global.edit_apply));
    click(mpc_ui_private_control_host_object(global.learn));
    global_view.learning = 1;
    global_view.status_text = "LEARNING: MOVE A CONTROL";
    require(mpclearn_global_learn_screen_update(&global, &global_view) &&
            !attached(mpc_ui_private_control_host_object(global.learn))->visible &&
            attached(mpc_ui_private_control_host_object(global.cancel))->visible,
            "learning view swaps Learn for Cancel");
    click(mpc_ui_private_control_host_object(global.cancel));
    click(mpc_ui_private_control_host_object(global.back));
    require(global_owner.count == 20 &&
            global_owner.values[0] == MPCLEARN_GLOBAL_LEARN_GROUP_1 + 3 &&
            global_owner.values[1] == MPCLEARN_GLOBAL_LEARN_TARGET_1 + 2 &&
            global_owner.values[2] == MPCLEARN_GLOBAL_LEARN_NEW &&
            global_owner.values[3] == MPCLEARN_GLOBAL_LEARN_COPY &&
            global_owner.values[4] == MPCLEARN_GLOBAL_LEARN_LOAD &&
            global_owner.values[5] == MPCLEARN_GLOBAL_LEARN_EXPORT &&
            global_owner.values[6] == MPCLEARN_GLOBAL_LEARN_CLEAR &&
            global_owner.values[7] == MPCLEARN_GLOBAL_LEARN_PREVIOUS_FILE &&
            global_owner.values[8] == MPCLEARN_GLOBAL_LEARN_NEXT_FILE &&
            global_owner.values[9] == MPCLEARN_GLOBAL_LEARN_TYPE &&
            global_owner.values[10] == MPCLEARN_GLOBAL_LEARN_CHANNEL_DOWN &&
            global_owner.values[11] == MPCLEARN_GLOBAL_LEARN_CHANNEL_UP &&
            global_owner.values[12] == MPCLEARN_GLOBAL_LEARN_DATA_DOWN &&
            global_owner.values[13] == MPCLEARN_GLOBAL_LEARN_DATA_UP &&
            global_owner.values[14] == MPCLEARN_GLOBAL_LEARN_MODE &&
            global_owner.values[15] == MPCLEARN_GLOBAL_LEARN_REVERSE &&
            global_owner.values[16] == MPCLEARN_GLOBAL_LEARN_APPLY &&
            global_owner.values[17] == MPCLEARN_GLOBAL_LEARN_BEGIN &&
            global_owner.values[18] == MPCLEARN_GLOBAL_LEARN_CANCEL &&
            global_owner.values[19] == MPCLEARN_GLOBAL_LEARN_BACK,
            "assignment editor publishes mapping-file, manual draft, clear, group, target, Learn, cancel and Back intents");
    require(mpclearn_global_learn_screen_close(&global) && !global.screen,
            "grouped assignment editor balances public toolkit screen lifetime");
    require(list_ctors == 2 && list_dtors == 2 && list_updates >= 4,
            "native lists clear borrowed models before balanced destruction");
    require(list_viewport_gets == list_ctors &&
                list_drag_enables == list_ctors &&
                list_drag_disables == list_dtors &&
                list_background_colours >= list_ctors,
            "native lists configure drag and background, then disable drag before teardown");

    MultiScreenApp multi = {};
    MpcUiAppHost *multi_host = mpc_ui_private_app_host_create(
        context, parent, bounds);
    require(multi_host &&
            mpc_ui_app_register(multi_host, &multi_screen_callbacks, &multi) &&
            mpc_ui_app_open(multi_host) && multi.shown && multi.shown_second &&
            multi.hidden && fixture.focused == 2,
            "multi-screen public consumer opens with two visible pages");
    void *multi_shown = mpc_ui_private_control_host_object(multi.shown_button);
    void *multi_shown_second =
        mpc_ui_private_control_host_object(multi.shown_second_button);
    void *multi_hidden = mpc_ui_private_control_host_object(multi.hidden_button);
    require(attached(multi_shown) && attached(multi_shown)->visible &&
            attached(multi_shown_second) && attached(multi_shown_second)->visible &&
            attached(multi_hidden) && !attached(multi_hidden)->visible,
            "multi-screen consumer retains a hidden page");
    click(multi_shown);
    require(!multi.callback_suspend_result &&
            !mpc_ui_private_app_host_is_suspended(multi_host) &&
            mpc_ui_context_error(context) == MPC_UI_CALLBACK_ACTIVE,
            "active consumer callback refuses suspension without changing presentation");
    mpc_ui_context_clear_error(context);
    fail_focus = 2;
    require(!mpc_ui_private_app_host_suspend(multi_host) &&
            !mpc_ui_private_app_host_is_suspended(multi_host) &&
            attached(multi_shown)->visible && attached(multi_shown_second)->visible &&
            !attached(multi_hidden)->visible && fixture.focused == 2,
            "focus refusal unwinds suspension and preserves visible pages");
    mpc_ui_context_clear_error(context);
    require(mpc_ui_private_app_host_suspend(multi_host) &&
            mpc_ui_private_app_host_is_suspended(multi_host) &&
            !attached(multi_shown)->visible &&
            !attached(multi_shown_second)->visible &&
            !attached(multi_hidden)->visible &&
            fixture.focused == 0 &&
            mpc_ui_private_app_host_suspend(multi_host),
            "suspension hides every shown page and is idempotent");
    fail_focus = 2;
    require(!mpc_ui_private_app_host_resume(multi_host) &&
            mpc_ui_private_app_host_is_suspended(multi_host) &&
            !attached(multi_shown)->visible &&
            !attached(multi_shown_second)->visible &&
            !attached(multi_hidden)->visible && fixture.focused == 0,
            "resume refusal unwinds partially shown pages and remains suspended");
    mpc_ui_context_clear_error(context);
    require(mpc_ui_private_app_host_resume(multi_host) &&
            !mpc_ui_private_app_host_is_suspended(multi_host) &&
            attached(multi_shown)->visible &&
            attached(multi_shown_second)->visible &&
            !attached(multi_hidden)->visible && fixture.focused == 2 &&
            mpc_ui_private_app_host_resume(multi_host),
            "resume restores only pages that were shown before suspension");
    require(mpc_ui_private_app_host_suspend(multi_host) &&
            mpc_ui_app_request_close(multi_host) &&
            !mpc_ui_private_app_host_resume(multi_host) &&
            mpc_ui_context_error(context) == MPC_UI_LIFETIME_ORDER,
            "pending close refuses resume and preserves the close owner");
    mpc_ui_context_clear_error(context);
    require(mpc_ui_private_app_host_drain(multi_host) &&
            !mpc_ui_app_is_open(multi_host) && !multi.shown &&
            !multi.shown_second && !multi.hidden &&
            mpc_ui_app_unregister(multi_host) &&
            mpc_ui_private_app_host_destroy(&multi_host, 0) && !multi_host &&
            fixture.focused == 0,
            "suspended multi-screen consumer closes and releases through its owner");

    PendingApp pending_app = {};
    MpcUiAppHost *pending_host = mpc_ui_private_app_host_create(
        context, parent, bounds);
    require(pending_host &&
            mpc_ui_app_register(pending_host, &pending_app_callbacks,
                                &pending_app) &&
            mpc_ui_app_open(pending_host) &&
            mpc_ui_app_request_close(pending_host) &&
            mpc_ui_private_app_host_drain(pending_host) &&
            pending_app.prepares == 1 && pending_app.screen &&
            mpc_ui_app_is_closing(pending_host),
            "pending prepare-close retains consumer and screen for the next UI drain");
    require(mpc_ui_private_app_host_drain(pending_host) &&
            pending_app.prepares == 2 && !pending_app.screen &&
            !mpc_ui_app_is_open(pending_host) &&
            mpc_ui_app_unregister(pending_host) &&
            mpc_ui_private_app_host_destroy(&pending_host, 0) && !pending_host,
            "ready prepare-close releases consumer after the later UI drain");

    FailedApp failed_app = {};
    MpcUiAppHost *failed_host = mpc_ui_private_app_host_create(
        context, parent, bounds);
    require(failed_host &&
            mpc_ui_app_register(failed_host, &failed_app_callbacks, &failed_app) &&
            !mpc_ui_app_open(failed_host) && !failed_app.screen &&
            !mpc_ui_app_is_open(failed_host),
            "failed consumer open closes its partial screen and releases the active slot");
    mpc_ui_context_clear_error(context);
    require(mpc_ui_app_unregister(failed_host) &&
            mpc_ui_private_app_host_destroy(&failed_host, 0) && !failed_host,
            "failed-open host remains reusable and releasable");

    IntentOwner forced_owner = {};
    MpcUiAppHost *forced_host = mpc_ui_private_app_host_create(
        context, parent, bounds);
    require(forced_host &&
            mpclearn_third_app_init(&forced_owner.app, intent, &forced_owner) &&
            mpc_ui_app_register(forced_host, mpclearn_third_app_callbacks(),
                                &forced_owner.app) &&
            mpc_ui_app_open(forced_host) && forced_owner.app.screen &&
            mpc_ui_private_app_host_destroy(&forced_host, 1) && !forced_host &&
            !forced_owner.app.screen && !forced_owner.app.host,
            "forced owner closure invalidates and destroys the public consumer before host release");

    IntentOwner retained_owner = {};
    MpcUiAppHost *retained_host = mpc_ui_private_app_host_create(
        context, parent, bounds);
    unsigned retained_base = fixture.count;
    require(retained_host &&
            mpclearn_third_app_init(&retained_owner.app, intent,
                                    &retained_owner) &&
            mpc_ui_app_register(retained_host, mpclearn_third_app_callbacks(),
                                &retained_owner.app) &&
            mpc_ui_app_open(retained_host) && retained_owner.app.screen,
            "forced-failure consumer opens before owner loss");
    unsigned callbacks_before_forced_failure = callback_calls;
    fail_detach = 1;
    require(!mpc_ui_private_app_host_destroy(&retained_host, 1) &&
            retained_host && retained_owner.app.screen &&
            mpc_ui_context_error(context) == MPC_UI_DETACH_FAILED,
            "failed forced detach retains consumer, screen, parent and host");
    click(fixture.children[retained_base + 3].object);
    require(callback_calls == callbacks_before_forced_failure,
            "failed forced destruction leaves every consumer callback inert");
    fail_detach = 0;
    mpc_ui_context_clear_error(context);
    require(mpc_ui_private_app_host_destroy(&retained_host, 1) &&
            !retained_host && !retained_owner.app.screen &&
            !retained_owner.app.host,
            "retained forced-failure state can detach while owner remains live");

    MpcUiScreen *entry_screen = mpc_ui_screen_create(
        context, parent, {0, 0, 240, 96});
    InvalidateOnClick entry_owner = {&entry_screen, 0, 1};
    MpcUiControl *entry_button = mpc_ui_button_create(
        entry_screen, "ENTRY", 1, invalidate_on_click, &entry_owner);
    require(entry_screen && entry_button &&
            mpc_ui_control_place(entry_button, {0, 0, 240, 96}) &&
            mpc_ui_screen_show(entry_screen),
            "non-consumer launcher entry fixture opens");
    void *entry_object = mpc_ui_private_control_host_object(entry_button);
    click(entry_object);
    require(entry_owner.calls == 1 && !entry_owner.close_result &&
            entry_screen &&
            mpc_ui_context_error(context) == MPC_UI_CALLBACK_ACTIVE,
            "active callback refuses destruction after forced invalidation");
    click(entry_object);
    require(entry_owner.calls == 1,
            "forced invalidation leaves the retained launcher entry inert");
    mpc_ui_context_clear_error(context);
    require(mpc_ui_screen_close(&entry_screen) && !entry_screen,
            "invalidated launcher entry detaches after callback return");
    require(mpc_ui_private_parent_destroy(&parent) && !parent,
            "adapter parent released after screen");

    MpcUiParent *component_parent = mpc_ui_private_component_parent(
        context, component_parent_object);
    require(component_parent != nullptr, "default Component parent adapter");
    MpcUiScreen *missing_relationship = mpc_ui_screen_create(
        context, component_parent, {0, 0, 120, 80});
    MpcUiControl *unattached = mpc_ui_button_create(
        missing_relationship, "UNATTACHED", 0, nullptr, nullptr);
    host_add_relationship = 0;
    require(missing_relationship && unattached &&
            !mpc_ui_control_place(unattached, {10, 10, 100, 60}) &&
            mpc_ui_context_error(context) == MPC_UI_HOST_CALL_FAILED,
            "void add is rejected until the admitted parent field changes");
    host_add_relationship = 1;
    mpc_ui_context_clear_error(context);
    require(mpc_ui_screen_close(&missing_relationship),
            "failed default attach leaves no toolkit attachment to detach");

    MpcUiScreen *component_screen = mpc_ui_screen_create(
        context, component_parent, {0, 0, 120, 80});
    MpcUiControl *component_button = mpc_ui_button_create(
        component_screen, "COMPONENT", 0, nullptr, nullptr);
    require(component_screen && component_button &&
            mpc_ui_button_style(component_button,
                                {0xff111111u, 0xff222222u, 0xffffffffu}) &&
            mpc_ui_control_place(component_button, {10, 10, 100, 60}) &&
            mpc_ui_control_keep_in_front(component_button) &&
            mpc_ui_private_control_host_object(component_button) ==
                component_fixture.children[0].object &&
            *(void **)((unsigned char *)component_fixture.children[0].object + 0x0c) ==
                component_parent_object &&
            mpc_ui_screen_show(component_screen),
            "default adapter accepts relationship and qualified z-order request");
    paint(component_fixture.children[0].object, 0, 1);
    require(paint_fill == 0xff222222u && paint_width == 84 && paint_height == 60,
            "pressed default-adapter button uses qualified full paint");
    host_remove_relationship = 0;
    require(!mpc_ui_screen_close(&component_screen) && component_screen &&
            mpc_ui_context_error(context) == MPC_UI_DETACH_FAILED,
            "void remove is rejected while the admitted parent field remains");
    host_remove_relationship = 1;
    mpc_ui_context_clear_error(context);
    require(mpc_ui_screen_close(&component_screen) && !component_screen &&
            host_add_calls == 2 && host_remove_calls == 2 &&
            keep_front_calls == 1 &&
            component_fixture.children[0].object == nullptr,
            "default adapter confirms relationship clearing before teardown");
    require(mpc_ui_private_parent_destroy(&component_parent) && !component_parent,
            "default Component parent released after screen");
    require(mpc_ui_private_context_destroy(&context) && !context,
            "context released after all instance ownership");
    require(allocations == frees && focus_calls >= 4,
            "balanced per-instance allocations and adapter focus lifecycle");
    printf("PASS canonical ARM UI toolkit: opaque context/parent/app host, public button, Label/Slider and grouped assignment screens, one-consumer registration, multi-screen suspend/resume with hidden-page preservation and refusal unwind, deferred Back, failed-open unwind, forced-owner closure, silent owner refresh, native-list selection suppression, Slider value/drag intents, local layout, listener removal, invalidation-before-detach, retryable detach and balanced teardown (host widgets/Component/Graphics/touch substituted)\n");
    return 0;
}
