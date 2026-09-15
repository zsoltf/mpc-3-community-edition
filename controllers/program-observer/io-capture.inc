/* Routing source projection runs on the existing UI command drain. Scalar
 * values below come from enrolled constructor/commit cells, never raw polls. */
#define IO_CONNECTORS 4u
#define IO_CHOICES 128u
typedef struct {_Atomic uint32_t id,selectable,text[IO_TEXT/4];} IOPortRow;
typedef struct {_Atomic uint32_t revision,count,valid;IOPortRow rows[IO_CHOICES];} IOPortCatalogue;
typedef struct {_Atomic uint32_t key,live,ready,binding,generation;IOPortCatalogue ports[2];} IOConnector;
typedef struct {uint32_t destination,owner;} IOSendDestination;
static _Atomic uint32_t io_destination_key,io_destination_ready,io_destination_generation,io_graph_generation;
static IOPortCatalogue io_send_catalogue;
static IOSendDestination io_send_destinations[IO_CHOICES];
static uint32_t io_send_generation,io_send_graph,io_send_track,io_send_owner,io_send_project;
static void io_graph_changed(void){unsigned n=atomic_load(&io_graph_generation);if(n==UINT32_MAX){atomic_store(&io_destination_ready,0);return;}atomic_store_explicit(&io_graph_generation,n+1,memory_order_release);}
static void io_destination_point(Context *c,unsigned site){
 uint32_t key=c->r[site==IO_DEST_READY?5:0],current=atomic_load(&io_destination_key);
 if(site==IO_DEST_BIRTH){if(token()!=command_owner)return;atomic_store(&io_destination_ready,0);atomic_store(&io_destination_key,key);io_graph_changed();return;}
 if(key!=current)return;
 atomic_store(&io_destination_ready,0);io_graph_changed();
 if(site==IO_DEST_DEATH||site==IO_DEST_DELETE){atomic_store(&io_destination_key,0);return;}
 if(token()!=command_owner)return;
 if(site==IO_DEST_READY&&typed(key,0x68ad25c)){unsigned n=atomic_load(&io_destination_generation);if(n!=UINT32_MAX){atomic_store(&io_destination_generation,n+1);atomic_store_explicit(&io_destination_ready,1,memory_order_release);}}
}
typedef struct {_Atomic uint32_t key,live,ready,binding,generation;IOPortCatalogue choices[2];} IOProgramConnector;
static IOProgramConnector io_program_connectors[IO_CONNECTORS];
static unsigned io_program_serial,io_pc_bound,io_ac_bound;
#define IO_BIND_FRAMES 8u
typedef struct {uint32_t key,sp,binding,generation;} IOProgramBindFrame;
typedef struct {unsigned depth;IOProgramBindFrame frames[IO_BIND_FRAMES];} IOProgramBinds;
static __thread IOProgramBinds io_program_binds[2] __attribute__((tls_model("initial-exec")));
static IOProgramConnector *io_program_copy(uint32_t key){for(unsigned i=0;i<IO_CONNECTORS;i++)if(io_program_connectors[i].live&&io_program_connectors[i].key==key)return io_program_connectors+i;return NULL;}
/* Bound reservation-loss retries and never adopt a changed generation.
 * An intervening foreign poison remains terminal for this lifetime. */
