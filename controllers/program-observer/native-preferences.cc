/* Freestanding C++ keeps the exception boundary in one registered ELF function.
 * The project ARM image intentionally has no cross g++ frontend; this file uses
 * only compiler types and the C ABI so host clang can emit the ARMhf object. */
using uint64_t=__UINT64_TYPE__;
using uintptr_t=__UINTPTR_TYPE__;
using size_t=__SIZE_TYPE__;
#define NATIVE_PREFERENCES_FREESTANDING
#include "native-preferences.h"
#define NATIVE_PREFERENCES_TAB_VTABLE_BYTES 32u
#define NATIVE_PREFERENCES_BUTTON_VTABLE_BYTES 212u
extern "C" void *memcpy(void *,const void *,size_t) noexcept;
extern "C" long syscall(long,...) noexcept;

extern "C" {
extern uint32_t observer_running;
}

namespace {

struct Context { uint64_t d[32];uint32_t apsr,fpscr,r[13],lr; };
static_assert(sizeof(Context)==320,"saved ARM context");

enum Failure : uint32_t {
 NP_BAD_CONTEXT=1,NP_BAD_VECTOR,NP_NO_SLOT,NP_ALLOCATION,NP_CONSTRUCTION,
 NP_PARENTING,NP_UNEXPECTED_EXCEPTION,NP_TEARDOWN
};

using Allocate=void *(*)(uint32_t);
using SizedDelete=void (*)(void *,uint32_t);
using StdStringCtor=void (*)(void *,const char *);
using NotImplementedCtor=void (*)(void *,uint32_t,void *,void *,float,float,float,float,float,float,void *,void *,void *);
using CompleteDestructor=void (*)(void *);
using TabChild=void *(*)(void *,void *);
using JuceStringCtor=void (*)(void *,const char *);
using JuceStringDestructor=void (*)(void *);
using TextButtonCtor=void (*)(void *,const void *);
using ComponentSetBounds=void (*)(void *,int,int,int,int);
using ComponentSetVisible=void (*)(void *,int);
using ButtonSetToggle=void (*)(void *,int,int);
using TabActive=void (*)(void *,int);

struct Host {
 Allocate allocate;
 SizedDelete sized_delete;
 StdStringCtor std_string_ctor;
 NotImplementedCtor tab_ctor;
 CompleteDestructor tab_dtor;
 TabChild tab_add;
 TabChild tab_remove;
 JuceStringCtor juce_string_ctor;
 JuceStringDestructor juce_string_dtor;
 TextButtonCtor button_ctor;
 CompleteDestructor button_dtor;
 ComponentSetBounds set_bounds;
 ComponentSetVisible set_visible;
 ButtonSetToggle set_toggle;
 const unsigned char *tab_vtable;
 const unsigned char *button_vtable;
};

struct Instance {
 void *overlay,*tab,*tab_vtable,*buttons[3],*button_vtables[3],*marker;TabActive original_active;
};

static NativePreferencesState *state;
static Host host;
static Instance instances[4];

static void increment(uint32_t *word){if(state)__atomic_add_fetch(word,1u,__ATOMIC_RELAXED);}
static void failure(Failure why){if(state){increment(&state->failures);__atomic_store_n(&state->last_failure,(uint32_t)why,__ATOMIC_RELAXED);}}
static uint32_t frame_word(const Context *c,unsigned offset){uint32_t value;memcpy(&value,(const unsigned char*)c+sizeof(*c)+offset,sizeof(value));return value;}
static void set_frame_word(Context *c,unsigned offset,uint32_t value){memcpy((unsigned char*)c+sizeof(*c)+offset,&value,sizeof(value));}
static float frame_float(const Context *c,unsigned offset){float value;memcpy(&value,(const unsigned char*)c+sizeof(*c)+offset,sizeof(value));return value;}
static Instance *available_instance(){for(auto &instance:instances)if(!instance.tab)return &instance;return nullptr;}
static Instance *overlay_instance(void *overlay){for(auto &instance:instances)if(instance.tab&&instance.overlay==overlay)return &instance;return nullptr;}
static Instance *tab_instance(void *tab){for(auto &instance:instances)if(instance.tab==tab)return &instance;return nullptr;}
static Instance *button_instance(void *button,unsigned *profile){for(auto &instance:instances)for(unsigned i=0;i<3;i++)if(instance.buttons[i]==button){*profile=i+1;return &instance;}return nullptr;}

static void refresh_selection(Instance *found) noexcept {
 if(!found)return;
 uint32_t status=state?__atomic_load_n(&state->status,__ATOMIC_ACQUIRE):NATIVE_PREFERENCES_IDLE;
 uint32_t saved=state?__atomic_load_n(&state->saved_profile,__ATOMIC_ACQUIRE):0;
 uint32_t selected=state?__atomic_load_n(&state->active_profile,__ATOMIC_ACQUIRE):0;
 if(status>NATIVE_PREFERENCES_RESTART_FAILED||saved>NATIVE_CONTROLLER_GENERIC||selected>NATIVE_CONTROLLER_GENERIC)selected=0;
 for(unsigned i=0;i<3;i++)if(found->buttons[i]){
  try { host.set_toggle(found->buttons[i],selected==i+1,0); }
  catch(...){failure(NP_UNEXPECTED_EXCEPTION);return;}
 }
 if(found->marker){
  try {
   if(selected)host.set_bounds(found->marker,340,36+(int)(selected-1)*72,160,56);
   host.set_visible(found->marker,selected!=0);
  }catch(...){failure(NP_UNEXPECTED_EXCEPTION);}
 }
}

extern "C" void native_preferences_button_clicked(void *button) noexcept {
 unsigned profile=0;
 if(!state||!button_instance(button,&profile)){failure(NP_BAD_CONTEXT);return;}
 increment(&state->clicks);
 __atomic_store_n(&state->request_profile,profile,__ATOMIC_RELAXED);
 __atomic_store_n(&state->status,NATIVE_PREFERENCES_PENDING,__ATOMIC_RELAXED);
 __atomic_add_fetch(&state->request_sequence,1u,__ATOMIC_RELEASE);
 (void)syscall(240,&state->request_sequence,1,0x7fffffff); /* ARM futex wake, shared page. */
}

extern "C" void native_preferences_tab_active(void *tab,int active) noexcept {
 Instance *found=tab_instance(tab);if(!found||!found->original_active){failure(NP_BAD_CONTEXT);return;}
 TabActive original=found->original_active;
 try { original(tab,active); }catch(...){failure(NP_UNEXPECTED_EXCEPTION);return;}
 if(!active)return;
 found=tab_instance(tab);if(!found)return; /* Stock callback may change lifetime. */
 refresh_selection(found);
}

static void destroy_button(Instance &instance,unsigned index) noexcept {
 void *button=instance.buttons[index];if(!button)return;
 try { host.tab_remove(*(void**)((unsigned char*)instance.tab+0x4c),button); }
 catch(...){failure(NP_TEARDOWN);}
 *(void**)button=(unsigned char*)host.button_vtable+8;
 try { host.button_dtor(button); }
 catch(...){failure(NP_TEARDOWN);}
 try { host.sized_delete(button,0x100); }
 catch(...){failure(NP_TEARDOWN);}
 increment(&state->buttons_destroyed);
 instance.buttons[index]=nullptr;
 if(instance.button_vtables[index]){
  try { host.sized_delete(instance.button_vtables[index],NATIVE_PREFERENCES_BUTTON_VTABLE_BYTES); }
  catch(...){failure(NP_TEARDOWN);}
  instance.button_vtables[index]=nullptr;
 }
}

static void destroy_marker(Instance &instance) noexcept {
 void *button=instance.marker;if(!button)return;
 try { host.tab_remove(*(void**)((unsigned char*)instance.tab+0x4c),button); }
 catch(...){failure(NP_TEARDOWN);}
 try { host.button_dtor(button); }
 catch(...){failure(NP_TEARDOWN);}
 try { host.sized_delete(button,0x100); }
 catch(...){failure(NP_TEARDOWN);}
 increment(&state->buttons_destroyed);instance.marker=nullptr;
}

extern "C" void native_preferences_tab_deleting_destructor(void *tab) noexcept {
 Instance *found=tab_instance(tab);
 if(!found){failure(NP_BAD_CONTEXT);return;}
 Instance instance=*found;
 for(unsigned i=0;i<3;i++)destroy_button(*found,i);
 destroy_marker(*found);
 *(void**)tab=(unsigned char*)host.tab_vtable+8;
 found->overlay=nullptr;found->tab=nullptr;found->tab_vtable=nullptr;found->original_active=nullptr;
 if(state){__atomic_sub_fetch(&state->live_tabs,1u,__ATOMIC_RELAXED);increment(&state->tabs_destroyed);}
 try { host.tab_dtor(tab); }
 catch(...){failure(NP_TEARDOWN);}
 try { host.sized_delete(tab,0x74); }
 catch(...){failure(NP_TEARDOWN);}
 if(instance.tab_vtable){
  try { host.sized_delete(instance.tab_vtable,NATIVE_PREFERENCES_TAB_VTABLE_BYTES); }
  catch(...){failure(NP_TEARDOWN);}
 }
}

static void configure_host(uint32_t bias){
 host.allocate=(Allocate)(uintptr_t)(bias+0x037fa0e0);
 host.sized_delete=(SizedDelete)(uintptr_t)(bias+0x037fa32c);
 host.std_string_ctor=(StdStringCtor)(uintptr_t)(bias+0x03665c60);
 host.tab_ctor=(NotImplementedCtor)(uintptr_t)(bias+0x0369d174);
 host.tab_dtor=(CompleteDestructor)(uintptr_t)(bias+0x0369cdec);
 host.tab_add=(TabChild)(uintptr_t)(bias+0x036a04d4);
 host.tab_remove=(TabChild)(uintptr_t)(bias+0x036a047c);
 host.juce_string_ctor=(JuceStringCtor)(uintptr_t)(bias+0x00961120);
 host.juce_string_dtor=(JuceStringDestructor)(uintptr_t)(bias+0x00911f58);
 host.button_ctor=(TextButtonCtor)(uintptr_t)(bias+0x00bd2cec);
 host.button_dtor=(CompleteDestructor)(uintptr_t)(bias+0x00be19e0);
 host.set_bounds=(ComponentSetBounds)(uintptr_t)(bias+0x00ba8680);
 host.set_visible=(ComponentSetVisible)(uintptr_t)(bias+0x00bbf2fc);
 host.set_toggle=(ButtonSetToggle)(uintptr_t)(bias+0x00bbba14);
 host.tab_vtable=(const unsigned char*)(uintptr_t)(bias+0x069cd0b4);
 host.button_vtable=(const unsigned char*)(uintptr_t)(bias+0x068905a0);
}

static int insert_tab(Context *c){
 void *tab=nullptr,*new_vector=nullptr,*tab_table=nullptr,*buttons[3]={},*button_tables[3]={},*marker=nullptr;uint32_t new_vector_bytes=0;TabActive original_active=nullptr;
 bool tab_constructed=false,button_constructed[3]={},button_added[3]={},marker_constructed=false,marker_added=false,juce_string=false;
 alignas(8) unsigned char label[24]={},button_name[4]={};
 static const char *const names[3]={"X-TOUCH","X-TOUCH MINI","GENERIC MCU"};
 Instance *instance=nullptr;
 try {
  uint32_t overlay=c->r[11];
  if(!overlay||frame_word(c,0x68)!=overlay+0x160){failure(NP_BAD_CONTEXT);return 0;}
  if(overlay_instance((void*)(uintptr_t)overlay))return 1;
  uint32_t *vector=(uint32_t*)(uintptr_t)(overlay+0x1b8);
  uint32_t begin=vector[0],end=vector[1],cap=vector[2];
  if((!begin&&(end||cap))||begin>end||end>cap||((begin|end|cap)&3)||(begin&&(end-begin)/4>64)){failure(NP_BAD_VECTOR);return 0;}
  instance=available_instance();if(!instance){failure(NP_NO_SLOT);return 0;}
  uint32_t count=begin?(end-begin)/4:0;
  if(!begin||cap-end<8){new_vector_bytes=(count+2)*4;new_vector=host.allocate(new_vector_bytes);if(!new_vector)throw NP_ALLOCATION;if(count)memcpy(new_vector,(void*)(uintptr_t)begin,count*4);}
  tab=host.allocate(0x74);if(!tab)throw NP_ALLOCATION;
  host.std_string_ctor(label,"CONTROLLERS");
  uint32_t scroll=*(uint32_t*)(uintptr_t)(overlay+0x1a4),parent=*(uint32_t*)(uintptr_t)(overlay+0x1a8);
  if(!scroll||!parent||parent>0xffffffffu-8u)throw NP_BAD_CONTEXT;
  uint32_t index=frame_word(c,0x5c);
  host.tab_ctor(tab,index,(void*)(uintptr_t)scroll,(void*)(uintptr_t)(parent+8),
   frame_float(c,0x198),frame_float(c,0x194),frame_float(c,0x1a0),frame_float(c,0x1a4),frame_float(c,0x19c),
   *(float*)(uintptr_t)(overlay+0x1c8),label,(void*)(uintptr_t)(overlay+0x160),(void*)(uintptr_t)frame_word(c,0x50));
  tab_constructed=true;
  tab_table=host.allocate(NATIVE_PREFERENCES_TAB_VTABLE_BYTES);if(!tab_table)throw NP_ALLOCATION;
  for(unsigned i=0;i<3;i++){
   buttons[i]=host.allocate(0x100);button_tables[i]=host.allocate(NATIVE_PREFERENCES_BUTTON_VTABLE_BYTES);
   if(!buttons[i]||!button_tables[i])throw NP_ALLOCATION;
   host.juce_string_ctor(button_name,names[i]);juce_string=true;
   host.button_ctor(buttons[i],button_name);button_constructed[i]=true;
   host.juce_string_dtor(button_name);juce_string=false;
   memcpy(button_tables[i],host.button_vtable,NATIVE_PREFERENCES_BUTTON_VTABLE_BYTES);
   ((void**)button_tables[i])[2+46]=(void*)(uintptr_t)&native_preferences_button_clicked;
   *(void**)buttons[i]=(unsigned char*)button_tables[i]+8;
   host.tab_add(*(void**)((unsigned char*)tab+0x4c),buttons[i]);button_added[i]=true;
   host.set_bounds(buttons[i],36,36+(int)i*72,280,56);
   host.set_visible(buttons[i],1);
  }
  marker=host.allocate(0x100);if(!marker)throw NP_ALLOCATION;
  host.juce_string_ctor(button_name,"SELECTED");juce_string=true;
  host.button_ctor(marker,button_name);marker_constructed=true;
  host.juce_string_dtor(button_name);juce_string=false;
  host.tab_add(*(void**)((unsigned char*)tab+0x4c),marker);marker_added=true;
  host.set_bounds(marker,340,36,160,56);host.set_visible(marker,0);
  memcpy(tab_table,host.tab_vtable,NATIVE_PREFERENCES_TAB_VTABLE_BYTES);
  original_active=((TabActive*)tab_table)[4];
  ((void**)tab_table)[4]=(void*)(uintptr_t)&native_preferences_tab_active;
  ((void**)tab_table)[3]=(void*)(uintptr_t)&native_preferences_tab_deleting_destructor;
  *(void**)tab=(unsigned char*)tab_table+8;
  instance->overlay=(void*)(uintptr_t)overlay;instance->tab=tab;instance->tab_vtable=tab_table;instance->original_active=original_active;
  for(unsigned i=0;i<3;i++){instance->buttons[i]=buttons[i];instance->button_vtables[i]=button_tables[i];}
  instance->marker=marker;
  /* The host may select a newly appended tab without invoking its activation
   * callback. Seed the visible state now; later entries refresh through the
   * stock-first activation hook above. */
  refresh_selection(instance);
  if(new_vector){
   uint32_t *next=(uint32_t*)new_vector;next[count]=(uint32_t)(uintptr_t)tab;
   vector[0]=(uint32_t)(uintptr_t)next;vector[1]=(uint32_t)(uintptr_t)(next+count+1);vector[2]=(uint32_t)(uintptr_t)(next+count+2);
   if(begin)host.sized_delete((void*)(uintptr_t)begin,cap-begin);
  }else{*(uint32_t*)(uintptr_t)end=(uint32_t)(uintptr_t)tab;vector[1]=end+4;}
  set_frame_word(c,0x5c,index+1);
  if(state){increment(&state->tabs_created);__atomic_add_fetch(&state->buttons_created,4u,__ATOMIC_RELAXED);increment(&state->live_tabs);state->last_overlay=overlay;state->last_tab=(uint32_t)(uintptr_t)tab;state->last_button=(uint32_t)(uintptr_t)buttons[0];for(unsigned i=0;i<3;i++)state->buttons[i]=(uint32_t)(uintptr_t)buttons[i];}
  return 1;
 }catch(Failure why){failure(why);}catch(...){failure(NP_UNEXPECTED_EXCEPTION);}
 if(juce_string)try{host.juce_string_dtor(button_name);}catch(...){failure(NP_TEARDOWN);}
 for(unsigned i=0;i<3;i++){
  if(button_added[i]&&tab_constructed)try{host.tab_remove(*(void**)((unsigned char*)tab+0x4c),buttons[i]);}catch(...){failure(NP_TEARDOWN);}
  if(button_constructed[i]){*(void**)buttons[i]=(unsigned char*)host.button_vtable+8;try{host.button_dtor(buttons[i]);}catch(...){failure(NP_TEARDOWN);}}
  if(buttons[i])try{host.sized_delete(buttons[i],0x100);}catch(...){failure(NP_TEARDOWN);}
  if(button_tables[i])try{host.sized_delete(button_tables[i],NATIVE_PREFERENCES_BUTTON_VTABLE_BYTES);}catch(...){failure(NP_TEARDOWN);}
 }
 if(marker_added&&tab_constructed)try{host.tab_remove(*(void**)((unsigned char*)tab+0x4c),marker);}catch(...){failure(NP_TEARDOWN);}
 if(marker_constructed)try{host.button_dtor(marker);}catch(...){failure(NP_TEARDOWN);}
 if(marker)try{host.sized_delete(marker,0x100);}catch(...){failure(NP_TEARDOWN);}
 if(tab_constructed)try{host.tab_dtor(tab);}catch(...){failure(NP_TEARDOWN);}
 if(tab)try{host.sized_delete(tab,0x74);}catch(...){failure(NP_TEARDOWN);}
 if(tab_table)try{host.sized_delete(tab_table,NATIVE_PREFERENCES_TAB_VTABLE_BYTES);}catch(...){failure(NP_TEARDOWN);}
 if(new_vector)try{host.sized_delete(new_vector,new_vector_bytes);}catch(...){failure(NP_TEARDOWN);}
 return 0;
}

}

extern "C" void native_preferences_set_state(NativePreferencesState *mapped){state=mapped;}
extern "C" void native_preferences_configure_host(uint32_t bias){configure_host(bias);}

extern "C" void native_preferences_hook(void *saved,unsigned) {
 if(!__atomic_load_n(&observer_running,__ATOMIC_ACQUIRE)||!saved)return;
 increment(&state->hook_entries);(void)insert_tab((Context*)saved);
}
