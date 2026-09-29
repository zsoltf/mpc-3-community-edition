/* ARM composition test for the exact production tab/button helper. Host MPC
 * constructors and the overlay storage are explicit substitutes. */
#include "native-preferences.cc"

extern "C" void *malloc(size_t);
extern "C" void free(void *);
extern "C" int printf(const char *,...);
extern "C" void exit(int);
extern "C" int strcmp(const char *,const char *);
extern "C" void *memset(void *,int,size_t) noexcept;

extern "C" { uint32_t observer_running=1; }
static unsigned allocation_calls,delete_calls,tab_ctor_calls,tab_dtor_calls,button_ctor_calls,button_dtor_calls,add_calls,remove_calls,bounds_calls,visible_calls,active_calls,toggle_calls;
static unsigned fail_allocation;
static unsigned char content[0x220],tab_vtable_fixture[NATIVE_PREFERENCES_TAB_VTABLE_BYTES],button_vtable_fixture[NATIVE_PREFERENCES_BUTTON_VTABLE_BYTES];
static const char *juce_text;
static void *expected_selected;
static void *attached_buttons[4];static bool button_visible[4];static int button_bounds[4][4];static unsigned attached_count;
static bool button_selected[3],active_original_returned;
struct RegisteredTab {void *tab;uint32_t index;char label[16];};
static RegisteredTab registered[2];static unsigned registered_count;

static void require(bool ok,const char *why){if(!ok){printf("FAIL %s\n",why);exit(1);}}
static void *test_allocate(uint32_t bytes){allocation_calls++;if(fail_allocation&&allocation_calls==fail_allocation)throw NP_ALLOCATION;void *p=malloc(bytes);if(p)memset(p,0,bytes);return p;}
static void test_delete(void *p,uint32_t){delete_calls++;free(p);}
static void test_std_string(void *storage,const char *text){require(!strcmp(text,"CONTROLLERS"),"tab label");uint32_t *s=(uint32_t*)storage;s[0]=(uint32_t)(uintptr_t)((unsigned char*)storage+8);s[1]=11;memcpy((unsigned char*)storage+8,text,12);}
static float expected_float(unsigned n){return 0.25f+(float)n;}
static void register_tab(void *tab,uint32_t index,const char *label){
 require(registered_count<2,"bounded tab registry");RegisteredTab &entry=registered[registered_count++];entry.tab=tab;entry.index=index;
 size_t length=0;while(label[length])length++;require(length<sizeof(entry.label),"bounded registered label");memcpy(entry.label,label,length+1);
}
static void test_tab_ctor(void *tab,uint32_t index,void *scroll,void *parent,float a,float b,float c,float d,float e,float f,void *label,void *selected,void *icon){
 float values[6]={a,b,c,d,e,f};for(unsigned n=0;n<6;n++)require(values[n]==expected_float(n),"hard-float constructor argument");
 require(index==7&&scroll==(void*)0x11110000&&parent==(void*)0x22220008&&selected==expected_selected&&icon==(void*)0x44440000,"tab integer/stack constructor arguments");
 require(!strcmp(*(const char**)label,"CONTROLLERS"),"inline tab string layout");
 register_tab(tab,index,*(const char**)label);*(void**)tab=tab_vtable_fixture+8;*(void**)((unsigned char*)tab+0x4c)=content;tab_ctor_calls++;
}
static void test_tab_dtor(void *tab){require(*(void**)tab==tab_vtable_fixture+8,"tab vptr restored before complete destructor");tab_dtor_calls++;}
static void test_tab_active(void *tab,int active){require(tab&&((active==0)||(active==1))&&!active_original_returned,"stock tab activation called once and first");active_original_returned=true;active_calls++;}
static void *test_add(void *owner,void *child){require(owner==content&&child&&attached_count<4,"button parent add");attached_buttons[attached_count++]=child;add_calls++;return owner;}
static void *test_remove(void *owner,void *child){require(owner==content&&child,"button parent remove");unsigned found=4;for(unsigned i=0;i<4;i++)if(attached_buttons[i]==child)found=i;require(found<4,"remove attached button");attached_buttons[found]=nullptr;remove_calls++;return owner;}
static void test_juce_string(void *storage,const char *text){require(!strcmp(text,"X-TOUCH")||!strcmp(text,"X-TOUCH MINI")||!strcmp(text,"GENERIC MCU")||!strcmp(text,"SELECTED"),"button name");*(const char**)storage=text;juce_text=text;}
static void test_juce_string_dtor(void *storage){require(*(const char**)storage==juce_text,"button name lifetime");juce_text=nullptr;}
static void test_button_ctor(void *button,const void *name){require(*(const char*const*)name,"named TextButton constructor");*(void**)button=button_vtable_fixture+8;button_ctor_calls++;}
static void test_button_dtor(void *button){require(*(void**)button==button_vtable_fixture+8,"button vptr restored before complete destructor");button_dtor_calls++;}
static unsigned attached_index(void *button){for(unsigned i=0;i<4;i++)if(attached_buttons[i]==button)return i;return 4;}
static void test_set_bounds(void *button,int x,int y,int width,int height){unsigned i=attached_index(button);require(i<4,"only attached button receives bounds");button_bounds[i][0]=x;button_bounds[i][1]=y;button_bounds[i][2]=width;button_bounds[i][3]=height;bounds_calls++;}
static void test_set_visible(void *button,int visible){unsigned i=attached_index(button);require(i<4,"only attached button becomes visible");button_visible[i]=visible!=0;visible_calls++;}
static void test_set_toggle(void *button,int selected,int notification){unsigned i=attached_index(button);require((!active_calls||active_original_returned)&&i<3&&(selected==0||selected==1)&&notification==0,"selection initialization or refresh uses no notification");button_selected[i]=selected!=0;toggle_calls++;}