static unsigned io_program_advance(IOProgramConnector *n,unsigned previous){
 if(!previous||io_program_serial==UINT32_MAX)return 0;
 unsigned next=++io_program_serial;
 for(unsigned attempt=0;attempt<4;attempt++){
  unsigned expected=previous;
  if(atomic_compare_exchange_weak_explicit(&n->generation,&expected,next,memory_order_acq_rel,memory_order_acquire))return next;
  if(expected!=previous)return 0;
 }
 return 0;
}
static void io_string_catalogue(IOPortCatalogue *out,uint32_t property,int seed){
 unsigned rev=atomic_load(&out->revision);if((rev&1)||rev>UINT32_MAX-2){atomic_store(&out->valid,0);return;}
 atomic_store(&out->valid,0);atomic_store(&out->revision,rev+1);unsigned count=0,valid=1;uint32_t begin=0;
 if(!seed){begin=word(property+0x10);uint32_t end=word(property+0x14),cap=word(property+0x18);if(begin>end||end>cap||(end-begin)%4||(cap-begin)%4||(end-begin)/4>IO_CHOICES||(begin&&!pointer(begin)))valid=0;else count=(end-begin)/4;}
 for(unsigned i=0;valid&&i<count;i++){uint32_t text=word(begin+i*4);if(!pointer(text)||text>UINT32_MAX-IO_TEXT){valid=0;break;}char name[IO_TEXT]={0};for(unsigned j=0;j<IO_TEXT-1;j++){name[j]=*(const volatile char*)(uintptr_t)(text+j);if(!name[j])break;}atomic_store(&out->rows[i].id,i);atomic_store(&out->rows[i].selectable,1);for(unsigned j=0;j<IO_TEXT/4;j++){uint32_t w;memcpy(&w,name+j*4,4);atomic_store(&out->rows[i].text[j],w);}}
 atomic_store(&out->count,valid?count:0);atomic_store(&out->valid,valid);atomic_store_explicit(&out->revision,rev+2,memory_order_release);
}
static void io_program_point(Context *c,unsigned site){
 if(site==IO_STRING_CATALOGUE){uint32_t property=c->r[5];for(unsigned i=0;i<IO_CONNECTORS;i++){IOProgramConnector *n=io_program_connectors+i;if(n->live&&(property==n->key+0x534||property==n->key+0x60c)){unsigned audio=property==n->key+0x60c;if(token()!=command_owner){atomic_store(&n->choices[audio].valid,0);n->generation=0;n->ready=0;return;}io_string_catalogue(n->choices+audio,property,0);return;}}return;}
 unsigned birth=site==IO_PC_BIRTH||site==IO_AC_BIRTH,dead=site==IO_PC_DEATH||site==IO_PC_DELETE||site==IO_AC_DEATH||site==IO_AC_DELETE,entry=site==IO_PC_BIND||site==IO_AC_BIND;
 uint32_t key=c->r[birth||dead||entry?0:site==IO_PC_BOUND?5:4];IOProgramConnector *n=io_program_copy(key);
 if(dead){if(n){n->ready=0;n->live=0;}return;}
 /* A foreign native bind can outlive this hook. Only a new enrolled
  * connector lifetime may recover this zero-generation poison. */
 if(token()!=command_owner){if(n){n->generation=0;n->ready=0;}return;}
 if(birth){if(!n)for(unsigned i=0;i<IO_CONNECTORS;i++)if(!io_program_connectors[i].live){n=io_program_connectors+i;break;}if(!n||io_program_serial==UINT32_MAX)return;memset(n,0,sizeof(*n));n->key=key;n->live=1;n->generation=++io_program_serial;return;}
 if(site==IO_PC_BOUND||site==IO_AC_BOUND){
  unsigned audio=site==IO_AC_BOUND;IOProgramBinds *stack=io_program_binds+audio;
  if(!stack->depth)return;
  IOProgramBindFrame f=stack->frames[stack->depth-1];
  /* A nested or unobserved return cannot consume another invocation's frame. */
  if(f.key!=key||f.sp!=source_sp(c)+(audio?232:64))return;
  stack->depth--;
  if(n&&f.generation&&n->generation==f.generation&&n->binding==f.binding&&typed(key,audio?0x68a241c:0x68d87e4)&&word(key+0x30)==f.binding)n->ready=1;
  return;
 }
 if(!n)return;
 if(site==IO_PC_EMPTY||site==IO_AC_EMPTY){io_string_catalogue(n->choices+(site==IO_AC_EMPTY),0,1);return;}
 if(entry){
  n->ready=0;unsigned generation=io_program_advance(n,atomic_load_explicit(&n->generation,memory_order_acquire));if(!generation)return;n->binding=c->r[2];
  IOProgramBinds *stack=io_program_binds+(site==IO_AC_BIND);
  if(stack->depth==IO_BIND_FRAMES){
   /* Overflow cannot certify any enclosing invocation. Keep its return
    * ownership so unwinding recovers without admitting a stale bind. */
   for(unsigned i=0;i<stack->depth;i++)stack->frames[i].generation=0;
   return;
  }
  stack->frames[stack->depth++]=(IOProgramBindFrame){key,source_sp(c),n->binding,generation};
 }

}
static IOConnector io_connectors[IO_CONNECTORS];
static unsigned io_connector_serial,io_bound_generation;
static __thread uint32_t io_binding_key __attribute__((tls_model("initial-exec")));
static __thread uint32_t io_binding_sp __attribute__((tls_model("initial-exec")));
static __thread uint32_t io_binding_generation __attribute__((tls_model("initial-exec")));
static IOConnector *io_connector_copy(uint32_t key){for(unsigned i=0;i<IO_CONNECTORS;i++)if(atomic_load_explicit(&io_connectors[i].key,memory_order_acquire)==key&&atomic_load_explicit(&io_connectors[i].live,memory_order_acquire))return io_connectors+i;return NULL;}
static void io_catalogue_copy(IOPortCatalogue *out,uint32_t property,int seed){
 unsigned revision=atomic_load(&out->revision);if((revision&1)||revision>UINT32_MAX-2){atomic_store(&out->valid,0);return;}
 atomic_store(&out->revision,revision+1);atomic_thread_fence(memory_order_release);
 unsigned count=0,valid=1;uint32_t start=0;
 if(!seed){start=word(property+0x28);uint32_t end=word(property+0x2c),cap=word(property+0x30);if(start>end||end>cap||(end-start)%12||(cap-start)%12||(end-start)/12>IO_CHOICES||(start&&!pointer(start)))valid=0;else count=(end-start)/12;}
 for(unsigned i=0;valid&&i<count;i++){
  uint32_t row=start+i*12,text=word(row+4);unsigned selectable=*(const volatile unsigned char*)(uintptr_t)(row+8);
  if(text<4096||text>UINT32_MAX-IO_TEXT||selectable>1){valid=0;break;}
  char name[IO_TEXT]={0};for(unsigned j=0;j<IO_TEXT-1;j++){name[j]=*(const volatile char*)(uintptr_t)(text+j);if(!name[j])break;}
  atomic_store(&out->rows[i].id,word(row));atomic_store(&out->rows[i].selectable,selectable);
  for(unsigned j=0;j<IO_TEXT/4;j++){uint32_t w;memcpy(&w,name+j*4,4);atomic_store(&out->rows[i].text[j],w);}
 }
 atomic_store(&out->count,valid?count:0);atomic_store(&out->valid,valid);atomic_store_explicit(&out->revision,revision+2,memory_order_release);
}
static void io_connector_point(Context *c,unsigned site){
 uint32_t key=c->r[site==IO_CONNECTOR_BIRTH||site==IO_CONNECTOR_DEATH||site==IO_CONNECTOR_DELETE||site==IO_BIND_ENTER?0:4];
 if(site==IO_PORT_CATALOGUE){uint32_t property=c->r[5];for(unsigned i=0;i<IO_CONNECTORS;i++){IOConnector *n=io_connectors+i;uint32_t k=atomic_load(&n->key);if(atomic_load(&n->live)&&(property==k+0x9bc||property==k+0x9f0)){if(token()!=command_owner){atomic_store(&n->ports[property==k+0x9f0].valid,0);atomic_store(&n->ready,0);return;}io_catalogue_copy(n->ports+(property==k+0x9f0),property,0);return;}}return;}
 IOConnector *n=io_connector_copy(key);
 if(site==IO_CONNECTOR_DEATH||site==IO_CONNECTOR_DELETE){if(n){atomic_store(&n->ready,0);atomic_store_explicit(&n->live,0,memory_order_release);}return;}
 if(token()!=command_owner){if(n)atomic_store(&n->ready,0);return;}
 if(site==IO_CONNECTOR_BIRTH){
  if(!pointer(key))return;
  if(n){atomic_store(&n->live,0);atomic_store(&n->ready,0);}
  if(!n)for(unsigned i=0;i<IO_CONNECTORS;i++)if(!atomic_load(&io_connectors[i].live)){n=io_connectors+i;break;}
  if(!n||io_connector_serial==UINT32_MAX)return;
  memset(n,0,sizeof(*n));atomic_store(&n->generation,++io_connector_serial);atomic_store(&n->key,key);atomic_store_explicit(&n->live,1,memory_order_release);return;
 }
 if(!n)return;
 if(site==IO_INPUT_CATALOGUE_SEED||site==IO_OUTPUT_CATALOGUE_SEED){io_catalogue_copy(n->ports+(site==IO_OUTPUT_CATALOGUE_SEED),0,1);return;}
 if(site==IO_CONNECTOR_READY){if(typed(key,0x68a2710))atomic_store_explicit(&n->ready,1,memory_order_release);return;}
 if(site==IO_BIND_ENTER){atomic_store(&n->ready,0);if(io_binding_key||io_connector_serial==UINT32_MAX)return;atomic_store(&n->generation,++io_connector_serial);atomic_store(&n->binding,c->r[2]);io_binding_key=key;io_binding_sp=source_sp(c);io_binding_generation=atomic_load(&n->generation);return;}
 if(site==IO_BIND_DONE){if(io_binding_key==key&&io_binding_sp==source_sp(c)+64&&io_binding_generation==atomic_load(&n->generation)&&typed(key,0x68a2710)&&word(key+0x30)==atomic_load(&n->binding))atomic_store_explicit(&n->ready,1,memory_order_release);io_binding_key=io_binding_sp=io_binding_generation=0;}
}
/* Returns a copied native row only; unavailable rows stay visible but cannot
 * become an external choice. The native source property lock owns writes. */
