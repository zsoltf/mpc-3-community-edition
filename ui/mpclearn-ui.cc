/* Canonical host-widget toolkit. Original code, MIT. */
using size_t = __SIZE_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;
#include "include/mpclearn-ui.h"
#include "private/mpclearn-ui-private.h"
#include "private/mpc-3.9.1-profile.h"

extern "C" void *memcpy(void *, const void *, size_t) noexcept;
extern "C" void *memset(void *, int, size_t) noexcept;
extern "C" long syscall(long, ...) noexcept;

namespace {

constexpr uint32_t kContextMagic = 0x31584355u; /* UCX1 */
constexpr uint32_t kParentMagic = 0x31524150u;  /* PAR1 */
constexpr uint32_t kScreenMagic = 0x314e4353u;  /* SCN1 */
constexpr uint32_t kControlMagic = 0x31525443u; /* CTR1 */
constexpr uint32_t kBindingMagic = 0x31444e42u; /* BND1 */
constexpr uint32_t kListMagic = 0x3154534cu;    /* LST1 */
constexpr uint32_t kListModelMagic = 0x314d534cu; /* LSM1 */
constexpr uint32_t kSliderMagic = 0x31444c53u;  /* SLD1 */
constexpr uint32_t kSliderBindingMagic = 0x31425353u; /* SSB1 */
constexpr uint32_t kAppHostMagic = 0x31505041u; /* APP1 */
constexpr uint32_t kButtonColour = 0x01000100u;
constexpr uint32_t kButtonOnColour = 0x01000101u;
constexpr uint32_t kTextOffColour = 0x01000102u;
constexpr uint32_t kTextOnColour = 0x01000103u;
constexpr uint32_t kControlButton = 1u;
constexpr uint32_t kControlLabel = 2u;
constexpr uint32_t kComponentParentOffset = 0x0cu;
constexpr long kArmGetTid = 224;

struct Binding;
struct ListModel;
struct SliderBinding;
struct MpcUiScreenImpl;

struct MpcUiContextImpl {
    uint32_t magic;
    uint32_t ui_thread_id;
    uint32_t error;
    uint32_t callback_depth;
    uint32_t parents;
    uint32_t screens;
    uint32_t app_hosts;
    MpcUiPrivateHost host;
};

struct MpcUiParentImpl {
    uint32_t magic;
    MpcUiContextImpl *context;
    void *owner;
    void *target;
    MpcUiParentAttach attach;
    MpcUiParentDetach detach;
    MpcUiParentFocus focus;
    MpcUiScreenImpl *screen_head;
    uint32_t screens;
    uint32_t app_hosts;
};

struct MpcUiAppHostImpl {
    uint32_t magic;
    MpcUiContextImpl *context;
    MpcUiParentImpl *parent;
    MpcUiRect bounds;
    MpcUiAppCallbacks callbacks;
    void *owner;
    uint32_t registered;
    uint32_t opened;
    uint32_t close_requested;
    uint32_t transition;
    uint32_t suspended;
};

struct MpcUiScreenImpl {
    uint32_t magic;
    MpcUiContextImpl *context;
    MpcUiParentImpl *parent;
    MpcUiScreenImpl *parent_next;
    MpcUiRect bounds;
    struct MpcUiControlImpl *controls;
    struct MpcUiListImpl *lists;
    struct MpcUiSliderImpl *sliders;
    uint32_t visible;
    uint32_t resume_visible;
    uint32_t closing;
};

struct Binding {
    uint32_t magic;
    uint32_t live;
    MpcUiContextImpl *context;
    MpcUiScreenImpl *screen;
    MpcUiIntentCallback callback;
    void *owner;
    uint32_t intent;
    uint32_t normal;
    uint32_t selected;
    uint32_t text;
    uint32_t is_selected;
    uint32_t width;
    uint32_t height;
    alignas(4) unsigned char paint_label[4];
    uint32_t paint_label_live;
};

struct MpcUiControlImpl {
    uint32_t magic;
    MpcUiScreenImpl *screen;
    MpcUiControlImpl *next;
    void *object;
    void *vtable;
    Binding *binding;
    MpcUiRect bounds;
    uint32_t kind;
    uint32_t attached;
    uint32_t requested_visible;
    uint32_t visible;
};

struct ListModel {
    void *vptr;
    uint32_t magic;
    uint32_t live;
    uint32_t suppress_selection;
    MpcUiContextImpl *context;
    MpcUiScreenImpl *screen;
    MpcUiListRowText row_text;
    MpcUiListSelectionCallback selected;
    void *owner;
    uint32_t row_count;
    uint32_t normal;
    uint32_t selection;
    uint32_t text;
};

struct MpcUiListImpl {
    uint32_t magic;
    MpcUiScreenImpl *screen;
    MpcUiListImpl *next;
    void *object;
    void *viewport;
    void *model_vtable;
    ListModel *model;
    MpcUiRect bounds;
    uint32_t attached;
    uint32_t model_attached;
    uint32_t requested_visible;
    uint32_t visible;
    uint32_t drag_enabled;
};

struct SliderBinding {
    void *vptr;
    uint32_t magic;
    uint32_t live;
    MpcUiContextImpl *context;
    MpcUiScreenImpl *screen;
    MpcUiSliderCallback callback;
    void *owner;
    uint32_t intent;
    void *listener_vtable[7];
};

struct MpcUiSliderImpl {
    uint32_t magic;
    MpcUiScreenImpl *screen;
    MpcUiSliderImpl *next;
    void *object;
    SliderBinding *binding;
    MpcUiRect bounds;
    uint32_t attached;
    uint32_t listener_attached;
    uint32_t requested_visible;
    uint32_t visible;
};

static_assert(sizeof(void *) == 4 && sizeof(uintptr_t) == 4,
              "the admitted MPC host ABI is ARM32");

static MpcUiContextImpl *as_context(MpcUiContext *context) {
    auto *value = (MpcUiContextImpl *)context;
    return value && value->magic == kContextMagic ? value : nullptr;
}
static const MpcUiContextImpl *as_context(const MpcUiContext *context) {
    auto *value = (const MpcUiContextImpl *)context;
    return value && value->magic == kContextMagic ? value : nullptr;
}
static MpcUiParentImpl *as_parent(MpcUiParent *parent) {
    auto *value = (MpcUiParentImpl *)parent;
    return value && value->magic == kParentMagic ? value : nullptr;
}
static MpcUiScreenImpl *as_screen(MpcUiScreen *screen) {
    auto *value = (MpcUiScreenImpl *)screen;
    return value && value->magic == kScreenMagic ? value : nullptr;
}
static MpcUiControlImpl *as_control(MpcUiControl *control) {
    auto *value = (MpcUiControlImpl *)control;
    return value && value->magic == kControlMagic ? value : nullptr;
}
static MpcUiListImpl *as_list(MpcUiList *list) {
    auto *value = (MpcUiListImpl *)list;
    return value && value->magic == kListMagic ? value : nullptr;
}
static MpcUiSliderImpl *as_slider(MpcUiSlider *slider) {
    auto *value = (MpcUiSliderImpl *)slider;
    return value && value->magic == kSliderMagic ? value : nullptr;
}
static MpcUiAppHostImpl *as_app_host(MpcUiAppHost *host) {
    auto *value = (MpcUiAppHostImpl *)host;
    return value && value->magic == kAppHostMagic ? value : nullptr;
}
static const MpcUiAppHostImpl *as_app_host(const MpcUiAppHost *host) {
    auto *value = (const MpcUiAppHostImpl *)host;
    return value && value->magic == kAppHostMagic ? value : nullptr;
}

static int on_ui_thread(MpcUiContextImpl *context) {
    if (context && (uint32_t)syscall(kArmGetTid) == context->ui_thread_id) return 1;
    if (context) context->error = MPC_UI_WRONG_THREAD;
    return 0;
}

static void set_error(MpcUiContextImpl *context, uint32_t error) {
    if (context && context->error == MPC_UI_OK) context->error = error;
}

static int valid_text(const char *text);

static Binding *binding_from_object(void *object) {
    if (!object) return nullptr;
    void *vptr = *(void **)object;
    if (!vptr) return nullptr;
    auto *table = (unsigned char *)vptr - 8;
    auto *binding = (Binding *)(table + MPC_UI_BUTTON_VTABLE_BYTES);
    return binding->magic == kBindingMagic ? binding : nullptr;
}

extern "C" void mpc_ui_button_clicked(void *object) noexcept {
    Binding *binding = binding_from_object(object);
    if (!binding || !binding->live || !binding->context || !binding->screen ||
        binding->screen->closing || !binding->callback) return;
    MpcUiContextImpl *context = binding->context;
    if (!on_ui_thread(context)) return;
    context->callback_depth++;
    try { binding->callback(binding->owner, binding->intent); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    context->callback_depth--;
}

extern "C" void mpc_ui_button_paint(void *object, void *graphics,
                                      int highlighted, int down) noexcept {
    Binding *binding = binding_from_object(object);
    if (!binding || !binding->live || !binding->context || !graphics ||
        binding->width <= 16 || binding->width > 2000 || !binding->height ||
        binding->height > 1200) return;
    MpcUiPrivateHost &host = binding->context->host;
    uint32_t fill = binding->is_selected || highlighted || down
                        ? binding->selected : binding->normal;
    try {
        host.graphics_fill_all(graphics, fill);
        host.graphics_set_colour(graphics, binding->text);
        host.graphics_set_font(graphics, 22.0f);
        host.graphics_draw_fitted_text(
            graphics, binding->paint_label,
            8, 0, (int)binding->width - 16, (int)binding->height, 36, 1, 0.0f);
    } catch (...) {
        set_error(binding->context, MPC_UI_HOST_CALL_FAILED);
    }
}

extern "C" void mpc_ui_list_model_complete_destructor(void *) noexcept {}
extern "C" void mpc_ui_list_model_deleting_destructor(void *) noexcept {}

extern "C" int mpc_ui_list_model_count(void *opaque) noexcept {
    auto *model = (ListModel *)opaque;
    if (!model || model->magic != kListModelMagic || !model->live ||
        model->row_count > 256u) return 0;
    return (int)model->row_count;
}

extern "C" void mpc_ui_list_model_paint(
    void *opaque, int row, void *graphics, int width, int height,
    int selected) noexcept {
    auto *model = (ListModel *)opaque;
    if (!model || model->magic != kListModelMagic || !model->live ||
        !model->context || !model->screen || model->screen->closing ||
        !graphics || row < 0 || (uint32_t)row >= model->row_count ||
        width <= 16 || width > 2000 || height <= 0 || height > 1200 ||
        !model->row_text) return;
    alignas(4) unsigned char label[4] = {};
    int label_live = 0;
    try {
        const char *text = model->row_text(model->owner, (uint32_t)row);
        if (!valid_text(text)) return;
        MpcUiPrivateHost &host = model->context->host;
        host.string_ctor(label, text);
        label_live = 1;
        host.graphics_fill_all(graphics,
                               selected ? model->selection : model->normal);
        host.graphics_set_colour(graphics, model->text);
        host.graphics_set_font(graphics, 22.0f);
        host.graphics_draw_fitted_text(graphics, label, 8, 0, width - 16,
                                       height, 36, 1, 0.0f);
        label_live = 0;
        host.string_dtor(label);
    } catch (...) {
        if (label_live) {
            try { model->context->host.string_dtor(label); } catch (...) {}
        }
        set_error(model->context, MPC_UI_HOST_CALL_FAILED);
    }
}

extern "C" void *mpc_ui_list_model_refresh(void *, int, int, void *) noexcept {
    return nullptr;
}

extern "C" void mpc_ui_list_model_clicked(void *, int, const void *) noexcept {}

extern "C" void mpc_ui_list_model_selected(void *opaque, int row) noexcept {
    auto *model = (ListModel *)opaque;
    if (!model || model->magic != kListModelMagic || !model->live ||
        model->suppress_selection || !model->context || !model->screen ||
        model->screen->closing || !model->selected || row < 0 ||
        (uint32_t)row >= model->row_count) return;
    if (!on_ui_thread(model->context)) return;
    model->context->callback_depth++;
    try { model->selected(model->owner, (uint32_t)row); }
    catch (...) { set_error(model->context, MPC_UI_HOST_CALL_FAILED); }
    model->context->callback_depth--;
}

extern "C" void mpc_ui_slider_listener_complete_destructor(void *) noexcept {}
extern "C" void mpc_ui_slider_listener_deleting_destructor(void *) noexcept {}

static void publish_slider_event(void *opaque, void *slider,
                                 uint32_t event) noexcept {
    auto *binding = (SliderBinding *)opaque;
    if (!binding || binding->magic != kSliderBindingMagic || !binding->live ||
        !binding->context || !binding->screen || binding->screen->closing ||
        !binding->callback || !slider) return;
    MpcUiContextImpl *context = binding->context;
    if (!on_ui_thread(context)) return;
    try {
        double value = context->host.slider_get_value(slider);
        context->callback_depth++;
        try {
            binding->callback(binding->owner, binding->intent, event, value);
        } catch (...) {
            set_error(context, MPC_UI_HOST_CALL_FAILED);
        }
        context->callback_depth--;
    } catch (...) {
        set_error(context, MPC_UI_HOST_CALL_FAILED);
    }
}

extern "C" void mpc_ui_slider_value_changed(void *binding,
                                               void *slider) noexcept {
    publish_slider_event(binding, slider, MPC_UI_SLIDER_VALUE_CHANGED);
}

extern "C" void mpc_ui_slider_drag_started(void *binding,
                                              void *slider) noexcept {
    publish_slider_event(binding, slider, MPC_UI_SLIDER_DRAG_STARTED);
}

extern "C" void mpc_ui_slider_drag_ended(void *binding,
                                            void *slider) noexcept {
    publish_slider_event(binding, slider, MPC_UI_SLIDER_DRAG_ENDED);
}

static int host_complete(const MpcUiPrivateHost *host) {
    return host && host->allocate && host->sized_delete && host->string_ctor &&
           host->string_dtor && host->set_colour && host->button_ctor &&
           host->button_dtor && host->label_ctor && host->label_dtor &&
           host->label_set_text && host->label_set_justification &&
           host->component_set_intercepts_mouse &&
           host->list_ctor && host->list_dtor &&
           host->list_set_model && host->list_update_content &&
           host->list_set_row_height && host->list_select_row &&
           host->list_get_viewport && host->viewport_set_drag &&
           host->slider_ctor && host->slider_dtor && host->slider_set_style &&
           host->slider_set_range && host->slider_set_text_box &&
           host->slider_add_listener && host->slider_remove_listener &&
           host->slider_get_value && host->slider_set_value &&
           host->add_child && host->remove_child &&
           host->set_bounds && host->set_visible && host->set_always_on_top &&
           host->set_toggle &&
           host->repaint && host->graphics_set_colour &&
           host->graphics_set_font && host->graphics_fill_all &&
           host->graphics_draw_fitted_text && host->button_vtable &&
           host->list_model_vtable;
}

static MpcUiContext *context_from_host(const MpcUiPrivateHost *host,
                                       uint32_t ui_thread_id) {
    if (!host_complete(host) || !ui_thread_id ||
        (uint32_t)syscall(kArmGetTid) != ui_thread_id) return nullptr;
    MpcUiContextImpl *context = nullptr;
    try {
        context = (MpcUiContextImpl *)host->allocate(sizeof(*context));
        if (!context) return nullptr;
        memset(context, 0, sizeof(*context));
        context->magic = kContextMagic;
        context->ui_thread_id = ui_thread_id;
        context->host = *host;
        return (MpcUiContext *)context;
    } catch (...) {
        if (context) try { host->sized_delete(context, sizeof(*context)); } catch (...) {}
        return nullptr;
    }
}

static MpcUiPrivateHost resolve_host(uint32_t bias) {
    MpcUiPrivateHost host = {};
#define MPC_UI_RESOLVE(field, type, rva) host.field = (type)(uintptr_t)(bias + rva)
    MPC_UI_RESOLVE(allocate, decltype(host.allocate), MPC_UI_RVA_ALLOCATE);
    MPC_UI_RESOLVE(sized_delete, decltype(host.sized_delete), MPC_UI_RVA_SIZED_DELETE);
    MPC_UI_RESOLVE(string_ctor, decltype(host.string_ctor), MPC_UI_RVA_STRING_CTOR);
    MPC_UI_RESOLVE(string_dtor, decltype(host.string_dtor), MPC_UI_RVA_STRING_DTOR);
    MPC_UI_RESOLVE(set_colour, decltype(host.set_colour), MPC_UI_RVA_SET_COLOUR);
    MPC_UI_RESOLVE(button_ctor, decltype(host.button_ctor), MPC_UI_RVA_BUTTON_CTOR);
    MPC_UI_RESOLVE(button_dtor, decltype(host.button_dtor), MPC_UI_RVA_BUTTON_DTOR);
    MPC_UI_RESOLVE(label_ctor, decltype(host.label_ctor), MPC_UI_RVA_LABEL_CTOR);
    MPC_UI_RESOLVE(label_dtor, decltype(host.label_dtor), MPC_UI_RVA_LABEL_DTOR);
    MPC_UI_RESOLVE(label_set_text, decltype(host.label_set_text),
                   MPC_UI_RVA_LABEL_SET_TEXT);
    MPC_UI_RESOLVE(label_set_justification,
                   decltype(host.label_set_justification),
                   MPC_UI_RVA_LABEL_SET_JUSTIFICATION);
    MPC_UI_RESOLVE(component_set_intercepts_mouse,
                   decltype(host.component_set_intercepts_mouse),
                   MPC_UI_RVA_COMPONENT_SET_INTERCEPTS_MOUSE);
    MPC_UI_RESOLVE(list_ctor, decltype(host.list_ctor), MPC_UI_RVA_LIST_CTOR);
    MPC_UI_RESOLVE(list_dtor, decltype(host.list_dtor), MPC_UI_RVA_LIST_DTOR);
    MPC_UI_RESOLVE(list_set_model, decltype(host.list_set_model),
                   MPC_UI_RVA_LIST_SET_MODEL);
    MPC_UI_RESOLVE(list_update_content, decltype(host.list_update_content),
                   MPC_UI_RVA_LIST_UPDATE_CONTENT);
    MPC_UI_RESOLVE(list_set_row_height, decltype(host.list_set_row_height),
                   MPC_UI_RVA_LIST_SET_ROW_HEIGHT);
    MPC_UI_RESOLVE(list_select_row, decltype(host.list_select_row),
                   MPC_UI_RVA_LIST_SELECT_ROW);
    MPC_UI_RESOLVE(list_get_viewport, decltype(host.list_get_viewport),
                   MPC_UI_RVA_LIST_GET_VIEWPORT);
    MPC_UI_RESOLVE(viewport_set_drag, decltype(host.viewport_set_drag),
                   MPC_UI_RVA_VIEWPORT_SET_DRAG);
    MPC_UI_RESOLVE(slider_ctor, decltype(host.slider_ctor),
                   MPC_UI_RVA_SLIDER_CTOR);
    MPC_UI_RESOLVE(slider_dtor, decltype(host.slider_dtor),
                   MPC_UI_RVA_SLIDER_DTOR);
    MPC_UI_RESOLVE(slider_set_style, decltype(host.slider_set_style),
                   MPC_UI_RVA_SLIDER_SET_STYLE);
    MPC_UI_RESOLVE(slider_set_range, decltype(host.slider_set_range),
                   MPC_UI_RVA_SLIDER_SET_RANGE);
    MPC_UI_RESOLVE(slider_set_text_box, decltype(host.slider_set_text_box),
                   MPC_UI_RVA_SLIDER_SET_TEXT_BOX);
    MPC_UI_RESOLVE(slider_add_listener, decltype(host.slider_add_listener),
                   MPC_UI_RVA_SLIDER_ADD_LISTENER);
    MPC_UI_RESOLVE(slider_remove_listener,
                   decltype(host.slider_remove_listener),
                   MPC_UI_RVA_SLIDER_REMOVE_LISTENER);
    MPC_UI_RESOLVE(slider_get_value, decltype(host.slider_get_value),
                   MPC_UI_RVA_SLIDER_GET_VALUE);
    MPC_UI_RESOLVE(slider_set_value, decltype(host.slider_set_value),
                   MPC_UI_RVA_SLIDER_SET_VALUE);
    MPC_UI_RESOLVE(add_child, decltype(host.add_child), MPC_UI_RVA_ADD_CHILD);
    MPC_UI_RESOLVE(remove_child, decltype(host.remove_child), MPC_UI_RVA_REMOVE_CHILD);
    MPC_UI_RESOLVE(set_bounds, decltype(host.set_bounds), MPC_UI_RVA_SET_BOUNDS);
    MPC_UI_RESOLVE(set_visible, decltype(host.set_visible), MPC_UI_RVA_SET_VISIBLE);
    MPC_UI_RESOLVE(set_always_on_top, decltype(host.set_always_on_top),
                   MPC_UI_RVA_SET_ALWAYS_ON_TOP);
    MPC_UI_RESOLVE(set_toggle, decltype(host.set_toggle), MPC_UI_RVA_SET_TOGGLE);
    MPC_UI_RESOLVE(repaint, decltype(host.repaint), MPC_UI_RVA_REPAINT);
    MPC_UI_RESOLVE(graphics_set_colour, decltype(host.graphics_set_colour),
                   MPC_UI_RVA_GRAPHICS_SET_COLOUR);
    MPC_UI_RESOLVE(graphics_set_font, decltype(host.graphics_set_font),
                   MPC_UI_RVA_GRAPHICS_SET_FONT);
    MPC_UI_RESOLVE(graphics_fill_all, decltype(host.graphics_fill_all),
                   MPC_UI_RVA_GRAPHICS_FILL_ALL);
    MPC_UI_RESOLVE(graphics_draw_fitted_text,
                   decltype(host.graphics_draw_fitted_text),
                   MPC_UI_RVA_GRAPHICS_DRAW_FITTED_TEXT);
    host.button_vtable = (const unsigned char *)(uintptr_t)(bias + MPC_UI_RVA_BUTTON_VTABLE);
    host.list_model_vtable =
        (const unsigned char *)(uintptr_t)(bias + MPC_UI_RVA_LIST_MODEL_VTABLE);
#undef MPC_UI_RESOLVE
    return host;
}

static int default_attach(void *owner, void *child) {
    auto *parent = (MpcUiParentImpl *)owner;
    if (!parent || !child) return 0;
    parent->context->host.add_child(parent->target, child, -1);
    return *(void **)((unsigned char *)child + kComponentParentOffset) ==
           parent->target;
}
static int default_detach(void *owner, void *child) {
    auto *parent = (MpcUiParentImpl *)owner;
    if (!parent || !child) return 0;
    parent->context->host.remove_child(parent->target, child);
    return *(void **)((unsigned char *)child + kComponentParentOffset) == nullptr;
}

static int valid_rect(MpcUiRect bounds) {
    return bounds.width > 0 && bounds.height > 0 && bounds.width <= 4096 &&
           bounds.height <= 4096 && bounds.x >= 0 && bounds.y >= 0 &&
           bounds.x <= 4096 && bounds.y <= 4096;
}

static int valid_text(const char *text) {
    if (!text) return 0;
    for (unsigned length = 0; length < 64; ++length)
        if (!text[length]) return length != 0;
    return 0;
}

static void invalidate_bindings(MpcUiScreenImpl *screen) {
    for (MpcUiControlImpl *control = screen->controls; control; control = control->next)
        if (control->binding) control->binding->live = 0;
    for (MpcUiListImpl *list = screen->lists; list; list = list->next)
        if (list->model) list->model->live = 0;
    for (MpcUiSliderImpl *slider = screen->sliders; slider;
         slider = slider->next)
        if (slider->binding) slider->binding->live = 0;
}

static void invalidate_parent_screens(MpcUiParentImpl *parent) {
    if (!parent) return;
    for (MpcUiScreenImpl *screen = parent->screen_head; screen;
         screen = screen->parent_next) {
        screen->closing = 1;
        invalidate_bindings(screen);
    }
}

static int disconnect_list_models(MpcUiScreenImpl *screen) {
    int success = 1;
    for (MpcUiListImpl *list = screen->lists; list; list = list->next) {
        if (!list->model_attached) continue;
        try {
            screen->context->host.list_set_model(list->object, nullptr);
            list->model_attached = 0;
        } catch (...) {
            set_error(screen->context, MPC_UI_HOST_CALL_FAILED);
            success = 0;
        }
    }
    return success;
}

static int disable_list_drag(MpcUiScreenImpl *screen) {
    int success = 1;
    for (MpcUiListImpl *list = screen->lists; list; list = list->next) {
        if (!list->drag_enabled) continue;
        try {
            screen->context->host.viewport_set_drag(
                list->viewport, 0, MPC_UI_LIST_DRAG_THRESHOLD);
            list->drag_enabled = 0;
        } catch (...) {
            set_error(screen->context, MPC_UI_HOST_CALL_FAILED);
            success = 0;
        }
    }
    return success;
}

static int disconnect_slider_listeners(MpcUiScreenImpl *screen) {
    int success = 1;
    for (MpcUiSliderImpl *slider = screen->sliders; slider;
         slider = slider->next) {
        if (!slider->listener_attached) continue;
        try {
            screen->context->host.slider_remove_listener(
                slider->object, slider->binding);
            slider->listener_attached = 0;
        } catch (...) {
            set_error(screen->context, MPC_UI_HOST_CALL_FAILED);
            success = 0;
        }
    }
    return success;
}

static int attach_control(MpcUiControlImpl *control) {
    if (control->attached) return 1;
    MpcUiScreenImpl *screen = control->screen;
    MpcUiParentImpl *parent = screen->parent;
    int ok = 0;
    try { ok = parent->attach(parent->owner, control->object); } catch (...) { ok = 0; }
    if (!ok) {
        set_error(screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
    control->attached = 1;
    return 1;
}

static int detach_control(MpcUiControlImpl *control) {
    if (!control->attached) return 1;
    MpcUiParentImpl *parent = control->screen->parent;
    int ok = 0;
    try { ok = parent->detach(parent->owner, control->object); } catch (...) { ok = 0; }
    if (!ok) {
        set_error(control->screen->context, MPC_UI_DETACH_FAILED);
        return 0;
    }
    control->attached = 0;
    return 1;
}

static int attach_list(MpcUiListImpl *list) {
    if (list->attached) return 1;
    MpcUiParentImpl *parent = list->screen->parent;
    int ok = 0;
    try { ok = parent->attach(parent->owner, list->object); } catch (...) { ok = 0; }
    if (!ok) {
        set_error(list->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
    list->attached = 1;
    return 1;
}

static int detach_list(MpcUiListImpl *list) {
    if (!list->attached) return 1;
    MpcUiParentImpl *parent = list->screen->parent;
    int ok = 0;
    try { ok = parent->detach(parent->owner, list->object); } catch (...) { ok = 0; }
    if (!ok) {
        set_error(list->screen->context, MPC_UI_DETACH_FAILED);
        return 0;
    }
    list->attached = 0;
    return 1;
}


static int attach_slider(MpcUiSliderImpl *slider) {
    if (slider->attached) return 1;
    MpcUiParentImpl *parent = slider->screen->parent;
    int ok = 0;
    try { ok = parent->attach(parent->owner, slider->object); } catch (...) { ok = 0; }
    if (!ok) {
        set_error(slider->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
    slider->attached = 1;
    return 1;
}

static int detach_slider(MpcUiSliderImpl *slider) {
    if (!slider->attached) return 1;
    MpcUiParentImpl *parent = slider->screen->parent;
    int ok = 0;
    try { ok = parent->detach(parent->owner, slider->object); } catch (...) { ok = 0; }
    if (!ok) {
        set_error(slider->screen->context, MPC_UI_DETACH_FAILED);
        return 0;
    }
    slider->attached = 0;
    return 1;
}

static void destroy_control(MpcUiControlImpl *control) {
    MpcUiContextImpl *context = control->screen->context;
    MpcUiPrivateHost &host = context->host;
    if (control->binding) control->binding->live = 0;
    if (control->kind == kControlButton) {
        if (control->vtable)
            *(void **)control->object = (unsigned char *)host.button_vtable + 8;
        try { host.button_dtor(control->object); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
        try { host.sized_delete(control->object, MPC_UI_BUTTON_BYTES); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    } else if (control->kind == kControlLabel) {
        try { host.label_dtor(control->object); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
        try { host.sized_delete(control->object, MPC_UI_LABEL_BYTES); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    }
    if (control->binding && control->binding->paint_label_live) {
        try { host.string_dtor(control->binding->paint_label); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
        control->binding->paint_label_live = 0;
    }
    if (control->binding) control->binding->magic = 0;
    if (control->vtable) {
        try { host.sized_delete(control->vtable,
                                MPC_UI_BUTTON_VTABLE_BYTES + sizeof(Binding)); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    }
    control->magic = 0;
    try { host.sized_delete(control, sizeof(*control)); } catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
}

static void destroy_list(MpcUiListImpl *list) {
    MpcUiContextImpl *context = list->screen->context;
    MpcUiPrivateHost &host = context->host;
    if (list->model) list->model->live = 0;
    try { host.list_dtor(list->object); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    try { host.sized_delete(list->object, MPC_UI_LIST_BYTES); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    if (list->model) list->model->magic = 0;
    if (list->model)
        try { host.sized_delete(list->model, sizeof(*list->model)); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    if (list->model_vtable)
        try { host.sized_delete(list->model_vtable,
                                MPC_UI_LIST_MODEL_VTABLE_BYTES); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    list->magic = 0;
    try { host.sized_delete(list, sizeof(*list)); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
}

static void destroy_slider(MpcUiSliderImpl *slider) {
    MpcUiContextImpl *context = slider->screen->context;
    MpcUiPrivateHost &host = context->host;
    if (slider->binding) slider->binding->live = 0;
    try { host.slider_dtor(slider->object); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    try { host.sized_delete(slider->object, MPC_UI_SLIDER_BYTES); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    if (slider->binding) {
        slider->binding->magic = 0;
        try { host.sized_delete(slider->binding, sizeof(*slider->binding)); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    }
    slider->magic = 0;
    try { host.sized_delete(slider, sizeof(*slider)); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
}

} // namespace

extern "C" uint32_t mpc_ui_context_error(const MpcUiContext *context) {
    const MpcUiContextImpl *value = as_context(context);
    return value ? value->error : MPC_UI_INVALID_ARGUMENT;
}

extern "C" void mpc_ui_context_clear_error(MpcUiContext *context) {
    MpcUiContextImpl *value = as_context(context);
    if (value && on_ui_thread(value)) value->error = MPC_UI_OK;
}

extern "C" MpcUiContext *mpc_ui_private_context_from_admitted(
    const MpcUiAdmittedProfile *admitted) {
    if (!admitted || admitted->bytes != sizeof(*admitted) ||
        admitted->profile != MPC_UI_PROFILE_MPC_3_9_1 ||
        !admitted->image_bias || !admitted->ui_thread_id ||
        admitted->image_bias > UINT32_MAX - MPC_UI_RVA_LIST_MODEL_VTABLE)
        return nullptr;
    MpcUiPrivateHost host = resolve_host(admitted->image_bias);
    return context_from_host(&host, admitted->ui_thread_id);
}

extern "C" MpcUiContext *mpc_ui_private_context_for_component(
    const MpcUiPrivateHost *host, uint32_t ui_thread_id) {
    return context_from_host(host, ui_thread_id);
}

extern "C" int mpc_ui_private_context_destroy(MpcUiContext **context) {
    if (!context) return 0;
    MpcUiContextImpl *value = as_context(*context);
    if (!value) return 0;
    if (!on_ui_thread(value)) return 0;
    if (value->parents || value->screens || value->app_hosts ||
        value->callback_depth) {
        set_error(value, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    MpcUiPrivateHost host = value->host;
    value->magic = 0;
    try { host.sized_delete(value, sizeof(*value)); } catch (...) { return 0; }
    *context = nullptr;
    return 1;
}

extern "C" MpcUiParent *mpc_ui_private_parent_create(
    MpcUiContext *context, void *owner, MpcUiParentAttach attach,
    MpcUiParentDetach detach, MpcUiParentFocus focus) {
    MpcUiContextImpl *value = as_context(context);
    if (!value || !owner || !attach || !detach || !on_ui_thread(value)) return nullptr;
    MpcUiParentImpl *parent = nullptr;
    try {
        parent = (MpcUiParentImpl *)value->host.allocate(sizeof(*parent));
        if (!parent) { set_error(value, MPC_UI_ALLOCATION_FAILED); return nullptr; }
        memset(parent, 0, sizeof(*parent));
        parent->magic = kParentMagic;
        parent->context = value;
        parent->owner = owner;
        parent->target = nullptr;
        parent->attach = attach;
        parent->detach = detach;
        parent->focus = focus;
        value->parents++;
        return (MpcUiParent *)parent;
    } catch (...) {
        if (parent) try { value->host.sized_delete(parent, sizeof(*parent)); } catch (...) {}
        set_error(value, MPC_UI_HOST_CALL_FAILED);
        return nullptr;
    }
}

extern "C" MpcUiParent *mpc_ui_private_component_parent(MpcUiContext *context,
                                                          void *component) {
    MpcUiContextImpl *value = as_context(context);
    if (!value || !component || !on_ui_thread(value)) return nullptr;
    MpcUiParentImpl *parent = nullptr;
    try {
        parent = (MpcUiParentImpl *)value->host.allocate(sizeof(*parent));
        if (!parent) { set_error(value, MPC_UI_ALLOCATION_FAILED); return nullptr; }
        memset(parent, 0, sizeof(*parent));
        parent->magic = kParentMagic;
        parent->context = value;
        parent->owner = parent;
        parent->target = component;
        /* The callbacks receive this wrapper so the host table remains private. */
        parent->attach = default_attach;
        parent->detach = default_detach;
        parent->focus = nullptr;
        value->parents++;
        return (MpcUiParent *)parent;
    } catch (...) {
        if (parent) try { value->host.sized_delete(parent, sizeof(*parent)); } catch (...) {}
        set_error(value, MPC_UI_HOST_CALL_FAILED);
        return nullptr;
    }
}

extern "C" int mpc_ui_private_parent_destroy(MpcUiParent **parent) {
    if (!parent) return 0;
    MpcUiParentImpl *value = as_parent(*parent);
    if (!value || !on_ui_thread(value->context)) return 0;
    if (value->screens || value->app_hosts) {
        set_error(value->context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    MpcUiContextImpl *context = value->context;
    value->magic = 0;
    context->parents--;
    try { context->host.sized_delete(value, sizeof(*value)); } catch (...) { return 0; }
    *parent = nullptr;
    return 1;
}

extern "C" MpcUiAppHost *mpc_ui_private_app_host_create(
    MpcUiContext *context, MpcUiParent *parent, MpcUiRect bounds) {
    MpcUiContextImpl *ctx = as_context(context);
    MpcUiParentImpl *owner = as_parent(parent);
    if (!ctx || !owner || owner->context != ctx || !valid_rect(bounds) ||
        owner->app_hosts || owner->screens || !on_ui_thread(ctx)) {
        if (ctx) set_error(ctx, MPC_UI_INVALID_ARGUMENT);
        return nullptr;
    }
    MpcUiAppHostImpl *host = nullptr;
    try {
        host = (MpcUiAppHostImpl *)ctx->host.allocate(sizeof(*host));
        if (!host) {
            set_error(ctx, MPC_UI_ALLOCATION_FAILED);
            return nullptr;
        }
        memset(host, 0, sizeof(*host));
        host->magic = kAppHostMagic;
        host->context = ctx;
        host->parent = owner;
        host->bounds = bounds;
        ctx->app_hosts++;
        owner->app_hosts++;
        return (MpcUiAppHost *)host;
    } catch (...) {
        if (host)
            try { ctx->host.sized_delete(host, sizeof(*host)); } catch (...) {}
        set_error(ctx, MPC_UI_HOST_CALL_FAILED);
        return nullptr;
    }
}

extern "C" int mpc_ui_app_register(MpcUiAppHost *host,
                                      const MpcUiAppCallbacks *callbacks,
                                      void *owner) {
    MpcUiAppHostImpl *value = as_app_host(host);
    if (!value || !callbacks || callbacks->bytes != sizeof(*callbacks) ||
        !callbacks->open || !callbacks->prepare_close || !callbacks->close ||
        !owner || !on_ui_thread(value->context) || value->registered ||
        value->opened || value->transition) {
        if (value) set_error(value->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    value->callbacks = *callbacks;
    value->owner = owner;
    value->registered = 1;
    return 1;
}

extern "C" int mpc_ui_app_unregister(MpcUiAppHost *host) {
    MpcUiAppHostImpl *value = as_app_host(host);
    if (!value || !on_ui_thread(value->context) || !value->registered ||
        value->opened || value->close_requested || value->transition ||
        value->parent->screens) {
        if (value) set_error(value->context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    value->callbacks = {};
    value->owner = nullptr;
    value->registered = 0;
    return 1;
}

extern "C" int mpc_ui_app_open(MpcUiAppHost *host) {
    MpcUiAppHostImpl *value = as_app_host(host);
    if (!value || !value->registered || value->opened ||
        value->close_requested || value->transition ||
        value->parent->screens || !on_ui_thread(value->context)) {
        if (value) set_error(value->context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    value->transition = 1;
    value->opened = 1;
    int opened = 0;
    try {
        opened = value->callbacks.open(
            value->owner, host, (MpcUiContext *)value->context,
            (MpcUiParent *)value->parent, value->bounds);
    } catch (...) {
        set_error(value->context, MPC_UI_HOST_CALL_FAILED);
    }
    if (opened && value->parent->screens) {
        value->transition = 0;
        return 1;
    }
    int unwound = 0;
    try { unwound = value->callbacks.close(value->owner); }
    catch (...) { set_error(value->context, MPC_UI_HOST_CALL_FAILED); }
    if (unwound && !value->parent->screens) {
        value->opened = 0;
        value->close_requested = 0;
    } else {
        set_error(value->context, MPC_UI_LIFETIME_ORDER);
    }
    value->transition = 0;
    return 0;
}

extern "C" int mpc_ui_app_request_close(MpcUiAppHost *host) {
    MpcUiAppHostImpl *value = as_app_host(host);
    if (!value || !value->registered || !value->opened ||
        !on_ui_thread(value->context)) {
        if (value) set_error(value->context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    value->close_requested = 1;
    return 1;
}

extern "C" int mpc_ui_app_is_open(const MpcUiAppHost *host) {
    const MpcUiAppHostImpl *value = as_app_host(host);
    return value && value->registered && value->opened;
}

extern "C" int mpc_ui_app_is_closing(const MpcUiAppHost *host) {
    const MpcUiAppHostImpl *value = as_app_host(host);
    return value && value->registered && value->opened &&
           value->close_requested;
}

extern "C" int mpc_ui_private_app_host_is_suspended(
    const MpcUiAppHost *host) {
    const MpcUiAppHostImpl *value = as_app_host(host);
    return value && value->registered && value->opened && value->suspended;
}

extern "C" int mpc_ui_private_app_host_suspend(MpcUiAppHost *host) {
    MpcUiAppHostImpl *value = as_app_host(host);
    if (!value || !value->registered || !value->opened ||
        value->close_requested || value->transition ||
        !on_ui_thread(value->context)) {
        if (value) set_error(value->context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    if (value->suspended) return 1;
    if (value->context->callback_depth) {
        set_error(value->context, MPC_UI_CALLBACK_ACTIVE);
        return 0;
    }
    if (!value->parent->screens) {
        set_error(value->context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    for (MpcUiScreenImpl *screen = value->parent->screen_head; screen;
         screen = screen->parent_next)
        if (screen->closing) {
            set_error(value->context, MPC_UI_LIFETIME_ORDER);
            return 0;
        }
    value->transition = 1;
    for (MpcUiScreenImpl *screen = value->parent->screen_head; screen;
         screen = screen->parent_next)
        screen->resume_visible = screen->visible;

    int hidden = 1;
    for (MpcUiScreenImpl *screen = value->parent->screen_head; screen;
         screen = screen->parent_next) {
        if (screen->resume_visible &&
            !mpc_ui_screen_hide((MpcUiScreen *)screen)) {
            hidden = 0;
            break;
        }
    }
    if (hidden) {
        value->suspended = 1;
        value->transition = 0;
        return 1;
    }

    int restored = 1;
    for (MpcUiScreenImpl *screen = value->parent->screen_head; screen;
         screen = screen->parent_next)
        if (screen->resume_visible && !screen->visible &&
            !mpc_ui_screen_show((MpcUiScreen *)screen))
            restored = 0;
    value->suspended = restored ? 0u : 1u;
    if (restored)
        for (MpcUiScreenImpl *screen = value->parent->screen_head; screen;
             screen = screen->parent_next)
            screen->resume_visible = 0;
    value->transition = 0;
    return 0;
}

extern "C" int mpc_ui_private_app_host_resume(MpcUiAppHost *host) {
    MpcUiAppHostImpl *value = as_app_host(host);
    if (!value || !value->registered || !value->opened ||
        value->close_requested || value->transition ||
        !on_ui_thread(value->context)) {
        if (value) set_error(value->context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    if (!value->suspended) return 1;
    if (value->context->callback_depth) {
        set_error(value->context, MPC_UI_CALLBACK_ACTIVE);
        return 0;
    }
    if (!value->parent->screens) {
        set_error(value->context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    for (MpcUiScreenImpl *screen = value->parent->screen_head; screen;
         screen = screen->parent_next)
        if (screen->closing) {
            set_error(value->context, MPC_UI_LIFETIME_ORDER);
            return 0;
        }
    value->transition = 1;
    int shown = 1;
    for (MpcUiScreenImpl *screen = value->parent->screen_head; screen;
         screen = screen->parent_next) {
        if (screen->resume_visible && !screen->visible &&
            !mpc_ui_screen_show((MpcUiScreen *)screen)) {
            shown = 0;
            break;
        }
    }
    if (!shown) {
        for (MpcUiScreenImpl *screen = value->parent->screen_head; screen;
             screen = screen->parent_next)
            if (screen->resume_visible && screen->visible &&
                !mpc_ui_screen_hide((MpcUiScreen *)screen))
                set_error(value->context, MPC_UI_HOST_CALL_FAILED);
        value->suspended = 1;
        value->transition = 0;
        return 0;
    }
    for (MpcUiScreenImpl *screen = value->parent->screen_head; screen;
         screen = screen->parent_next)
        screen->resume_visible = 0;
    value->suspended = 0;
    value->transition = 0;
    return 1;
}

extern "C" uint32_t mpc_ui_private_app_host_callback_depth(
    const MpcUiAppHost *host) {
    const MpcUiAppHostImpl *value = as_app_host(host);
    return value ? value->context->callback_depth : 0;
}

extern "C" int mpc_ui_private_screen_invalidate(MpcUiScreen *screen) {
    MpcUiScreenImpl *value = as_screen(screen);
    if (!value || !on_ui_thread(value->context)) {
        if (value) set_error(value->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    value->closing = 1;
    invalidate_bindings(value);
    return 1;
}

extern "C" int mpc_ui_private_app_host_drain(MpcUiAppHost *host) {
    MpcUiAppHostImpl *value = as_app_host(host);
    if (!value || !on_ui_thread(value->context) || value->transition ||
        value->context->callback_depth) {
        if (value) set_error(value->context, MPC_UI_CALLBACK_ACTIVE);
        return 0;
    }
    if (!value->close_requested) return 1;
    if (!value->registered || !value->opened) {
        set_error(value->context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    value->transition = 1;
    int readiness = MPC_UI_APP_CLOSE_FAILED;
    try { readiness = value->callbacks.prepare_close(value->owner, 0); }
    catch (...) { set_error(value->context, MPC_UI_HOST_CALL_FAILED); }
    if (readiness == MPC_UI_APP_CLOSE_PENDING) {
        value->transition = 0;
        return 1;
    }
    if (readiness != MPC_UI_APP_CLOSE_READY) {
        set_error(value->context, MPC_UI_HOST_CALL_FAILED);
        value->transition = 0;
        return 0;
    }
    int closed = 0;
    try { closed = value->callbacks.close(value->owner); }
    catch (...) { set_error(value->context, MPC_UI_HOST_CALL_FAILED); }
    if (!closed || value->parent->screens) {
        set_error(value->context, MPC_UI_DETACH_FAILED);
        value->transition = 0;
        return 0;
    }
    value->opened = 0;
    value->close_requested = 0;
    value->suspended = 0;
    value->transition = 0;
    return 1;
}

extern "C" int mpc_ui_private_app_host_destroy(MpcUiAppHost **host,
                                                  int forced) {
    if (!host) return 0;
    MpcUiAppHostImpl *value = as_app_host(*host);
    if (!value || !on_ui_thread(value->context)) return 0;
    if (value->transition || value->context->callback_depth) {
        if (forced) invalidate_parent_screens(value->parent);
        set_error(value->context, MPC_UI_CALLBACK_ACTIVE);
        return 0;
    }
    if (value->opened) {
        if (!forced) {
            set_error(value->context, MPC_UI_LIFETIME_ORDER);
            return 0;
        }
        value->transition = 1;
        value->close_requested = 1;
        int readiness = MPC_UI_APP_CLOSE_FAILED;
        try { readiness = value->callbacks.prepare_close(value->owner, 1); }
        catch (...) { set_error(value->context, MPC_UI_HOST_CALL_FAILED); }
        int closed = 0;
        if (readiness == MPC_UI_APP_CLOSE_READY) {
            try { closed = value->callbacks.close(value->owner); }
            catch (...) { set_error(value->context, MPC_UI_HOST_CALL_FAILED); }
        }
        if (readiness != MPC_UI_APP_CLOSE_READY || !closed ||
            value->parent->screens) {
            invalidate_parent_screens(value->parent);
            set_error(value->context, MPC_UI_LIFETIME_ORDER);
            value->transition = 0;
            return 0;
        }
        value->opened = 0;
        value->close_requested = 0;
        value->suspended = 0;
        value->transition = 0;
    }
    MpcUiContextImpl *context = value->context;
    MpcUiParentImpl *parent = value->parent;
    value->callbacks = {};
    value->owner = nullptr;
    value->registered = 0;
    value->magic = 0;
    context->app_hosts--;
    parent->app_hosts--;
    try { context->host.sized_delete(value, sizeof(*value)); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); return 0; }
    *host = nullptr;
    return 1;
}

extern "C" void *mpc_ui_private_control_host_object(
    const MpcUiControl *control) {
    auto *value = as_control((MpcUiControl *)control);
    return value ? value->object : nullptr;
}

extern "C" void *mpc_ui_private_list_host_object(const MpcUiList *list) {
    auto *value = as_list((MpcUiList *)list);
    return value ? value->object : nullptr;
}

extern "C" void *mpc_ui_private_slider_host_object(
    const MpcUiSlider *slider) {
    auto *value = as_slider((MpcUiSlider *)slider);
    return value ? value->object : nullptr;
}

extern "C" MpcUiScreen *mpc_ui_screen_create(MpcUiContext *context,
                                               MpcUiParent *parent,
                                               MpcUiRect bounds) {
    MpcUiContextImpl *ctx = as_context(context);
    MpcUiParentImpl *owner = as_parent(parent);
    if (!ctx || !owner || owner->context != ctx || !valid_rect(bounds) ||
        !on_ui_thread(ctx)) {
        set_error(ctx, MPC_UI_INVALID_ARGUMENT);
        return nullptr;
    }
    MpcUiScreenImpl *screen = nullptr;
    try {
        screen = (MpcUiScreenImpl *)ctx->host.allocate(sizeof(*screen));
        if (!screen) { set_error(ctx, MPC_UI_ALLOCATION_FAILED); return nullptr; }
        memset(screen, 0, sizeof(*screen));
        screen->magic = kScreenMagic;
        screen->context = ctx;
        screen->parent = owner;
        screen->parent_next = owner->screen_head;
        owner->screen_head = screen;
        screen->bounds = bounds;
        ctx->screens++;
        owner->screens++;
        return (MpcUiScreen *)screen;
    } catch (...) {
        if (screen) try { ctx->host.sized_delete(screen, sizeof(*screen)); } catch (...) {}
        set_error(ctx, MPC_UI_HOST_CALL_FAILED);
        return nullptr;
    }
}

extern "C" MpcUiControl *mpc_ui_button_create(
    MpcUiScreen *screen, const char *text, uint32_t intent,
    MpcUiIntentCallback callback, void *callback_owner) {
    MpcUiScreenImpl *owner = as_screen(screen);
    if (!owner || !valid_text(text) || owner->closing || !on_ui_thread(owner->context)) {
        if (owner) set_error(owner->context, MPC_UI_INVALID_ARGUMENT);
        return nullptr;
    }
    MpcUiContextImpl *context = owner->context;
    MpcUiPrivateHost &host = context->host;
    MpcUiControlImpl *control = nullptr;
    bool label_live = false, button_live = false;
    alignas(4) unsigned char label[4] = {};
    try {
        control = (MpcUiControlImpl *)host.allocate(sizeof(*control));
        if (!control) throw (uint32_t)MPC_UI_ALLOCATION_FAILED;
        memset(control, 0, sizeof(*control));
        control->magic = kControlMagic;
        control->screen = owner;
        control->kind = kControlButton;
        control->object = host.allocate(MPC_UI_BUTTON_BYTES);
        if (!control->object) throw (uint32_t)MPC_UI_ALLOCATION_FAILED;
        host.string_ctor(label, text); label_live = true;
        host.button_ctor(control->object, label); button_live = true;
        host.string_dtor(label); label_live = false;
        const uint32_t table_bytes = MPC_UI_BUTTON_VTABLE_BYTES + sizeof(Binding);
        control->vtable = host.allocate(table_bytes);
        if (!control->vtable) throw (uint32_t)MPC_UI_ALLOCATION_FAILED;
        memcpy(control->vtable, host.button_vtable, MPC_UI_BUTTON_VTABLE_BYTES);
        control->binding = (Binding *)((unsigned char *)control->vtable +
                                      MPC_UI_BUTTON_VTABLE_BYTES);
        memset(control->binding, 0, sizeof(*control->binding));
        control->binding->magic = kBindingMagic;
        control->binding->live = 1u;
        control->binding->context = context;
        control->binding->screen = owner;
        control->binding->callback = callback;
        control->binding->owner = callback_owner;
        control->binding->intent = intent;
        host.string_ctor(control->binding->paint_label, text);
        control->binding->paint_label_live = 1u;
        ((void **)control->vtable)[2 + 46] = (void *)(uintptr_t)&mpc_ui_button_clicked;
        ((void **)control->vtable)[2 + 48] = (void *)(uintptr_t)&mpc_ui_button_paint;
        *(void **)control->object = (unsigned char *)control->vtable + 8;
        control->next = owner->controls;
        owner->controls = control;
        return (MpcUiControl *)control;
    } catch (uint32_t error) {
        set_error(context, error);
    } catch (...) {
        set_error(context, MPC_UI_HOST_CALL_FAILED);
    }
    if (label_live) try { host.string_dtor(label); } catch (...) {}
    if (button_live && control && control->object) {
        *(void **)control->object = (unsigned char *)host.button_vtable + 8;
        try { host.button_dtor(control->object); } catch (...) {}
    }
    if (control && control->object)
        try { host.sized_delete(control->object, MPC_UI_BUTTON_BYTES); } catch (...) {}
    if (control && control->binding && control->binding->paint_label_live) {
        try { host.string_dtor(control->binding->paint_label); } catch (...) {}
        control->binding->paint_label_live = 0;
    }
    if (control && control->vtable)
        try { host.sized_delete(control->vtable,
                                MPC_UI_BUTTON_VTABLE_BYTES + sizeof(Binding)); } catch (...) {}
    if (control) try { host.sized_delete(control, sizeof(*control)); } catch (...) {}
    return nullptr;
}

extern "C" MpcUiControl *mpc_ui_label_create(MpcUiScreen *screen,
                                                const char *text) {
    MpcUiScreenImpl *owner = as_screen(screen);
    if (!owner || !valid_text(text) || owner->closing ||
        !on_ui_thread(owner->context)) {
        if (owner) set_error(owner->context, MPC_UI_INVALID_ARGUMENT);
        return nullptr;
    }
    MpcUiContextImpl *context = owner->context;
    MpcUiPrivateHost &host = context->host;
    MpcUiControlImpl *control = nullptr;
    int name_live = 0, text_live = 0, label_live = 0;
    alignas(4) unsigned char native_name[4] = {};
    alignas(4) unsigned char native_text[4] = {};
    try {
        control = (MpcUiControlImpl *)host.allocate(sizeof(*control));
        if (!control) throw (uint32_t)MPC_UI_ALLOCATION_FAILED;
        memset(control, 0, sizeof(*control));
        control->magic = kControlMagic;
        control->screen = owner;
        control->kind = kControlLabel;
        control->object = host.allocate(MPC_UI_LABEL_BYTES);
        if (!control->object) throw (uint32_t)MPC_UI_ALLOCATION_FAILED;
        host.string_ctor(native_name, "mpclearn-label");
        name_live = 1;
        host.string_ctor(native_text, text);
        text_live = 1;
        host.label_ctor(control->object, native_name, native_text);
        label_live = 1;
        host.string_dtor(native_text);
        text_live = 0;
        host.string_dtor(native_name);
        name_live = 0;
        host.component_set_intercepts_mouse(control->object, 0, 0);
        control->next = owner->controls;
        owner->controls = control;
        return (MpcUiControl *)control;
    } catch (uint32_t error) {
        set_error(context, error);
    } catch (...) {
        set_error(context, MPC_UI_HOST_CALL_FAILED);
    }
    if (text_live) try { host.string_dtor(native_text); } catch (...) {}
    if (name_live) try { host.string_dtor(native_name); } catch (...) {}
    if (label_live && control && control->object)
        try { host.label_dtor(control->object); } catch (...) {}
    if (control && control->object)
        try { host.sized_delete(control->object, MPC_UI_LABEL_BYTES); }
        catch (...) {}
    if (control)
        try { host.sized_delete(control, sizeof(*control)); } catch (...) {}
    return nullptr;
}

extern "C" MpcUiList *mpc_ui_list_create(
    MpcUiScreen *screen, const char *name, uint32_t row_count,
    uint32_t row_height, MpcUiListRowText row_text,
    MpcUiListSelectionCallback selected, void *model_owner) {
    MpcUiScreenImpl *owner = as_screen(screen);
    if (!owner || !valid_text(name) || row_count > 256u || !row_height ||
        row_height > 256u || !row_text || !model_owner || owner->closing ||
        !on_ui_thread(owner->context)) {
        if (owner) set_error(owner->context, MPC_UI_INVALID_ARGUMENT);
        return nullptr;
    }
    MpcUiContextImpl *context = owner->context;
    MpcUiPrivateHost &host = context->host;
    MpcUiListImpl *list = nullptr;
    int name_live = 0, native_live = 0;
    alignas(4) unsigned char native_name[4] = {};
    try {
        list = (MpcUiListImpl *)host.allocate(sizeof(*list));
        if (!list) throw (uint32_t)MPC_UI_ALLOCATION_FAILED;
        memset(list, 0, sizeof(*list));
        list->magic = kListMagic;
        list->screen = owner;
        list->model_vtable = host.allocate(MPC_UI_LIST_MODEL_VTABLE_BYTES);
        list->model = (ListModel *)host.allocate(sizeof(*list->model));
        list->object = host.allocate(MPC_UI_LIST_BYTES);
        if (!list->model_vtable || !list->model || !list->object)
            throw (uint32_t)MPC_UI_ALLOCATION_FAILED;
        memcpy(list->model_vtable, host.list_model_vtable,
               MPC_UI_LIST_MODEL_VTABLE_BYTES);
        memset(list->model, 0, sizeof(*list->model));
        list->model->vptr = (unsigned char *)list->model_vtable + 8;
        list->model->magic = kListModelMagic;
        list->model->live = 1;
        list->model->context = context;
        list->model->screen = owner;
        list->model->row_text = row_text;
        list->model->selected = selected;
        list->model->owner = model_owner;
        list->model->row_count = row_count;
        void **table = (void **)list->model_vtable;
        table[2 + 0] = (void *)(uintptr_t)&mpc_ui_list_model_complete_destructor;
        table[2 + 1] = (void *)(uintptr_t)&mpc_ui_list_model_deleting_destructor;
        table[2 + 2] = (void *)(uintptr_t)&mpc_ui_list_model_count;
        table[2 + 3] = (void *)(uintptr_t)&mpc_ui_list_model_paint;
        table[2 + 4] = (void *)(uintptr_t)&mpc_ui_list_model_refresh;
        table[2 + 5] = (void *)(uintptr_t)&mpc_ui_list_model_clicked;
        table[2 + 8] = (void *)(uintptr_t)&mpc_ui_list_model_selected;
        host.string_ctor(native_name, name);
        name_live = 1;
        host.list_ctor(list->object, native_name, list->model);
        native_live = 1;
        list->model_attached = 1;
        name_live = 0;
        host.string_dtor(native_name);
        host.list_set_row_height(list->object, (int)row_height);
        list->viewport = host.list_get_viewport(list->object);
        if (!list->viewport) throw (uint32_t)MPC_UI_HOST_CALL_FAILED;
        host.viewport_set_drag(list->viewport, 1, MPC_UI_LIST_DRAG_THRESHOLD);
        list->drag_enabled = 1;
        list->next = owner->lists;
        owner->lists = list;
        return (MpcUiList *)list;
    } catch (uint32_t error) {
        set_error(context, error);
    } catch (...) {
        set_error(context, MPC_UI_HOST_CALL_FAILED);
    }
    if (name_live) try { host.string_dtor(native_name); } catch (...) {}
    if (native_live && list && list->object) {
        if (list->drag_enabled && list->viewport) {
            try { host.viewport_set_drag(list->viewport, 0,
                                         MPC_UI_LIST_DRAG_THRESHOLD); }
            catch (...) {}
            list->drag_enabled = 0;
        }
        try { host.list_set_model(list->object, nullptr); } catch (...) {}
        try { host.list_dtor(list->object); } catch (...) {}
    }
    if (list && list->object)
        try { host.sized_delete(list->object, MPC_UI_LIST_BYTES); } catch (...) {}
    if (list && list->model)
        try { host.sized_delete(list->model, sizeof(*list->model)); } catch (...) {}
    if (list && list->model_vtable)
        try { host.sized_delete(list->model_vtable,
                                MPC_UI_LIST_MODEL_VTABLE_BYTES); } catch (...) {}
    if (list) try { host.sized_delete(list, sizeof(*list)); } catch (...) {}
    return nullptr;
}

extern "C" MpcUiSlider *mpc_ui_slider_create(
    MpcUiScreen *screen, double minimum, double maximum, double interval,
    double initial_value, enum MpcUiSliderStyle style, uint32_t intent,
    MpcUiSliderCallback callback, void *callback_owner) {
    MpcUiScreenImpl *owner = as_screen(screen);
    if (!owner || !callback || owner->closing ||
        !__builtin_isfinite(minimum) || !__builtin_isfinite(maximum) ||
        !__builtin_isfinite(interval) || !__builtin_isfinite(initial_value) ||
        !(minimum < maximum) || interval < 0.0 || initial_value < minimum ||
        initial_value > maximum ||
        (style != MPC_UI_SLIDER_HORIZONTAL &&
         style != MPC_UI_SLIDER_VERTICAL) ||
        !on_ui_thread(owner->context)) {
        if (owner) set_error(owner->context, MPC_UI_INVALID_ARGUMENT);
        return nullptr;
    }
    MpcUiContextImpl *context = owner->context;
    MpcUiPrivateHost &host = context->host;
    MpcUiSliderImpl *slider = nullptr;
    int native_live = 0;
    try {
        slider = (MpcUiSliderImpl *)host.allocate(sizeof(*slider));
        if (!slider) throw (uint32_t)MPC_UI_ALLOCATION_FAILED;
        memset(slider, 0, sizeof(*slider));
        slider->magic = kSliderMagic;
        slider->screen = owner;
        slider->binding =
            (SliderBinding *)host.allocate(sizeof(*slider->binding));
        slider->object = host.allocate(MPC_UI_SLIDER_BYTES);
        if (!slider->binding || !slider->object)
            throw (uint32_t)MPC_UI_ALLOCATION_FAILED;
        memset(slider->binding, 0, sizeof(*slider->binding));
        slider->binding->magic = kSliderBindingMagic;
        slider->binding->live = 1;
        slider->binding->context = context;
        slider->binding->screen = owner;
        slider->binding->callback = callback;
        slider->binding->owner = callback_owner;
        slider->binding->intent = intent;
        slider->binding->listener_vtable[0] = nullptr;
        slider->binding->listener_vtable[1] = nullptr;
        slider->binding->listener_vtable[2] =
            (void *)(uintptr_t)&mpc_ui_slider_listener_complete_destructor;
        slider->binding->listener_vtable[3] =
            (void *)(uintptr_t)&mpc_ui_slider_listener_deleting_destructor;
        slider->binding->listener_vtable[4] =
            (void *)(uintptr_t)&mpc_ui_slider_value_changed;
        slider->binding->listener_vtable[5] =
            (void *)(uintptr_t)&mpc_ui_slider_drag_started;
        slider->binding->listener_vtable[6] =
            (void *)(uintptr_t)&mpc_ui_slider_drag_ended;
        slider->binding->vptr = &slider->binding->listener_vtable[2];
        host.slider_ctor(slider->object);
        native_live = 1;
        host.slider_set_style(slider->object, (int)style);
        host.slider_set_range(slider->object, minimum, maximum, interval);
        host.slider_set_text_box(slider->object, 0, 1, 0, 0);
        host.slider_set_value(slider->object, initial_value, 0);
        host.slider_add_listener(slider->object, slider->binding);
        slider->listener_attached = 1;
        slider->next = owner->sliders;
        owner->sliders = slider;
        return (MpcUiSlider *)slider;
    } catch (uint32_t error) {
        set_error(context, error);
    } catch (...) {
        set_error(context, MPC_UI_HOST_CALL_FAILED);
    }
    if (slider && slider->listener_attached) {
        try { host.slider_remove_listener(slider->object, slider->binding); }
        catch (...) {}
        slider->listener_attached = 0;
    }
    if (native_live && slider && slider->object)
        try { host.slider_dtor(slider->object); } catch (...) {}
    if (slider && slider->object)
        try { host.sized_delete(slider->object, MPC_UI_SLIDER_BYTES); }
        catch (...) {}
    if (slider && slider->binding)
        try { host.sized_delete(slider->binding, sizeof(*slider->binding)); }
        catch (...) {}
    if (slider)
        try { host.sized_delete(slider, sizeof(*slider)); } catch (...) {}
    return nullptr;
}

extern "C" int mpc_ui_list_style(MpcUiList *list,
                                   MpcUiButtonStyle style) {
    MpcUiListImpl *value = as_list(list);
    if (!value || !on_ui_thread(value->screen->context)) return 0;
    value->model->normal = style.normal;
    value->model->selection = style.selected;
    value->model->text = style.text;
    try {
        value->screen->context->host.set_colour(
            value->object, MPC_UI_LIST_BACKGROUND_COLOUR_ID, style.normal);
        value->screen->context->host.repaint(value->object);
        return 1;
    } catch (...) {
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_list_update(MpcUiList *list, uint32_t row_count,
                                    uint32_t selected_row) {
    MpcUiListImpl *value = as_list(list);
    if (!value || row_count > 256u ||
        (selected_row != MPC_UI_NO_SELECTION && selected_row >= row_count) ||
        !value->model_attached || !on_ui_thread(value->screen->context)) {
        if (value) set_error(value->screen->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    value->model->suppress_selection = 1;
    value->model->row_count = row_count;
    try {
        value->screen->context->host.list_update_content(value->object);
        value->screen->context->host.list_select_row(
            value->object,
            selected_row == MPC_UI_NO_SELECTION ? -1 : (int)selected_row,
            0, 1);
        value->screen->context->host.repaint(value->object);
        value->model->suppress_selection = 0;
        return 1;
    } catch (...) {
        value->model->suppress_selection = 0;
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_list_place(MpcUiList *list, MpcUiRect bounds) {
    MpcUiListImpl *value = as_list(list);
    if (!value || !valid_rect(bounds) || !on_ui_thread(value->screen->context))
        return 0;
    if (bounds.width > value->screen->bounds.width ||
        bounds.height > value->screen->bounds.height ||
        bounds.x > value->screen->bounds.width - bounds.width ||
        bounds.y > value->screen->bounds.height - bounds.height) {
        set_error(value->screen->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    if (!attach_list(value)) return 0;
    try {
        value->screen->context->host.set_bounds(
            value->object, value->screen->bounds.x + bounds.x,
            value->screen->bounds.y + bounds.y, bounds.width, bounds.height);
        value->screen->context->host.set_visible(value->object,
                                                  value->screen->visible != 0);
        value->bounds = bounds;
        value->requested_visible = 1;
        value->visible = value->screen->visible;
        return 1;
    } catch (...) {
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_list_visible(MpcUiList *list, int visible) {
    MpcUiListImpl *value = as_list(list);
    if (!value || !on_ui_thread(value->screen->context)) return 0;
    value->requested_visible = visible != 0;
    try {
        int shown = value->screen->visible && value->requested_visible;
        if (value->attached)
            value->screen->context->host.set_visible(value->object, shown);
        value->visible = shown;
        return 1;
    } catch (...) {
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_slider_set_value(MpcUiSlider *slider, double value) {
    MpcUiSliderImpl *widget = as_slider(slider);
    if (!widget || !__builtin_isfinite(value) ||
        !on_ui_thread(widget->screen->context)) {
        if (widget) set_error(widget->screen->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    try {
        widget->screen->context->host.slider_set_value(widget->object, value, 0);
        return 1;
    } catch (...) {
        set_error(widget->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_slider_place(MpcUiSlider *slider,
                                      MpcUiRect bounds) {
    MpcUiSliderImpl *value = as_slider(slider);
    if (!value || !valid_rect(bounds) ||
        !on_ui_thread(value->screen->context)) return 0;
    if (bounds.width > value->screen->bounds.width ||
        bounds.height > value->screen->bounds.height ||
        bounds.x > value->screen->bounds.width - bounds.width ||
        bounds.y > value->screen->bounds.height - bounds.height) {
        set_error(value->screen->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    if (!attach_slider(value)) return 0;
    try {
        value->screen->context->host.set_bounds(
            value->object, value->screen->bounds.x + bounds.x,
            value->screen->bounds.y + bounds.y, bounds.width, bounds.height);
        value->screen->context->host.set_visible(
            value->object, value->screen->visible != 0);
        value->bounds = bounds;
        value->requested_visible = 1;
        value->visible = value->screen->visible;
        return 1;
    } catch (...) {
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_slider_visible(MpcUiSlider *slider, int visible) {
    MpcUiSliderImpl *value = as_slider(slider);
    if (!value || !on_ui_thread(value->screen->context)) return 0;
    value->requested_visible = visible != 0;
    try {
        int shown = value->screen->visible && value->requested_visible;
        if (value->attached)
            value->screen->context->host.set_visible(value->object, shown);
        value->visible = shown;
        return 1;
    } catch (...) {
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_slider_place_grid(
    MpcUiSlider *slider, MpcUiGrid grid, unsigned row, unsigned column,
    unsigned row_span, unsigned column_span) {
    MpcUiSliderImpl *value = as_slider(slider);
    if (!value || !grid.rows || !grid.columns || grid.rows > 64 ||
        grid.columns > 64 || !row_span || !column_span ||
        row_span > grid.rows || column_span > grid.columns ||
        row >= grid.rows || column >= grid.columns ||
        row > grid.rows - row_span || column > grid.columns - column_span ||
        grid.row_gap < 0 || grid.row_gap > 4096 || grid.column_gap < 0 ||
        grid.column_gap > 4096 || grid.inset_top < 0 ||
        grid.inset_top > 4096 || grid.inset_right < 0 ||
        grid.inset_right > 4096 || grid.inset_bottom < 0 ||
        grid.inset_bottom > 4096 || grid.inset_left < 0 ||
        grid.inset_left > 4096) {
        if (value) set_error(value->screen->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    MpcUiRect outer = value->screen->bounds;
    int width = outer.width - grid.inset_left - grid.inset_right -
                (int)(grid.columns - 1) * grid.column_gap;
    int height = outer.height - grid.inset_top - grid.inset_bottom -
                 (int)(grid.rows - 1) * grid.row_gap;
    if (width < (int)grid.columns || height < (int)grid.rows) return 0;
    int cell_width = width / (int)grid.columns;
    int cell_height = height / (int)grid.rows;
    MpcUiRect bounds = {
        grid.inset_left + (int)column * (cell_width + grid.column_gap),
        grid.inset_top + (int)row * (cell_height + grid.row_gap),
        cell_width * (int)column_span + grid.column_gap * (int)(column_span - 1),
        cell_height * (int)row_span + grid.row_gap * (int)(row_span - 1),
    };
    return mpc_ui_slider_place(slider, bounds);
}

extern "C" int mpc_ui_button_style(MpcUiControl *control,
                                     MpcUiButtonStyle style) {
    MpcUiControlImpl *value = as_control(control);
    if (!value || value->kind != kControlButton ||
        !on_ui_thread(value->screen->context)) return 0;
    MpcUiContextImpl *context = value->screen->context;
    value->binding->normal = style.normal;
    value->binding->selected = style.selected;
    value->binding->text = style.text;
    try {
        context->host.set_colour(value->object, kButtonColour, style.normal);
        context->host.set_colour(value->object, kButtonOnColour, style.selected);
        context->host.set_colour(value->object, kTextOffColour, style.text);
        context->host.set_colour(value->object, kTextOnColour, style.text);
        context->host.repaint(value->object);
        return 1;
    } catch (...) {
        set_error(context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_button_text(MpcUiControl *control, const char *text) {
    MpcUiControlImpl *value = as_control(control);
    if (!value || value->kind != kControlButton || !valid_text(text) ||
        !on_ui_thread(value->screen->context)) {
        if (value) set_error(value->screen->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    MpcUiContextImpl *context = value->screen->context;
    alignas(4) unsigned char replacement[4] = {};
    int replacement_live = 0;
    try {
        context->host.string_ctor(replacement, text);
        replacement_live = 1;
        unsigned char previous[4];
        memcpy(previous, value->binding->paint_label, sizeof(previous));
        memcpy(value->binding->paint_label, replacement, sizeof(replacement));
        memcpy(replacement, previous, sizeof(previous));
        replacement_live = 0;
        context->host.string_dtor(replacement);
        context->host.repaint(value->object);
        return 1;
    } catch (...) {
        if (replacement_live) {
            try { context->host.string_dtor(replacement); } catch (...) {}
        }
        set_error(context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_button_selected(MpcUiControl *control, int selected) {
    MpcUiControlImpl *value = as_control(control);
    if (!value || value->kind != kControlButton ||
        !on_ui_thread(value->screen->context)) return 0;
    MpcUiContextImpl *context = value->screen->context;
    value->binding->is_selected = selected != 0;
    try {
        context->host.set_toggle(value->object, selected != 0, 0);
        context->host.repaint(value->object);
        return 1;
    } catch (...) {
        set_error(context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_label_style(MpcUiControl *control,
                                     MpcUiLabelStyle style) {
    MpcUiControlImpl *value = as_control(control);
    if (!value || value->kind != kControlLabel ||
        !on_ui_thread(value->screen->context)) return 0;
    try {
        value->screen->context->host.set_colour(
            value->object, MPC_UI_LABEL_BACKGROUND_COLOUR_ID,
            style.background);
        value->screen->context->host.set_colour(
            value->object, MPC_UI_LABEL_TEXT_COLOUR_ID, style.text);
        value->screen->context->host.label_set_justification(
            value->object, style.justification);
        value->screen->context->host.repaint(value->object);
        return 1;
    } catch (...) {
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_label_text(MpcUiControl *control,
                                    const char *text) {
    MpcUiControlImpl *value = as_control(control);
    if (!value || value->kind != kControlLabel || !valid_text(text) ||
        !on_ui_thread(value->screen->context)) {
        if (value) set_error(value->screen->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    alignas(4) unsigned char native_text[4] = {};
    int text_live = 0;
    try {
        value->screen->context->host.string_ctor(native_text, text);
        text_live = 1;
        value->screen->context->host.label_set_text(
            value->object, native_text, 0);
        text_live = 0;
        value->screen->context->host.string_dtor(native_text);
        return 1;
    } catch (...) {
        if (text_live) {
            try { value->screen->context->host.string_dtor(native_text); }
            catch (...) {}
        }
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_control_place(MpcUiControl *control, MpcUiRect bounds) {
    MpcUiControlImpl *value = as_control(control);
    if (!value || !valid_rect(bounds) || !on_ui_thread(value->screen->context)) return 0;
    if (bounds.width > value->screen->bounds.width ||
        bounds.height > value->screen->bounds.height ||
        bounds.x > value->screen->bounds.width - bounds.width ||
        bounds.y > value->screen->bounds.height - bounds.height) {
        set_error(value->screen->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    if (!attach_control(value)) return 0;
    if (value->binding) {
        value->binding->width = (uint32_t)bounds.width;
        value->binding->height = (uint32_t)bounds.height;
    }
    try {
        value->screen->context->host.set_bounds(
            value->object, value->screen->bounds.x + bounds.x,
            value->screen->bounds.y + bounds.y, bounds.width, bounds.height);
        value->screen->context->host.set_visible(value->object,
                                                  value->screen->visible != 0);
        value->bounds = bounds;
        value->requested_visible = 1;
        value->visible = value->screen->visible;
        return 1;
    } catch (...) {
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_control_visible(MpcUiControl *control, int visible) {
    MpcUiControlImpl *value = as_control(control);
    if (!value || !on_ui_thread(value->screen->context)) return 0;
    value->requested_visible = visible != 0;
    try {
        int shown = value->screen->visible && value->requested_visible;
        if (value->attached) value->screen->context->host.set_visible(value->object, shown);
        value->visible = shown;
        return 1;
    } catch (...) {
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_control_keep_in_front(MpcUiControl *control) {
    MpcUiControlImpl *value = as_control(control);
    if (!value || !value->attached || !on_ui_thread(value->screen->context)) return 0;
    try {
        value->screen->context->host.set_always_on_top(value->object, 1);
        return 1;
    } catch (...) {
        set_error(value->screen->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_control_place_grid(MpcUiControl *control, MpcUiGrid grid,
                                           unsigned row, unsigned column,
                                           unsigned row_span,
                                           unsigned column_span) {
    MpcUiControlImpl *value = as_control(control);
    if (!value || !grid.rows || !grid.columns || grid.rows > 64 ||
        grid.columns > 64 || !row_span || !column_span ||
        row_span > grid.rows || column_span > grid.columns ||
        row >= grid.rows || column >= grid.columns ||
        row > grid.rows - row_span || column > grid.columns - column_span ||
        grid.row_gap < 0 || grid.row_gap > 4096 || grid.column_gap < 0 ||
        grid.column_gap > 4096 || grid.inset_top < 0 || grid.inset_top > 4096 ||
        grid.inset_right < 0 || grid.inset_right > 4096 ||
        grid.inset_bottom < 0 || grid.inset_bottom > 4096 ||
        grid.inset_left < 0 || grid.inset_left > 4096) {
        if (value) set_error(value->screen->context, MPC_UI_INVALID_ARGUMENT);
        return 0;
    }
    MpcUiRect outer = value->screen->bounds;
    int width = outer.width - grid.inset_left - grid.inset_right -
                (int)(grid.columns - 1) * grid.column_gap;
    int height = outer.height - grid.inset_top - grid.inset_bottom -
                 (int)(grid.rows - 1) * grid.row_gap;
    if (width < (int)grid.columns || height < (int)grid.rows) return 0;
    int cell_width = width / (int)grid.columns;
    int cell_height = height / (int)grid.rows;
    MpcUiRect bounds = {
        grid.inset_left + (int)column * (cell_width + grid.column_gap),
        grid.inset_top + (int)row * (cell_height + grid.row_gap),
        cell_width * (int)column_span + grid.column_gap * (int)(column_span - 1),
        cell_height * (int)row_span + grid.row_gap * (int)(row_span - 1),
    };
    return mpc_ui_control_place(control, bounds);
}

static void set_screen_children_visible(MpcUiScreenImpl *value, int shown) {
    for (MpcUiControlImpl *control = value->controls; control; control = control->next) {
        if (control->attached) {
            int visible = shown && control->requested_visible;
            value->context->host.set_visible(control->object, visible);
            control->visible = visible;
        }
    }
    for (MpcUiListImpl *list = value->lists; list; list = list->next) {
        if (list->attached) {
            int visible = shown && list->requested_visible;
            value->context->host.set_visible(list->object, visible);
            list->visible = visible;
        }
    }
    for (MpcUiSliderImpl *slider = value->sliders; slider;
         slider = slider->next) {
        if (slider->attached) {
            int visible = shown && slider->requested_visible;
            value->context->host.set_visible(slider->object, visible);
            slider->visible = visible;
        }
    }
}

static void restore_screen_children(MpcUiScreenImpl *value, int shown) noexcept {
    try { set_screen_children_visible(value, shown); }
    catch (...) { set_error(value->context, MPC_UI_HOST_CALL_FAILED); }
}

extern "C" int mpc_ui_screen_show(MpcUiScreen *screen) {
    MpcUiScreenImpl *value = as_screen(screen);
    if (!value || value->closing || !on_ui_thread(value->context)) return 0;
    if (value->visible) return 1;
    int focused = 0;
    try {
        if (value->parent->focus && !value->parent->focus(value->parent->owner, 1))
            throw 1;
        focused = value->parent->focus != nullptr;
        set_screen_children_visible(value, 1);
        value->visible = 1;
        return 1;
    } catch (...) {
        restore_screen_children(value, 0);
        if (focused)
            try { (void)value->parent->focus(value->parent->owner, 0); }
            catch (...) { set_error(value->context, MPC_UI_HOST_CALL_FAILED); }
        set_error(value->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_screen_hide(MpcUiScreen *screen) {
    MpcUiScreenImpl *value = as_screen(screen);
    if (!value || !on_ui_thread(value->context)) return 0;
    if (!value->visible) return 1;
    int hidden = 0;
    try {
        set_screen_children_visible(value, 0);
        hidden = 1;
        if (value->parent->focus && !value->parent->focus(value->parent->owner, 0))
            throw 1;
        value->visible = 0;
        return 1;
    } catch (...) {
        if (hidden) restore_screen_children(value, 1);
        set_error(value->context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
}

extern "C" int mpc_ui_screen_close(MpcUiScreen **screen) {
    if (!screen) return 0;
    MpcUiScreenImpl *value = as_screen(*screen);
    if (!value || !on_ui_thread(value->context)) return 0;
    if (value->context->callback_depth) {
        set_error(value->context, MPC_UI_CALLBACK_ACTIVE);
        return 0;
    }
    value->closing = 1;
    invalidate_bindings(value);
    if (!disable_list_drag(value)) return 0;
    if (!disconnect_list_models(value)) return 0;
    if (!disconnect_slider_listeners(value)) return 0;
    (void)mpc_ui_screen_hide((MpcUiScreen *)value);
    int detached = 1;
    for (MpcUiControlImpl *control = value->controls; control; control = control->next)
        if (!detach_control(control)) detached = 0;
    for (MpcUiListImpl *list = value->lists; list; list = list->next)
        if (!detach_list(list)) detached = 0;
    for (MpcUiSliderImpl *slider = value->sliders; slider;
         slider = slider->next)
        if (!detach_slider(slider)) detached = 0;
    if (!detached) return 0;
    MpcUiControlImpl *control = value->controls;
    while (control) {
        MpcUiControlImpl *next = control->next;
        destroy_control(control);
        control = next;
    }
    MpcUiListImpl *list = value->lists;
    while (list) {
        MpcUiListImpl *next = list->next;
        destroy_list(list);
        list = next;
    }
    MpcUiSliderImpl *slider = value->sliders;
    while (slider) {
        MpcUiSliderImpl *next = slider->next;
        destroy_slider(slider);
        slider = next;
    }
    MpcUiContextImpl *context = value->context;
    MpcUiParentImpl *parent = value->parent;
    MpcUiScreenImpl **link = &parent->screen_head;
    while (*link && *link != value) link = &(*link)->parent_next;
    if (*link == value) *link = value->parent_next;
    else {
        set_error(context, MPC_UI_LIFETIME_ORDER);
        return 0;
    }
    value->magic = 0;
    context->screens--;
    parent->screens--;
    try { context->host.sized_delete(value, sizeof(*value)); } catch (...) {
        set_error(context, MPC_UI_HOST_CALL_FAILED);
        return 0;
    }
    *screen = nullptr;
    return 1;
}
