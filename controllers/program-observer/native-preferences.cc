/* Exact-firmware adapters for the canonical MPC host-widget toolkit. */
using uint64_t=__UINT64_TYPE__;
using uintptr_t=__UINTPTR_TYPE__;
using size_t=__SIZE_TYPE__;
using int32_t=__INT32_TYPE__;
#define NATIVE_PREFERENCES_FREESTANDING
#include "native-preferences.h"
#include "../../ui/include/mpclearn-ui.h"
#include "../../ui/private/mpclearn-ui-private.h"
#include "../../ui/private/mpc-3.9.1-profile.h"
#include "../../ui/screens/global-midi-learn-groups.h"
#include "../../ui/screens/global-midi-learn-screen.h"
#ifndef MPCLEARN_NATIVE_UI_EXAMPLE
#define MPCLEARN_NATIVE_UI_EXAMPLE 0
#endif
#if MPCLEARN_NATIVE_UI_EXAMPLE
#include "../../ui/examples/toolkit-example.h"
#endif

#define NATIVE_PREFERENCES_TAB_VTABLE_BYTES 32u
#define NATIVE_LAUNCHER_OVERLAY_VTABLE_BYTES 100u
extern "C" void *memcpy(void *,const void *,size_t) noexcept;
extern "C" int memcmp(const void *,const void *,size_t) noexcept;
extern "C" long syscall(long,...) noexcept;

extern "C" { extern uint32_t observer_running; }

