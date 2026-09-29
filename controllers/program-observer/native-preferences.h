#ifndef MPCLEARN_NATIVE_PREFERENCES_H
#define MPCLEARN_NATIVE_PREFERENCES_H

#ifdef NATIVE_PREFERENCES_FREESTANDING
typedef __UINT32_TYPE__ uint32_t;
#else
#include <stdint.h>
#endif

#define NATIVE_PREFERENCES_MAGIC 0x3355504eu /* NPU3 */
#define NATIVE_PREFERENCES_VERSION 11u
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
enum {
 NATIVE_GLOBAL_LEARN_IDLE=0,
 NATIVE_GLOBAL_LEARN_ARMING=1,
 NATIVE_GLOBAL_LEARN_ARMED=2,
 NATIVE_GLOBAL_LEARN_CANCELING=3,
 NATIVE_GLOBAL_LEARN_UNSETTLED=4
};
#define NATIVE_PREFERENCES_ENDPOINT_MAX 128u
#define NATIVE_GLOBAL_ROWS 6u
#define NATIVE_GLOBAL_TEXT_MAX 64u

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
 uint32_t launcher_overlays_created,launcher_overlays_destroyed;
 uint32_t launcher_tiles_created,launcher_tiles_destroyed,launcher_clicks;
 uint32_t shells_created,shells_destroyed,back_clicks;
 uint32_t live_launcher_overlays,live_launcher_tiles,live_shells;
 uint32_t last_launcher_overlay,last_launcher_page,last_launcher_tile;
 uint32_t launcher_bind_overlay,launcher_bind_page,launcher_bind_parent;
 uint32_t launcher_bind_bounds[4];
 uint32_t launcher_layout_overlay,launcher_layout_page,launcher_layout_parent;
 uint32_t launcher_layout_bounds[4];
 uint32_t example_opens,example_closes,example_intents,example_back_clicks;
 uint32_t live_examples,last_example_intent;
 uint32_t overlay_destroy_entries,overlay_destroy_ui_thread_matches;
 uint32_t overlay_destroy_observer_running,overlay_destroy_consumer_open;
 uint32_t overlay_destroy_busy,overlay_destroy_callback_depth;
 uint32_t overlay_destroy_cleanup_complete,overlay_destroy_stock_calls;
 uint32_t overlay_destroy_retained;
 uint32_t global_owner_captures,global_screens_created,global_screens_destroyed;
 uint32_t global_refreshes,global_row_clicks,global_page_clicks;
 uint32_t live_global_screens,global_target_count,global_page_start;
 uint32_t global_selected_target,global_assigned_rows;
 uint32_t last_global_owner,last_global_vc,last_global_file,global_revision;
 uint32_t global_learn_requests,global_learn_settled,global_learn_cancels;
 uint32_t global_learn_timeouts,global_learn_state,global_learn_intent;
 uint32_t global_learn_pending,global_learn_owner_value,global_learn_mirror_value;
 uint32_t global_learn_target,global_learn_waits,global_target_selections;
 uint32_t global_pairing_polls,global_pairing_changes,global_row_repaints;
 uint32_t global_file_actions,global_file_completed,global_file_import_no_additions;
 uint32_t global_file_creates,global_file_copies,global_file_imports;
 uint32_t global_file_exports,global_file_selections,global_file_clears,global_file_no_changes;
 uint32_t global_file_action,global_file_count,global_file_index;
 uint32_t global_edit_requests,global_edit_completed,global_edit_no_changes;
 uint32_t global_edit_type,global_edit_channel,global_edit_data,global_edit_control,global_edit_reverse;
 unsigned char endpoint_client[NATIVE_PREFERENCES_ENDPOINT_MAX];
 unsigned char endpoint_port[NATIVE_PREFERENCES_ENDPOINT_MAX];
 unsigned char global_page_text[NATIVE_GLOBAL_TEXT_MAX];
 unsigned char global_status_text[NATIVE_GLOBAL_TEXT_MAX];
 unsigned char global_mapping_text[NATIVE_GLOBAL_TEXT_MAX];
 unsigned char global_file_status_text[NATIVE_GLOBAL_TEXT_MAX];
 unsigned char global_rows[NATIVE_GLOBAL_ROWS][NATIVE_GLOBAL_TEXT_MAX];
 unsigned char reserved[NATIVE_PREFERENCES_BYTES-124u*4u-2u*NATIVE_PREFERENCES_ENDPOINT_MAX-10u*NATIVE_GLOBAL_TEXT_MAX];
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
void native_preferences_refresh(void);
#ifdef __cplusplus
}
#endif

#endif