static int io_port_row(const IOPortCatalogue *p,unsigned row,uint32_t *id,unsigned *selectable,char text[IO_TEXT]){
 if(row>=atomic_load(&p->count)||row>=IO_CHOICES)return 0;
 *id=atomic_load(&p->rows[row].id);*selectable=atomic_load(&p->rows[row].selectable);
 for(unsigned j=0;j<IO_TEXT/4;j++){uint32_t w=atomic_load(&p->rows[row].text[j]);memcpy(text+j*4,&w,4);}return 1;
}
static IOCopy io_current;
static unsigned io_generation;
static __thread unsigned io_submission __attribute__((tls_model("initial-exec")));
static __thread unsigned io_setter_sp __attribute__((tls_model("initial-exec")));
static _Atomic uint32_t io_payload[COMMAND_SLOTS],io_finished[COMMAND_SLOTS];
static uint32_t io_source_frame[COMMAND_SLOTS],io_helper[COMMAND_SLOTS],io_queue_result[COMMAND_SLOTS];
typedef struct {uint32_t seq,payload,sp,lr;} IOCall;
static __thread IOCall io_call __attribute__((tls_model("initial-exec")));
static unsigned io_set_seen[COMMAND_SLOTS],io_done_seen[COMMAND_SLOTS],io_commit_seen[COMMAND_SLOTS],io_receipt[COMMAND_SLOTS],io_fault[COMMAND_SLOTS],io_audio_incarnation[COMMAND_SLOTS];
static void io_initialize(void){memset(&io_current,0,sizeof(io_current));io_generation=io_connector_serial=io_bound_generation=0;io_program_serial=io_pc_bound=io_ac_bound=0;memset(io_program_binds,0,sizeof(io_program_binds));memset(io_program_connectors,0,sizeof(io_program_connectors));io_binding_key=io_binding_sp=io_binding_generation=0;memset(io_connectors,0,sizeof(io_connectors));atomic_store(&io_destination_key,0);atomic_store(&io_destination_ready,0);atomic_store(&io_destination_generation,0);atomic_store(&io_graph_generation,0);memset(&io_send_catalogue,0,sizeof(io_send_catalogue));memset(io_send_destinations,0,sizeof(io_send_destinations));io_send_generation=io_send_graph=io_send_track=io_send_owner=io_send_project=0;io_submission=io_setter_sp=0;memset(&io_call,0,sizeof(io_call));memset(io_payload,0,sizeof(io_payload));memset(io_finished,0,sizeof(io_finished));memset(io_source_frame,0,sizeof(io_source_frame));memset(io_helper,0,sizeof(io_helper));memset(io_queue_result,0,sizeof(io_queue_result));memset(io_set_seen,0,sizeof(io_set_seen));memset(io_done_seen,0,sizeof(io_done_seen));memset(io_commit_seen,0,sizeof(io_commit_seen));memset(io_receipt,0,sizeof(io_receipt));memset(io_fault,0,sizeof(io_fault));memset(io_audio_incarnation,0,sizeof(io_audio_incarnation));}
static void io_publish(void){
 IOState *s=&mirror_state->io;unsigned rev=atomic_load(&s->revision);if(rev>UINT32_MAX-2){atomic_store(&s->error,IO_INVALID);return;}
 uint32_t words[IO_WORDS];memcpy(words,&io_current,sizeof(words));atomic_store(&s->revision,rev+1);atomic_thread_fence(memory_order_release);
 for(unsigned i=0;i<IO_WORDS;i++)atomic_store_explicit(s->words+i,words[i],memory_order_relaxed);
 atomic_store_explicit(&s->revision,rev+2,memory_order_release);
}
static Track *io_track_source(Context *c,const IODemand *d,unsigned selected){
 if(!d->enabled||d->epoch!=epoch||token()!=command_owner||!ready||loading||reset_pending||depth||!active()||!root||!project||!pool||!command_queues||c->r[0]!=command_queues+4||word(c->r[0]+0xf0)||(selected&&(atomic_load(&command_state->new_project_intent)||atomic_load(&command_state->stop_requested))))return NULL;
 if(!typed(root,0x68d7bf8)||word(root+0x24c)!=project||!typed(project,0x6933c4c)||word(project+0xa94)!=pool||!typed(pool,0x68ad2c4))return NULL;
 ChannelCell *selection=selected?channel_cell(project+0x78):NULL;if(selected&&(!selection||!atomic_load(&selection->live)))return NULL;
 Track *t=NULL;for(unsigned i=0;i<count;i++)if(tracks[i].serial==d->serial){if(t)return NULL;t=tracks+i;}
 if(!t||t->transition||!t->track_owner||!t->program_owner||(selected&&atomic_load(&selection->bits)!=t->track)||channel_owner_id(t->track)!=t->track_owner||channel_owner_id(t->program)!=t->program_owner||!vector(pool,0x34,t->track)||!typed(t->track,0x6935554)||word(t->track+0x648)!=t->program||word(t->track+0x64c)!=t->kind||!typed(word(project+0xa90),0x68ad07c)||!program_member(word(project+0xa90),t))return NULL;
 return t;
}
static Track *io_track(Context *c,const IODemand *d){return io_track_source(c,d,1);}
static int io_scalar(Track *t,unsigned tag,uint32_t property,IOField *out,uint32_t now){
 if(tag>=CHANNEL_FIELD_KINDS||atomic_load(&mirror_state->channel.errors[tag]))return 0;
 ChannelCell *cell=channel_cell(property);if(!cell)return 0;
 unsigned rev=atomic_load_explicit(&cell->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 uint32_t owner=channel_kind(tag)==CO_TRACK?t->track_owner:t->program_owner;
 if(!atomic_load(&cell->live)||atomic_load(&cell->field)!=tag||atomic_load(&cell->owner_incarnation)!=owner)return 0;
 uint32_t incarnation=atomic_load(&cell->incarnation),value=atomic_load(&cell->bits);
 atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load_explicit(&cell->revision,memory_order_relaxed))return 0;
 out->property=property;out->incarnation=incarnation;out->revision=rev;out->value=value;out->tick=now;return 1;
}
/* Borrow this chain only during the existing non-reentrant UI invocation.
 * No O/PC pointer or native String survives that invocation. */
