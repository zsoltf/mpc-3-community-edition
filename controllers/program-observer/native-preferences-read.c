#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "native-preferences.h"

static const char *profile_name(unsigned profile){return profile==1?"xtouch":profile==2?"xtouch-mini":profile==3?"generic":"none";}
static const char *status_name(unsigned status){
 static const char *const names[]={"idle","pending","applying","active","unavailable","ambiguous","invalid","write-failed","restart-failed"};
 return status<sizeof(names)/sizeof(names[0])?names[status]:"unknown";
}
static const char *learn_name(unsigned status){
 static const char *const names[]={"idle","arming","armed","canceling","unsettled"};
 return status<sizeof(names)/sizeof(names[0])?names[status]:"unknown";
}

int main(int argc,char **argv){
 if(argc!=2||argv[1][0]!='/'){fprintf(stderr,"native-preferences-read /absolute/native-preferences.state\n");return 2;}
 int fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0){perror("open");return 1;}struct stat st={};
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=NATIVE_PREFERENCES_BYTES){fputs("invalid state file\n",stderr);close(fd);return 1;}
 const NativePreferencesState *s=mmap(NULL,sizeof(*s),PROT_READ,MAP_SHARED,fd,0);close(fd);if(s==MAP_FAILED){perror("mmap");return 1;}
 NativePreferencesState copy;unsigned result_before=0,result_after=0,global_before=0,global_after=0,stable=0;
 for(unsigned attempt=0;attempt<100&&!stable;attempt++){
  result_before=__atomic_load_n(&s->result_revision,__ATOMIC_ACQUIRE);
  global_before=__atomic_load_n(&s->global_revision,__ATOMIC_ACQUIRE);
  if((result_before|global_before)&1u)continue;
  memcpy(&copy,s,sizeof(copy));
  result_after=__atomic_load_n(&s->result_revision,__ATOMIC_ACQUIRE);
  global_after=__atomic_load_n(&s->global_revision,__ATOMIC_ACQUIRE);
  stable=result_before==result_after&&global_before==global_after&&!((result_after|global_after)&1u);
 }
 munmap((void*)s,sizeof(*s));if(!stable){fputs("changing state\n",stderr);return 1;}
 if(copy.magic!=NATIVE_PREFERENCES_MAGIC||copy.version!=NATIVE_PREFERENCES_VERSION||copy.bytes!=sizeof(copy)){fputs("invalid state header\n",stderr);return 1;}
 printf("pid=%u hook_entries=%u tabs=%u/%u buttons=%u/%u clicks=%u live_tabs=%u failures=%u last_failure=%u overlay=%08x tab=%08x button=%08x choices=%08x,%08x,%08x request=%u/%s claimed=%u completed=%u status=%s saved=%s active=%s detail=%u endpoint=%.*s/%.*s launcher_overlays=%u/%u launcher_tiles=%u/%u launcher_clicks=%u shells=%u/%u back_clicks=%u live_launcher=%u/%u/%u launcher=%08x/%08x/%08x examples=%u/%u intents=%u example_back=%u live_examples=%u last_example=%u overlay_destroy=%u ui=%u running=%u open=%u busy=%u depth=%u cleanup=%u stock=%u retained=%u global_owner_captures=%u global_screens=%u/%u global_refreshes=%u row_clicks=%u page_clicks=%u live_global=%u targets=%u page_start=%u selected=%u assigned_rows=%u owner=%08x vc=%08x handler=%08x global_revision=%u learn=%s intent=%u learn_requests=%u settled=%u cancels=%u timeouts=%u learn_pending=%u learn_owner=%u learn_mirror=%u learn_target=%u learn_waits=%u target_selections=%u pairing_polls=%u pairing_changes=%u row_repaints=%u file_actions=%u completed=%u import_no_additions=%u creates=%u copies=%u imports=%u exports=%u selections=%u clears=%u no_changes=%u file_action=%u file_count=%u file_index=%u edit_requests=%u edit_completed=%u edit_no_changes=%u edit=%u/%u/%u/%u/%u mapping=%.*s file_status=%.*s page=%.*s status_text=%.*s\n",
  copy.pid,copy.hook_entries,copy.tabs_created,copy.tabs_destroyed,copy.buttons_created,copy.buttons_destroyed,copy.clicks,copy.live_tabs,copy.failures,copy.last_failure,copy.last_overlay,copy.last_tab,copy.last_button,
  copy.buttons[0],copy.buttons[1],copy.buttons[2],copy.request_sequence,profile_name(copy.request_profile),copy.claimed_sequence,copy.completed_sequence,status_name(copy.status),profile_name(copy.saved_profile),profile_name(copy.active_profile),copy.result_detail,
  (int)copy.endpoint_client_length,(const char*)copy.endpoint_client,(int)copy.endpoint_port_length,(const char*)copy.endpoint_port,
  copy.launcher_overlays_created,copy.launcher_overlays_destroyed,copy.launcher_tiles_created,copy.launcher_tiles_destroyed,copy.launcher_clicks,
  copy.shells_created,copy.shells_destroyed,copy.back_clicks,copy.live_launcher_overlays,copy.live_launcher_tiles,copy.live_shells,
  copy.last_launcher_overlay,copy.last_launcher_page,copy.last_launcher_tile,
  copy.example_opens,copy.example_closes,copy.example_intents,copy.example_back_clicks,copy.live_examples,copy.last_example_intent,
  copy.overlay_destroy_entries,copy.overlay_destroy_ui_thread_matches,copy.overlay_destroy_observer_running,
  copy.overlay_destroy_consumer_open,copy.overlay_destroy_busy,copy.overlay_destroy_callback_depth,
  copy.overlay_destroy_cleanup_complete,copy.overlay_destroy_stock_calls,copy.overlay_destroy_retained,
  copy.global_owner_captures,copy.global_screens_created,copy.global_screens_destroyed,copy.global_refreshes,
  copy.global_row_clicks,copy.global_page_clicks,copy.live_global_screens,copy.global_target_count,
  copy.global_page_start,copy.global_selected_target,copy.global_assigned_rows,copy.last_global_owner,
  copy.last_global_vc,copy.last_global_file,copy.global_revision,
  learn_name(copy.global_learn_state),copy.global_learn_intent,copy.global_learn_requests,
  copy.global_learn_settled,copy.global_learn_cancels,copy.global_learn_timeouts,
  copy.global_learn_pending,copy.global_learn_owner_value,copy.global_learn_mirror_value,
  copy.global_learn_target,copy.global_learn_waits,copy.global_target_selections,
  copy.global_pairing_polls,copy.global_pairing_changes,copy.global_row_repaints,
  copy.global_file_actions,copy.global_file_completed,copy.global_file_import_no_additions,
  copy.global_file_creates,copy.global_file_copies,copy.global_file_imports,
  copy.global_file_exports,
  copy.global_file_selections,copy.global_file_clears,copy.global_file_no_changes,
  copy.global_file_action,copy.global_file_count,copy.global_file_index,
  copy.global_edit_requests,copy.global_edit_completed,copy.global_edit_no_changes,
  copy.global_edit_type,copy.global_edit_channel,copy.global_edit_data,copy.global_edit_control,copy.global_edit_reverse,
  NATIVE_GLOBAL_TEXT_MAX,(const char*)copy.global_mapping_text,
  NATIVE_GLOBAL_TEXT_MAX,(const char*)copy.global_file_status_text,
  NATIVE_GLOBAL_TEXT_MAX,(const char*)copy.global_page_text,NATIVE_GLOBAL_TEXT_MAX,(const char*)copy.global_status_text);
 printf("launcher_bind=%08x/%08x parent=%08x bounds=%d,%d,%d,%d launcher_layout=%08x/%08x parent=%08x bounds=%d,%d,%d,%d\n",
  copy.launcher_bind_overlay,copy.launcher_bind_page,copy.launcher_bind_parent,
  (int32_t)copy.launcher_bind_bounds[0],(int32_t)copy.launcher_bind_bounds[1],
  (int32_t)copy.launcher_bind_bounds[2],(int32_t)copy.launcher_bind_bounds[3],
  copy.launcher_layout_overlay,copy.launcher_layout_page,copy.launcher_layout_parent,
  (int32_t)copy.launcher_layout_bounds[0],(int32_t)copy.launcher_layout_bounds[1],
  (int32_t)copy.launcher_layout_bounds[2],(int32_t)copy.launcher_layout_bounds[3]);
 for(unsigned row=0;row<NATIVE_GLOBAL_ROWS;row++)
  printf("global_row%u=%.*s\n",row,NATIVE_GLOBAL_TEXT_MAX,(const char*)copy.global_rows[row]);
 return copy.failures?1:0;
}
