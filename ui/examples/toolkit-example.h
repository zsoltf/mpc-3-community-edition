#ifndef MPCLEARN_UI_TOOLKIT_EXAMPLE_H
#define MPCLEARN_UI_TOOLKIT_EXAMPLE_H

#include "../include/mpclearn-ui.h"

#ifdef __cplusplus
extern "C" {
#endif

enum MpclearnToolkitExampleIntent {
    MPCLEARN_TOOLKIT_EXAMPLE_ACTION_ONE = 1,
    MPCLEARN_TOOLKIT_EXAMPLE_ACTION_TWO = 2,
    MPCLEARN_TOOLKIT_EXAMPLE_SLIDER = 3,
    MPCLEARN_TOOLKIT_EXAMPLE_BACK = 4,
};

typedef struct {
    MpcUiScreen *screen;
    MpcUiControl *title;
    MpcUiSlider *slider;
    MpcUiControl *action_one;
    MpcUiControl *action_two;
    MpcUiControl *back;
} MpclearnToolkitExampleScreen;

typedef struct {
    MpclearnToolkitExampleScreen view;
    MpcUiAppHost *host;
    MpcUiIntentCallback action_callback;
    MpcUiSliderCallback slider_callback;
    void *callback_owner;
    double initial_value;
} MpclearnToolkitExampleApp;

int mpclearn_toolkit_example_app_init(
    MpclearnToolkitExampleApp *app, double initial_value,
    MpcUiIntentCallback action_callback,
    MpcUiSliderCallback slider_callback, void *callback_owner);
const MpcUiAppCallbacks *mpclearn_toolkit_example_app_callbacks(void);
int mpclearn_toolkit_example_set_title(MpclearnToolkitExampleApp *app,
                                       const char *text);
int mpclearn_toolkit_example_set_value(MpclearnToolkitExampleApp *app,
                                       double value);

#ifdef __cplusplus
}
#endif

#endif
