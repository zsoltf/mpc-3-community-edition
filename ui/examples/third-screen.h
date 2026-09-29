#ifndef MPCLEARN_UI_THIRD_SCREEN_H
#define MPCLEARN_UI_THIRD_SCREEN_H

#include "../include/mpclearn-ui.h"

#ifdef __cplusplus
extern "C" {
#endif

enum MpclearnThirdScreenIntent {
    MPCLEARN_THIRD_ACTION_ONE = 1,
    MPCLEARN_THIRD_ACTION_TWO = 2,
    MPCLEARN_THIRD_BACK = 3,
};

typedef MpcUiScreen MpclearnThirdScreen;

typedef struct {
    MpclearnThirdScreen *screen;
    MpcUiAppHost *host;
    MpcUiIntentCallback callback;
    void *callback_owner;
} MpclearnThirdApp;

MpclearnThirdScreen *mpclearn_third_screen_open(
    MpcUiContext *context, MpcUiParent *parent, MpcUiRect bounds,
    MpcUiIntentCallback callback, void *callback_owner);
int mpclearn_third_screen_close(MpclearnThirdScreen **screen);

/* Public app consumer: register this state and callback table with an opaque
 * MpcUiAppHost supplied by the admitted app adapter. */
int mpclearn_third_app_init(MpclearnThirdApp *app,
                            MpcUiIntentCallback callback,
                            void *callback_owner);
const MpcUiAppCallbacks *mpclearn_third_app_callbacks(void);

#ifdef __cplusplus
}
#endif

#endif
