#ifndef MPCLEARN_UI_SLIDER_SCREEN_H
#define MPCLEARN_UI_SLIDER_SCREEN_H

#include "../include/mpclearn-ui.h"

#ifdef __cplusplus
extern "C" {
#endif

enum MpclearnSliderScreenIntent {
    MPCLEARN_SLIDER_VALUE = 1,
    MPCLEARN_SLIDER_BACK = 2,
};

typedef struct {
    MpcUiScreen *screen;
    MpcUiControl *title;
    MpcUiSlider *slider;
    MpcUiControl *back;
} MpclearnSliderScreen;

int mpclearn_slider_screen_open(
    MpclearnSliderScreen *screen, MpcUiContext *context, MpcUiParent *parent,
    MpcUiRect bounds, double initial_value,
    MpcUiIntentCallback action_callback,
    MpcUiSliderCallback slider_callback, void *callback_owner);
int mpclearn_slider_screen_set_value(MpclearnSliderScreen *screen,
                                     double value);
int mpclearn_slider_screen_set_title(MpclearnSliderScreen *screen,
                                     const char *text);
int mpclearn_slider_screen_close(MpclearnSliderScreen *screen);

#ifdef __cplusplus
}
#endif

#endif
