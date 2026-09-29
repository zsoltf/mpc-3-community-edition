#ifndef MPCLEARN_UI_PRIVATE_H
#define MPCLEARN_UI_PRIVATE_H

#include "../include/mpclearn-ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*MpcUiParentAttach)(void *owner, void *child);
typedef int (*MpcUiParentDetach)(void *owner, void *child);
typedef int (*MpcUiParentFocus)(void *owner, int active);

typedef struct {
    uint32_t bytes;
    uint32_t profile;
    uint32_t image_bias;
    uint32_t ui_thread_id;
} MpcUiAdmittedProfile;

typedef struct {
    void *(*allocate)(uint32_t);
    void (*sized_delete)(void *, uint32_t);
    void (*string_ctor)(void *, const char *);
    void (*string_dtor)(void *);
    void (*set_colour)(void *, int, uint32_t);
    void (*button_ctor)(void *, const void *);
    void (*button_dtor)(void *);
    void (*label_ctor)(void *, const void *, const void *);
    void (*label_dtor)(void *);
    void (*label_set_text)(void *, const void *, int);
    void (*label_set_justification)(void *, int);
    void (*component_set_intercepts_mouse)(void *, int, int);
    void (*list_ctor)(void *, const void *, void *);
    void (*list_dtor)(void *);
    void (*list_set_model)(void *, void *);
    void (*list_update_content)(void *);
    void (*list_set_row_height)(void *, int);
    void (*list_select_row)(void *, int, int, int);
    void *(*list_get_viewport)(void *);
    void (*viewport_set_drag)(void *, int, float);
    void (*slider_ctor)(void *);
    void (*slider_dtor)(void *);
    void (*slider_set_style)(void *, int);
    void (*slider_set_range)(void *, double, double, double);
    void (*slider_set_text_box)(void *, int, int, int, int);
    void (*slider_add_listener)(void *, void *);
    void (*slider_remove_listener)(void *, void *);
    double (*slider_get_value)(void *);
    void (*slider_set_value)(void *, double, int);
    void (*add_child)(void *, void *, int);
    void (*remove_child)(void *, void *);
    void (*set_bounds)(void *, int, int, int, int);
    void (*set_visible)(void *, int);
    void (*set_always_on_top)(void *, int);
    void (*set_toggle)(void *, int, int);
    void (*repaint)(void *);
    void (*graphics_set_colour)(void *, uint32_t);
    void (*graphics_set_font)(void *, float);
    void (*graphics_fill_all)(void *, uint32_t);
    void (*graphics_draw_fitted_text)(void *, const void *, int, int, int,
                                      int, int, int, float);
    const unsigned char *button_vtable;
    const unsigned char *list_model_vtable;
} MpcUiPrivateHost;

/* Call only after the consumer's existing admission owner has accepted the
 * exact pinned executable and established the current owner UI thread. */
MpcUiContext *mpc_ui_private_context_from_admitted(
    const MpcUiAdmittedProfile *admitted);

/* Component composition only: the same toolkit implementation with explicit
 * substituted host functions. It performs no executable admission. */
MpcUiContext *mpc_ui_private_context_for_component(
    const MpcUiPrivateHost *host, uint32_t ui_thread_id);

int mpc_ui_private_context_destroy(MpcUiContext **context);

MpcUiParent *mpc_ui_private_parent_create(
    MpcUiContext *context, void *owner,
    MpcUiParentAttach attach, MpcUiParentDetach detach,
    MpcUiParentFocus focus);

/* Convenience for a qualified JUCE Component parent. Focus remains adapter
 * owned and therefore absent from this generic parent. */
MpcUiParent *mpc_ui_private_component_parent(MpcUiContext *context,
                                             void *component);
int mpc_ui_private_parent_destroy(MpcUiParent **parent);

/* Adapter-owned host for one public app consumer. The adapter supplies its
 * already admitted context, qualified parent and parent-local bounds. */
MpcUiAppHost *mpc_ui_private_app_host_create(
    MpcUiContext *context, MpcUiParent *parent, MpcUiRect bounds);
/* Runs a requested normal close. A pending prepare-close is retained for the
 * next owning UI-thread drain. */
int mpc_ui_private_app_host_drain(MpcUiAppHost *host);
/* Temporarily yields the native parent to its stock screen without closing the
 * consumer. Each owned screen resumes only if it was visible at suspension. */
int mpc_ui_private_app_host_suspend(MpcUiAppHost *host);
int mpc_ui_private_app_host_resume(MpcUiAppHost *host);
int mpc_ui_private_app_host_is_suspended(const MpcUiAppHost *host);
/* Owner destruction has no later drain. This invalidates callbacks and asks
 * the consumer for synchronous forced cleanup before releasing the slot. */
int mpc_ui_private_app_host_destroy(MpcUiAppHost **host, int forced);
/* Diagnostic readback for the admitted owner. No consumer should branch on it. */
uint32_t mpc_ui_private_app_host_callback_depth(const MpcUiAppHost *host);
/* Forced native-owner teardown can invalidate a non-consumer screen before
 * close is attempted from an active callback. The screen remains owned and
 * must still be detached and destroyed when the owner permits it. */
int mpc_ui_private_screen_invalidate(MpcUiScreen *screen);

/* Adapter diagnostics and native ownership checks only. Public screens remain
 * opaque and must not derive host layout or state from this pointer. */
void *mpc_ui_private_control_host_object(const MpcUiControl *control);
void *mpc_ui_private_list_host_object(const MpcUiList *list);
void *mpc_ui_private_slider_host_object(const MpcUiSlider *slider);

#ifdef __cplusplus
}
#endif

#endif