static uint32_t io_connector(Track *t){
 uint32_t o=word(root+0x75c);if(!pointer(o)||o>UINT32_MAX-8)return 0;
 uint32_t pc=word(o+8);if(!pointer(pc)||pc>UINT32_MAX-0x1540)return 0;
 uint32_t c=pc+0x2f4;
 IOConnector *n=io_connector_copy(c);if(!n||!atomic_load(&n->ready)||!typed(c,0x68a2710)||word(c+0x30)!=t->track||word(c+0xa34)!=pool)return 0;
 return c;
}
static uint32_t io_program_native(Track *t,unsigned audio){
 uint32_t o=word(root+0x75c);if(!pointer(o)||o>UINT32_MAX-8)return 0;uint32_t pc=word(o+8);if(!pointer(pc)||pc>UINT32_MAX-0x1a00)return 0;
 uint32_t key=pc+(audio?0x12ec:0xd4c);IOProgramConnector *n=io_program_copy(key);
 unsigned generation=n?atomic_load_explicit(&n->generation,memory_order_acquire):0;
 if(!generation||!n->ready||!typed(key,audio?0x68a241c:0x68d87e4)||word(key+0x30)!=t->program||n->binding!=t->program)return 0;
 return n->live&&n->key==key&&n->ready&&atomic_load_explicit(&n->generation,memory_order_acquire)==generation?key:0;
}
static uint32_t io_destination_native(void){
 uint32_t d=atomic_load_explicit(&io_destination_key,memory_order_acquire);
 return d&&atomic_load_explicit(&io_destination_ready,memory_order_acquire)&&typed(d,0x68ad25c)&&word(project+0xad4)==d&&word(d+0x34)==pool?d:0;
}
static int io_send_seed(Track *t){
 uint32_t d=io_destination_native(),generation=atomic_load(&io_destination_generation),graph=atomic_load(&io_graph_generation),project_owner=channel_owner_id(project);
 if(!d)return 0;
 if(io_send_generation==generation&&io_send_graph==graph&&io_send_track==t->track&&io_send_owner==t->track_owner&&io_send_project==project_owner)return atomic_load(&io_send_catalogue.valid);
 atomic_store(&io_send_catalogue.valid,0);
 unsigned members=word(d+0x4c);if(members>=IO_CHOICES)return 0;
 /* The native builder owns labels and cycle eligibility. Bound its membership
  * before invocation; copied row lifetimes end with the exact native destructor. */
 uint32_t nodes[IO_CHOICES],keys[IO_CHOICES],destinations[IO_CHOICES],stack[IO_CHOICES];unsigned used=0,depth=0;uint32_t node=word(d+0x40);
 while(node||depth){while(node){if(!pointer(node)||node>UINT32_MAX-24||depth==IO_CHOICES)return 0;for(unsigned i=0;i<used;i++)if(nodes[i]==node)return 0;for(unsigned i=0;i<depth;i++)if(stack[i]==node)return 0;stack[depth++]=node;node=word(node+8);}node=stack[--depth];if(used>=members)return 0;nodes[used]=node;keys[used]=word(node+0x10);destinations[used]=word(node+0x14);if(!channel_owner_id(destinations[used])||!vector(pool,0x34,destinations[used]))return 0;used++;node=word(node+0xc);}
 if(used!=members)return 0;
 uint32_t native[3]={0,0,0};((void (*)(uint32_t*,void*,void*))(uintptr_t)(observer_image_bias+0x2699a90))(native,(void*)(uintptr_t)d,(void*)(uintptr_t)t->track);
 unsigned valid=1,count=0;uint32_t begin=native[0],end=native[1],cap=native[2];if(begin>end||end>cap||(end-begin)%12||(cap-begin)%12||(end-begin)/12!=members+1||!pointer(begin))valid=0;else count=(end-begin)/12;
 unsigned rev=atomic_load(&io_send_catalogue.revision);if((rev&1)||rev>UINT32_MAX-2)valid=0;atomic_store(&io_send_catalogue.revision,rev+1);
 for(unsigned i=0;valid&&i<count;i++){
  uint32_t row=begin+i*12,key=word(row),destination=0,owner=0,text=word(row+4);unsigned selectable=*(const volatile unsigned char*)(uintptr_t)(row+8);
  if(i==0){if(key!=0x7fffffff)valid=0;}else{unsigned matches=0;for(unsigned j=0;j<members;j++)if(keys[j]==key){destination=destinations[j];matches++;}if(matches!=1)valid=0;owner=channel_owner_id(destination);if(!owner)valid=0;}
  if(!pointer(text)||text>UINT32_MAX-IO_TEXT||selectable>1){valid=0;break;}
  char name[IO_TEXT]={0};for(unsigned j=0;j<IO_TEXT-1;j++){name[j]=*(const volatile char*)(uintptr_t)(text+j);if(!name[j])break;}
  atomic_store(&io_send_catalogue.rows[i].id,destination?destination+0x43c:0);atomic_store(&io_send_catalogue.rows[i].selectable,selectable);for(unsigned j=0;j<IO_TEXT/4;j++){uint32_t w;memcpy(&w,name+j*4,4);atomic_store(&io_send_catalogue.rows[i].text[j],w);}io_send_destinations[i]=(IOSendDestination){destination,owner};
 }
 ((void (*)(uint32_t*))(uintptr_t)(observer_image_bias+0x4c8140))(native);
 if(d!=io_destination_native()||generation!=atomic_load(&io_destination_generation)||graph!=atomic_load(&io_graph_generation)||channel_owner_id(t->track)!=t->track_owner||channel_owner_id(project)!=project_owner)valid=0;
 atomic_store(&io_send_catalogue.count,valid?count:0);atomic_store(&io_send_catalogue.valid,valid);atomic_store_explicit(&io_send_catalogue.revision,rev+2,memory_order_release);
 io_send_generation=generation;io_send_graph=graph;io_send_track=t->track;io_send_owner=t->track_owner;io_send_project=project_owner;return valid;
}
static unsigned io_send_preflight(Track *t,uint32_t destination){
 uint32_t d=io_destination_native();if(!d||!typed(t->track,0x6935554)||channel_owner_id(t->track)!=t->track_owner)return C_IDENTITY;
 if(((uint32_t (*)(void*,void*))(uintptr_t)(observer_image_bias+0x257eec4))((void*)(uintptr_t)pool,(void*)(uintptr_t)(t->track+0x468))!=t->track)return C_NATIVE_MEMBERSHIP;
 if(destination){uint32_t target=destination-0x43c;unsigned found=0;for(unsigned i=0;i<atomic_load(&io_send_catalogue.count);i++)if(io_send_destinations[i].destination==target&&io_send_destinations[i].owner==channel_owner_id(target))found++;if(found!=1||!typed(target,0x6935554)||!vector(pool,0x34,target)||((uint32_t (*)(void*,void*))(uintptr_t)(observer_image_bias+0x25765d4))((void*)(uintptr_t)d,(void*)(uintptr_t)(target+0x468))!=target)return C_NATIVE_MEMBERSHIP;}
 uint32_t visited[3]={0,0,0};unsigned cyclic=((unsigned (*)(void*,void*,uint32_t*))(uintptr_t)(observer_image_bias+0x245f088))((void*)(uintptr_t)(t->track+0x41c),(void*)(uintptr_t)destination,visited);
 if(visited[0])((void (*)(void*,unsigned))(uintptr_t)(observer_image_bias+0x4a66cc))((void*)(uintptr_t)visited[0],visited[2]-visited[0]);
 return cyclic?C_NATIVE_MEMBERSHIP:C_OK;
}
static int io_output_read(Track *t,IOField *f,uint32_t now){
 if(!command_mixable(t->program)||!io_scalar(t,CI_AUDIO_OUT,t->program+0x348,f,now))return 0;
 unsigned revision=f->revision;float value=((float (*)(void*,unsigned,unsigned))(uintptr_t)(observer_image_bias+0x250b754))((void*)(uintptr_t)t->program,0x101,325);
 IOField after={0};if(!(value>=0&&value<65536)||value!=(float)(unsigned)value||!io_scalar(t,CI_AUDIO_OUT,t->program+0x348,&after,now)||after.revision!=revision||after.incarnation!=f->incarnation||!command_mixable(t->program)||channel_owner_id(t->program)!=t->program_owner)return 0;
 f->value=(unsigned)value;return 1;
}
static int io_output_agrees(Track *t){
 uint32_t words[2][6];unsigned revisions[2];
 for(unsigned side=0;side<2;side++){ChannelCell *cell=channel_cell(t->program+(side?0x370:0x348));if(!cell||!atomic_load(&cell->live)||atomic_load(&cell->owner_incarnation)!=t->program_owner||atomic_load(&cell->field)!=(side?CI_AUDIO_OUT_AUDIO:CI_AUDIO_OUT)||atomic_load(&cell->length)!=24)return 0;unsigned rev=atomic_load_explicit(&cell->revision,memory_order_acquire);if(!rev||(rev&1))return 0;for(unsigned j=0;j<6;j++)words[side][j]=atomic_load(&cell->text[j]);atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load(&cell->revision))return 0;revisions[side]=rev;}
 return revisions[0]&&revisions[1]&&!memcmp(words[0],words[1],sizeof(words[0]));
}
static int io_audio_choice(Track *t,uint32_t connector,unsigned audio,unsigned row,unsigned *value){
 IOProgramConnector *n=io_program_copy(connector);if(!n||!n->ready||row>=atomic_load(&n->choices[audio].count)||!atomic_load(&n->choices[audio].valid))return 0;
 if(audio){uint32_t mapper=word(connector+0x6fc);if(!pointer(mapper)||mapper>UINT32_MAX-0x14)return 0;*value=((unsigned (*)(void*,unsigned))(uintptr_t)(observer_image_bias+0x269741c))((void*)(uintptr_t)mapper,row);}
 else{unsigned base=((unsigned (*)(void*))(uintptr_t)(observer_image_bias+0x26991b0))((void*)(uintptr_t)t->program);if(base>65535-row)return 0;*value=base+row;}
 return *value<65536;
}
static void io_service(Context *c){
 IODemand d;uint32_t now=atomic_load(&mirror_state->heartbeat);
 if(!io_demand_read(&command_state->io_interest,&d)||!d.enabled||d.until<=now){if(io_current.status!=IO_OFF){memset(&io_current,0,sizeof(io_current));io_publish();}return;}
 Track *t=io_track(c,&d);if(!t){io_current.status=IO_UNAVAILABLE;io_current.tick=now;io_publish();return;}
 uint32_t connector=io_connector(t);IOConnector *native=connector?io_connector_copy(connector):NULL;unsigned binding_generation=native?atomic_load(&native->generation):0;
 uint32_t pc=io_program_native(t,0),ac=io_program_native(t,1);unsigned pcgen=pc?io_program_copy(pc)->generation:0,acgen=ac?io_program_copy(ac)->generation:0;
 if(io_pc_bound!=pcgen||io_ac_bound!=acgen||io_bound_generation!=binding_generation||io_current.connector!=connector||io_current.epoch!=d.epoch||io_current.serial!=d.serial||io_current.track_owner!=t->track_owner||io_current.program_owner!=t->program_owner){
  unsigned generation=++io_generation;if(!generation){atomic_store(&mirror_state->io.error,IO_INVALID);return;}
  io_pc_bound=pcgen;io_ac_bound=acgen;io_bound_generation=binding_generation;memset(&io_current,0,sizeof(io_current));io_current.generation=generation;io_current.epoch=d.epoch;io_current.serial=d.serial;io_current.binding=t->binding;io_current.incarnation=t->incarnation;io_current.track_owner=t->track_owner;io_current.program_owner=t->program_owner;
 }
 io_current.connector=connector;io_current.program_connector=pc;io_current.audio_connector=ac;
 io_current.demand=d.revision;io_current.status=IO_READY;io_current.tick=now;
 IOField prior[IO_FIELDS];memcpy(prior,io_current.fields,sizeof(prior));
 for(unsigned i=0;i<IO_FIELDS;i++){IOField *f=io_current.fields+i;memset(f,0,sizeof(*f));f->status=IO_UNAVAILABLE;}
 /* Both native channel menus encode native0 as Any and native1..16 as the
  * numbered rows. The native widget puts Any after the sixteen channels. */
 const unsigned fields[]={IO_MIDI_IN_CHANNEL,IO_MIDI_OUT_CHANNEL};
 const unsigned tags[]={CI_MIDI_IN_CHANNEL,CI_MIDI_OUT_CHANNEL};
 const unsigned offsets[]={0x378,0x3d0};
 for(unsigned i=0;i<2;i++){
  IOField *f=io_current.fields+fields[i];if((i==0&&t->kind>5)||(i==1&&t->kind!=4)){f->status=IO_NOT_APPLICABLE;continue;}if(io_current.connector&&io_scalar(t,tags[i],t->track+offsets[i],f,now)&&f->value<=16){f->status=IO_READY;f->choice_generation=1;f->choice_count=17;f->choice_index=f->value?f->value-1:16;if(f->value){unsigned at=0;if(f->value>=10)f->text[at++]='1';f->text[at++]='0'+f->value%10;f->text[at]=0;}else memcpy(f->text,"Any",4);}
 }
 for(unsigned output=0;output<2;output++){
  unsigned field=output?IO_MIDI_OUT_PORT:IO_MIDI_IN_PORT;IOField *f=io_current.fields+field;
  if(output?t->kind!=4:t->kind>5){f->status=IO_NOT_APPLICABLE;continue;}
  if(!native||!io_scalar(t,output?CI_MIDI_OUT_PORT:CI_MIDI_IN_PORT,t->track+(output?0x3a4:0x364),f,now))continue;
  IOPortCatalogue *p=native->ports+output;unsigned rev=atomic_load_explicit(&p->revision,memory_order_acquire),count=atomic_load(&p->count);
  if(!rev||(rev&1)||!atomic_load(&p->valid)||count>IO_CHOICES)continue;
  const IOField *old=prior+field;
  if(old->status==IO_READY&&old->incarnation==f->incarnation&&old->revision==f->revision&&old->choice_generation==rev){f->choice_index=old->choice_index;memcpy(f->text,old->text,IO_TEXT);}
  else{
   unsigned matches=0;for(unsigned i=0;i<count;i++)if(atomic_load(&p->rows[i].id)==f->value){f->choice_index=i;matches++;}
   if(matches!=1)continue;
   uint32_t id;unsigned selectable;if(!io_port_row(p,f->choice_index,&id,&selectable,f->text)||id!=f->value)continue;
  }
  atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load(&p->revision))continue;
  f->choice_generation=rev;f->choice_count=count;f->status=IO_READY;
 }
 if(t->kind<=5){IOField *f=io_current.fields+IO_SEND_TO;if(connector&&io_scalar(t,CI_SEND_TO,t->track+0x420,f,now)&&io_send_seed(t)){IOPortCatalogue *p=&io_send_catalogue;unsigned count=atomic_load(&p->count),matches=0;for(unsigned i=0;i<count;i++)if(atomic_load(&p->rows[i].id)==f->value){f->choice_index=i;matches++;}uint32_t id;unsigned selectable;if(matches==1&&io_port_row(p,f->choice_index,&id,&selectable,f->text)){f->choice_generation=atomic_load(&p->revision);f->choice_count=count;f->status=IO_READY;}}}
 else io_current.fields[IO_SEND_TO].status=IO_NOT_APPLICABLE;
 if(t->kind<=5){IOField *f=io_current.fields+IO_MONITOR;if(connector&&io_scalar(t,CI_MIDI_MONITOR,t->track+0x400,f,now)&&f->value<4){static const char *names[]={"Off","In","Auto","Merge"};f->status=IO_READY;f->choice_generation=1;f->choice_count=4;f->choice_index=f->value;memcpy(f->text,names[f->value],strlen(names[f->value])+1);}}
 else if(t->kind==6){IOField *f=io_current.fields+IO_MONITOR;if(ac&&typed(t->program,0x692fec4)&&io_scalar(t,CI_AUDIO_MONITOR,t->program+0x26c8,f,now)&&f->value<4){static const char *names[]={"Off","In","Auto","Merge"};f->status=IO_READY;f->choice_generation=1;f->choice_count=4;f->choice_index=f->value;memcpy(f->text,names[f->value],strlen(names[f->value])+1);}}
 else io_current.fields[IO_MONITOR].status=IO_NOT_APPLICABLE;
 for(unsigned audio=0;audio<2;audio++){
  unsigned field=audio?IO_AUDIO_IN:IO_AUDIO_OUT;IOField *f=io_current.fields+field;uint32_t key=audio?ac:pc;
  if(audio?t->kind!=6:!(t->kind<=3||t->kind==6)){f->status=IO_NOT_APPLICABLE;continue;}
  IOProgramConnector *n=key?io_program_copy(key):NULL;if(!n)continue;IOPortCatalogue *cat=n->choices+audio;unsigned rev=atomic_load(&cat->revision),count=atomic_load(&cat->count);if(!rev||(rev&1)||!atomic_load(&cat->valid)||count>IO_CHOICES)continue;
  if(!io_scalar(t,audio?CI_AUDIO_IN:CI_AUDIO_OUT,t->program+(audio?0x2784:0x348),f,now))continue;
  const IOField *old=prior+field;
  if(old->status==IO_READY&&old->connector==key&&old->incarnation==f->incarnation&&old->revision==f->revision&&old->choice_generation==rev){f->value=old->value;f->choice_index=old->choice_index;memcpy(f->text,old->text,IO_TEXT);}
  else{
   if(!audio&&!io_output_read(t,f,now))continue;
   unsigned matches=0;for(unsigned row=0;row<count;row++){unsigned value;if(!io_audio_choice(t,key,audio,row,&value))break;if(value==f->value){f->choice_index=row;matches++;}}
   if(matches!=1)continue;
   uint32_t id;unsigned selectable;if(!io_port_row(cat,f->choice_index,&id,&selectable,f->text))continue;
  }
  if(rev!=atomic_load(&cat->revision)||io_program_native(t,audio)!=key)continue;
  f->choice_generation=rev;f->choice_count=count;f->status=IO_READY;
 }
 for(unsigned i=0;i<IO_FIELDS;i++)io_current.fields[i].connector=i==IO_AUDIO_OUT?pc:i==IO_AUDIO_IN||(i==IO_MONITOR&&t->kind==6)?ac:connector;
 io_publish();
}