namespace {

struct Context { uint64_t d[32];uint32_t apsr,fpscr,r[13],lr; };
static_assert(sizeof(Context)==320,"saved ARM context");

enum Failure : uint32_t {
 NP_BAD_CONTEXT=1,NP_BAD_VECTOR,NP_NO_SLOT,NP_ALLOCATION,NP_CONSTRUCTION,
 NP_PARENTING,NP_UNEXPECTED_EXCEPTION,NP_TEARDOWN,NP_LAUNCHER_CONTEXT,
 NP_LAUNCHER_VECTOR,NP_LAUNCHER_CONSTRUCTION,NP_LAUNCHER_TEARDOWN,
 NP_TOOLKIT_CONTEXT,NP_TOOLKIT_LIFETIME,NP_TOOLKIT_NAVIGATION,
 NP_GLOBAL_CONTEXT,NP_GLOBAL_READBACK,NP_GLOBAL_LEARN_INTENT,
 NP_GLOBAL_LEARN_SETTLEMENT,NP_GLOBAL_FILE_ACTION
};

enum LauncherIntent : uint32_t { LAUNCHER_OPEN_SHELL=1 };
enum LearnIntent : uint32_t { LEARN_INTENT_NONE=0,LEARN_INTENT_ARM=1,LEARN_INTENT_CANCEL=2 };
enum FileAction : uint32_t {
 FILE_ACTION_NONE=0,FILE_ACTION_NEW=1,FILE_ACTION_COPY=2,
 FILE_ACTION_LOAD=3,FILE_ACTION_EXPORT=4,FILE_ACTION_CLEAR=5,
 FILE_ACTION_SELECT=6,FILE_ACTION_APPLY=7
};

using HostAllocate=void *(*)(uint32_t);
using HostSizedDelete=void (*)(void *,uint32_t);
using HostSetVisible=void (*)(void *,int);
using HostRepaint=void (*)(void *);
using HostRemoveChild=void (*)(void *,void *);
using HostCompleteDestructor=void (*)(void *);
using StdStringCtor=void (*)(void *,const char *);
using NotImplementedCtor=void (*)(void *,uint32_t,void *,void *,float,float,float,float,float,float,void *,void *,void *);
using TabChild=void *(*)(void *,void *);
using TabActive=void (*)(void *,int);
using OverlayAdd=void *(*)(void *,void *);
struct GlobalTarget { int32_t id;uint32_t word4,word8; };
struct GlobalLookup { void *mapping;unsigned char found,pad[3]; };
struct HostString { const char *chars; };
struct GlobalSharedFile {void *object,*control;};
static_assert(sizeof(GlobalTarget)==12&&sizeof(GlobalLookup)==8&&sizeof(HostString)==4,
 "Global MIDI Learn host ABI objects");
using GlobalFileHandler=void *(*)(void *);
using GlobalTargetFixed=GlobalTarget *(*)(GlobalTarget *,int32_t);
using GlobalTargetDtor=GlobalTarget *(*)(GlobalTarget *);
using GlobalLookupPairing=GlobalLookup *(*)(GlobalLookup *,void *,const GlobalTarget *);
using GlobalFormatMapping=HostString *(*)(HostString *,const void *);
using GlobalTargetLabel=HostString *(*)(HostString *,uint32_t);
using GlobalStringDtor=void (*)(HostString *);
using GlobalSelectTarget=void (*)(void *,const GlobalTarget *);
using GlobalSetLearnIntent=void (*)(void *,bool);
using GlobalFileName=HostString *(*)(HostString *,const void *);
using GlobalFileAction=void (*)(void *);
using GlobalExportFile=bool (*)(void *,const void *);
using GlobalSelectFile=void (*)(void *,int32_t);
using GlobalClearTarget=void (*)(void *,const GlobalTarget *);
using GlobalMessageFactory=void (*)(void *,int32_t,int32_t,uint32_t);
using GlobalPitchFactory=void (*)(void *,int32_t,int32_t);
using GlobalMessageDtor=void (*)(void *);
using GlobalMappingCtor=void *(*)(void *,const void *);
using GlobalCommitMapping=void (*)(void *,const GlobalTarget *,const void *);
using GlobalSetControl=void (*)(void *,const GlobalTarget *,int32_t);
using GlobalSetReverse=void (*)(void *,const GlobalTarget *,bool);
using GlobalSignalDtor=void (*)(void *);
using GlobalFree=void (*)(void *);
using GlobalStringCtor=void (*)(HostString *,const char *);
using GlobalChooserOpen=void (*)(HostString *,void *,const HostString *,const HostString *,const HostString *,const HostString *,const HostString *,const HostString *,const HostString *,bool,bool,int32_t);
using GlobalFileCtor=void *(*)(void *,const HostString *);
using GlobalFileFactory=void (*)(void *);
using GlobalFileNameOnly=HostString *(*)(HostString *,const void *);
using GlobalStringUtf8=void (*)(void **,const HostString *);
using GlobalChildFile=void *(*)(void *,const void *,const char *);
using GlobalFilePredicate=bool (*)(const void *);
using GlobalCopyFile=bool (*)(const void *,const void *);
using GlobalOpenMapping=void (*)(GlobalSharedFile *,const void *);
using GlobalRegisterMapping=bool (*)(void *,const GlobalSharedFile *);
using GlobalSelectHandlerFile=void (*)(void *,int32_t);
using GlobalReleaseControl=void (*)(void *);

struct CoreHost { HostAllocate allocate;HostSizedDelete sized_delete;HostSetVisible set_visible;HostRepaint repaint; };
struct PreferencesHost {
 StdStringCtor std_string_ctor;NotImplementedCtor tab_ctor;HostCompleteDestructor tab_dtor;
 TabChild tab_remove;const unsigned char *tab_vtable;
};
struct LauncherHost {
 OverlayAdd overlay_add;HostRemoveChild remove_child;HostCompleteDestructor overlay_complete,overlay_deleting;
 const unsigned char *overlay_vtable;
};
struct GlobalHost {
 GlobalFileHandler file_handler;GlobalTargetFixed target_fixed;GlobalTargetDtor target_dtor;
 GlobalLookupPairing lookup;GlobalFormatMapping format_mapping;GlobalTargetLabel target_label;
 GlobalStringDtor string_dtor;GlobalSelectTarget select_target;GlobalSetLearnIntent set_learn_intent;
 GlobalFileName file_name;GlobalFileAction duplicate_file,create_file;GlobalExportFile export_file;
 GlobalSelectFile select_file;GlobalClearTarget clear_target;
 GlobalMessageFactory note_message,cc_message;GlobalPitchFactory pitch_message;GlobalMessageDtor message_dtor;
 GlobalMappingCtor mapping_ctor;GlobalCommitMapping commit_mapping;
 GlobalSetControl set_control;GlobalSetReverse set_reverse;
 GlobalSignalDtor signal_dtor;GlobalFree free_block;
 GlobalStringCtor string_ctor;GlobalFileCtor file_ctor;
 GlobalFileFactory user_midi_directory,chooser_initial_directory;GlobalFileNameOnly filename;GlobalStringUtf8 string_utf8;
 GlobalChildFile child_file;GlobalFilePredicate file_exists,file_exists_as_file;GlobalCopyFile copy_file;
 GlobalOpenMapping open_mapping;GlobalRegisterMapping register_mapping;
 GlobalSelectHandlerFile select_handler_file;GlobalReleaseControl release_control;GlobalChooserOpen chooser_open;
};

struct PreferencesInstance {
 void *overlay,*tab,*tab_vtable;TabActive original_active;
 MpcUiContext *context;MpcUiParent *parent;MpcUiScreen *screen;
 MpcUiControl *buttons[3],*marker;uint32_t completed_sequence;
};
struct GlobalMappingSnapshot {
 int32_t type,control,channel;unsigned char data1,reverse,valid,pad;
};
struct LauncherInstance {
 void *page,*overlay,*overlay_vtable;
 MpcUiContext *context;MpcUiParent *entry_parent,*navigation_parent;MpcUiAppHost *app_host;
 MpcUiScreen *entry_screen;MpclearnGlobalLearnScreen global_screen;
 MpcUiControl *launcher;
 uint32_t selected_group,page_start,selected_target;
 char page_text[NATIVE_GLOBAL_TEXT_MAX],mapping_text[NATIVE_GLOBAL_TEXT_MAX];
 char selected_text[NATIVE_GLOBAL_TEXT_MAX];
 char assignment_text[NATIVE_GLOBAL_TEXT_MAX],status_text[NATIVE_GLOBAL_TEXT_MAX];
 char file_status_text[NATIVE_GLOBAL_TEXT_MAX];
 char edit_type_text[24],edit_channel_text[24],edit_data_text[24],edit_mode_text[32],edit_reverse_text[24];
 char row_text[MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS][NATIVE_GLOBAL_TEXT_MAX];
 char row_label[MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS][28];
 char row_assignment[MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS][32];
 GlobalMappingSnapshot row_mapping[MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS];
 void *selected_file;uint32_t row_count,assigned_rows,poll_divider,poll_row;
 uint32_t focus_depth,resume_requested,learn_state,learn_intent,learn_waits;
 uint32_t file_action,file_count,file_index;
 void *native_common_files[8],*unsaved_files[8];uint32_t native_common_count,unsaved_count;
 GlobalMappingSnapshot draft_mapping;void *draft_file;uint32_t draft_target,draft_dirty;
#if MPCLEARN_NATIVE_UI_EXAMPLE
 MpclearnToolkitExampleApp example_app;uint32_t example_open_counted;
#endif
 uint32_t orphaned;
};
struct GlobalCapture { void *app,*owner,*view_controller;uint32_t ui_thread_id; };

static NativePreferencesState *state;
static CoreHost core;
static PreferencesHost preferences;
static LauncherHost launcher;
static GlobalHost global_host;
static GlobalCapture global_capture;
static uint32_t image_bias;
/* Production leaves this null. The ARM composition test supplies a fixture. */
static const MpcUiPrivateHost *toolkit_component_host;
static PreferencesInstance preferences_instances[4];
static LauncherInstance launcher_instances[4];

static void increment(uint32_t *word){if(state)__atomic_add_fetch(word,1u,__ATOMIC_RELAXED);}
static void decrement(uint32_t *word){if(state)__atomic_sub_fetch(word,1u,__ATOMIC_RELAXED);}
static void failure(Failure why){if(state){increment(&state->failures);__atomic_store_n(&state->last_failure,(uint32_t)why,__ATOMIC_RELAXED);}}
static uint32_t frame_word(const Context *c,unsigned offset){uint32_t value;memcpy(&value,(const unsigned char*)c+sizeof(*c)+offset,sizeof(value));return value;}
static void set_frame_word(Context *c,unsigned offset,uint32_t value){memcpy((unsigned char*)c+sizeof(*c)+offset,&value,sizeof(value));}
static float frame_float(const Context *c,unsigned offset){float value;memcpy(&value,(const unsigned char*)c+sizeof(*c)+offset,sizeof(value));return value;}
static PreferencesInstance *available_preferences_instance(){for(auto &instance:preferences_instances)if(!instance.tab)return &instance;return nullptr;}
static PreferencesInstance *preferences_overlay_instance(void *overlay){for(auto &instance:preferences_instances)if(instance.tab&&instance.overlay==overlay)return &instance;return nullptr;}
static PreferencesInstance *tab_instance(void *tab){for(auto &instance:preferences_instances)if(instance.tab==tab)return &instance;return nullptr;}
static LauncherInstance *available_launcher_instance(){for(auto &instance:launcher_instances)if(!instance.page&&!instance.orphaned)return &instance;return nullptr;}
static LauncherInstance *launcher_page_instance(void *page){for(auto &instance:launcher_instances)if(instance.page==page)return &instance;return nullptr;}
static LauncherInstance *launcher_overlay_instance(void *overlay){for(auto &instance:launcher_instances)if(instance.overlay==overlay)return &instance;return nullptr;}

static MpcUiContext *create_toolkit_context() {
 uint32_t thread=(uint32_t)syscall(224);
 if(toolkit_component_host)return mpc_ui_private_context_for_component(toolkit_component_host,thread);
 MpcUiAdmittedProfile admitted={sizeof(admitted),MPC_UI_PROFILE_MPC_3_9_1,image_bias,thread};
 return mpc_ui_private_context_from_admitted(&admitted);
}

static bool component_bounds_are(void *component,int x,int y,int width,int height) {
 if(!component)return false;int bounds[4]={};memcpy(bounds,(unsigned char*)component+0x10,sizeof(bounds));
 return bounds[0]==x&&bounds[1]==y&&bounds[2]==width&&bounds[3]==height;
}
static bool component_children(void *parent,uint32_t *children,int *count) {
 if(!parent||!children||!count)return false;
 uint32_t begin=(uint32_t)(uintptr_t)*(void**)((unsigned char*)parent+0x28);
 int capacity=*(int*)((unsigned char*)parent+0x2c),used=*(int*)((unsigned char*)parent+0x30);
 if(used<0||used>64||capacity<used||capacity>128||(!begin&&used)||(begin&3))return false;
 *children=begin;*count=used;return true;
}
static void *unique_component_child(void *parent,int x,int y,int width,int height,int child_count=-1) {
 uint32_t children=0;int count=0;if(!component_children(parent,&children,&count))return nullptr;void *found=nullptr;
 for(int n=0;n<count;n++){
  void *child=*(void**)(uintptr_t)(children+(uint32_t)n*4u);if(!child||*(void**)((unsigned char*)child+0x0c)!=parent)continue;
  if(!component_bounds_are(child,x,y,width,height))continue;
  if(child_count>=0){uint32_t ignored=0;int actual=0;if(!component_children(child,&ignored,&actual)||actual!=child_count)continue;}
  if(found)return nullptr;found=child;
 }
 return found;
}
static void *launcher_second_page(LauncherInstance &instance,int *launcher_bounds) {
 if(!instance.overlay||!instance.page)return nullptr;
 void *owner=*(void**)((unsigned char*)instance.overlay+0x134);if(!owner)return nullptr;
 void *overlay_parent=(unsigned char*)owner+8,*outer=(unsigned char*)instance.page+8;
 if(*(void**)((unsigned char*)outer+0x0c)!=overlay_parent||!component_bounds_are(outer,0,0,1280,690))return nullptr;
 void *grid=unique_component_child(outer,228,0,1052,690,5);if(!grid)return nullptr;
 void *viewport=unique_component_child(grid,74,0,904,690,1);if(!viewport)return nullptr;
 void *content=unique_component_child(viewport,0,0,1808,690,2);if(!content)return nullptr;
 void *first=unique_component_child(content,0,0,904,684,40);if(!first)return nullptr;
 void *second=unique_component_child(content,904,0,904,684,40);if(!second)return nullptr;
 if(!unique_component_child(second,678,136,226,136,0)||!unique_component_child(second,681,142,220,124,2))return nullptr;
 launcher_bounds[0]=681;launcher_bounds[1]=142;launcher_bounds[2]=220;launcher_bounds[3]=124;
 (void)first;return second;
}

static void record_launcher_phase(bool layout,void *overlay,void *page) {
 if(!state||!overlay||!page)return;uint32_t words[5]={};
 memcpy(words,(unsigned char*)page+0x14,sizeof(words));
 if(layout){
  state->launcher_layout_overlay=(uint32_t)(uintptr_t)overlay;
  state->launcher_layout_page=(uint32_t)(uintptr_t)page;
  state->launcher_layout_parent=words[0];
  memcpy(state->launcher_layout_bounds,words+1,sizeof(state->launcher_layout_bounds));
 }else{
  state->launcher_bind_overlay=(uint32_t)(uintptr_t)overlay;
  state->launcher_bind_page=(uint32_t)(uintptr_t)page;
  state->launcher_bind_parent=words[0];
  memcpy(state->launcher_bind_bounds,words+1,sizeof(state->launcher_bind_bounds));
 }
}

static void controller_intent(void *owner,uint32_t profile) noexcept {
 auto *instance=(PreferencesInstance*)owner;
 if(!state||!instance||!instance->tab||profile<NATIVE_CONTROLLER_XTOUCH||profile>NATIVE_CONTROLLER_GENERIC){failure(NP_BAD_CONTEXT);return;}
 increment(&state->clicks);
 __atomic_store_n(&state->request_profile,profile,__ATOMIC_RELAXED);
 __atomic_store_n(&state->status,NATIVE_PREFERENCES_PENDING,__ATOMIC_RELAXED);
 __atomic_add_fetch(&state->request_sequence,1u,__ATOMIC_RELEASE);
 (void)syscall(240,&state->request_sequence,1,0x7fffffff);
}
static void refresh_selection(PreferencesInstance *found) noexcept {
 if(!found||!found->screen)return;
 uint32_t status=state?__atomic_load_n(&state->status,__ATOMIC_ACQUIRE):NATIVE_PREFERENCES_IDLE;
 uint32_t saved=state?__atomic_load_n(&state->saved_profile,__ATOMIC_ACQUIRE):0;
 uint32_t selected=state?__atomic_load_n(&state->active_profile,__ATOMIC_ACQUIRE):0;
 if(status>NATIVE_PREFERENCES_RESTART_FAILED||saved>NATIVE_CONTROLLER_GENERIC||selected>NATIVE_CONTROLLER_GENERIC)selected=0;
 for(unsigned i=0;i<3;i++)if(found->buttons[i]&&!mpc_ui_button_selected(found->buttons[i],selected==i+1)){failure(NP_UNEXPECTED_EXCEPTION);return;}
 if(found->marker){
  MpcUiRect marker={340,36+(int)(selected?selected-1:0)*72,160,56};
  if(!mpc_ui_control_place(found->marker,marker)||!mpc_ui_control_visible(found->marker,selected!=0))failure(NP_UNEXPECTED_EXCEPTION);
 }
}

extern "C" void native_preferences_tab_active(void *tab,int active) noexcept {
 PreferencesInstance *found=tab_instance(tab);if(!found||!found->original_active){failure(NP_BAD_CONTEXT);return;}
 TabActive original=found->original_active;
 try {original(tab,active);}catch(...){failure(NP_UNEXPECTED_EXCEPTION);return;}
 if(active&&(found=tab_instance(tab)))refresh_selection(found);
}
static bool close_preferences_ui(PreferencesInstance &instance) noexcept {
 bool ok=true;
 if(instance.screen&&!mpc_ui_screen_close(&instance.screen))ok=false;
 if(instance.parent&&!mpc_ui_private_parent_destroy(&instance.parent))ok=false;
 if(instance.context&&!mpc_ui_private_context_destroy(&instance.context))ok=false;
 if(!ok)failure(NP_TOOLKIT_LIFETIME);
 return ok;
}
extern "C" void native_preferences_tab_deleting_destructor(void *tab) noexcept {
 PreferencesInstance *found=tab_instance(tab);if(!found){failure(NP_BAD_CONTEXT);return;}
 PreferencesInstance instance=*found;bool closed=close_preferences_ui(*found);
 if(closed&&state)__atomic_add_fetch(&state->buttons_destroyed,4u,__ATOMIC_RELAXED);
 *(void**)tab=(unsigned char*)preferences.tab_vtable+8;*found={};
 if(state){decrement(&state->live_tabs);increment(&state->tabs_destroyed);}
 try {preferences.tab_dtor(tab);}catch(...){failure(NP_TEARDOWN);}
 try {core.sized_delete(tab,0x74);}catch(...){failure(NP_TEARDOWN);}
 if(instance.tab_vtable)try {core.sized_delete(instance.tab_vtable,NATIVE_PREFERENCES_TAB_VTABLE_BYTES);}catch(...){failure(NP_TEARDOWN);}
}

static void configure_host(uint32_t bias) {
 image_bias=bias;
 core.allocate=(HostAllocate)(uintptr_t)(bias+MPC_UI_RVA_ALLOCATE);
 core.sized_delete=(HostSizedDelete)(uintptr_t)(bias+MPC_UI_RVA_SIZED_DELETE);
 core.set_visible=(HostSetVisible)(uintptr_t)(bias+MPC_UI_RVA_SET_VISIBLE);
 core.repaint=(HostRepaint)(uintptr_t)(bias+MPC_UI_RVA_REPAINT);
 preferences.std_string_ctor=(StdStringCtor)(uintptr_t)(bias+0x03665c60);
 preferences.tab_ctor=(NotImplementedCtor)(uintptr_t)(bias+0x0369d174);
 preferences.tab_dtor=(HostCompleteDestructor)(uintptr_t)(bias+0x0369cdec);
 preferences.tab_remove=(TabChild)(uintptr_t)(bias+0x036a047c);
 preferences.tab_vtable=(const unsigned char*)(uintptr_t)(bias+0x069cd0b4);
 launcher.overlay_add=(OverlayAdd)(uintptr_t)(bias+0x03664150);
 launcher.remove_child=(HostRemoveChild)(uintptr_t)(bias+MPC_UI_RVA_REMOVE_CHILD);
 launcher.overlay_complete=(HostCompleteDestructor)(uintptr_t)(bias+0x03663334);
 launcher.overlay_deleting=(HostCompleteDestructor)(uintptr_t)(bias+0x03663404);
 launcher.overlay_vtable=(const unsigned char*)(uintptr_t)(bias+0x069cabac);
 global_host.file_handler=(GlobalFileHandler)(uintptr_t)(bias+0x018b49f8);
 global_host.target_fixed=(GlobalTargetFixed)(uintptr_t)(bias+0x018bdd90);
 global_host.target_dtor=(GlobalTargetDtor)(uintptr_t)(bias+0x018bdc20);
 global_host.lookup=(GlobalLookupPairing)(uintptr_t)(bias+0x018b5840);
 global_host.format_mapping=(GlobalFormatMapping)(uintptr_t)(bias+0x018bed00);
 global_host.target_label=(GlobalTargetLabel)(uintptr_t)(bias+0x02fa6e84);
 global_host.string_dtor=(GlobalStringDtor)(uintptr_t)(bias+MPC_UI_RVA_STRING_DTOR);
 global_host.select_target=(GlobalSelectTarget)(uintptr_t)(bias+0x018737dc);
 global_host.set_learn_intent=(GlobalSetLearnIntent)(uintptr_t)(bias+0x02ad5e0c);
 global_host.file_name=(GlobalFileName)(uintptr_t)(bias+0x018a7da8);
 global_host.duplicate_file=(GlobalFileAction)(uintptr_t)(bias+0x02ad5f88);
 global_host.create_file=(GlobalFileAction)(uintptr_t)(bias+0x02ad7adc);
 global_host.export_file=(GlobalExportFile)(uintptr_t)(bias+0x018b2068);
 global_host.select_file=(GlobalSelectFile)(uintptr_t)(bias+0x02ad71e0);
 global_host.clear_target=(GlobalClearTarget)(uintptr_t)(bias+0x02ad60f0);
 global_host.note_message=(GlobalMessageFactory)(uintptr_t)(bias+0x007d6d8c);
 global_host.cc_message=(GlobalMessageFactory)(uintptr_t)(bias+0x007d6d4c);
 global_host.pitch_message=(GlobalPitchFactory)(uintptr_t)(bias+0x007d6c90);
 global_host.message_dtor=(GlobalMessageDtor)(uintptr_t)(bias+0x007d594c);
 global_host.mapping_ctor=(GlobalMappingCtor)(uintptr_t)(bias+0x018ba570);
 global_host.commit_mapping=(GlobalCommitMapping)(uintptr_t)(bias+0x018b7558);
 global_host.set_control=(GlobalSetControl)(uintptr_t)(bias+0x018b6220);
 global_host.set_reverse=(GlobalSetReverse)(uintptr_t)(bias+0x018b6360);
 global_host.signal_dtor=(GlobalSignalDtor)(uintptr_t)(bias+0x01efc91c);
 global_host.free_block=(GlobalFree)(uintptr_t)(bias+0x004a68f4);
 global_host.string_ctor=(GlobalStringCtor)(uintptr_t)(bias+MPC_UI_RVA_STRING_CTOR);
 global_host.file_ctor=(GlobalFileCtor)(uintptr_t)(bias+0x00986680);
 global_host.user_midi_directory=(GlobalFileFactory)(uintptr_t)(bias+0x015a7cf0);
 global_host.chooser_initial_directory=(GlobalFileFactory)(uintptr_t)(bias+0x015a8034);
 global_host.filename=(GlobalFileNameOnly)(uintptr_t)(bias+0x00956d44);
 global_host.string_utf8=(GlobalStringUtf8)(uintptr_t)(bias+0x0093352c);
 global_host.child_file=(GlobalChildFile)(uintptr_t)(bias+0x009866f8);
 global_host.file_exists=(GlobalFilePredicate)(uintptr_t)(bias+0x0094da2c);
 global_host.file_exists_as_file=(GlobalFilePredicate)(uintptr_t)(bias+0x0094da68);
 global_host.copy_file=(GlobalCopyFile)(uintptr_t)(bias+0x0096b708);
 global_host.open_mapping=(GlobalOpenMapping)(uintptr_t)(bias+0x018bed5c);
 global_host.register_mapping=(GlobalRegisterMapping)(uintptr_t)(bias+0x018ad794);
 global_host.select_handler_file=(GlobalSelectHandlerFile)(uintptr_t)(bias+0x018b3420);
 global_host.release_control=(GlobalReleaseControl)(uintptr_t)(bias+0x00c96a68);
 global_host.chooser_open=nullptr;
}

static int insert_tab(Context *c) {
 void *tab=nullptr,*new_vector=nullptr,*tab_table=nullptr;uint32_t new_vector_bytes=0;bool tab_constructed=false;
 MpcUiContext *context=nullptr;MpcUiParent *parent=nullptr;MpcUiScreen *screen=nullptr;
 alignas(8) unsigned char label[24]={};PreferencesInstance *instance=nullptr;
 static const char *const names[3]={"X-TOUCH","X-TOUCH MINI","GENERIC MCU"};
 try {
  uint32_t overlay=c->r[11];
  if(!overlay||frame_word(c,0x68)!=overlay+0x160){failure(NP_BAD_CONTEXT);return 0;}
  if(preferences_overlay_instance((void*)(uintptr_t)overlay))return 1;
  uint32_t *vector=(uint32_t*)(uintptr_t)(overlay+0x1b8);uint32_t begin=vector[0],end=vector[1],cap=vector[2];
  if((!begin&&(end||cap))||begin>end||end>cap||((begin|end|cap)&3)||(begin&&(end-begin)/4>64)){failure(NP_BAD_VECTOR);return 0;}
  instance=available_preferences_instance();if(!instance){failure(NP_NO_SLOT);return 0;}
  uint32_t count=begin?(end-begin)/4:0;
  if(!begin||cap-end<8){new_vector_bytes=(count+2)*4;new_vector=core.allocate(new_vector_bytes);if(!new_vector)throw NP_ALLOCATION;if(count)memcpy(new_vector,(void*)(uintptr_t)begin,count*4);}
  tab=core.allocate(0x74);if(!tab)throw NP_ALLOCATION;
  preferences.std_string_ctor(label,"CONTROLLERS");
  uint32_t scroll=*(uint32_t*)(uintptr_t)(overlay+0x1a4),stock_parent=*(uint32_t*)(uintptr_t)(overlay+0x1a8);
  if(!scroll||!stock_parent||stock_parent>0xffffffffu-8u)throw NP_BAD_CONTEXT;
  uint32_t index=frame_word(c,0x5c);
  preferences.tab_ctor(tab,index,(void*)(uintptr_t)scroll,(void*)(uintptr_t)(stock_parent+8),
   frame_float(c,0x198),frame_float(c,0x194),frame_float(c,0x1a0),frame_float(c,0x1a4),frame_float(c,0x19c),
   *(float*)(uintptr_t)(overlay+0x1c8),label,(void*)(uintptr_t)(overlay+0x160),(void*)(uintptr_t)frame_word(c,0x50));
  tab_constructed=true;
  void *content=*(void**)((unsigned char*)tab+0x4c),*placeholder=*(void**)((unsigned char*)tab+0x70);
  if(!content||!placeholder)throw NP_BAD_CONTEXT;
  preferences.tab_remove(content,placeholder);
  context=create_toolkit_context();if(!context)throw NP_TOOLKIT_CONTEXT;
  parent=mpc_ui_private_component_parent(context,content);if(!parent)throw NP_PARENTING;
  screen=mpc_ui_screen_create(context,parent,{0,0,1280,690});if(!screen)throw NP_CONSTRUCTION;
  instance->overlay=(void*)(uintptr_t)overlay;instance->tab=tab;instance->context=context;instance->parent=parent;instance->screen=screen;
  for(unsigned i=0;i<3;i++){
   instance->buttons[i]=mpc_ui_button_create(screen,names[i],i+1,controller_intent,instance);
   if(!instance->buttons[i]||!mpc_ui_button_style(instance->buttons[i],{0xff2f2f34u,0xffd31145u,0xffffffffu})||
      !mpc_ui_control_place(instance->buttons[i],{36,36+(int)i*72,280,56}))throw NP_CONSTRUCTION;
  }
  instance->marker=mpc_ui_button_create(screen,"SELECTED",0,nullptr,nullptr);
  if(!instance->marker||!mpc_ui_button_style(instance->marker,{0xffd31145u,0xffd31145u,0xffffffffu})||
     !mpc_ui_control_place(instance->marker,{340,36,160,56})||!mpc_ui_control_visible(instance->marker,0)||
     !mpc_ui_screen_show(screen))throw NP_CONSTRUCTION;
  tab_table=core.allocate(NATIVE_PREFERENCES_TAB_VTABLE_BYTES);if(!tab_table)throw NP_ALLOCATION;
  memcpy(tab_table,preferences.tab_vtable,NATIVE_PREFERENCES_TAB_VTABLE_BYTES);
  instance->original_active=((TabActive*)tab_table)[4];
  ((void**)tab_table)[4]=(void*)(uintptr_t)&native_preferences_tab_active;
  ((void**)tab_table)[3]=(void*)(uintptr_t)&native_preferences_tab_deleting_destructor;
  *(void**)tab=(unsigned char*)tab_table+8;instance->tab_vtable=tab_table;
  instance->completed_sequence=state?__atomic_load_n(&state->completed_sequence,__ATOMIC_ACQUIRE):0;
  refresh_selection(instance);
  if(new_vector){
   uint32_t *next=(uint32_t*)new_vector;next[count]=(uint32_t)(uintptr_t)tab;
   vector[0]=(uint32_t)(uintptr_t)next;vector[1]=(uint32_t)(uintptr_t)(next+count+1);vector[2]=(uint32_t)(uintptr_t)(next+count+2);
   if(begin)core.sized_delete((void*)(uintptr_t)begin,cap-begin);
  }else{*(uint32_t*)(uintptr_t)end=(uint32_t)(uintptr_t)tab;vector[1]=end+4;}
  set_frame_word(c,0x5c,index+1);
  if(state){
   increment(&state->tabs_created);__atomic_add_fetch(&state->buttons_created,4u,__ATOMIC_RELAXED);increment(&state->live_tabs);
   state->last_overlay=overlay;state->last_tab=(uint32_t)(uintptr_t)tab;
   for(unsigned i=0;i<3;i++)state->buttons[i]=(uint32_t)(uintptr_t)mpc_ui_private_control_host_object(instance->buttons[i]);
   state->last_button=state->buttons[0];
  }
  return 1;
 }catch(Failure why){failure(why);}catch(...){failure(NP_UNEXPECTED_EXCEPTION);}
 if(instance){(void)close_preferences_ui(*instance);*instance={};}
 else {
  if(screen)(void)mpc_ui_screen_close(&screen);
  if(parent)(void)mpc_ui_private_parent_destroy(&parent);
  if(context)(void)mpc_ui_private_context_destroy(&context);
 }
 if(tab_constructed)try {preferences.tab_dtor(tab);}catch(...){failure(NP_TEARDOWN);}
 if(tab)try {core.sized_delete(tab,0x74);}catch(...){failure(NP_TEARDOWN);}
 if(tab_table)try {core.sized_delete(tab_table,NATIVE_PREFERENCES_TAB_VTABLE_BYTES);}catch(...){failure(NP_TEARDOWN);}
 if(new_vector)try {core.sized_delete(new_vector,new_vector_bytes);}catch(...){failure(NP_TEARDOWN);}
 return 0;
}

extern "C" void native_launcher_overlay_complete_destructor(void *overlay) noexcept;
extern "C" void native_launcher_overlay_deleting_destructor(void *overlay) noexcept;

static void *launcher_owner(LauncherInstance *instance) {
 if(!instance||!instance->overlay)return nullptr;void *owner=*(void**)((unsigned char*)instance->overlay+0x134);
 return owner?(unsigned char*)owner+8:nullptr;
}
static int launcher_attach(void *owner,void *child) {
 auto *instance=(LauncherInstance*)owner;void *parent=launcher_owner(instance);if(!parent||!child)return 0;
 launcher.overlay_add(instance->overlay,child);
 return *(void**)((unsigned char*)child+0x0c)==parent;
}
static int launcher_detach(void *owner,void *child) {
 auto *instance=(LauncherInstance*)owner;void *parent=launcher_owner(instance);if(!parent||!child)return 0;
 launcher.remove_child(parent,child);
 return *(void**)((unsigned char*)child+0x0c)==nullptr;
}
static int launcher_focus(void *owner,int active) {
 auto *instance=(LauncherInstance*)owner;if(!instance||!instance->page)return 0;
 if(active){if(instance->focus_depth==0xffffffffu)return 0;instance->focus_depth++;}
 else if(instance->focus_depth)instance->focus_depth--;
 void *outer=(unsigned char*)instance->page+8;
 try {core.set_visible(outer,instance->focus_depth?0:1);if(!instance->focus_depth)core.repaint(outer);return 1;}
 catch(...){return 0;}
}

static void text_clear(char *out,size_t bytes){if(out&&bytes)out[0]=0;}
static void text_append(char *out,size_t bytes,const char *text) {
 if(!out||!bytes||!text)return;size_t used=0;while(used+1<bytes&&out[used])used++;
 for(size_t n=0;used+1<bytes&&text[n];n++)out[used++]=text[n];out[used]=0;
}
static void text_append_uint(char *out,size_t bytes,uint32_t value) {
 char reversed[10];unsigned count=0;do{reversed[count++]=(char)('0'+value%10u);value/=10u;}while(value&&count<sizeof(reversed));
 while(count){char digit[2]={reversed[--count],0};text_append(out,bytes,digit);}
}
static void text_append_int(char *out,size_t bytes,int32_t value) {
 if(value<0){text_append(out,bytes,"-");uint32_t magnitude=(uint32_t)(-(value+1))+1u;text_append_uint(out,bytes,magnitude);}
 else text_append_uint(out,bytes,(uint32_t)value);
}
static void text_append_target(char *out,size_t bytes,uint32_t target) {
 uint32_t shown=target+1u;if(shown<100u)text_append(out,bytes,"0");if(shown<10u)text_append(out,bytes,"0");text_append_uint(out,bytes,shown);
}
static bool host_text(char *out,size_t bytes,const HostString &text) {
 if(!out||bytes<2||!text.chars)return false;text_clear(out,bytes);
 for(size_t n=0;n+1<bytes;n++){char value=text.chars[n];out[n]=value;if(!value)return true;out[n+1]=0;}
 return true;
}
static bool global_ready() {
 return global_capture.owner&&global_capture.view_controller&&global_capture.ui_thread_id==(uint32_t)syscall(224)&&
  global_host.file_handler&&global_host.target_fixed&&global_host.target_dtor&&global_host.lookup&&
  global_host.format_mapping&&global_host.target_label&&global_host.string_dtor&&global_host.select_target&&
  global_host.set_learn_intent&&global_host.file_name&&global_host.duplicate_file&&global_host.create_file&&
  global_host.export_file&&global_host.select_file&&global_host.clear_target&&
  global_host.note_message&&global_host.cc_message&&global_host.pitch_message&&global_host.message_dtor&&
  global_host.mapping_ctor&&global_host.commit_mapping&&global_host.set_control&&global_host.set_reverse&&
  global_host.signal_dtor&&global_host.free_block&&global_host.string_ctor&&global_host.file_ctor&&
  global_host.user_midi_directory&&global_host.chooser_initial_directory&&global_host.filename&&global_host.string_utf8&&global_host.child_file&&
  global_host.file_exists&&global_host.file_exists_as_file&&global_host.copy_file&&global_host.open_mapping&&
  global_host.register_mapping&&global_host.select_handler_file&&global_host.release_control;
}
struct GlobalFileCatalog {void *handler,*selected;uint32_t count,index;};
static void *global_file_handler() {
 return global_host.file_handler((unsigned char*)global_capture.owner+0x19c);
}
static void *global_selected_file(void *handler) {
 return handler?*(void**)((unsigned char*)handler+0x70):nullptr;
}
static bool snapshot_global_files(GlobalFileCatalog &catalog) noexcept {
 catalog={};
 try {
  void *handler=global_file_handler();if(!handler)return false;
  uint32_t begin=*(uint32_t*)((unsigned char*)handler+0xa4),end=*(uint32_t*)((unsigned char*)handler+0xa8);
  if((!begin&&end)||begin>end||((begin|end)&3)||((end-begin)&7)||(end-begin)/8u>512u)return false;
  void *selected=global_selected_file(handler);uint32_t count=(end-begin)/8u,index=count;
  for(uint32_t row=0;row<count;row++){
   void *file=*(void**)(uintptr_t)(begin+row*8u);if(!file)return false;
   if(file==selected)index=row;
  }
  catalog={handler,selected,count,index};return selected&&index<count;
 }catch(...){return false;}
}
static bool read_global_file_name(void *file,char *out,size_t bytes) noexcept {
 HostString name={};bool name_live=false;
 try {
  if(!file||*(void**)file!=(void*)(uintptr_t)(image_bias+0x0689c878u))throw NP_GLOBAL_CONTEXT;
  if(global_host.file_name(&name,file)!=&name)throw NP_GLOBAL_READBACK;name_live=true;
  text_clear(out,bytes);text_append(out,bytes,"MAPPING: ");
  char value[48]={};if(!host_text(value,sizeof(value),name))throw NP_GLOBAL_READBACK;
  text_append(out,bytes,value);name_live=false;global_host.string_dtor(&name);return true;
 }catch(...){if(name_live)try {global_host.string_dtor(&name);}catch(...){}failure(NP_GLOBAL_READBACK);return false;}
}
static const char *mapping_mode_name(int32_t mode);
static const char *mapping_assignment_mode_name(int32_t mode) {
 static const char *const names[]={"","TOGGLE","MOMENTARY","FIXED","NOTE","ABS","REL CC OFFSET","REL CC 2S COMPLEMENT"};
 return mode>=1&&mode<=7?names[mode]:"UNKNOWN MODE";
}
static void global_assignment_text(char *out,size_t bytes,int32_t type,int32_t control,int32_t channel,
 unsigned char data1,unsigned char reverse,const char *stock_text) {
 text_clear(out,bytes);text_append(out,bytes,"CH");text_append_int(out,bytes,channel);text_append(out,bytes," ");
 if(type==1){
  text_append(out,bytes,"NOTE");
  if(stock_text&&stock_text[0])text_append(out,bytes,stock_text);else text_append_uint(out,bytes,data1);
 }else if(type==2){
  text_append(out,bytes,"CC");
  if(stock_text&&stock_text[0])text_append(out,bytes,stock_text);else text_append_uint(out,bytes,data1);
 }else if(type==3)text_append(out,bytes,"PITCH BEND");
 else {
  text_append(out,bytes,"TYPE");text_append_int(out,bytes,type);text_append(out,bytes," DATA");
  if(stock_text&&stock_text[0])text_append(out,bytes,stock_text);else text_append_uint(out,bytes,data1);
 }
 text_append(out,bytes," ");text_append(out,bytes,mapping_assignment_mode_name(control));
 if(reverse)text_append(out,bytes," REV");
}
static void compose_global_target(LauncherInstance &instance,unsigned row,uint32_t target,const char *assignment) {
 char *output=instance.row_text[row];text_clear(output,NATIVE_GLOBAL_TEXT_MAX);text_append_target(output,NATIVE_GLOBAL_TEXT_MAX,target);
 text_append(output,NATIVE_GLOBAL_TEXT_MAX," ");text_append(output,NATIVE_GLOBAL_TEXT_MAX,instance.row_label[row][0]?instance.row_label[row]:"[NO LABEL]");
 text_clear(instance.row_assignment[row],sizeof(instance.row_assignment[row]));
 text_append(instance.row_assignment[row],sizeof(instance.row_assignment[row]),instance.row_mapping[row].valid?assignment:"UNASSIGNED");
}
static const char *mapping_type_name(int32_t type) {
 return type==1?"NOTE":type==2?"CC":type==3?"PITCH BEND":"UNKNOWN";
}
static const char *mapping_mode_name(int32_t mode) {
 static const char *const names[]={"","TOGGLE","MOMENTARY","FIXED","NOTE","ABS CC","REL CC OFFSET","REL CC 2S COMPLEMENT"};
 return mode>=1&&mode<=7?names[mode]:"UNKNOWN";
}
static void compose_global_draft(LauncherInstance &instance) {
 text_clear(instance.edit_type_text,sizeof(instance.edit_type_text));text_append(instance.edit_type_text,sizeof(instance.edit_type_text),"TYPE: ");text_append(instance.edit_type_text,sizeof(instance.edit_type_text),mapping_type_name(instance.draft_mapping.type));
 text_clear(instance.edit_channel_text,sizeof(instance.edit_channel_text));text_append(instance.edit_channel_text,sizeof(instance.edit_channel_text),"CHANNEL: ");text_append_int(instance.edit_channel_text,sizeof(instance.edit_channel_text),instance.draft_mapping.channel);
 text_clear(instance.edit_data_text,sizeof(instance.edit_data_text));text_append(instance.edit_data_text,sizeof(instance.edit_data_text),"NUMBER: ");text_append_uint(instance.edit_data_text,sizeof(instance.edit_data_text),instance.draft_mapping.data1);
 text_clear(instance.edit_mode_text,sizeof(instance.edit_mode_text));text_append(instance.edit_mode_text,sizeof(instance.edit_mode_text),"MODE: ");text_append(instance.edit_mode_text,sizeof(instance.edit_mode_text),mapping_mode_name(instance.draft_mapping.control));
 text_clear(instance.edit_reverse_text,sizeof(instance.edit_reverse_text));text_append(instance.edit_reverse_text,sizeof(instance.edit_reverse_text),instance.draft_mapping.reverse?"REVERSE: ON":"REVERSE: OFF");
}
static bool read_global_mapping(uint32_t target,const GlobalMappingSnapshot *before,GlobalMappingSnapshot &mapping,
 char *assignment,size_t assignment_bytes,bool *changed) noexcept {
 GlobalTarget native_target={};GlobalLookup lookup={};HostString mapping_text={};
 bool target_live=false,mapping_text_live=false;mapping={};if(assignment&&assignment_bytes)text_clear(assignment,assignment_bytes);
 try {
  if(global_host.target_fixed(&native_target,(int32_t)target)!=&native_target)throw NP_GLOBAL_READBACK;target_live=true;
  if(global_host.lookup(&lookup,(unsigned char*)global_capture.owner+0x19c,&native_target)!=&lookup)throw NP_GLOBAL_READBACK;
  if(lookup.found&&lookup.mapping){
   memcpy(&mapping.type,(unsigned char*)lookup.mapping+0x9c,4);memcpy(&mapping.control,(unsigned char*)lookup.mapping+0xa0,4);
   memcpy(&mapping.channel,(unsigned char*)lookup.mapping+0xa4,4);memcpy(&mapping.data1,(unsigned char*)lookup.mapping+0xa8,1);
   memcpy(&mapping.reverse,(unsigned char*)lookup.mapping+0xa9,1);mapping.valid=mapping.type!=0&&mapping.channel!=-1;
  }
  bool differs=!before||memcmp(before,&mapping,sizeof(mapping));if(changed)*changed=differs;
  if(differs&&mapping.valid&&assignment&&assignment_bytes){
   if(!lookup.mapping)throw NP_GLOBAL_READBACK;
    if(global_host.format_mapping(&mapping_text,lookup.mapping)!=&mapping_text)throw NP_GLOBAL_READBACK;mapping_text_live=true;
    char stock_text[12]={};if(!host_text(stock_text,sizeof(stock_text),mapping_text))throw NP_GLOBAL_READBACK;
   global_assignment_text(assignment,assignment_bytes,mapping.type,mapping.control,mapping.channel,mapping.data1,mapping.reverse,stock_text);
  }
  if(mapping_text_live){mapping_text_live=false;global_host.string_dtor(&mapping_text);}
  if(target_live){target_live=false;global_host.target_dtor(&native_target);}
 }catch(...){
  if(mapping_text_live)try {mapping_text_live=false;global_host.string_dtor(&mapping_text);}catch(...){}
  if(target_live)try {target_live=false;global_host.target_dtor(&native_target);}catch(...){}
  failure(NP_GLOBAL_READBACK);return false;
 }
 return true;
}
static bool build_global_row(LauncherInstance &instance,unsigned row,uint32_t target) noexcept {
 HostString label={};bool label_live=false;char assignment[32]={};GlobalMappingSnapshot mapping={};
 try {
  if(global_host.target_label(&label,target)!=&label)throw NP_GLOBAL_READBACK;label_live=true;
  if(!host_text(instance.row_label[row],sizeof(instance.row_label[row]),label))throw NP_GLOBAL_READBACK;
  label_live=false;global_host.string_dtor(&label);
  if(!read_global_mapping(target,nullptr,mapping,assignment,sizeof(assignment),nullptr))return false;
  instance.row_mapping[row]=mapping;compose_global_target(instance,row,target,assignment);return true;
 }catch(...){if(label_live)try {global_host.string_dtor(&label);}catch(...){}failure(NP_GLOBAL_READBACK);return false;}
}

struct GlobalLearnObservation {uint32_t pending,owner_value,mirror_value;};
static constexpr uint32_t GLOBAL_LEARN_WAIT_LIMIT=128u;

static bool global_context_live() {
 if(!global_ready()||*(void**)((unsigned char*)global_capture.app+0x3c0)!=global_capture.owner||
    *(void**)((unsigned char*)global_capture.app+0x7cc)!=global_capture.view_controller||
    *(void**)((unsigned char*)global_capture.view_controller+0x138)!=global_capture.owner)return false;
 try {return *(void**)((unsigned char*)global_capture.view_controller+0x13c)==global_file_handler();}
 catch(...){return false;}
}
static bool observe_global_learn(GlobalLearnObservation &observed) noexcept {
 if(!global_context_live()){failure(NP_GLOBAL_CONTEXT);return false;}
 observed.pending=__atomic_load_n((uint32_t*)((unsigned char*)global_capture.view_controller+0x10),__ATOMIC_ACQUIRE);
 observed.owner_value=__atomic_load_n((unsigned char*)global_capture.owner+0x34,__ATOMIC_ACQUIRE)!=0;
 observed.mirror_value=__atomic_load_n((unsigned char*)global_capture.view_controller+0x23,__ATOMIC_ACQUIRE)!=0;
 return true;
}
static bool request_global_learn(LauncherInstance &instance,bool enabled) noexcept {
 if(!global_context_live()){failure(NP_GLOBAL_CONTEXT);return false;}
 try {global_host.set_learn_intent(global_capture.view_controller,enabled);}
 catch(...){failure(NP_GLOBAL_LEARN_INTENT);return false;}
 instance.learn_intent=enabled?LEARN_INTENT_ARM:LEARN_INTENT_CANCEL;
 instance.learn_state=enabled?NATIVE_GLOBAL_LEARN_ARMING:NATIVE_GLOBAL_LEARN_CANCELING;
 instance.learn_waits=0;
 if(state){increment(&state->global_learn_requests);if(!enabled)increment(&state->global_learn_cancels);}
 return true;
}
static bool settle_global_learn(LauncherInstance &instance,GlobalLearnObservation &observed,bool *changed=nullptr) noexcept {
 if(!observe_global_learn(observed))return false;
 uint32_t old_state=instance.learn_state,old_intent=instance.learn_intent;
 if(instance.learn_intent!=LEARN_INTENT_NONE){
  uint32_t desired=instance.learn_intent==LEARN_INTENT_ARM;
  if(!observed.pending&&observed.owner_value==desired&&observed.mirror_value==desired){
   instance.learn_state=desired?NATIVE_GLOBAL_LEARN_ARMED:NATIVE_GLOBAL_LEARN_IDLE;
   instance.learn_intent=LEARN_INTENT_NONE;instance.learn_waits=0;
   if(state)increment(&state->global_learn_settled);
  }else if(++instance.learn_waits>=GLOBAL_LEARN_WAIT_LIMIT){
   instance.learn_state=NATIVE_GLOBAL_LEARN_UNSETTLED;instance.learn_intent=LEARN_INTENT_NONE;
   if(state)increment(&state->global_learn_timeouts);failure(NP_GLOBAL_LEARN_SETTLEMENT);
  }
 }else if(!observed.pending&&observed.owner_value==observed.mirror_value){
  instance.learn_state=observed.owner_value?NATIVE_GLOBAL_LEARN_ARMED:NATIVE_GLOBAL_LEARN_IDLE;
  instance.learn_waits=0;
 }else if(instance.learn_state==NATIVE_GLOBAL_LEARN_IDLE){
  instance.learn_state=NATIVE_GLOBAL_LEARN_UNSETTLED;
 }
 if(changed)*changed=old_state!=instance.learn_state||old_intent!=instance.learn_intent;
 return true;
}
static bool select_global_target(LauncherInstance &instance,uint32_t target) noexcept {
 if(target>=120u||instance.learn_state!=NATIVE_GLOBAL_LEARN_IDLE||instance.learn_intent!=LEARN_INTENT_NONE)return false;
 GlobalTarget native_target={};bool target_live=false;
 try {
  if(!global_context_live()||global_host.target_fixed(&native_target,(int32_t)target)!=&native_target)throw NP_GLOBAL_CONTEXT;
  target_live=true;global_host.select_target((unsigned char*)global_capture.owner+0x38,&native_target);
  target_live=false;global_host.target_dtor(&native_target);instance.selected_target=target;
  if(state)increment(&state->global_target_selections);return true;
 }catch(Failure why){if(target_live)try {global_host.target_dtor(&native_target);}catch(...){}failure(why);}
 catch(...){if(target_live)try {global_host.target_dtor(&native_target);}catch(...){}failure(NP_GLOBAL_LEARN_INTENT);}
 return false;
}
static void set_file_status(LauncherInstance &instance,const char *text) {
 text_clear(instance.file_status_text,sizeof(instance.file_status_text));
 text_append(instance.file_status_text,sizeof(instance.file_status_text),text);
}
static bool refresh_global_screen(LauncherInstance &,bool,bool) noexcept;
static void record_global_file_action(uint32_t action) {
 if(!state)return;increment(&state->global_file_actions);state->global_file_action=action;
}
static void record_global_file_result(uint32_t action,bool changed,bool import_no_addition=false) {
 if(!state)return;increment(&state->global_file_completed);
 if(action==FILE_ACTION_NEW)increment(&state->global_file_creates);
 else if(action==FILE_ACTION_COPY)increment(&state->global_file_copies);
 else if(action==FILE_ACTION_LOAD)increment(&state->global_file_imports);
 else if(action==FILE_ACTION_EXPORT)increment(&state->global_file_exports);
 else if(action==FILE_ACTION_CLEAR)increment(&state->global_file_clears);
 else if(action==FILE_ACTION_SELECT)increment(&state->global_file_selections);
 else if(action==FILE_ACTION_APPLY)increment(&state->global_edit_completed);
 if(!changed)increment(&state->global_file_no_changes);if(import_no_addition)increment(&state->global_file_import_no_additions);
 state->global_file_action=FILE_ACTION_NONE;
}
static bool listed_file(void *const *files,uint32_t count,void *file) {
 for(uint32_t index=0;index<count;index++)if(files[index]==file)return true;return false;
}
static void remember_file(void **files,uint32_t &count,void *file) {
 if(!file||listed_file(files,count,file)||count>=8u)return;files[count++]=file;
}
enum ImportOutcome : uint32_t {IMPORT_FAILED=0,IMPORT_NO_ADDITION=1,IMPORT_ADDED=2,IMPORT_COLLISION=3};
enum ExportOutcome : uint32_t {EXPORT_FAILED=0,EXPORT_CANCELED=1,EXPORT_WRITTEN=2};
static ImportOutcome import_global_file(LauncherInstance &instance,void *handler) noexcept {
 HostString title={},empty={},extension={},filter={},description={},result={},basename={};
 alignas(4) unsigned char source[4]={},directory[4]={},destination[4]={};GlobalSharedFile opened={};
 bool title_live=false,empty_live=false,extension_live=false,filter_live=false,description_live=false;
 bool result_live=false,source_live=false,directory_live=false,destination_live=false,basename_live=false;
 ImportOutcome outcome=IMPORT_FAILED;bool registered=false;
 try {
  void *chooser=*(void**)((unsigned char*)global_capture.view_controller+0x130);
  if(!chooser)throw NP_GLOBAL_CONTEXT;
  auto open=global_host.chooser_open;
  if(!open){
   if(*(void**)chooser!=(void*)(uintptr_t)(image_bias+0x0689c134u))throw NP_GLOBAL_CONTEXT;
   open=(GlobalChooserOpen)(*(void***)chooser)[2];
   if(open!=(GlobalChooserOpen)(uintptr_t)(image_bias+0x01801cd4u))throw NP_GLOBAL_CONTEXT;
  }
  global_host.string_ctor(&title,"Import MIDI Mapping");title_live=true;
  global_host.string_ctor(&empty,"");empty_live=true;
  global_host.string_ctor(&extension,".xmm");extension_live=true;
  global_host.string_ctor(&filter,"*.xmm");filter_live=true;
  global_host.string_ctor(&description,"MIDI Mapping File");description_live=true;
  open(&result,chooser,&title,&empty,&empty,&extension,&filter,&empty,&description,false,true,2);result_live=true;
  if(!result.chars||!result.chars[0]){outcome=IMPORT_NO_ADDITION;throw outcome;}
  if(global_host.file_ctor(source,&result)!=source)throw NP_GLOBAL_FILE_ACTION;source_live=true;
  if(!global_host.file_exists_as_file(source))throw NP_GLOBAL_FILE_ACTION;
  global_host.user_midi_directory(directory);directory_live=true;
  if(global_host.filename(&basename,source)!=&basename)throw NP_GLOBAL_FILE_ACTION;basename_live=true;
  void *utf8=nullptr;global_host.string_utf8(&utf8,&basename);
  if(!utf8||!*(const char*)utf8||global_host.child_file(destination,directory,(const char*)utf8)!=destination)
   throw NP_GLOBAL_FILE_ACTION;
  destination_live=true;
  if(global_host.file_exists(destination)){outcome=IMPORT_COLLISION;throw outcome;}
  if(!global_host.copy_file(source,destination))throw NP_GLOBAL_FILE_ACTION;
  global_host.open_mapping(&opened,destination);
  if(!opened.object||!opened.control||!global_host.register_mapping(handler,&opened))throw NP_GLOBAL_FILE_ACTION;
  registered=true;
  uint32_t begin=*(uint32_t*)((unsigned char*)handler+0xa4),end=*(uint32_t*)((unsigned char*)handler+0xa8);
  if(!begin||begin>end||((begin|end)&3)||((end-begin)&7)||(end-begin)/8u>512u)throw NP_GLOBAL_FILE_ACTION;
  uint32_t count=(end-begin)/8u,index=count;
  for(uint32_t row=0;row<count;row++)if(*(void**)(uintptr_t)(begin+row*8u)==opened.object){if(index!=count)throw NP_GLOBAL_FILE_ACTION;index=row;}
  if(index==count)throw NP_GLOBAL_FILE_ACTION;
  global_host.select_handler_file(handler,(int32_t)index);
  if(global_selected_file(handler)!=opened.object)throw NP_GLOBAL_FILE_ACTION;
  outcome=IMPORT_ADDED;
 }catch(ImportOutcome expected){outcome=expected;}
 catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(opened.control)try {global_host.release_control(opened.control);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(basename_live)try {global_host.string_dtor(&basename);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(destination_live)try {global_host.string_dtor((HostString*)destination);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(directory_live)try {global_host.string_dtor((HostString*)directory);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(source_live)try {global_host.string_dtor((HostString*)source);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(result_live)try {global_host.string_dtor(&result);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(description_live)try {global_host.string_dtor(&description);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(filter_live)try {global_host.string_dtor(&filter);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(extension_live)try {global_host.string_dtor(&extension);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(empty_live)try {global_host.string_dtor(&empty);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(title_live)try {global_host.string_dtor(&title);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=IMPORT_FAILED;}
 if(registered&&outcome!=IMPORT_ADDED){
  set_file_status(instance,"IMPORT REGISTERED BUT UNSETTLED");failure(NP_GLOBAL_FILE_ACTION);
 }
 return outcome;
}
static ExportOutcome export_global_file(void *handler,void *selected) noexcept {
 HostString title={},empty={},extension={},filter={},description={},initial_name={},result={};
 alignas(4) unsigned char initial_directory[4]={},destination[4]={};
 bool title_live=false,empty_live=false,extension_live=false,filter_live=false,description_live=false;
 bool initial_name_live=false,result_live=false,initial_directory_live=false,destination_live=false;
 ExportOutcome outcome=EXPORT_FAILED;
 try {
  if(!handler||!selected||global_selected_file(handler)!=selected||
     *(void**)selected!=(void*)(uintptr_t)(image_bias+0x0689c878u))throw NP_GLOBAL_CONTEXT;
  void *chooser=*(void**)((unsigned char*)global_capture.view_controller+0x130);
  if(!chooser)throw NP_GLOBAL_CONTEXT;
  auto open=global_host.chooser_open;
  if(!open){
   if(*(void**)chooser!=(void*)(uintptr_t)(image_bias+0x0689c134u))throw NP_GLOBAL_CONTEXT;
   open=(GlobalChooserOpen)(*(void***)chooser)[2];
   if(open!=(GlobalChooserOpen)(uintptr_t)(image_bias+0x01801cd4u))throw NP_GLOBAL_CONTEXT;
  }
  global_host.string_ctor(&title,"Export MIDI Mapping");title_live=true;
  global_host.string_ctor(&empty,"");empty_live=true;
  global_host.string_ctor(&extension,".xmm");extension_live=true;
  global_host.string_ctor(&filter,"*.xmm");filter_live=true;
  global_host.string_ctor(&description,"MIDI Mapping File");description_live=true;
  global_host.chooser_initial_directory(initial_directory);initial_directory_live=true;
  if(global_host.file_name(&initial_name,selected)!=&initial_name)throw NP_GLOBAL_FILE_ACTION;
  initial_name_live=true;
  if(!initial_name.chars||!initial_name.chars[0])throw NP_GLOBAL_FILE_ACTION;
  open(&result,chooser,&title,(HostString*)initial_directory,&initial_name,&extension,&filter,&empty,&description,true,false,2);result_live=true;
  if(!result.chars||!result.chars[0]){outcome=EXPORT_CANCELED;throw outcome;}
  if(global_host.file_ctor(destination,&result)!=destination)throw NP_GLOBAL_FILE_ACTION;destination_live=true;
  if(!global_host.export_file(handler,destination))throw NP_GLOBAL_FILE_ACTION;
  outcome=EXPORT_WRITTEN;
 }catch(ExportOutcome expected){outcome=expected;}
 catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 if(destination_live)try {global_host.string_dtor((HostString*)destination);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 if(result_live)try {global_host.string_dtor(&result);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 if(initial_name_live)try {global_host.string_dtor(&initial_name);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 if(initial_directory_live)try {global_host.string_dtor((HostString*)initial_directory);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 if(description_live)try {global_host.string_dtor(&description);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 if(filter_live)try {global_host.string_dtor(&filter);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 if(extension_live)try {global_host.string_dtor(&extension);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 if(empty_live)try {global_host.string_dtor(&empty);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 if(title_live)try {global_host.string_dtor(&title);}catch(...){failure(NP_GLOBAL_FILE_ACTION);outcome=EXPORT_FAILED;}
 return outcome;
}
static bool run_global_file_action(LauncherInstance &instance,uint32_t action) noexcept {
 if(!global_context_live()||instance.file_action!=FILE_ACTION_NONE||
    instance.learn_state!=NATIVE_GLOBAL_LEARN_IDLE||instance.learn_intent!=LEARN_INTENT_NONE||
    mpc_ui_app_is_closing(instance.app_host)){failure(NP_GLOBAL_FILE_ACTION);return false;}
 GlobalFileCatalog before={};if(!snapshot_global_files(before)){failure(NP_GLOBAL_CONTEXT);return false;}
 if(action==FILE_ACTION_EXPORT&&!listed_file(instance.native_common_files,instance.native_common_count,before.selected)){
  failure(NP_GLOBAL_FILE_ACTION);return false;
 }
 void *before_handler=before.handler,*before_file=before.selected;
 instance.file_action=action;record_global_file_action(action);
 if(action==FILE_ACTION_LOAD)set_file_status(instance,"IMPORT CHOOSER OPEN");
 else if(action==FILE_ACTION_EXPORT)set_file_status(instance,"EXPORT CHOOSER OPEN");
 else if(action==FILE_ACTION_NEW)set_file_status(instance,"CREATING MAPPING");
 else if(action==FILE_ACTION_COPY)set_file_status(instance,"COPYING MAPPING");
 else if(action==FILE_ACTION_CLEAR)set_file_status(instance,"CLEARING ASSIGNMENT");
 else {instance.file_action=FILE_ACTION_NONE;failure(NP_TOOLKIT_NAVIGATION);return false;}
 if(!refresh_global_screen(instance,false,true)){
  instance.file_action=FILE_ACTION_NONE;if(state)state->global_file_action=FILE_ACTION_NONE;return false;
 }
 bool called=false;ImportOutcome import_outcome=IMPORT_FAILED;ExportOutcome export_outcome=EXPORT_FAILED;
 GlobalTarget target={};bool target_live=false;GlobalMappingSnapshot before_mapping={};
 if(action==FILE_ACTION_CLEAR&&instance.selected_target>=instance.page_start&&
    instance.selected_target<instance.page_start+instance.row_count)
  before_mapping=instance.row_mapping[instance.selected_target-instance.page_start];
 try {
  if(action==FILE_ACTION_LOAD){import_outcome=import_global_file(instance,before_handler);called=import_outcome!=IMPORT_FAILED;}
  else if(action==FILE_ACTION_EXPORT){export_outcome=export_global_file(before_handler,before_file);called=export_outcome!=EXPORT_FAILED;}
  else if(action==FILE_ACTION_NEW)global_host.create_file(global_capture.view_controller);
  else if(action==FILE_ACTION_COPY)global_host.duplicate_file(global_capture.view_controller);
  else {
   if(instance.selected_target>=MPCLEARN_GLOBAL_LEARN_TARGET_COUNT||
      __atomic_load_n((unsigned char*)before_handler+0x9c,__ATOMIC_ACQUIRE))throw NP_GLOBAL_FILE_ACTION;
   if(global_host.target_fixed(&target,(int32_t)instance.selected_target)!=&target)throw NP_GLOBAL_FILE_ACTION;
   target_live=true;global_host.clear_target(global_capture.view_controller,&target);
   target_live=false;global_host.target_dtor(&target);
  }
  if(action!=FILE_ACTION_LOAD&&action!=FILE_ACTION_EXPORT)called=true;
 }catch(...){if(target_live)try {global_host.target_dtor(&target);}catch(...){}failure(NP_GLOBAL_FILE_ACTION);}
 if(instance.orphaned){
  instance.file_action=FILE_ACTION_NONE;
  if(state)state->global_file_action=FILE_ACTION_NONE;
  return false;
 }
 instance.file_action=FILE_ACTION_NONE;
 if(!called){if(state)state->global_file_action=FILE_ACTION_NONE;set_file_status(instance,"MAPPING ACTION FAILED");(void)refresh_global_screen(instance,false,true);return false;}
 GlobalFileCatalog after={};if(!snapshot_global_files(after)){if(state)state->global_file_action=FILE_ACTION_NONE;failure(NP_GLOBAL_CONTEXT);return false;}
 if(after.handler!=before_handler||!after.selected){if(state)state->global_file_action=FILE_ACTION_NONE;failure(NP_GLOBAL_CONTEXT);return false;}
 bool import_no_addition=false;
 if(action==FILE_ACTION_LOAD){
  import_no_addition=import_outcome==IMPORT_NO_ADDITION||import_outcome==IMPORT_COLLISION;
  bool unchanged=after.count==before.count&&after.selected==before.selected&&after.index==before.index;
  bool added=import_outcome==IMPORT_ADDED&&after.count==before.count+1u&&after.selected!=before.selected&&after.index<after.count;
  if((import_no_addition&&!unchanged)||(!import_no_addition&&!added)){if(state)state->global_file_action=FILE_ACTION_NONE;failure(NP_GLOBAL_FILE_ACTION);return false;}
 }else if(action==FILE_ACTION_EXPORT){
  if(after.count!=before.count||after.index!=before.index||after.selected!=before.selected){if(state)state->global_file_action=FILE_ACTION_NONE;failure(NP_GLOBAL_FILE_ACTION);return false;}
 }else if((action==FILE_ACTION_NEW||action==FILE_ACTION_COPY)&&
          (after.count!=before.count+1u||after.index+1u!=after.count||after.selected==before.selected)){
  if(state)state->global_file_action=FILE_ACTION_NONE;failure(NP_GLOBAL_FILE_ACTION);return false;
 }
 void *after_file=after.selected;
 bool changed=after_file!=before_file;
 if(action==FILE_ACTION_LOAD)set_file_status(instance,import_outcome==IMPORT_COLLISION?"IMPORT DESTINATION EXISTS":
  import_no_addition?"IMPORT CLOSED - NO FILE ADDED":"IMPORTED BYTE-EXACT COPY SELECTED");
 else if(action==FILE_ACTION_EXPORT)set_file_status(instance,export_outcome==EXPORT_CANCELED?
  "EXPORT CLOSED - NO FILE WRITTEN":"EXPORT COMPLETED");
 else if(action==FILE_ACTION_NEW){
  set_file_status(instance,changed?"NEW MAPPING UNSAVED - USE EXPORT":"NEW MAPPING UNCHANGED");
  if(changed){remember_file(instance.native_common_files,instance.native_common_count,after_file);remember_file(instance.unsaved_files,instance.unsaved_count,after_file);}
 }
 else if(action==FILE_ACTION_COPY){
  set_file_status(instance,changed?"COPY SELECTED":"COPY UNCHANGED");
  if(changed)remember_file(instance.native_common_files,instance.native_common_count,after_file);
 }
 else {
 GlobalMappingSnapshot after_mapping={};char assignment[32]={};
  if(!read_global_mapping(instance.selected_target,&before_mapping,after_mapping,assignment,sizeof(assignment),nullptr))return false;
  changed=memcmp(&before_mapping,&after_mapping,sizeof(before_mapping))!=0;
  set_file_status(instance,before_mapping.valid&&!after_mapping.valid?"ASSIGNMENT CLEARED":"CLEAR LEFT ASSIGNMENT UNCHANGED");
  instance.draft_file=nullptr;
 }
 record_global_file_result(action,changed,import_no_addition);
 return refresh_global_screen(instance,true,true);
}
static bool select_global_file_index(LauncherInstance &instance,uint32_t index) noexcept {
 if(!global_context_live()||instance.file_action!=FILE_ACTION_NONE||
    instance.learn_state!=NATIVE_GLOBAL_LEARN_IDLE||instance.learn_intent!=LEARN_INTENT_NONE||
    mpc_ui_app_is_closing(instance.app_host)){failure(NP_GLOBAL_FILE_ACTION);return false;}
 GlobalFileCatalog before={};if(!snapshot_global_files(before)||index>=before.count){failure(NP_GLOBAL_CONTEXT);return false;}
 instance.file_action=FILE_ACTION_SELECT;record_global_file_action(FILE_ACTION_SELECT);set_file_status(instance,"SELECTING MAPPING");
 if(!refresh_global_screen(instance,false,true)){
  instance.file_action=FILE_ACTION_NONE;if(state)state->global_file_action=FILE_ACTION_NONE;return false;
 }
 try {global_host.select_file(global_capture.view_controller,(int32_t)index);}
 catch(...){instance.file_action=FILE_ACTION_NONE;if(state)state->global_file_action=FILE_ACTION_NONE;failure(NP_GLOBAL_FILE_ACTION);return false;}
 instance.file_action=FILE_ACTION_NONE;GlobalFileCatalog after={};
 if(!snapshot_global_files(after)||after.handler!=before.handler||after.index!=index){if(state)state->global_file_action=FILE_ACTION_NONE;failure(NP_GLOBAL_FILE_ACTION);return false;}
 set_file_status(instance,"MAPPING SELECTED");record_global_file_result(FILE_ACTION_SELECT,after.selected!=before.selected);return refresh_global_screen(instance,true,true);
}
static int global_app_close(void *owner) noexcept {
 auto &instance=*(LauncherInstance*)owner;
 if(!instance.global_screen.screen)return 1;
 if(!mpclearn_global_learn_screen_close(&instance.global_screen)){failure(NP_TOOLKIT_LIFETIME);return false;}
 if(state){increment(&state->shells_destroyed);decrement(&state->live_shells);increment(&state->global_screens_destroyed);decrement(&state->live_global_screens);}return 1;
}
static int global_app_prepare_close(void *owner,int forced) noexcept {
 auto &instance=*(LauncherInstance*)owner;
 if(!instance.global_screen.screen)return MPC_UI_APP_CLOSE_READY;
 if(instance.file_action!=FILE_ACTION_NONE)return MPC_UI_APP_CLOSE_FAILED;
 GlobalLearnObservation observed={};
 if(!settle_global_learn(instance,observed))return MPC_UI_APP_CLOSE_FAILED;
 bool idle=!observed.pending&&!observed.owner_value&&!observed.mirror_value&&
  instance.learn_intent==LEARN_INTENT_NONE&&instance.learn_state==NATIVE_GLOBAL_LEARN_IDLE;
 if(idle)return MPC_UI_APP_CLOSE_READY;
 if(instance.learn_state==NATIVE_GLOBAL_LEARN_UNSETTLED)return MPC_UI_APP_CLOSE_FAILED;
 if(instance.learn_intent!=LEARN_INTENT_CANCEL&&!request_global_learn(instance,false))return MPC_UI_APP_CLOSE_FAILED;
 /* The captured mapping owner outlives the launcher overlay. Forced owner
  * destruction queues the same false intent but cannot wait for another UI
  * drain before detaching this screen. */
 return forced?MPC_UI_APP_CLOSE_READY:MPC_UI_APP_CLOSE_PENDING;
}

static bool refresh_global_screen(LauncherInstance &instance,bool rebuild_rows,bool force_view) noexcept {
 const MpclearnGlobalLearnGroup *group=mpclearn_global_learn_group(instance.selected_group);
 if(!instance.global_screen.screen||!global_ready()||!group||group->target_count>MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS||
    instance.page_start!=group->first_target||instance.page_start+group->target_count>MPCLEARN_GLOBAL_LEARN_TARGET_COUNT){failure(NP_GLOBAL_CONTEXT);return false;}
 GlobalLearnObservation learn={};bool learn_changed=false;if(!settle_global_learn(instance,learn,&learn_changed))return false;
 GlobalFileCatalog catalog={};if(!snapshot_global_files(catalog)){failure(NP_GLOBAL_CONTEXT);return false;}
 void *handler=catalog.handler,*selected_file=catalog.selected;
 bool selected_file_changed=selected_file!=instance.selected_file;if(selected_file_changed)rebuild_rows=true;
 uint32_t rows=group->target_count;
 if(rebuild_rows||instance.row_count!=rows){
  instance.assigned_rows=0;
  for(uint32_t row=0;row<rows;row++){
   if(!build_global_row(instance,row,instance.page_start+row))return false;
   if(instance.row_mapping[row].valid)instance.assigned_rows++;
  }
  for(uint32_t row=rows;row<MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS;row++){
   text_clear(instance.row_text[row],NATIVE_GLOBAL_TEXT_MAX);text_clear(instance.row_label[row],sizeof(instance.row_label[row]));
   text_clear(instance.row_assignment[row],sizeof(instance.row_assignment[row]));instance.row_mapping[row]={};
  }
  instance.selected_file=selected_file;instance.row_count=rows;instance.poll_divider=0;instance.poll_row=0;
 }
 instance.file_count=catalog.count;instance.file_index=catalog.index;
 if(!read_global_file_name(selected_file,instance.mapping_text,sizeof(instance.mapping_text)))return false;
 text_clear(instance.page_text,sizeof(instance.page_text));text_append(instance.page_text,sizeof(instance.page_text),group->name);
 text_append(instance.page_text,sizeof(instance.page_text)," ");text_append_uint(instance.page_text,sizeof(instance.page_text),instance.page_start+1u);
 text_append(instance.page_text,sizeof(instance.page_text),"-");text_append_uint(instance.page_text,sizeof(instance.page_text),instance.page_start+rows);
 uint32_t selected_row=instance.selected_target>=instance.page_start&&instance.selected_target<instance.page_start+rows?
  instance.selected_target-instance.page_start:MPCLEARN_GLOBAL_LEARN_NO_SELECTION;
 if(selected_row==MPCLEARN_GLOBAL_LEARN_NO_SELECTION){
  instance.draft_file=selected_file;instance.draft_target=MPCLEARN_GLOBAL_LEARN_NO_SELECTION;instance.draft_dirty=0;
  instance.draft_mapping={2,5,1,0,0,1,0};
 }else if(selected_file_changed||instance.draft_file!=selected_file||instance.draft_target!=instance.selected_target){
  instance.draft_file=selected_file;instance.draft_target=instance.selected_target;instance.draft_dirty=0;
  instance.draft_mapping=instance.row_mapping[selected_row];
  if(!instance.draft_mapping.valid)instance.draft_mapping={2,5,1,0,0,1,0};
 }
 compose_global_draft(instance);
 text_clear(instance.selected_text,sizeof(instance.selected_text));text_clear(instance.assignment_text,sizeof(instance.assignment_text));
 if(selected_row!=MPCLEARN_GLOBAL_LEARN_NO_SELECTION){
  text_append(instance.selected_text,sizeof(instance.selected_text),instance.row_text[selected_row]);
  text_append(instance.assignment_text,sizeof(instance.assignment_text),"ASSIGNMENT: ");
  text_append(instance.assignment_text,sizeof(instance.assignment_text),instance.row_assignment[selected_row]);
 }else{
  text_append(instance.selected_text,sizeof(instance.selected_text),"SELECT A TARGET");
  text_append(instance.assignment_text,sizeof(instance.assignment_text),"ASSIGNMENT: UNASSIGNED");
 }
 text_clear(instance.status_text,sizeof(instance.status_text));
 if(instance.learn_state==NATIVE_GLOBAL_LEARN_ARMING){text_append(instance.status_text,sizeof(instance.status_text),"ARMING TARGET ");text_append_uint(instance.status_text,sizeof(instance.status_text),instance.selected_target+1u);}
 else if(instance.learn_state==NATIVE_GLOBAL_LEARN_ARMED){text_append(instance.status_text,sizeof(instance.status_text),instance.selected_target<120u?"LEARNING TARGET ":"LEARNING - CANCEL");if(instance.selected_target<120u)text_append_uint(instance.status_text,sizeof(instance.status_text),instance.selected_target+1u);}
 else if(instance.learn_state==NATIVE_GLOBAL_LEARN_CANCELING)text_append(instance.status_text,sizeof(instance.status_text),mpc_ui_app_is_closing(instance.app_host)?"CANCELING BEFORE BACK":"CANCELING LEARN");
 else if(instance.learn_state==NATIVE_GLOBAL_LEARN_UNSETTLED)text_append(instance.status_text,sizeof(instance.status_text),"LEARN UNSETTLED - CANCEL");
 else if(instance.file_status_text[0])text_append(instance.status_text,sizeof(instance.status_text),instance.file_status_text);
 else if(listed_file(instance.unsaved_files,instance.unsaved_count,selected_file))text_append(instance.status_text,sizeof(instance.status_text),"UNSAVED MAPPING - USE EXPORT");
 else if(instance.selected_target<120u){text_append(instance.status_text,sizeof(instance.status_text),"SELECTED TARGET ");text_append_uint(instance.status_text,sizeof(instance.status_text),instance.selected_target+1u);}
 else text_append(instance.status_text,sizeof(instance.status_text),"SELECT A TARGET");
 MpclearnGlobalLearnView view={};view.selected_target_text=instance.selected_text;view.assignment_text=instance.assignment_text;
 view.status_text=instance.status_text;view.mapping_text=instance.mapping_text;
 view.edit_type_text=instance.edit_type_text;view.edit_channel_text=instance.edit_channel_text;view.edit_data_text=instance.edit_data_text;
 view.edit_mode_text=instance.edit_mode_text;view.edit_reverse_text=instance.edit_reverse_text;
 view.target_count=rows;view.selected_group=instance.selected_group;view.selected_target_row=selected_row;
 for(uint32_t row=0;row<rows;row++)view.target_text[row]=instance.row_text[row];
 bool learn_locked=instance.learn_state!=NATIVE_GLOBAL_LEARN_IDLE||instance.learn_intent!=LEARN_INTENT_NONE;
 view.can_learn=instance.selected_target<MPCLEARN_GLOBAL_LEARN_TARGET_COUNT||learn_locked;view.learning=learn_locked;
 bool file_idle=!learn_locked&&instance.file_action==FILE_ACTION_NONE&&!mpc_ui_app_is_closing(instance.app_host);
 view.can_new=file_idle;view.can_copy=file_idle;view.can_load=file_idle;
 view.can_export=file_idle&&listed_file(instance.native_common_files,instance.native_common_count,selected_file);
 view.can_clear=file_idle&&instance.selected_target<MPCLEARN_GLOBAL_LEARN_TARGET_COUNT&&
  !__atomic_load_n((unsigned char*)handler+0x9c,__ATOMIC_ACQUIRE);
 view.can_previous_file=file_idle&&catalog.index>0;view.can_next_file=file_idle&&catalog.index+1u<catalog.count;
 view.can_edit=file_idle&&selected_row!=MPCLEARN_GLOBAL_LEARN_NO_SELECTION&&
  listed_file(instance.native_common_files,instance.native_common_count,selected_file)&&
  !__atomic_load_n((unsigned char*)handler+0x9c,__ATOMIC_ACQUIRE);
 view.can_edit_data=view.can_edit&&instance.draft_mapping.type!=3;view.can_apply=view.can_edit&&instance.draft_dirty;
 if((force_view||rebuild_rows||learn_changed)&&!mpclearn_global_learn_screen_update(&instance.global_screen,&view)){failure(NP_LAUNCHER_CONSTRUCTION);return false;}
 if(state){
  __atomic_add_fetch(&state->global_revision,1u,__ATOMIC_RELEASE);
  increment(&state->global_refreshes);state->global_target_count=120u;state->global_page_start=instance.page_start;
  state->global_selected_target=instance.selected_target;state->global_assigned_rows=instance.assigned_rows;
  state->last_global_owner=(uint32_t)(uintptr_t)global_capture.owner;state->last_global_vc=(uint32_t)(uintptr_t)global_capture.view_controller;
  state->last_global_file=(uint32_t)(uintptr_t)handler;memcpy(state->global_page_text,instance.page_text,NATIVE_GLOBAL_TEXT_MAX);
  memcpy(state->global_status_text,instance.status_text,NATIVE_GLOBAL_TEXT_MAX);
  memcpy(state->global_mapping_text,instance.mapping_text,NATIVE_GLOBAL_TEXT_MAX);
  memcpy(state->global_file_status_text,instance.file_status_text,NATIVE_GLOBAL_TEXT_MAX);
  state->global_file_action=instance.file_action;state->global_file_count=catalog.count;state->global_file_index=catalog.index;
  for(uint32_t row=0;row<NATIVE_GLOBAL_ROWS;row++){
   if(row<rows)memcpy(state->global_rows[row],instance.row_text[row],NATIVE_GLOBAL_TEXT_MAX);
   else text_clear((char*)state->global_rows[row],NATIVE_GLOBAL_TEXT_MAX);
  }
  state->global_learn_state=instance.learn_state;state->global_learn_intent=instance.learn_intent;
  state->global_learn_pending=learn.pending;state->global_learn_owner_value=learn.owner_value;
  state->global_learn_mirror_value=learn.mirror_value;state->global_learn_target=instance.selected_target;
  state->global_learn_waits=instance.learn_waits;
  __atomic_add_fetch(&state->global_revision,1u,__ATOMIC_RELEASE);
 }
 return true;
}
static constexpr uint32_t GLOBAL_PAIRING_POLL_DIVISOR=4u;
static bool poll_visible_global_pairing(LauncherInstance &instance) noexcept {
 if(!instance.global_screen.screen||instance.learn_intent!=LEARN_INTENT_NONE)return true;
 if(++instance.poll_divider<GLOBAL_PAIRING_POLL_DIVISOR)return true;instance.poll_divider=0;
 GlobalFileCatalog catalog={};if(!snapshot_global_files(catalog)){failure(NP_GLOBAL_CONTEXT);return false;}
 void *selected_file=catalog.selected;
 if(selected_file!=instance.selected_file)return refresh_global_screen(instance,true,true);
 if(!instance.row_count)return true;
 uint32_t row=instance.poll_row++%instance.row_count;
 if(instance.learn_state==NATIVE_GLOBAL_LEARN_ARMED&&instance.selected_target>=instance.page_start&&
    instance.selected_target<instance.page_start+instance.row_count)row=instance.selected_target-instance.page_start;
 GlobalMappingSnapshot observed={};char assignment[32]={};bool changed=false;
 if(!read_global_mapping(instance.page_start+row,&instance.row_mapping[row],observed,assignment,sizeof(assignment),&changed))return false;
 if(changed){
  if(instance.row_mapping[row].valid&&instance.assigned_rows)instance.assigned_rows--;
  if(observed.valid)instance.assigned_rows++;instance.row_mapping[row]=observed;
  compose_global_target(instance,row,instance.page_start+row,assignment);
  if(instance.selected_target==instance.page_start+row&&!refresh_global_screen(instance,false,true))return false;
 }
 if(state){
  __atomic_add_fetch(&state->global_revision,1u,__ATOMIC_RELEASE);increment(&state->global_pairing_polls);
  if(changed){increment(&state->global_pairing_changes);increment(&state->global_row_repaints);state->global_assigned_rows=instance.assigned_rows;memcpy(state->global_rows[row],instance.row_text[row],NATIVE_GLOBAL_TEXT_MAX);}
  __atomic_add_fetch(&state->global_revision,1u,__ATOMIC_RELEASE);
 }
 return true;
}
static bool destroy_global_mapping(void *mapping) noexcept {
 bool ok=true;
 try {global_host.free_block(*(void**)((unsigned char*)mapping+0x88));}catch(...){ok=false;}
 try {global_host.signal_dtor((unsigned char*)mapping+0x5c);}catch(...){ok=false;}
 try {global_host.signal_dtor((unsigned char*)mapping+0x30);}catch(...){ok=false;}
 try {global_host.signal_dtor((unsigned char*)mapping+0x08);}catch(...){ok=false;}
 return ok;
}
static bool apply_global_draft(LauncherInstance &instance) noexcept {
 const GlobalMappingSnapshot requested=instance.draft_mapping;
 if(instance.file_action!=FILE_ACTION_NONE||mpc_ui_app_is_closing(instance.app_host)||
    instance.learn_state!=NATIVE_GLOBAL_LEARN_IDLE||instance.learn_intent!=LEARN_INTENT_NONE||
    instance.selected_target>=MPCLEARN_GLOBAL_LEARN_TARGET_COUNT||
    instance.draft_target!=instance.selected_target||!instance.draft_dirty||
    requested.type<1||requested.type>3||requested.channel<1||requested.channel>16||
    requested.control<1||requested.control>7||requested.reverse>1)return false;
 GlobalFileCatalog before={};
 if(!snapshot_global_files(before)||before.selected!=instance.draft_file||
    !listed_file(instance.native_common_files,instance.native_common_count,before.selected)||
    __atomic_load_n((unsigned char*)before.handler+0x9c,__ATOMIC_ACQUIRE))return false;
 instance.file_action=FILE_ACTION_APPLY;record_global_file_action(FILE_ACTION_APPLY);
 if(state)increment(&state->global_edit_requests);
 set_file_status(instance,"APPLYING ASSIGNMENT");
 if(!refresh_global_screen(instance,false,true)){
  instance.file_action=FILE_ACTION_NONE;if(state)state->global_file_action=FILE_ACTION_NONE;return false;
 }
 alignas(8) unsigned char message[24]={},mapping[172]={};GlobalTarget target={};
 bool target_live=false,message_live=false,mapping_live=false,called=false,cleanup_ok=true;
 try {
  GlobalFileCatalog current={};
  if(!snapshot_global_files(current)||current.handler!=before.handler||current.selected!=before.selected||
     current.index!=before.index||__atomic_load_n((unsigned char*)current.handler+0x9c,__ATOMIC_ACQUIRE))
   throw NP_GLOBAL_FILE_ACTION;
  if(global_host.target_fixed(&target,(int32_t)instance.selected_target)!=&target)throw NP_GLOBAL_FILE_ACTION;
  target_live=true;
  if(requested.type==1)global_host.note_message(message,requested.channel,requested.data1,127u);
  else if(requested.type==2)global_host.cc_message(message,requested.channel,requested.data1,0u);
  else global_host.pitch_message(message,requested.channel,8192);
  message_live=true;
  if(global_host.mapping_ctor(mapping,message)!=mapping)throw NP_GLOBAL_FILE_ACTION;
  mapping_live=true;
  global_host.commit_mapping((unsigned char*)global_capture.owner+0x19c,&target,mapping);
  global_host.set_control((unsigned char*)global_capture.owner+0x19c,&target,requested.control);
  global_host.set_reverse((unsigned char*)global_capture.owner+0x19c,&target,requested.reverse!=0);
  called=true;
 }catch(...){failure(NP_GLOBAL_FILE_ACTION);}
 if(mapping_live)cleanup_ok=destroy_global_mapping(mapping)&&cleanup_ok;
 if(message_live)try {global_host.message_dtor(message);}catch(...){cleanup_ok=false;}
 if(target_live)try {global_host.target_dtor(&target);}catch(...){cleanup_ok=false;}
 instance.file_action=FILE_ACTION_NONE;
 if(!called||!cleanup_ok){
  if(!cleanup_ok)failure(NP_GLOBAL_FILE_ACTION);
  if(state)state->global_file_action=FILE_ACTION_NONE;
  set_file_status(instance,"ASSIGNMENT APPLY FAILED");(void)refresh_global_screen(instance,true,true);return false;
 }
 GlobalFileCatalog after={};GlobalMappingSnapshot observed={};char assignment[32]={};
 bool matches=snapshot_global_files(after)&&after.handler==before.handler&&after.selected==before.selected&&
  after.index==before.index&&read_global_mapping(instance.selected_target,nullptr,observed,assignment,sizeof(assignment),nullptr)&&
  observed.valid&&observed.type==requested.type&&observed.channel==requested.channel&&
  observed.control==requested.control&&observed.reverse==requested.reverse&&
  (requested.type==3||observed.data1==requested.data1);
 if(!matches){
  if(state)state->global_file_action=FILE_ACTION_NONE;
  set_file_status(instance,"ASSIGNMENT DID NOT SETTLE");failure(NP_GLOBAL_FILE_ACTION);
  (void)refresh_global_screen(instance,true,true);return false;
 }
 bool changed=!instance.row_mapping[instance.selected_target-instance.page_start].valid||
  memcmp(&instance.row_mapping[instance.selected_target-instance.page_start],&observed,sizeof(observed));
 instance.draft_file=nullptr;record_global_file_result(FILE_ACTION_APPLY,changed);
 if(state){
  if(!changed)increment(&state->global_edit_no_changes);
  state->global_edit_type=(uint32_t)observed.type;state->global_edit_channel=(uint32_t)observed.channel;
  state->global_edit_data=observed.data1;state->global_edit_control=(uint32_t)observed.control;
  state->global_edit_reverse=observed.reverse;
 }
 set_file_status(instance,listed_file(instance.unsaved_files,instance.unsaved_count,after.selected)?
  "EDIT APPLIED - FILE SAVE UNCONFIRMED":"ASSIGNMENT APPLIED");
 return refresh_global_screen(instance,true,true);
}
static bool update_global_draft(LauncherInstance &instance,uint32_t intent) noexcept {
 if(instance.file_action!=FILE_ACTION_NONE||mpc_ui_app_is_closing(instance.app_host)||
    instance.learn_state!=NATIVE_GLOBAL_LEARN_IDLE||instance.learn_intent!=LEARN_INTENT_NONE||
    instance.selected_target>=MPCLEARN_GLOBAL_LEARN_TARGET_COUNT||
    instance.draft_target!=instance.selected_target)return false;
 GlobalFileCatalog files={};if(!snapshot_global_files(files)||files.selected!=instance.draft_file||
  !listed_file(instance.native_common_files,instance.native_common_count,files.selected)||
  __atomic_load_n((unsigned char*)files.handler+0x9c,__ATOMIC_ACQUIRE))return false;
 if(intent==MPCLEARN_GLOBAL_LEARN_TYPE)instance.draft_mapping.type=instance.draft_mapping.type>=3?1:instance.draft_mapping.type+1;
 else if(intent==MPCLEARN_GLOBAL_LEARN_CHANNEL_DOWN)instance.draft_mapping.channel=instance.draft_mapping.channel<=1?16:instance.draft_mapping.channel-1;
 else if(intent==MPCLEARN_GLOBAL_LEARN_CHANNEL_UP)instance.draft_mapping.channel=instance.draft_mapping.channel>=16?1:instance.draft_mapping.channel+1;
 else if(intent==MPCLEARN_GLOBAL_LEARN_DATA_DOWN){if(instance.draft_mapping.type==3)return false;instance.draft_mapping.data1=instance.draft_mapping.data1?instance.draft_mapping.data1-1:127;}
 else if(intent==MPCLEARN_GLOBAL_LEARN_DATA_UP){if(instance.draft_mapping.type==3)return false;instance.draft_mapping.data1=instance.draft_mapping.data1>=127?0:instance.draft_mapping.data1+1;}
 else if(intent==MPCLEARN_GLOBAL_LEARN_MODE)instance.draft_mapping.control=instance.draft_mapping.control>=7?1:instance.draft_mapping.control+1;
 else if(intent==MPCLEARN_GLOBAL_LEARN_REVERSE)instance.draft_mapping.reverse=!instance.draft_mapping.reverse;
 else return false;
 instance.draft_mapping.valid=1;instance.draft_dirty=1;set_file_status(instance,"EDIT READY - TAP APPLY");
 return refresh_global_screen(instance,false,true);
}
static void global_screen_intent(void *owner,uint32_t intent) noexcept {
 auto *instance=(LauncherInstance*)owner;if(!instance||!instance->global_screen.screen){failure(NP_GLOBAL_CONTEXT);return;}
 bool learn_locked=instance->learn_state!=NATIVE_GLOBAL_LEARN_IDLE||instance->learn_intent!=LEARN_INTENT_NONE;
 bool interaction_locked=learn_locked||instance->file_action!=FILE_ACTION_NONE;
 if(intent>=MPCLEARN_GLOBAL_LEARN_GROUP_1&&intent<MPCLEARN_GLOBAL_LEARN_GROUP_1+MPCLEARN_GLOBAL_LEARN_GROUP_COUNT){
  if(interaction_locked){(void)refresh_global_screen(*instance,false,true);return;}
  uint32_t group_index=intent-MPCLEARN_GLOBAL_LEARN_GROUP_1;
  const MpclearnGlobalLearnGroup *group=mpclearn_global_learn_group(group_index);if(!group){failure(NP_TOOLKIT_NAVIGATION);return;}
  set_file_status(*instance,"");
  instance->selected_group=group_index;instance->page_start=group->first_target;instance->selected_target=MPCLEARN_GLOBAL_LEARN_NO_SELECTION;
  if(state)increment(&state->global_page_clicks);(void)refresh_global_screen(*instance,true,true);return;
 }
 if(intent>=MPCLEARN_GLOBAL_LEARN_TARGET_1&&intent<MPCLEARN_GLOBAL_LEARN_TARGET_1+MPCLEARN_GLOBAL_LEARN_MAX_GROUP_TARGETS){
  if(interaction_locked){(void)refresh_global_screen(*instance,false,true);return;}
  uint32_t row=intent-MPCLEARN_GLOBAL_LEARN_TARGET_1;if(row>=instance->row_count){failure(NP_TOOLKIT_NAVIGATION);return;}
  uint32_t target=instance->page_start+row;if(target>=MPCLEARN_GLOBAL_LEARN_TARGET_COUNT){failure(NP_TOOLKIT_NAVIGATION);return;}
  set_file_status(*instance,"");
  if(select_global_target(*instance,target)){if(state)increment(&state->global_row_clicks);(void)refresh_global_screen(*instance,false,true);}return;
 }
 if(intent==MPCLEARN_GLOBAL_LEARN_NEW){(void)run_global_file_action(*instance,FILE_ACTION_NEW);return;}
 if(intent==MPCLEARN_GLOBAL_LEARN_COPY){(void)run_global_file_action(*instance,FILE_ACTION_COPY);return;}
 if(intent==MPCLEARN_GLOBAL_LEARN_LOAD){(void)run_global_file_action(*instance,FILE_ACTION_LOAD);return;}
 if(intent==MPCLEARN_GLOBAL_LEARN_EXPORT){(void)run_global_file_action(*instance,FILE_ACTION_EXPORT);return;}
 if(intent==MPCLEARN_GLOBAL_LEARN_CLEAR){(void)run_global_file_action(*instance,FILE_ACTION_CLEAR);return;}
 if(intent==MPCLEARN_GLOBAL_LEARN_PREVIOUS_FILE){
  GlobalFileCatalog files={};if(!snapshot_global_files(files)||!files.index){failure(NP_TOOLKIT_NAVIGATION);return;}
  (void)select_global_file_index(*instance,files.index-1u);return;
 }
 if(intent==MPCLEARN_GLOBAL_LEARN_NEXT_FILE){
  GlobalFileCatalog files={};if(!snapshot_global_files(files)||files.index+1u>=files.count){failure(NP_TOOLKIT_NAVIGATION);return;}
  (void)select_global_file_index(*instance,files.index+1u);return;
 }
 if(intent>=MPCLEARN_GLOBAL_LEARN_TYPE&&intent<=MPCLEARN_GLOBAL_LEARN_REVERSE){
  if(!update_global_draft(*instance,intent))(void)refresh_global_screen(*instance,false,true);return;
 }
 if(intent==MPCLEARN_GLOBAL_LEARN_APPLY){if(!apply_global_draft(*instance))(void)refresh_global_screen(*instance,false,true);return;}
 if(intent==MPCLEARN_GLOBAL_LEARN_BEGIN){
  if(instance->selected_target>=120u||interaction_locked)return;
  set_file_status(*instance,"");
  if(request_global_learn(*instance,true))(void)refresh_global_screen(*instance,false,true);return;
 }
 if(intent==MPCLEARN_GLOBAL_LEARN_CANCEL){
  if(instance->learn_intent==LEARN_INTENT_CANCEL)return;
  if(request_global_learn(*instance,false))(void)refresh_global_screen(*instance,false,true);return;
 }
 if(intent==MPCLEARN_GLOBAL_LEARN_BACK){
  if(instance->file_action!=FILE_ACTION_NONE)return;
  if(state)increment(&state->back_clicks);
  if(!mpc_ui_app_request_close(instance->app_host)){failure(NP_TOOLKIT_NAVIGATION);return;}
  if(learn_locked&&instance->learn_intent!=LEARN_INTENT_CANCEL&&
     request_global_learn(*instance,false))(void)refresh_global_screen(*instance,false,true);
  return;
 }
 failure(NP_TOOLKIT_NAVIGATION);
}
static int global_app_open(void *owner,MpcUiAppHost *host,MpcUiContext *context,
                           MpcUiParent *parent,MpcUiRect bounds) noexcept {
 auto &instance=*(LauncherInstance*)owner;
 if(instance.global_screen.screen||instance.app_host!=host||instance.context!=context||
    instance.navigation_parent!=parent||!global_ready()){failure(NP_GLOBAL_CONTEXT);return 0;}
 if(bounds.width<1280||bounds.width>2000||bounds.height<620||bounds.height>1200){failure(NP_LAUNCHER_CONTEXT);return 0;}
 instance.selected_group=0;instance.page_start=0;instance.selected_target=MPCLEARN_GLOBAL_LEARN_NO_SELECTION;
 instance.learn_state=NATIVE_GLOBAL_LEARN_IDLE;instance.learn_intent=LEARN_INTENT_NONE;instance.learn_waits=0;
 instance.file_action=FILE_ACTION_NONE;instance.selected_file=nullptr;set_file_status(instance,"");
 if(!mpclearn_global_learn_screen_open(&instance.global_screen,context,parent,bounds,global_screen_intent,&instance)||
    !refresh_global_screen(instance,true,true)){
  failure(NP_LAUNCHER_CONSTRUCTION);if(instance.global_screen.screen)(void)mpclearn_global_learn_screen_close(&instance.global_screen);return false;
 }
 if(state){increment(&state->shells_created);increment(&state->live_shells);increment(&state->global_screens_created);increment(&state->live_global_screens);}return 1;
}
static const MpcUiAppCallbacks global_app_callbacks __attribute__((unused))={
 sizeof(MpcUiAppCallbacks),global_app_open,global_app_prepare_close,global_app_close
};
#if MPCLEARN_NATIVE_UI_EXAMPLE
static void example_mark_closed(LauncherInstance &instance) {
 if(!instance.example_open_counted)return;
 instance.example_open_counted=0;
 if(state){increment(&state->example_closes);decrement(&state->live_examples);increment(&state->shells_destroyed);decrement(&state->live_shells);}
}
static void example_action(void *owner,uint32_t intent) noexcept {
 auto &instance=*(LauncherInstance*)owner;
 if(state){increment(&state->example_intents);state->last_example_intent=intent;}
 if(intent==MPCLEARN_TOOLKIT_EXAMPLE_ACTION_ONE){
  if(!mpclearn_toolkit_example_set_title(&instance.example_app,"ACTION ONE - OWNER VALUE 32")||
     !mpclearn_toolkit_example_set_value(&instance.example_app,32.0))failure(NP_TOOLKIT_NAVIGATION);
 }else if(intent==MPCLEARN_TOOLKIT_EXAMPLE_ACTION_TWO){
  if(!mpclearn_toolkit_example_set_title(&instance.example_app,"ACTION TWO - OWNER VALUE 96")||
     !mpclearn_toolkit_example_set_value(&instance.example_app,96.0))failure(NP_TOOLKIT_NAVIGATION);
 }else if(intent==MPCLEARN_TOOLKIT_EXAMPLE_BACK){
  if(state)increment(&state->example_back_clicks);
 }else failure(NP_TOOLKIT_NAVIGATION);
}
static void example_slider(void *owner,uint32_t intent,uint32_t event,double) noexcept {
 auto &instance=*(LauncherInstance*)owner;
 if(intent!=MPCLEARN_TOOLKIT_EXAMPLE_SLIDER||event<MPC_UI_SLIDER_VALUE_CHANGED||
    event>MPC_UI_SLIDER_DRAG_ENDED){failure(NP_TOOLKIT_NAVIGATION);return;}
 if(state){increment(&state->example_intents);state->last_example_intent=0x100u+event;}
 const char *text=event==MPC_UI_SLIDER_DRAG_STARTED?"SLIDER DRAG START":
  event==MPC_UI_SLIDER_DRAG_ENDED?"SLIDER DRAG END":"SLIDER VALUE CHANGED";
 if(!mpclearn_toolkit_example_set_title(&instance.example_app,text))failure(NP_TOOLKIT_NAVIGATION);
}
#endif
static bool register_launcher_app(LauncherInstance &instance,MpcUiRect bounds) {
 if(instance.app_host)return true;
 MpcUiAppHost *host=mpc_ui_private_app_host_create(instance.context,instance.navigation_parent,bounds);
 if(!host)return false;
#if MPCLEARN_NATIVE_UI_EXAMPLE
 bool ok=mpclearn_toolkit_example_app_init(&instance.example_app,64.0,example_action,example_slider,&instance)&&
  mpc_ui_app_register(host,mpclearn_toolkit_example_app_callbacks(),&instance.example_app);
#else
 bool ok=mpc_ui_app_register(host,&global_app_callbacks,&instance);
#endif
 if(!ok){(void)mpc_ui_private_app_host_destroy(&host,1);return false;}
 instance.app_host=host;return true;
}
static bool launcher_suspend_safe(const LauncherInstance &instance) {
 if(!instance.app_host||!mpc_ui_app_is_open(instance.app_host)||
    mpc_ui_private_app_host_is_suspended(instance.app_host))return true;
 if(mpc_ui_app_is_closing(instance.app_host))return false;
#if MPCLEARN_NATIVE_UI_EXAMPLE
 return true;
#else
 return instance.file_action==FILE_ACTION_NONE&&
  instance.learn_state==NATIVE_GLOBAL_LEARN_IDLE&&
  instance.learn_intent==LEARN_INTENT_NONE&&
  (!state||!__atomic_load_n(&state->global_learn_pending,__ATOMIC_ACQUIRE));
#endif
}
static void launcher_intent(void *owner,uint32_t intent) noexcept {
 auto *instance=(LauncherInstance*)owner;
 if(!instance||!instance->overlay){failure(NP_LAUNCHER_CONTEXT);return;}
 if(intent==LAUNCHER_OPEN_SHELL){
  if(state)increment(&state->launcher_clicks);
  if(mpc_ui_app_is_open(instance->app_host)){
   if(!mpc_ui_private_app_host_is_suspended(instance->app_host)||instance->resume_requested)
    failure(NP_TOOLKIT_NAVIGATION);
   else instance->resume_requested=1;
   return;
  }
  if(!mpc_ui_app_open(instance->app_host)){failure(NP_LAUNCHER_CONSTRUCTION);return;}
#if MPCLEARN_NATIVE_UI_EXAMPLE
  instance->example_open_counted=1;
  if(state){increment(&state->example_opens);increment(&state->live_examples);increment(&state->shells_created);increment(&state->live_shells);}
#endif
  return;
 }
 failure(NP_TOOLKIT_NAVIGATION);
}

static int attach_launcher_entry(Context *c) {
 LauncherInstance *instance=nullptr;uint32_t begin=0,end=0,cap=0;
 for(auto &candidate:launcher_instances)if(candidate.page&&candidate.overlay){
  uint32_t *records=(uint32_t*)((unsigned char*)candidate.page+0xc8);
  uint32_t candidate_begin=records[0],candidate_end=records[1],candidate_cap=records[2];
  if((!candidate_begin&&(candidate_end||candidate_cap))||candidate_begin>candidate_end||candidate_end>candidate_cap||
   ((candidate_begin|candidate_end|candidate_cap)&3)||(candidate_begin&&(((candidate_end-candidate_begin)&7)||((candidate_cap-candidate_begin)&7)||(candidate_end-candidate_begin)/8>64))){failure(NP_LAUNCHER_CONTEXT);return 0;}
  bool direct_empty=candidate_begin==candidate_end&&c->r[5]==(uint32_t)(uintptr_t)candidate.page;
  bool completed_populated=candidate_begin!=candidate_end&&c->r[4]==candidate_end&&c->r[5]==candidate_end;
  if(!direct_empty&&!completed_populated)continue;
  if(instance){failure(NP_LAUNCHER_CONTEXT);return 0;}
  instance=&candidate;begin=candidate_begin;end=candidate_end;cap=candidate_cap;
 }
 if(!instance)return 1;
 record_launcher_phase(true,instance->overlay,instance->page);
 if(instance->entry_screen){
  if(!instance->entry_parent||!instance->launcher||!instance->app_host){failure(NP_LAUNCHER_CONTEXT);return 0;}
  if(begin!=end&&mpc_ui_app_is_open(instance->app_host)&&
     !mpc_ui_private_app_host_is_suspended(instance->app_host)&&
     launcher_suspend_safe(*instance)&&
     !mpc_ui_private_app_host_suspend(instance->app_host)){
   failure(NP_TOOLKIT_NAVIGATION);return 0;
  }
  return 1;
 }
 if(begin==end)return 1;(void)cap;
 int bounds[4]={};void *second=nullptr;
 try {second=launcher_second_page(*instance,bounds);if(!second)throw NP_LAUNCHER_CONTEXT;}
 catch(Failure why){failure(why);return 0;}catch(...){failure(NP_LAUNCHER_CONTEXT);return 0;}
 int app_bounds[4]={};memcpy(app_bounds,(unsigned char*)instance->page+0x18,sizeof(app_bounds));
 if(!instance->context||!instance->navigation_parent||
    !register_launcher_app(*instance,{app_bounds[0],app_bounds[1],app_bounds[2],app_bounds[3]})){
  failure(NP_TOOLKIT_CONTEXT);return 0;
 }
 MpcUiParent *parent=mpc_ui_private_component_parent(instance->context,second);
 MpcUiScreen *screen=parent?mpc_ui_screen_create(instance->context,parent,{0,0,904,684}):nullptr;
 const char *label=
#if MPCLEARN_NATIVE_UI_EXAMPLE
  "UI TOOLKIT EXAMPLE";
#else
  "GLOBAL MIDI LEARN";
#endif
 MpcUiControl *button=screen?mpc_ui_button_create(screen,label,LAUNCHER_OPEN_SHELL,launcher_intent,instance):nullptr;
 bool ok=button&&mpc_ui_button_style(button,{0xff2f2f34u,0xffd31145u,0xffffffffu})&&
  mpc_ui_control_place(button,{bounds[0],bounds[1],bounds[2],bounds[3]})&&
  mpc_ui_control_keep_in_front(button)&&mpc_ui_screen_show(screen);
 if(!ok){failure(NP_LAUNCHER_CONSTRUCTION);if(screen)(void)mpc_ui_screen_close(&screen);if(parent)(void)mpc_ui_private_parent_destroy(&parent);return 0;}
 instance->entry_parent=parent;instance->entry_screen=screen;instance->launcher=button;
 if(state){increment(&state->launcher_tiles_created);increment(&state->live_launcher_tiles);state->last_launcher_tile=(uint32_t)(uintptr_t)mpc_ui_private_control_host_object(button);}return 1;
}

static int bind_launcher_overlay(Context *c) {
 void *overlay=(void*)(uintptr_t)c->r[4],*page=(void*)(uintptr_t)c->r[5];
 if(!overlay||!page){failure(NP_LAUNCHER_CONTEXT);return 0;}
 record_launcher_phase(false,overlay,page);
 LauncherInstance *instance=launcher_page_instance(page);
 if(!instance){instance=available_launcher_instance();if(!instance){failure(NP_NO_SLOT);return 0;}instance->page=page;}
 if(instance->overlay){failure(NP_LAUNCHER_CONTEXT);return 0;}
 void *table=nullptr;MpcUiContext *context=nullptr;MpcUiParent *navigation_parent=nullptr;
 try {
  context=create_toolkit_context();if(!context)throw NP_TOOLKIT_CONTEXT;
  instance->overlay=overlay;instance->context=context;
  navigation_parent=mpc_ui_private_parent_create(context,instance,launcher_attach,launcher_detach,launcher_focus);
  if(!navigation_parent)throw NP_PARENTING;
  table=core.allocate(NATIVE_LAUNCHER_OVERLAY_VTABLE_BYTES);if(!table)throw NP_ALLOCATION;
  memcpy(table,launcher.overlay_vtable,NATIVE_LAUNCHER_OVERLAY_VTABLE_BYTES);
  ((void**)table)[2]=(void*)(uintptr_t)&native_launcher_overlay_complete_destructor;
  ((void**)table)[3]=(void*)(uintptr_t)&native_launcher_overlay_deleting_destructor;
  *(void**)overlay=(unsigned char*)table+8;instance->navigation_parent=navigation_parent;instance->overlay_vtable=table;
  if(state){increment(&state->launcher_overlays_created);increment(&state->live_launcher_overlays);state->last_launcher_overlay=(uint32_t)(uintptr_t)overlay;state->last_launcher_page=(uint32_t)(uintptr_t)page;}return 1;
 }catch(Failure why){failure(why);}catch(...){failure(NP_UNEXPECTED_EXCEPTION);}
 if(navigation_parent)(void)mpc_ui_private_parent_destroy(&navigation_parent);
 if(context)(void)mpc_ui_private_context_destroy(&context);
 if(table)try {core.sized_delete(table,NATIVE_LAUNCHER_OVERLAY_VTABLE_BYTES);}catch(...){failure(NP_LAUNCHER_TEARDOWN);}
 *instance={};return 0;
}

static int capture_global_owner(Context *c) {
 void *app=(void*)(uintptr_t)c->r[4],*view_controller=(void*)(uintptr_t)c->r[5];
 void *file_handler=(void*)(uintptr_t)c->r[6],*owner=(void*)(uintptr_t)c->r[7];
 if(!app||!view_controller||!owner||
    *(void**)((unsigned char*)app+0x3c0)!=owner||
    *(void**)((unsigned char*)view_controller+0x138)!=owner||
    *(void**)((unsigned char*)view_controller+0x13c)!=file_handler){failure(NP_GLOBAL_CONTEXT);return 0;}
 void *observed=nullptr;
 try {observed=global_host.file_handler((unsigned char*)owner+0x19c);}
 catch(...){failure(NP_GLOBAL_CONTEXT);return 0;}
 if(observed!=file_handler){failure(NP_GLOBAL_CONTEXT);return 0;}
 uint32_t thread=(uint32_t)syscall(224);
 if(global_capture.owner){
  if(global_capture.app!=app||global_capture.owner!=owner||
     global_capture.view_controller!=view_controller||global_capture.ui_thread_id!=thread){failure(NP_GLOBAL_CONTEXT);return 0;}
  return 1;
 }
 global_capture={app,owner,view_controller,thread};
 if(state){
  increment(&state->global_owner_captures);
  state->last_global_owner=(uint32_t)(uintptr_t)owner;
  state->last_global_vc=(uint32_t)(uintptr_t)view_controller;
  state->last_global_file=(uint32_t)(uintptr_t)file_handler;
 }
 return 1;
}

static void process_launcher_pending() noexcept {
 for(auto &instance:launcher_instances)if(instance.overlay&&instance.app_host){
  if(instance.resume_requested){
   instance.resume_requested=0;
   if(!mpc_ui_private_app_host_is_suspended(instance.app_host)||
      !mpc_ui_private_app_host_resume(instance.app_host))failure(NP_TOOLKIT_NAVIGATION);
  }
  if(mpc_ui_app_is_closing(instance.app_host)){
   if(!mpc_ui_private_app_host_drain(instance.app_host))failure(NP_TOOLKIT_NAVIGATION);
#if MPCLEARN_NATIVE_UI_EXAMPLE
   else if(!mpc_ui_app_is_open(instance.app_host))example_mark_closed(instance);
#endif
  }
 }
}
static void process_global_pending() noexcept {
 for(auto &instance:launcher_instances)if(instance.overlay&&instance.global_screen.screen){
  if(instance.file_action!=FILE_ACTION_NONE)continue;
  if(instance.learn_intent!=LEARN_INTENT_NONE)(void)refresh_global_screen(instance,false,false);
  (void)poll_visible_global_pairing(instance);
 }
}
static void launcher_overlay_destructor(void *overlay,int deleting) noexcept {
 LauncherInstance *instance=launcher_overlay_instance(overlay);if(!instance){failure(NP_LAUNCHER_CONTEXT);return;}
 uint32_t callback_depth=instance->app_host?mpc_ui_private_app_host_callback_depth(instance->app_host):0;
 uint32_t consumer_open=instance->app_host&&mpc_ui_app_is_open(instance->app_host);
 uint32_t busy=instance->resume_requested||instance->file_action!=FILE_ACTION_NONE||instance->learn_intent!=LEARN_INTENT_NONE||
  (state&&__atomic_load_n(&state->global_learn_pending,__ATOMIC_ACQUIRE));
 if(state){
  increment(&state->overlay_destroy_entries);
  state->overlay_destroy_ui_thread_matches=global_capture.ui_thread_id==(uint32_t)syscall(224);
  state->overlay_destroy_observer_running=__atomic_load_n(&observer_running,__ATOMIC_ACQUIRE)!=0;
  state->overlay_destroy_consumer_open=consumer_open;
  state->overlay_destroy_busy=busy;
  state->overlay_destroy_callback_depth=callback_depth;
 }
 bool releasable=true;
 if(instance->entry_screen&&!mpc_ui_private_screen_invalidate(instance->entry_screen)){
  failure(NP_TOOLKIT_LIFETIME);releasable=false;
 }
 if(instance->app_host){
  if(!mpc_ui_private_app_host_destroy(&instance->app_host,1)){failure(NP_TOOLKIT_LIFETIME);releasable=false;}
#if MPCLEARN_NATIVE_UI_EXAMPLE
  else example_mark_closed(*instance);
#endif
 }
 if(instance->entry_screen){
  if(!mpc_ui_screen_close(&instance->entry_screen)){failure(NP_LAUNCHER_TEARDOWN);releasable=false;}
  else if(state){increment(&state->launcher_tiles_destroyed);decrement(&state->live_launcher_tiles);}
  if(!instance->entry_screen)instance->launcher=nullptr;
 }
 if(releasable&&instance->entry_parent&&!mpc_ui_private_parent_destroy(&instance->entry_parent)){failure(NP_TOOLKIT_LIFETIME);releasable=false;}
 if(releasable&&instance->navigation_parent&&!mpc_ui_private_parent_destroy(&instance->navigation_parent)){failure(NP_TOOLKIT_LIFETIME);releasable=false;}
 if(releasable&&instance->context&&!mpc_ui_private_context_destroy(&instance->context)){failure(NP_TOOLKIT_LIFETIME);releasable=false;}
 void *table=instance->overlay_vtable;*(void**)overlay=(unsigned char*)launcher.overlay_vtable+8;
 if(state){state->overlay_destroy_cleanup_complete=releasable;state->overlay_destroy_retained=!releasable;}
 if(releasable)*instance={};
 else {instance->page=nullptr;instance->overlay=nullptr;instance->overlay_vtable=nullptr;instance->orphaned=1;}
 if(state){increment(&state->launcher_overlays_destroyed);decrement(&state->live_launcher_overlays);}
 try {(deleting?launcher.overlay_deleting:launcher.overlay_complete)(overlay);if(state)increment(&state->overlay_destroy_stock_calls);}catch(...){failure(NP_LAUNCHER_TEARDOWN);}
 if(table)try {core.sized_delete(table,NATIVE_LAUNCHER_OVERLAY_VTABLE_BYTES);}catch(...){failure(NP_LAUNCHER_TEARDOWN);}
}
extern "C" void native_launcher_overlay_complete_destructor(void *overlay) noexcept {launcher_overlay_destructor(overlay,0);}
extern "C" void native_launcher_overlay_deleting_destructor(void *overlay) noexcept {launcher_overlay_destructor(overlay,1);}

}

extern "C" void native_preferences_set_state(NativePreferencesState *mapped){state=mapped;}
extern "C" void native_preferences_configure_host(uint32_t bias){configure_host(bias);}
extern "C" void native_preferences_refresh(void) {
 if(!state||!__atomic_load_n(&observer_running,__ATOMIC_ACQUIRE))return;
 process_global_pending();
 process_launcher_pending();
 uint32_t completed=__atomic_load_n(&state->completed_sequence,__ATOMIC_ACQUIRE);
 for(auto &instance:preferences_instances)if(instance.tab&&instance.completed_sequence!=completed){refresh_selection(&instance);instance.completed_sequence=completed;}
}
extern "C" void native_preferences_hook(void *saved,unsigned kind) {
 if(!saved||!__atomic_load_n(&observer_running,__ATOMIC_ACQUIRE))return;Context *context=(Context*)saved;
 increment(&state->hook_entries);
 if(kind==0)(void)insert_tab(context);
 else if(kind==1)(void)bind_launcher_overlay(context);
 else if(kind==2)(void)attach_launcher_entry(context);
 else if(kind==3)(void)capture_global_owner(context);
 else failure(NP_BAD_CONTEXT);
}