struct Frame {Context context;unsigned char stack[0x200];};
static void setup_frame(Frame *frame,unsigned char *overlay){
 memset(frame,0,sizeof(*frame));frame->context.r[11]=(uint32_t)(uintptr_t)overlay;
 for(unsigned n=0;n<6;n++){float poison=100.0f+(float)n;memcpy((unsigned char*)frame->context.d+n*4,&poison,4);}
 const unsigned offsets[5]={0x198,0x194,0x1a0,0x1a4,0x19c};
 for(unsigned n=0;n<5;n++){float value=expected_float(n);memcpy(frame->stack+offsets[n],&value,4);}
 float sixth=expected_float(5);memcpy(overlay+0x1c8,&sixth,4);
 *(uint32_t*)(frame->stack+0x68)=(uint32_t)(uintptr_t)(overlay+0x160);*(uint32_t*)(frame->stack+0x50)=0x44440000;*(uint32_t*)(frame->stack+0x5c)=7;
}

static RegisteredTab *row(uint32_t index){for(unsigned n=0;n<registered_count;n++)if(registered[n].index==index)return &registered[n];return nullptr;}

int main(){
 NativePreferencesState fixture={};state=&fixture;
 host={test_allocate,test_delete,test_std_string,test_tab_ctor,test_tab_dtor,test_add,test_remove,test_juce_string,test_juce_string_dtor,test_button_ctor,test_button_dtor,test_set_bounds,test_set_visible,test_set_toggle,tab_vtable_fixture,button_vtable_fixture};
 ((void**)tab_vtable_fixture)[2]=(void*)(uintptr_t)test_tab_dtor;
 ((void**)tab_vtable_fixture)[4]=(void*)(uintptr_t)test_tab_active;
 ((void**)button_vtable_fixture)[2]=(void*)(uintptr_t)test_button_dtor;
 alignas(8) unsigned char overlay[0x200]={};*(uint32_t*)(overlay+0x1a4)=0x11110000;*(uint32_t*)(overlay+0x1a8)=0x22220000;
 uint32_t *old=(uint32_t*)test_allocate(4);old[0]=0x55550000;*(uint32_t*)(overlay+0x1b8)=(uint32_t)(uintptr_t)old;*(uint32_t*)(overlay+0x1bc)=(uint32_t)(uintptr_t)(old+1);*(uint32_t*)(overlay+0x1c0)=(uint32_t)(uintptr_t)(old+1);
 Frame frame;expected_selected=overlay+0x160;setup_frame(&frame,overlay);native_preferences_hook(&frame.context,0);
 require(fixture.hook_entries==1&&fixture.tabs_created==1&&fixture.buttons_created==4&&fixture.live_tabs==1&&!fixture.failures,"successful production hook state");
 require(toggle_calls==3&&!button_selected[0]&&!button_selected[1]&&!button_selected[2],"new tab initializes its selection even if the host omits activation");
 require(attached_buttons[0]==(void*)(uintptr_t)fixture.last_button&&attached_count==4&&bounds_calls==4&&visible_calls==5&&!button_visible[3],"three choices and a hidden selection marker are attached and bounded");
 for(unsigned i=0;i<3;i++)require(fixture.buttons[i]==(uint32_t)(uintptr_t)attached_buttons[i]&&button_visible[i]&&button_bounds[i][0]==36&&button_bounds[i][1]==36+(int)i*72&&button_bounds[i][2]==280&&button_bounds[i][3]==56,"choice button bounds stay within tab content");
 require(*(uint32_t*)(frame.stack+0x5c)==8,"custom tab consumes current selection index before stock Splice");
 uint32_t begin=*(uint32_t*)(overlay+0x1b8),end=*(uint32_t*)(overlay+0x1bc),cap=*(uint32_t*)(overlay+0x1c0);require((end-begin)==8&&(cap-end)==4,"reserve two slots then append custom tab");
 uint32_t *items=(uint32_t*)(uintptr_t)begin;require(items[0]==0x55550000&&items[1]==fixture.last_tab,"existing tab order and custom append");
 native_preferences_hook(&frame.context,0);require(fixture.hook_entries==2&&fixture.tabs_created==1&&fixture.buttons_created==4&&fixture.live_tabs==1,"cached overlay hook cannot duplicate custom controls");
 void *splice=(void*)0x66660000;register_tab(splice,*(uint32_t*)(frame.stack+0x5c),"SPLICE");
 *(uint32_t*)(uintptr_t)end=(uint32_t)(uintptr_t)splice;*(uint32_t*)(overlay+0x1bc)=end+4;require(*(uint32_t*)(overlay+0x1bc)==cap,"stock Splice append needs no allocation");
 RegisteredTab *controllers=row(7),*stock=row(8);
 require(controllers&&controllers->tab==(void*)(uintptr_t)items[1]&&!strcmp(controllers->label,"CONTROLLERS"),"CONTROLLERS row names and selects custom tab");
 require(stock&&stock->tab==(void*)(uintptr_t)items[2]&&!strcmp(stock->label,"SPLICE"),"shifted stock row names and selects Splice tab");
 void *tab=(void*)(uintptr_t)fixture.last_tab;void **tab_slots=*(void***)tab;auto activate=(void(*)(void*,int))tab_slots[2];
 active_original_returned=false;activate(tab,0);require(active_calls==1&&toggle_calls==3,"inactive callback delegates to stock without changing selection");
 fixture.saved_profile=fixture.active_profile=NATIVE_CONTROLLER_XTOUCH;fixture.status=NATIVE_PREFERENCES_ACTIVE;active_original_returned=false;activate(tab,1);require(toggle_calls==6&&button_selected[0]&&!button_selected[1]&&!button_selected[2]&&button_visible[3]&&button_bounds[3][1]==36,"activation marks only confirmed active X-Touch");
 for(unsigned i=0;i<3;i++){void *button=(void*)(uintptr_t)fixture.buttons[i];void **button_slots=*(void***)button;auto click=(void(*)(void*))button_slots[46];click(button);require(fixture.clicks==i+1&&fixture.request_sequence==i+1&&fixture.request_profile==i+1&&fixture.status==NATIVE_PREFERENCES_PENDING,"production clicked slot atomically publishes its bounded controller request");}
 active_original_returned=false;activate(tab,1);require(toggle_calls==9&&button_selected[0]&&!button_selected[2],"pending Generic request does not optimistically replace confirmed active selection");
 fixture.status=NATIVE_PREFERENCES_AMBIGUOUS;active_original_returned=false;activate(tab,1);require(toggle_calls==12&&button_selected[0]&&!button_selected[2],"rejected Generic request retains the actual active X-Touch indication");
 fixture.saved_profile=fixture.active_profile=NATIVE_CONTROLLER_GENERIC;fixture.status=NATIVE_PREFERENCES_ACTIVE;active_original_returned=false;activate(tab,1);require(toggle_calls==15&&!button_selected[0]&&!button_selected[1]&&button_selected[2]&&button_visible[3]&&button_bounds[3][1]==180,"confirmed owner result selects only Generic and moves its marker");
 auto destroy=(void(*)(void*))tab_slots[1];destroy(tab);
 require(fixture.tabs_destroyed==1&&fixture.buttons_destroyed==4&&!fixture.live_tabs&&!fixture.failures,"production teardown counters");
 require(tab_ctor_calls==1&&button_ctor_calls==4&&add_calls==4&&remove_calls==4&&bounds_calls==8&&visible_calls==9&&active_calls==5&&toggle_calls==15&&button_dtor_calls==4&&tab_dtor_calls==1,"host construction/presentation/activation/teardown calls");
 test_delete((void*)(uintptr_t)begin,cap-begin);
 alignas(8) unsigned char rejected[0x200]={};*(uint32_t*)(rejected+0x1a4)=0x11110000;*(uint32_t*)(rejected+0x1a8)=0x22220000;expected_selected=rejected+0x160;setup_frame(&frame,rejected);*(uint32_t*)(frame.stack+0x68)=0;native_preferences_hook(&frame.context,0);
 require(fixture.hook_entries==3&&fixture.failures==1&&fixture.last_failure==NP_BAD_CONTEXT&&fixture.tabs_created==1,"invalid source context is inert");
 printf("PASS production native Preferences helper: pre-Splice CONTROLLERS routing, exact stack arguments, three choices plus explicit moving selection marker, initial and stock-first activation refresh from confirmed owner state, bounded atomic request publication, rejection retention, cached-overlay deduplication, and destructor composition (MPC constructors/rendering/native destruction substituted)\n");
 return 0;
}