static int io_receipted(unsigned at){return io_receipt[at]==accepted[at].request.seq;}
static unsigned io_resolve(Context *c,const CommandRequest *r,Track **track,uint32_t *connector,IOField *field){
 if(!command_io(r->reserved)||r->io_field>=IO_FIELDS||!command_mode_delta(r->bits)||!r->serial||!r->track_owner||!r->program_owner||!r->project_owner||!r->field_incarnation||!r->global_owner||!r->io_generation||!r->io_choice_generation||!r->io_connector)return C_FORMAT;
 if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)return C_IDENTITY;
 unsigned now=atomic_load(&mirror_state->heartbeat);if(r->expires<r->created||r->expires-r->created>120000||r->expires<now)return C_EXPIRED;if(r->created>now)return COMMAND_WAIT_HEARTBEAT;
 IODemand demand={.enabled=1,.epoch=r->epoch,.serial=r->serial};Track *t=io_track(c,&demand);
 if(!t||t->binding!=r->binding||t->incarnation!=r->incarnation||t->track_owner!=r->track_owner||t->program_owner!=r->program_owner||channel_owner_id(project)!=r->project_owner)return C_IDENTITY;
 uint32_t native=r->io_field==IO_AUDIO_OUT?io_program_native(t,0):r->io_field==IO_AUDIO_IN||(r->io_field==IO_MONITOR&&t->kind==6)?io_program_native(t,1):io_connector(t);if(!native||native!=r->io_connector)return C_NATIVE_MEMBERSHIP;
 if(io_current.status!=IO_READY||io_current.epoch!=r->epoch||io_current.serial!=r->serial||io_current.generation!=r->io_generation)return C_SOURCE_CHANGED;
 *field=io_current.fields[r->io_field];
 if(field->status!=IO_READY||field->property!=r->global_owner||field->incarnation!=r->field_incarnation||field->choice_generation!=r->io_choice_generation||!field->choice_count)return C_SOURCE_CHANGED;
 static const unsigned tags[IO_FIELDS]={CI_MIDI_MONITOR,CI_MIDI_IN_PORT,CI_MIDI_IN_CHANNEL,CI_SEND_TO,CI_AUDIO_OUT,CI_MIDI_OUT_PORT,CI_MIDI_OUT_CHANNEL,CI_AUDIO_IN};
 unsigned source_tag=r->io_field==IO_MONITOR&&t->kind==6?CI_AUDIO_MONITOR:tags[r->io_field];
 if(!(r->io_field==IO_AUDIO_OUT?io_output_read(t,field,now):io_scalar(t,source_tag,r->global_owner,field,now))||field->incarnation!=r->field_incarnation)return C_SOURCE_CHANGED;
 *track=t;*connector=native;return C_OK;
}
/* A dispatched request owns its original source, not the current GUI binding.
 * Never borrow the old connector or choices after native work was admitted. */
