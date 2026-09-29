#ifndef MPCLEARN_UI_H
#define MPCLEARN_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MpcUiContext MpcUiContext;
typedef struct MpcUiParent MpcUiParent;
typedef struct MpcUiScreen MpcUiScreen;
typedef struct MpcUiControl MpcUiControl;
typedef struct MpcUiList MpcUiList;
typedef struct MpcUiSlider MpcUiSlider;
typedef struct MpcUiAppHost MpcUiAppHost;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} MpcUiRect;

typedef struct {
    unsigned rows;
    unsigned columns;
    int row_gap;
    int column_gap;
    int inset_top;
    int inset_right;
    int inset_bottom;
    int inset_left;
} MpcUiGrid;

typedef struct {
    uint32_t normal;
    uint32_t selected;
    uint32_t text;
} MpcUiButtonStyle;

typedef struct {
    uint32_t background;
    uint32_t text;
    int justification;
} MpcUiLabelStyle;

typedef void (*MpcUiIntentCallback)(void *owner, uint32_t intent);
typedef const char *(*MpcUiListRowText)(void *owner, uint32_t row);
typedef void (*MpcUiListSelectionCallback)(void *owner, uint32_t row);
typedef void (*MpcUiSliderCallback)(void *owner, uint32_t intent,
                                    uint32_t event, double value);
typedef int (*MpcUiAppOpenCallback)(void *owner, MpcUiAppHost *host,
                                    MpcUiContext *context, MpcUiParent *parent,
                                    MpcUiRect bounds);
typedef int (*MpcUiAppPrepareCloseCallback)(void *owner, int forced);
typedef int (*MpcUiAppCloseCallback)(void *owner);

typedef struct {
    uint32_t bytes;
    MpcUiAppOpenCallback open;
    MpcUiAppPrepareCloseCallback prepare_close;
    MpcUiAppCloseCallback close;
} MpcUiAppCallbacks;

#define MPC_UI_NO_SELECTION 0xffffffffu

enum MpcUiSliderStyle {
    MPC_UI_SLIDER_HORIZONTAL = 0,
    MPC_UI_SLIDER_VERTICAL = 1,
};

enum MpcUiSliderEvent {
    MPC_UI_SLIDER_VALUE_CHANGED = 1,
    MPC_UI_SLIDER_DRAG_STARTED = 2,
    MPC_UI_SLIDER_DRAG_ENDED = 3,
};

enum MpcUiError {
    MPC_UI_OK = 0,
    MPC_UI_INVALID_ARGUMENT = 1,
    MPC_UI_WRONG_THREAD = 2,
    MPC_UI_ALLOCATION_FAILED = 3,
    MPC_UI_HOST_CALL_FAILED = 4,
    MPC_UI_PARENT_INVALID = 5,
    MPC_UI_DETACH_FAILED = 6,
    MPC_UI_CALLBACK_ACTIVE = 7,
    MPC_UI_LIFETIME_ORDER = 8,
};

enum MpcUiAppCloseState {
    MPC_UI_APP_CLOSE_FAILED = 0,
    MPC_UI_APP_CLOSE_READY = 1,
    MPC_UI_APP_CLOSE_PENDING = 2,
};

uint32_t mpc_ui_context_error(const MpcUiContext *context);
void mpc_ui_context_clear_error(MpcUiContext *context);

/* The admitted app adapter supplies the opaque host. One consumer can be
 * registered at a time. Back callbacks request closure; the adapter drains
 * that request only after the active widget callback has returned. */
int mpc_ui_app_register(MpcUiAppHost *host,
                        const MpcUiAppCallbacks *callbacks, void *owner);
int mpc_ui_app_unregister(MpcUiAppHost *host);
int mpc_ui_app_open(MpcUiAppHost *host);
int mpc_ui_app_request_close(MpcUiAppHost *host);
int mpc_ui_app_is_open(const MpcUiAppHost *host);
int mpc_ui_app_is_closing(const MpcUiAppHost *host);

MpcUiScreen *mpc_ui_screen_create(MpcUiContext *context, MpcUiParent *parent,
                                  MpcUiRect bounds);
MpcUiControl *mpc_ui_button_create(MpcUiScreen *screen, const char *text,
                                   uint32_t intent, MpcUiIntentCallback callback,
                                   void *callback_owner);
MpcUiControl *mpc_ui_label_create(MpcUiScreen *screen, const char *text);
MpcUiList *mpc_ui_list_create(MpcUiScreen *screen, const char *name,
                              uint32_t row_count, uint32_t row_height,
                              MpcUiListRowText row_text,
                              MpcUiListSelectionCallback selected,
                              void *model_owner);
MpcUiSlider *mpc_ui_slider_create(MpcUiScreen *screen, double minimum,
                                  double maximum, double interval,
                                  double initial_value,
                                  enum MpcUiSliderStyle style,
                                  uint32_t intent,
                                  MpcUiSliderCallback callback,
                                  void *callback_owner);
int mpc_ui_list_style(MpcUiList *list, MpcUiButtonStyle style);
int mpc_ui_list_update(MpcUiList *list, uint32_t row_count,
                       uint32_t selected_row);
int mpc_ui_list_place(MpcUiList *list, MpcUiRect bounds);
int mpc_ui_list_visible(MpcUiList *list, int visible);
/* Owner refresh is silent: it never publishes a consumer gesture intent. */
int mpc_ui_slider_set_value(MpcUiSlider *slider, double value);
int mpc_ui_slider_place(MpcUiSlider *slider, MpcUiRect bounds);
int mpc_ui_slider_place_grid(MpcUiSlider *slider, MpcUiGrid grid,
                             unsigned row, unsigned column,
                             unsigned row_span, unsigned column_span);
int mpc_ui_slider_visible(MpcUiSlider *slider, int visible);
int mpc_ui_button_style(MpcUiControl *control, MpcUiButtonStyle style);
int mpc_ui_button_text(MpcUiControl *control, const char *text);
int mpc_ui_button_selected(MpcUiControl *control, int selected);
int mpc_ui_label_style(MpcUiControl *control, MpcUiLabelStyle style);
int mpc_ui_label_text(MpcUiControl *control, const char *text);
int mpc_ui_control_place(MpcUiControl *control, MpcUiRect bounds);
int mpc_ui_control_visible(MpcUiControl *control, int visible);
int mpc_ui_control_keep_in_front(MpcUiControl *control);
int mpc_ui_control_place_grid(MpcUiControl *control, MpcUiGrid grid,
                              unsigned row, unsigned column,
                              unsigned row_span, unsigned column_span);
int mpc_ui_screen_show(MpcUiScreen *screen);
int mpc_ui_screen_hide(MpcUiScreen *screen);
int mpc_ui_screen_close(MpcUiScreen **screen);

#ifdef __cplusplus
}
#endif

#endif
