/* Open the native Add Track selector only. Human selection runs through MPC's
 * normal command/history path after this invocation returns. No menu navigation,
 * track factory call, retained popup pointer or recurring observer is added. */
typedef struct {uint32_t tmi,pages,launcher;} NewTrackOpen;
static int new_track_open_roots(NewTrackOpen *n){
 if(!root||root>UINT32_MAX-0x840||!typed(root,0x68d7bf8))return 0;
 n->tmi=word(root+0x83c);n->pages=word(root+0x114);
 if(!typed(n->tmi,0x699efd0)||n->tmi>UINT32_MAX-0x190||word(n->tmi+0x124)!=root||!pointer(n->pages)||word(n->tmi+0x128)!=n->pages)return 0;
 n->launcher=word(n->tmi+0x188);
 return typed(n->launcher,0x69498a8)&&n->launcher<=UINT32_MAX-0x34;
}
static unsigned new_track_page_open(uint32_t pages,unsigned page){
 return ((unsigned (*)(void*,unsigned))(uintptr_t)(observer_image_bias+0x157507c))((void*)(uintptr_t)pages,page)!=0;
}
static unsigned new_track_open_native(Context *c,const CommandRequest *r,uint32_t editor){
 NewTrackOpen before,after;
 if(!new_track_open_roots(&before))return C_NATIVE_MEMBERSHIP;
 if(word(before.launcher+0x30)||new_track_page_open(before.pages,0x30a8)||new_track_page_open(before.pages,0x30f4))return C_BUSY;
 uint32_t fn[4]={0,0,0,0};
 ((void (*)(void*,unsigned,uint32_t*))(uintptr_t)(observer_image_bias+0x15762ec))((void*)(uintptr_t)before.pages,0x30f4,fn);
 if(fn[2])((void (*)(uint32_t*,uint32_t*,unsigned))(uintptr_t)fn[2])(fn,fn,3);
 uint32_t target=0,function=0;unsigned why=global_resolve(c,r,&target,&function);
 if(why)return why;
 if(target!=editor||!new_track_open_roots(&after)||before.tmi!=after.tmi||before.pages!=after.pages||before.launcher!=after.launcher||!new_track_page_open(after.pages,0x30f4))return C_SOURCE_CHANGED;
 return C_OK;
}
