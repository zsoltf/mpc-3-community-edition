#ifndef MPCLEARN_NATIVE_PREFERENCES_H
#define MPCLEARN_NATIVE_PREFERENCES_H

#ifdef NATIVE_PREFERENCES_FREESTANDING
typedef __UINT32_TYPE__ uint32_t;
#else
#include <stdint.h>
#endif

#define NATIVE_PREFERENCES_MAGIC 0x3255504eu /* NPU2 */
#define NATIVE_PREFERENCES_VERSION 2u
#define NATIVE_PREFERENCES_BYTES 4096u

enum {
 NATIVE_CONTROLLER_XTOUCH=1,
 NATIVE_CONTROLLER_XTOUCH_MINI=2,
 NATIVE_CONTROLLER_GENERIC=3
};
enum {
 NATIVE_PREFERENCES_IDLE=0,
 NATIVE_PREFERENCES_PENDING=1,
 NATIVE_PREFERENCES_APPLYING=2,
 NATIVE_PREFERENCES_ACTIVE=3,
 NATIVE_PREFERENCES_UNAVAILABLE=4,
 NATIVE_PREFERENCES_AMBIGUOUS=5,
 NATIVE_PREFERENCES_INVALID=6,
 NATIVE_PREFERENCES_WRITE_FAILED=7,
 NATIVE_PREFERENCES_RESTART_FAILED=8
};
#define NATIVE_PREFERENCES_ENDPOINT_MAX 128u

typedef struct {
 uint32_t magic,version,bytes,pid;
 uint32_t hook_entries,tabs_created,tabs_destroyed;
 uint32_t buttons_created,buttons_destroyed,clicks;
 uint32_t live_tabs,failures,last_failure;
 uint32_t last_overlay,last_tab,last_button;
 uint32_t buttons[3];
 uint32_t request_sequence,request_profile,claimed_sequence,completed_sequence;
 uint32_t status,saved_profile,active_profile,result_detail,result_revision;
 uint32_t endpoint_client_length,endpoint_port_length;
 unsigned char endpoint_client[NATIVE_PREFERENCES_ENDPOINT_MAX];
 unsigned char endpoint_port[NATIVE_PREFERENCES_ENDPOINT_MAX];
 unsigned char reserved[NATIVE_PREFERENCES_BYTES-30u*4u-2u*NATIVE_PREFERENCES_ENDPOINT_MAX];
} NativePreferencesState;

#ifdef __cplusplus
static_assert(sizeof(NativePreferencesState)==NATIVE_PREFERENCES_BYTES,"fixed native Preferences state page");
#else
_Static_assert(sizeof(NativePreferencesState)==NATIVE_PREFERENCES_BYTES,"fixed native Preferences state page");
#endif

#ifdef __cplusplus
extern "C" {
#endif
int native_preferences_initialize(void);
int native_preferences_install(uint32_t bias,void **mapping);
void native_preferences_hook(void *context,unsigned kind);
#ifdef __cplusplus
}
#endif

#endif