static int io_completion_source(Context *c,unsigned at,Track **track,IOField *field){
 const CommandRequest *r=&accepted[at].request;IODemand d={.enabled=1,.epoch=r->epoch,.serial=r->serial};
 Track *t=io_track_source(c,&d,0);
 if(!t||t->track!=accepted[at].track||t->program!=accepted[at].program||t->binding!=r->binding||t->incarnation!=r->incarnation||t->track_owner!=r->track_owner||t->program_owner!=r->program_owner||channel_owner_id(project)!=r->project_owner)return -1;
 ChannelCell *cell=channel_cell(r->global_owner);
 if(!cell||!atomic_load(&cell->live)||atomic_load(&cell->incarnation)!=r->field_incarnation)return -1;
 static const unsigned tags[IO_FIELDS]={CI_MIDI_MONITOR,CI_MIDI_IN_PORT,CI_MIDI_IN_CHANNEL,CI_SEND_TO,CI_AUDIO_OUT,CI_MIDI_OUT_PORT,CI_MIDI_OUT_CHANNEL,CI_AUDIO_IN};
 unsigned tag=r->io_field==IO_MONITOR&&t->kind==6?CI_AUDIO_MONITOR:tags[r->io_field];
 if(!(r->io_field==IO_AUDIO_OUT?io_output_read(t,field,atomic_load(&mirror_state->heartbeat)):io_scalar(t,tag,r->global_owner,field,atomic_load(&mirror_state->heartbeat))))return 0;
 if(io_track_source(c,&d,0)!=t||field->incarnation!=r->field_incarnation)return -1;
 *track=t;return 1;
}
static void io_event(Context *c,unsigned kind,unsigned at,unsigned bits,unsigned revision){
 const CommandRequest *r=&accepted[at].request;
 unsigned desired=accepted[at].native_bits;if(r->io_field==IO_AUDIO_IN||r->io_field==IO_AUDIO_OUT){float scalar;memcpy(&scalar,&desired,4);desired=(unsigned)scalar;}
 event(c,kind,r->seq,r->io_connector,r->track_owner,accepted[at].program,r->io_field,desired,bits,accepted[at].property,revision);
}
static void io_setter_point(Context *c,unsigned site){
 if(!io_submission)return;
 unsigned at=command_find(command_state,io_submission);if(at>=COMMAND_SLOTS){trace_fail(C_TRACE_AMBIGUOUS);return;}
 const CommandRequest *r=&accepted[at].request;unsigned field=r->io_field;
 unsigned expected=field==IO_MIDI_IN_PORT?IO_INPUT_SET:field==IO_MIDI_OUT_PORT?IO_OUTPUT_SET:IO_FILTER_SET;
 unsigned lr=expected==IO_INPUT_SET?0x1e93c8c:expected==IO_OUTPUT_SET?0x1e93d6c:0x1e93cfc;
 if(token()!=command_owner||!atomic_load(&command_in_hook)){io_fault[at]=C_OWNER;return;}
 if(site<=IO_FILTER_SET){
  if(site!=expected||c->r[0]!=accepted[at].property||c->lr!=observer_image_bias+lr)return;
  unsigned value=c->r[1];if(site==IO_FILTER_SET){if(!pointer(value)||value>UINT32_MAX-4){io_fault[at]=C_NATIVE_MEMBERSHIP;return;}value=word(value);}
  if(io_set_seen[at]||value!=accepted[at].native_bits){io_fault[at]=C_TRACE_AMBIGUOUS;return;}
  io_setter_sp=source_sp(c);io_set_seen[at]=r->seq;
  ChannelCell *cell=channel_cell(accepted[at].property);if(!cell||!atomic_load(&cell->live)||atomic_load(&cell->incarnation)!=r->field_incarnation){io_fault[at]=C_IDENTITY;return;}
  io_event(c,CE_IO_SET,at,atomic_load(&cell->bits),atomic_load(&cell->revision));
 }else{
  if(site!=expected+3)return;
  if(io_set_seen[at]!=r->seq||io_done_seen[at]||source_sp(c)!=io_setter_sp){io_fault[at]=C_TRACE_AMBIGUOUS;return;}
  io_done_seen[at]=r->seq;io_event(c,CE_IO_DONE,at,accepted[at].native_bits,r->io_generation);
 }
}
static void io_source_commit(Context *c,unsigned site,unsigned at){
 if(io_submission!=accepted[at].request.seq||io_set_seen[at]!=io_submission||io_done_seen[at])return;
 uint32_t property=site==IO_IN_PORT_COMMIT?c->r[0]:c->r[5];
 if(property!=accepted[at].property)return;
 ChannelCell *cell=channel_cell(property);const CommandRequest *r=&accepted[at].request;
 if(!cell||!atomic_load(&cell->live)||atomic_load(&cell->incarnation)!=r->field_incarnation||atomic_load(&cell->bits)!=accepted[at].native_bits||io_commit_seen[at]){io_fault[at]=C_TRACE_AMBIGUOUS;return;}
 io_commit_seen[at]=r->seq;io_event(c,CE_IO_COMMIT,at,atomic_load(&cell->bits),atomic_load(&cell->revision));
}
/* These two native listeners publish a single retained helper job. A result
 * failure never erases a published payload or its pending-counter obligation. */
static int io_async_field(unsigned field){return field==IO_MONITOR||field==IO_SEND_TO;}
static int io_async_slot(unsigned at){return accepted[at].request.io_field==IO_SEND_TO||(accepted[at].request.io_field==IO_MONITOR&&accepted[at].property==accepted[at].track+0x400);}
static int io_job_find(uint32_t payload){
 int found=COMMAND_SLOTS;
 for(unsigned at=0;at<COMMAND_SLOTS;at++)if(atomic_load_explicit(active_request+at,memory_order_seq_cst)==accepted[at].request.seq&&command_io(accepted[at].request.reserved)&&io_async_slot(at)&&atomic_load_explicit(io_payload+at,memory_order_acquire)==payload&&!atomic_load_explicit(io_finished+at,memory_order_acquire)){if(found!=COMMAND_SLOTS)return -1;found=(int)at;}
 return found;
}
static void io_job_event(Context *c,unsigned kind,unsigned at,unsigned bits){
 const CommandRequest *r=&accepted[at].request;
 event(c,kind,r->seq,atomic_load_explicit(io_payload+at,memory_order_acquire),io_helper[at],accepted[at].program,r->io_field,accepted[at].native_bits,bits,accepted[at].property,r->io_generation);
}
static void io_job_point(Context *c,unsigned site){
 if(site==JG_PUBLISH||site==IO_MONITOR_PREPARE||site==IO_SEND_PREPARE||site==IO_MONITOR_RESULT||site==IO_SEND_RESULT){
  unsigned at=command_find(command_state,io_submission);if(at>=COMMAND_SLOTS)return;
  if(site==JG_PUBLISH&&(!io_source_frame[at]||c->r[8]!=io_source_frame[at]))return;
  const CommandRequest *r=&accepted[at].request;unsigned send=r->io_field==IO_SEND_TO;
  if(!io_async_field(r->io_field)||token()!=command_owner||!atomic_load(&command_in_hook)){io_fault[at]=C_OWNER;return;}
  uint32_t object=accepted[at].track+(send?0x41c:0x3fc),invoker=observer_image_bias+(send?0x245ec10:0x245e634),manager=observer_image_bias+(send?0x245ebc4:0x245e5e8);
  if(site==IO_MONITOR_PREPARE||site==IO_SEND_PREPARE){
   if(site!=(send?IO_SEND_PREPARE:IO_MONITOR_PREPARE)||io_source_frame[at]||io_queue_result[at]||c->r[4]!=object||c->r[5]!=io_helper[at]||c->r[9]!=accepted[at].native_bits||c->r[3]!=invoker){io_fault[at]=C_TRACE_AMBIGUOUS;return;}io_source_frame[at]=source_sp(c);return;
  }
  if(site==JG_PUBLISH){
   /* The GUI setter notifies other native listeners too. Only this live
    * prepared callable address owns our publication; its contents have moved. */
   uint32_t cell=c->r[10];if(atomic_load(io_payload+at)||!qlink_span(cell,20)||c->r[7]!=command_queues+0xfc||c->r[9]!=(io_helper[at]|1u)||word(cell)!=invoker||word(cell+4)!=manager||word(cell+8)!=io_helper[at]||word(cell+12)!=accepted[at].native_bits||word(cell+16)!=object){io_fault[at]=C_TRACE_AMBIGUOUS;return;}
   atomic_store_explicit(io_payload+at,cell+8,memory_order_release);io_job_event(c,CE_IO_QUEUE,at,accepted[at].native_bits);return;
  }
  if(site!=(send?IO_SEND_RESULT:IO_MONITOR_RESULT)||source_sp(c)!=io_source_frame[at]||io_queue_result[at]||c->r[0]>1){io_fault[at]=C_TRACE_AMBIGUOUS;return;}
  io_queue_result[at]=c->r[0]+1;io_job_event(c,CE_IO_RESULT,at,c->r[0]);
  io_source_frame[at]=0; /* Result ends the borrow; later listeners may reuse F. */
  return;
 }
 if((site==IO_MONITOR_DONE||site==IO_SEND_DONE)&&c->r[2])return; /* Failed native STREX retries without a receipt. */
 int found=io_job_find(c->r[0]);if(found==COMMAND_SLOTS)return;if(found<0){trace_fail(C_TRACE_AMBIGUOUS);return;}unsigned at=(unsigned)found;
 const CommandRequest *r=&accepted[at].request;unsigned send=r->io_field==IO_SEND_TO,enter=site==IO_MONITOR_ENTER||site==IO_SEND_ENTER;
 if(site!=(send?(enter?IO_SEND_ENTER:IO_SEND_DONE):(enter?IO_MONITOR_ENTER:IO_MONITOR_DONE))){trace_fail(C_TRACE_AMBIGUOUS);return;}
 uint32_t payload=c->r[0];
 if(enter){
  if(io_call.seq||!qlink_span(payload,12)||word(payload)!=io_helper[at]||word(payload+4)!=accepted[at].native_bits||word(payload+8)!=accepted[at].track+(send?0x41c:0x3fc)){trace_fail(C_TRACE_AMBIGUOUS);return;}
  io_call=(IOCall){r->seq,payload,source_sp(c),c->lr};io_job_event(c,CE_IO_ENTER,at,accepted[at].native_bits);
 }else{
  if(io_call.seq!=r->seq||io_call.payload!=payload||io_call.sp!=source_sp(c)||io_call.lr!=c->lr){trace_fail(C_TRACE_AMBIGUOUS);return;}
  io_job_event(c,CE_IO_AUDIO_DONE,at,accepted[at].native_bits);memset(&io_call,0,sizeof(io_call));atomic_store_explicit(io_finished+at,r->seq,memory_order_release);
 }
}
/* The audio cell can be publishing again after this request's callback ends.
 * Identity loss is an unavailable outcome; an odd/changing value is a retry. */
static int io_completion_audio(unsigned at,Track *t,IOField *audio){
 const CommandRequest *r=&accepted[at].request;
 unsigned tag=r->io_field==IO_SEND_TO?CI_SEND_TO_AUDIO:CI_MIDI_MONITOR_AUDIO;
 uint32_t property=accepted[at].track+(r->io_field==IO_SEND_TO?0x434:0x414);
 ChannelCell *cell=channel_cell(property);unsigned incarnation=io_audio_incarnation[at];
 if(!incarnation||!cell||!atomic_load(&cell->live)||atomic_load(&cell->incarnation)!=incarnation||atomic_load(&cell->field)!=tag||atomic_load(&cell->owner_incarnation)!=r->track_owner||channel_owner_id(accepted[at].track)!=r->track_owner)return -1;
 int stable=io_scalar(t,tag,property,audio,atomic_load(&mirror_state->heartbeat));
 if(channel_cell(property)!=cell||!atomic_load(&cell->live)||atomic_load(&cell->incarnation)!=incarnation||atomic_load(&cell->owner_incarnation)!=r->track_owner||channel_owner_id(accepted[at].track)!=r->track_owner)return -1;
 return stable?1:0;
}
static void io_sync_complete(Context *c,unsigned at){
 const CommandRequest *r=&accepted[at].request;Track *t=NULL;IOField actual={0};
 int source=io_completion_source(c,at,&t,&actual);if(!source)return;
 if(source>0&&actual.value!=accepted[at].native_bits)return;
 io_event(c,CE_IO_VALUE,at,source>0?actual.value:0,source>0?actual.revision:0);io_receipt[at]=r->seq;
 atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&command_state->slots[at].done,r->seq,memory_order_release);
}
static void io_complete(Context *c){
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  const CommandRequest *r=&accepted[at].request;CommandSlot *slot=command_state->slots+at;
  unsigned program_field=r->io_field==IO_AUDIO_IN||r->io_field==IO_AUDIO_OUT;
  if(native_started[at]&&command_io(r->reserved)&&accepted[at].synchronous&&!io_receipted(at)&&atomic_load_explicit(&slot->returned,memory_order_acquire)==r->seq){io_sync_complete(c,at);continue;}
  if(native_started[at]&&command_io(r->reserved)&&program_field&&!io_receipted(at)&&atomic_load_explicit(&slot->done,memory_order_acquire)==r->seq&&atomic_load_explicit(&slot->returned,memory_order_acquire)==r->seq){
   Track *t=NULL;IOField actual={0},audio={0};int source=io_completion_source(c,at,&t,&actual);if(!source)continue;
   if(source<0){io_event(c,CE_IO_VALUE,at,0,0);io_receipt[at]=r->seq;continue;}
   float scalar;memcpy(&scalar,&accepted[at].native_bits,4);unsigned wanted=(unsigned)scalar;
   if(actual.value!=wanted)continue;
   if(r->io_field==IO_AUDIO_IN){if(!io_scalar(t,CI_AUDIO_IN_AUDIO,t->program+0x2798,&audio,atomic_load(&mirror_state->heartbeat))||audio.value!=wanted)continue;}
   else if(!io_output_agrees(t))continue;
   io_event(c,CE_IO_VALUE,at,actual.value,actual.revision);io_receipt[at]=r->seq;continue;
  }
  if(!native_started[at]||!command_io(r->reserved)||!io_async_slot(at)||io_receipted(at)||atomic_load_explicit(io_finished+at,memory_order_acquire)!=r->seq||atomic_load_explicit(&slot->returned,memory_order_acquire)!=r->seq)continue;
  Track *t=NULL;IOField actual={0},audio={0};int source=io_completion_source(c,at,&t,&actual);if(!source)continue;
  if(source<0){io_event(c,CE_IO_VALUE,at,0,0);io_receipt[at]=r->seq;atomic_store(active_request+at,0);atomic_store_explicit(&slot->done,r->seq,memory_order_release);continue;}
  int secondary=io_completion_audio(at,t,&audio);if(!secondary)continue;
  if(secondary<0){io_event(c,CE_IO_VALUE,at,0,0);io_receipt[at]=r->seq;atomic_store(active_request+at,0);atomic_store_explicit(&slot->done,r->seq,memory_order_release);continue;}
  if(actual.value!=accepted[at].native_bits||audio.value!=accepted[at].native_bits)continue;
  io_event(c,CE_IO_VALUE,at,actual.value,actual.revision);io_receipt[at]=r->seq;atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&slot->done,r->seq,memory_order_release);
 }
}
static void io_input_service(Context *c,unsigned at,const CommandRequest *r){
 CommandSlot *slot=command_state->slots+at;Track *t=NULL;uint32_t connector=0;IOField field={0};unsigned why=io_resolve(c,r,&t,&connector,&field);if(why==COMMAND_WAIT_HEARTBEAT)return;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(i!=at){CommandSlot *other=command_state->slots+i;unsigned seq=atomic_load(&other->published);if(seq&&seq<r->seq&&seq!=atomic_load(&other->reclaimed)){CommandRequest old;if(!command_request_read(other,seq,&old)||command_conflict(&old,r))return;}}
 accepted[at].request=*r;accepted[at].property=r->global_owner;accepted[at].program=t?t->program:0;accepted[at].track=t?t->track:0;accepted[at].synchronous=1;native_started[at]=0;io_set_seen[at]=io_done_seen[at]=io_commit_seen[at]=io_receipt[at]=io_fault[at]=io_audio_incarnation[at]=0;io_source_frame[at]=io_helper[at]=io_queue_result[at]=0;atomic_store(io_payload+at,0);atomic_store(io_finished+at,0);
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}atomic_store(&slot->rejected,0);
 if(why)goto refused;
 unsigned value=0;
 if(r->io_field==IO_AUDIO_IN||r->io_field==IO_AUDIO_OUT){
  unsigned audio=r->io_field==IO_AUDIO_IN;IOProgramConnector *n=io_program_copy(connector);IOPortCatalogue *cat=n->choices+audio;unsigned rev=atomic_load(&cat->revision),count=atomic_load(&cat->count),matches=0;int row=-1;
  if(rev!=r->io_choice_generation||!atomic_load(&cat->valid)||!count||count>IO_CHOICES){why=C_SOURCE_CHANGED;goto refused;}
  for(unsigned i=0;i<count;i++){unsigned candidate;if(!io_audio_choice(t,connector,audio,i,&candidate)){why=C_SOURCE_CHANGED;goto refused;}if(candidate==field.value){row=(int)i;matches++;}}
  if(matches!=1){why=C_SOURCE_CHANGED;goto refused;}row+=(int32_t)r->bits;if(row<0)row=0;if(row>=(int)count)row=(int)count-1;
  if(!io_audio_choice(t,connector,audio,(unsigned)row,&value)||rev!=atomic_load(&cat->revision)){why=C_SOURCE_CHANGED;goto refused;}
 }else if(r->io_field==IO_MONITOR){int target=(int)field.value+(int32_t)r->bits;if(field.value>3){why=C_SOURCE_CHANGED;goto refused;}value=target<0?0:target>3?3:(unsigned)target;}else if(r->io_field==IO_MIDI_IN_CHANNEL||r->io_field==IO_MIDI_OUT_CHANNEL){
  if(field.value>16){why=C_SOURCE_CHANGED;goto refused;}
  int row=field.value?(int)field.value-1:16;row+=(int32_t)r->bits;if(row<0)row=0;if(row>16)row=16;value=row==16?0:(unsigned)row+1;
 }else if(r->io_field==IO_MIDI_IN_PORT||r->io_field==IO_MIDI_OUT_PORT||r->io_field==IO_SEND_TO){
  IOConnector *n=io_connector_copy(connector);IOPortCatalogue *p=r->io_field==IO_SEND_TO?&io_send_catalogue:n->ports+(r->io_field==IO_MIDI_OUT_PORT);unsigned rev=atomic_load_explicit(&p->revision,memory_order_acquire),count=atomic_load(&p->count);
  if(rev!=r->io_choice_generation||(rev&1)||!atomic_load(&p->valid)||count>IO_CHOICES){why=C_SOURCE_CHANGED;goto refused;}
  int row=-1;unsigned matches=0;for(unsigned i=0;i<count;i++)if(atomic_load(&p->rows[i].id)==field.value){row=(int)i;matches++;}
  if(matches!=1){why=C_SOURCE_CHANGED;goto refused;}
  int delta=(int32_t)r->bits,step=delta<0?-1:1;for(unsigned i=0;i<(unsigned)(delta<0?-delta:delta);i++){int next=row+step;while(next>=0&&next<(int)count&&!atomic_load(&p->rows[next].selectable))next+=step;if(next<0||next>=(int)count)break;row=next;}
  if(!atomic_load(&p->rows[row].selectable)){why=C_SOURCE_CHANGED;goto refused;}value=atomic_load(&p->rows[row].id);
  atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load(&p->revision)){why=C_SOURCE_CHANGED;goto refused;}
 }else{why=C_NOT_READY;goto refused;}
 if(r->io_field==IO_SEND_TO){why=io_send_preflight(t,value);if(why)goto refused;}
 if(value==field.value){why=C_SOURCE_CHANGED;goto refused;}
 accepted[at].controller=r->io_field;accepted[at].native_bits=value;
 if(r->io_field==IO_AUDIO_IN||r->io_field==IO_AUDIO_OUT){float scalar=(float)value;memcpy(&accepted[at].native_bits,&scalar,4);accepted[at].controller=r->io_field==IO_AUDIO_IN?1162:325;accepted[at].synchronous=0;why=recording_prepare(at);if(why)goto refused;}
 why=io_resolve(c,r,&t,&connector,&field);if(why||io_submission||!atomic_load(&observer_running)||atomic_load(&command_state->trace_error)){if(!why)why=C_SOURCE_FAILURE;goto refused;}
 if(io_async_slot(at)){
  IOField audio_before={0};unsigned tag=r->io_field==IO_SEND_TO?CI_SEND_TO_AUDIO:CI_MIDI_MONITOR_AUDIO;
  if(!io_scalar(t,tag,t->track+(r->io_field==IO_SEND_TO?0x434:0x414),&audio_before,atomic_load(&mirror_state->heartbeat))){why=C_SOURCE_CHANGED;goto refused;}
  io_audio_incarnation[at]=audio_before.incarnation;
 }
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);io_event(c,CE_DISPATCH,at,value,r->io_generation);
 if(r->io_field==IO_AUDIO_IN||r->io_field==IO_AUDIO_OUT){
  native_started[at]=r->seq;float scalar=(float)value;recording_dispatch(at,scalar);
  io_event(c,CE_RETURN,at,value,r->io_generation);atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);return;
 }
 if(io_async_slot(at)){
  io_helper[at]=word(t->track+(r->io_field==IO_SEND_TO?0x438:0x418));
  if(!pointer(io_helper[at])||word(io_helper[at])!=command_queues){why=C_NATIVE_MEMBERSHIP;goto refused;}
  accepted[at].synchronous=0;io_event(c,CE_IO_SET,at,field.value,field.revision);
 }
 native_started[at]=r->seq;io_submission=r->seq;
 if(io_async_slot(at)){
  if(r->io_field==IO_MONITOR)((void (*)(void*,void*,const unsigned*))(uintptr_t)(observer_image_bias+0x1e93ba0))((void*)(uintptr_t)connector,(void*)(uintptr_t)accepted[at].property,&value);
  else ((void (*)(void*,void*,uint32_t))(uintptr_t)(observer_image_bias+0x1e8ac24))((void*)(uintptr_t)(connector+0x464),NULL,value);
  io_submission=0;io_event(c,CE_RETURN,at,value,r->io_generation);
  /* Native listeners may already have rebound the visible connector. The
   * accepted job retains its original source; completion checks it separately. */
  if(io_fault[at]||io_queue_result[at]!=2||!atomic_load_explicit(io_payload+at,memory_order_acquire)){atomic_store(&slot->processed,r->seq);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);command_fail(io_fault[at]?io_fault[at]:C_TRACE_AMBIGUOUS);return;}
  atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);return;
 }
 if(r->io_field==IO_MONITOR){
  io_set_seen[at]=r->seq;io_event(c,CE_IO_SET,at,field.value,field.revision);
  ((void (*)(void*,void*,unsigned))(uintptr_t)(observer_image_bias+0x1e5d8b8))((void*)(uintptr_t)connector,NULL,value);
  io_done_seen[at]=r->seq;io_event(c,CE_IO_DONE,at,value,r->io_generation);
 }else{
 uint32_t function=observer_image_bias+(r->io_field==IO_MIDI_IN_CHANNEL?0x1e8943c:r->io_field==IO_MIDI_OUT_CHANNEL?0x1e88d30:r->io_field==IO_MIDI_IN_PORT?0x1e897c0:0x1e890b4);
 ((void (*)(void*,void*,uint32_t))(uintptr_t)function)((void*)(uintptr_t)connector,(void*)(uintptr_t)r->global_owner,value);
 }
 io_submission=0;io_setter_sp=0;io_event(c,CE_RETURN,at,value,r->io_generation);
 if(io_fault[at]||io_set_seen[at]!=r->seq||io_done_seen[at]!=r->seq||(!io_commit_seen[at]&&field.value!=value)){
  why=io_fault[at]?io_fault[at]:C_TRACE_AMBIGUOUS;atomic_store(active_request+at,0);atomic_store(&slot->processed,r->seq);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);command_fail(why);return;
 }
 /* Setter entry, source commit and Do return qualify native acceptance.
  * Fresh readback uses original lifetimes, never post-listener menu state. */
 atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);
 io_sync_complete(c,at);return;
refused:
 atomic_store(active_request+at,0);atomic_store(&slot->rejected,why);atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);
}
