/* Finite MCU bank over MMV4; optional concurrent CMD9 input. Original MIT. */
#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "mirror-motor-core.h"
#include "mirror-fresh.h"
#include "sha256.h"
static volatile sig_atomic_t stopping;
#ifdef MIRROR_INPUT
#include <sys/file.h>
#include "../stop-route.h"
#define INPUT_STOPPED() (stopping!=0)
static unsigned bridge_verbose;
static unsigned bridge_log_count;
static void bridge_log(const char *format,...){
 if(!bridge_verbose&&bridge_log_count++>=128)return;
 va_list args;va_start(args,format);vprintf(format,args);va_end(args);
}
#define BRIDGE_LOG(...) bridge_log(__VA_ARGS__)
#define INPUT_LOG(...) do{if(bridge_verbose)BRIDGE_LOG(__VA_ARGS__);}while(0)
#include "mirror-input-core.h"
static MirrorInput input;
#include "motor-follow.h"
#include "surface-preferences.h"
static MotorFollowing following={.enabled=1};
static const char *preferences_path,*stop_adapter_exe;
static uint32_t preference_notice_until;
static unsigned preference_notice_error;
static uint32_t page_notice_until;static unsigned requested_page;
static uint32_t effect_notice_until,effect_notice_epoch,effect_notice_generation,effect_notice_serial,effect_notice_slot=EFFECT_LIST,effect_notice_page;
static uint32_t mode_notice_until,mode_notice_controller,mode_notice_generation,mode_notice_epoch;static unsigned mode_notice_id;
static unsigned inherited_holds;
static unsigned servo_disabled=1; /* X-Touch native A/B: no raw echo by default. */
static int servo_arguments(int argc,char **argv){
 servo_disabled=1;bridge_verbose=0;
 while(argc>=4){
  const char *arg=argv[argc-1];
  if(!strncmp(arg,"--stop-adapter-exe=",19)&&arg[19]=='/')stop_adapter_exe=arg+19;
  else if(!strcmp(arg,"--servo=off"))servo_disabled=1;
  else if(!strcmp(arg,"--servo=on"))servo_disabled=0;
  else if(!strcmp(arg,"--verbose"))bridge_verbose=1;
  else if(!strcmp(arg,"--motors=on"))following.enabled=1;
  else if(!strcmp(arg,"--motors=off"))following.enabled=0;
  else if(!strncmp(arg,"--preferences=",14)&&arg[14]=='/')preferences_path=arg+14;
  else if(!strncmp(arg,"--held-mask=",12)){char *end;unsigned long mask=strtoul(arg+12,&end,10);if(!arg[12]||*end||mask>511)return -1;inherited_holds=(unsigned)mask;}
  else break;
  argc--;
 }
 return argc;
}
static int input_fd=-1;
static CommandState *input_mapping=MAP_FAILED;
static struct stat input_file;
static inline int fresh_copy(const MirrorState*,CopiedMirror*,uint32_t*);
static int fresh_copy_view(const MirrorState*,CopiedMirror*,uint32_t*,int);
#endif
#ifndef BRIDGE_LOG
#define BRIDGE_LOG(...) printf(__VA_ARGS__)
#endif
#ifndef INPUT_LOG
#define INPUT_LOG(...) BRIDGE_LOG(__VA_ARGS__)
#endif
static const char expected_sha[]="bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426";
static void signal_stop(int sig){(void)sig;stopping=1;}
static uint64_t monotonic_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return UINT64_MAX;return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
/* Same signed producer-relative arithmetic, including fractional second edges. */
static uint32_t relative_time(int64_t sec,int64_t nsec,uint32_t began_sec,uint32_t began_nsec){
 if(sec<(int64_t)began_sec||(sec==began_sec&&nsec<began_nsec))return UINT32_MAX;
 int64_t n=(sec-began_sec)*1000+(nsec-began_nsec)/1000000;return n<0||n>=UINT32_MAX?UINT32_MAX:(uint32_t)n;
}
static uint32_t source_now(const MirrorState *s){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return UINT32_MAX;return relative_time(t.tv_sec,t.tv_nsec,s->origin_sec,s->origin_nsec);}
static int number(const char *text,uint64_t max,uint64_t *value){char *end;errno=0;if(!*text||*text=='-'||*text=='+')return 0;unsigned long long n=strtoull(text,&end,10);if(errno||*end||!n||n>max)return 0;*value=n;return 1;}
static uint64_t start_time(unsigned pid){
 char path[64],data[4096];snprintf(path,sizeof(path),"/proc/%u/stat",pid);int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return 0;
 ssize_t n=read(fd,data,sizeof(data)-1);close(fd);if(n<=0)return 0;data[n]=0;char *end=strrchr(data,')');if(!end||end[1]!=' ')return 0;
 char *save=NULL,*part=strtok_r(end+2," ",&save);for(unsigned field=3;part;field++,part=strtok_r(NULL," ",&save))if(field==22){uint64_t value;return number(part,UINT64_MAX,&value)?value:0;}return 0;
}
static int same_stat(const struct stat *a,const struct stat *b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino;}
static int process_file(unsigned pid,struct stat *identity,int hash){
 char proc[64],path[128];snprintf(proc,sizeof(proc),"/proc/%u/exe",pid);ssize_t n=readlink(proc,path,sizeof(path)-1);if(n<0)return 0;path[n]=0;if(strcmp(path,"/usr/bin/MPC"))return 0;
 int fd=open(proc,O_RDONLY|O_CLOEXEC);if(fd<0)return 0;struct stat st;int ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size==112222004;
 if(hash&&ok){Sha sha;sha_init(&sha);unsigned char bytes[65536],sum[32];size_t total=0;
  while(ok){ssize_t got=read(fd,bytes,sizeof(bytes));if(got<0){ok=0;break;}if(!got)break;total+=(size_t)got;if(total>112222004){ok=0;break;}sha_add(&sha,bytes,(size_t)got);}
  sha_end(&sha,sum);char hex[65];for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",sum[i]);ok=ok&&total==112222004&&!strcmp(hex,expected_sha);
  struct stat after;if(fstat(fd,&after)||!same_stat(&after,&st)||after.st_size!=st.st_size)ok=0;if(ok)*identity=st;
 }else if(ok)ok=same_stat(identity,&st);
 close(fd);return ok;
}
static int address_equal(snd_seq_addr_t a,snd_seq_addr_t b){return a.client==b.client&&a.port==b.port;}
static int discover(snd_seq_t *seq,snd_seq_addr_t *out){
 snd_seq_client_info_t *ci;snd_seq_port_info_t *pi;snd_seq_client_info_alloca(&ci);snd_seq_port_info_alloca(&pi);snd_seq_client_info_set_client(ci,-1);int n=0,rc;
 while((rc=snd_seq_query_next_client(seq,ci))>=0){
  if(strcmp(snd_seq_client_info_get_name(ci),"X-Touch"))continue;
  if(snd_seq_client_info_get_type(ci)!=SND_SEQ_KERNEL_CLIENT)return 0;
  snd_seq_port_info_set_client(pi,snd_seq_client_info_get_client(ci));snd_seq_port_info_set_port(pi,-1);int pr;
  while((pr=snd_seq_query_next_port(seq,pi))>=0){
   if(strcmp(snd_seq_port_info_get_name(pi),"X-TOUCH_INT"))continue;
   unsigned need=SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE;
   if((snd_seq_port_info_get_capability(pi)&need)!=need)return 0;
   *out=*snd_seq_port_info_get_addr(pi);n++;
  }if(pr!=-ENOENT)return 0;
 }return rc==-ENOENT&&n==1;
}
/* Only the discovered physical surface enters policy. Optional input accepts
 * channel pitch-bend; transport and arbitrary MIDI never become commands. */
static int surface_event(const snd_seq_event_t *event,snd_seq_addr_t full,unsigned *kind,unsigned *channel,int *value){
 if(!address_equal(event->source,full))return 0;
#ifdef MIRROR_INPUT
 if(event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.channel==0&&event->data.control.param==60){
  int raw=event->data.control.value;if(raw<0||raw>127)return -1;*kind=8;*channel=0;*value=(raw&64)?-(raw&63):(raw&63);return *value!=0;
 }
 if(event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.channel==0&&event->data.control.param>=0x10&&event->data.control.param<0x18){
  int raw=event->data.control.value;if(raw<0||raw>127)return -1;*kind=4;*channel=event->data.control.param-0x10;*value=(raw&64)?-(raw&63):(raw&63);return *value!=0;
 }
 if(event->type==SND_SEQ_EVENT_PITCHBEND){
  if(event->data.control.channel>MIRROR_BANK)return 0;
  if(event->data.control.value< -8192||event->data.control.value>8191)return -1;
  *kind=event->data.control.channel==MIRROR_BANK?10:3;*channel=event->data.control.channel;*value=event->data.control.value+8192;return 1;
 }
#endif
 if((event->type!=SND_SEQ_EVENT_NOTEON&&event->type!=SND_SEQ_EVENT_NOTEOFF)||event->data.note.channel!=0)return 0;
 if(event->data.note.velocity>127)return -1;
 unsigned note=event->data.note.note;int down=event->type==SND_SEQ_EVENT_NOTEON&&event->data.note.velocity>0;
 if(note==93||note==94){*kind=12;*channel=note;*value=down;return 1;}
 if(note==70||note==91||note==92){*kind=9;*channel=note;*value=down;return 1;}
 if(note==112){*kind=7;*channel=8;*value=down;return 1;}
 if(note>=104&&note<=111){*kind=1;*channel=note-104;*value=down;return 1;}
 if(note>=46&&note<=49){*kind=2;*channel=note-46;*value=down;return 1;}
#ifdef MIRROR_INPUT
 if((note>=54&&note<=61)||note==95||note==86||note==89||note==52||note==74||note==75||note==79||note==80||note==81||note==82||note==83||(note>=96&&note<=100)){*kind=11;*channel=note;*value=down;return 1;}
 if(note==40||note==41||note==42||note==43||note==44||note==45||note==50||note==51||(note>=62&&note<=69)){*kind=6;*channel=note;*value=down;return 1;}
 if(note<40){*kind=5;*channel=note;*value=down;return 1;}
#endif
 return 0;
}
static int lost_events(snd_seq_t *seq){snd_seq_client_info_t *info;snd_seq_client_info_alloca(&info);return snd_seq_get_client_info(seq,info)<0||snd_seq_client_info_get_event_lost(info)!=0;}
#ifdef MIRROR_INPUT
#include "channel-wire.h"
#endif
typedef struct {snd_seq_t *seq;snd_seq_addr_t full,ingress;int sink,source;unsigned bank_down[4],button_down[40],display_field[8],general_down[128];
#ifdef MIRROR_INPUT
 snd_seq_addr_t stop_adapter,stop_mpc;int stop_sink;
 ChannelWire wire[8];unsigned char colors[8];unsigned wire_valid[8],colors_valid,next_wire,mode_valid,mode_led[128],playing_valid,playing_value,master_owner,master_incarnation,master_touch,master_tick;int master_sent;unsigned char position_text[10];unsigned position_valid,chooser_cleared;
 unsigned state_valid[7];
 unsigned meter_enabled[8],meter_identity[8],meter_valid,meter_tick,zoom_mode,name_value,display_until[8];
#endif
} Surface;
static void motor_event(snd_seq_event_t*,const Surface*,unsigned,int);
#ifdef MIRROR_INPUT
static int parent_led_clear(Surface*);
#endif
#ifdef MIRROR_INPUT
/* Closed-loop servo acknowledgement follows physical input immediately. It
 * changes neither MMV4 nor request settlement and is distinct from MPC output. */
static int surface_servo(Surface *s,MirrorBank *bank,unsigned strip,int position,uint32_t now){
 if(stopping||!following.enabled)return 1;
 if(servo_disabled){INPUT_LOG("SERVO_SKIPPED fader=%u position=%d tick=%u; X-Touch raw echo disabled\n",strip+1,position,now);return 1;}
 snd_seq_event_t echo;motor_event(&echo,s,strip,position);
 if(snd_seq_event_output_direct(s->seq,&echo)<0)return 0;
 bank->faders[strip].last_sent=position;bank->faders[strip].last_tick=now;
 INPUT_LOG("SERVO fader=%u position=%d tick=%u; physical acknowledgement only\n",strip+1,position,now);return 1;
}
static void surface_encoder(Surface *s,MirrorInput *in,const MirrorBank *bank,const CopiedMirror *snapshot,unsigned strip,int delta,int push,uint32_t now){
 if(bank->assignment==BA_IO){if(!push)input_io_edit(in,bank,snapshot,strip,delta,now);return;}
 if(bank->assignment==BA_QLINK){
  if(bank->flip){unsigned field=bank->volume_fields[strip];input_control_bound(in,bank,snapshot,strip,field,delta,push,now,&bank->strips[strip],3);s->display_field[strip]=field;s->display_until[strip]=now+2000;}
  else if(!push)input_qlink_edit(in,bank,snapshot,strip,delta,-1,now);
  return;
 }
 if(bank->assignment==BA_EFFECT){
  const EffectsCopy *e=&snapshot->effects;
  if(!snapshot->effects_available||e->serial!=bank->selected.serial||e->status!=EF_READY)return;
  if(bank->effects_slot==EFFECT_LIST){if(push&&in->jog_shift)input_effect_enable(in,bank,snapshot,strip,now);if(push&&!in->jog_shift&&strip<EFFECT_SLOTS&&e->slots[strip].status==EF_EMPTY)input_effect_chooser(in,bank,snapshot,strip,0,now);if(push&&!in->jog_shift&&strip<EFFECT_SLOTS&&e->slots[strip].status==EF_READY&&e->slots[strip].count){MirrorBank *mutable_bank=(MirrorBank*)bank;mutable_bank->effects_slot=strip;mutable_bank->effects_page=0;memset(s->wire_valid,0,sizeof(s->wire_valid));}}
  else if(bank->flip){
   unsigned field=bank->volume_fields[strip];input_control_bound(in,bank,snapshot,strip,field,delta,push,now,&bank->strips[strip],3);s->display_field[strip]=field;s->display_until[strip]=now+2000;
  }else input_effect_edit(in,bank,snapshot,strip,delta,push&&in->jog_shift?2:push,now);
  return;
 }
 unsigned field=bank_encoder_field(bank,strip);if(field>=CF_COUNT)return;
 unsigned selected=bank->assignment==BA_SEND&&!bank->flip;
 input_control_bound(in,bank,snapshot,strip,field,delta,push,now,bank_encoder_identity(bank,strip),selected?1:3);
 s->display_until[strip]=now>UINT32_MAX-2000?UINT32_MAX:now+2000;
 s->display_field[strip]=bank->assignment==BA_TRACK&&!bank->flip?CF_VOLUME:field;
}
static void surface_assignment(Surface *s,MirrorInput *in,MirrorBank *bank,const CopiedMirror *snapshot,unsigned note,uint32_t now){
 if(note==43&&in->jog_shift&&bank->assignment==BA_EFFECT&&bank->effects_slot<EFFECT_SLOTS){input_effect_chooser(in,bank,snapshot,bank->effects_slot,1,now);return;}
 if(note==50&&in->jog_shift){
  unsigned next=!following.enabled;preference_notice_error=!preferences_path||!surface_preferences_write(preferences_path,next);
  if(!preference_notice_error){
   following.enabled=next;
   if(next){
    following.barrier=now;
    /* Old emitted positions cannot suppress reacquisition after movement
     * while Off. Keep observed travel/quiet and all touch/request ownership. */
    for(unsigned i=0;i<9;i++){
     following.motors[i].last_sent=-1;
    }
    for(unsigned i=0;i<MIRROR_BANK;i++){bank->faders[i].last_sent=-1;bank->faders[i].wait=1;bank->faders[i].barrier=now;}
   }
  }
  preference_notice_until=now>UINT32_MAX-2000?UINT32_MAX:now+2000;memset(s->wire_valid,0,sizeof(s->wire_valid));
  fprintf(stderr,"MOTOR_FOLLOW %s preference_write=%s\n",following.enabled?"ON":"OFF",preference_notice_error?"failed":"ok");return;
 }
 if(note==45){input_global_add(in,snapshot,GLOBAL_TRACK_NEW,0,now);return;}
 if(note==40&&in->jog_shift){input_global_add(in,snapshot,GLOBAL_TRACK_TYPE,0,now);return;}
 if(note==63&&!in->jog_shift){bank->assignment=BA_IO;bank->flip=0;bank_view(bank,BV_TRACK,now);}
 else if(note==50&&bank->assignment==BA_IO)return;
 else if(note==44){bank->assignment=BA_QLINK;bank->flip=0;bank->qlink_page=0;bank_view(bank,BV_TRACK,now);}
 else if(note==43){bank->assignment=BA_EFFECT;bank->flip=0;bank->effects_slot=EFFECT_LIST;bank->effects_page=0;bank_view(bank,BV_TRACK,now);}
 else if(note==50&&bank->assignment==BA_EFFECT){if(bank->effects_slot==EFFECT_LIST)return;bank->flip=!bank->flip;}
 else if((note==66&&!in->jog_shift)||(note==65&&in->jog_shift)){bank_view(bank,bank->view==BV_DRUM_PADS?BV_TRACK:BV_DRUM_PADS,now);bank->assignment=BA_TRACK;bank->flip=0;}
 else if(note==40||note==41||note==42){bank->assignment=note==40?BA_TRACK:note==41?BA_SEND:BA_PAN;bank->flip=note==41;if(note==41)bank_view(bank,BV_TRACK,now);if(note==40)bank_view(bank,BV_TRACK,now);}
 else if(note==50)bank->flip=!bank->flip;
 else{static const unsigned views[8]={BV_MIDI,BV_INPUT,BV_AUDIO,BV_INSTRUMENT,BV_RETURN,BV_SUBMIX,BV_OUTPUT,BV_ALL};bank_view(bank,note==51?BV_ALL:views[note-62],now);if(note==66||bank->assignment==BA_SEND||bank->assignment==BA_EFFECT||bank->assignment==BA_QLINK||bank->assignment==BA_IO){bank->assignment=BA_TRACK;bank->flip=0;}}
 bank->fader_field=bank->flip&&bank->assignment!=BA_SEND&&bank->assignment!=BA_EFFECT&&bank->assignment!=BA_QLINK?CF_PAN:CF_VOLUME;
 bank_drop(bank,now);input_discard(in);memset(s->display_until,0,sizeof(s->display_until));
 for(unsigned j=0;j<8;j++)s->display_field[j]=bank->assignment==BA_TRACK?CF_VOLUME:bank_parameter(bank,j);
 BRIDGE_LOG("MODE view=%u assignment=%u flip=%u; unsent work canceled\n",bank->view,bank->assignment,bank->flip);
}
static void surface_jog(MirrorInput *in,const CopiedMirror *snapshot,unsigned kind,unsigned key,int value,uint32_t now){
 if(in->jog_epoch!=snapshot->epoch){input_jog_discard(in);in->jog_epoch=snapshot->epoch;}
 if(kind==8)input_jog_add(in,snapshot,in->jog_shift?JOG_PULSE:JOG_BEAT,value,now);
 else{if(key==70)in->jog_shift=value;else if(key==91)in->jog_rewind=value;else in->jog_forward=value;if(!value){in->jog_repeat_tick=now;if(key==91||key==92)input_jog_release(in,key==92?1:2);}}
}
static void surface_navigation(MirrorInput *in,MirrorBank *bank,const CopiedMirror *snapshot,unsigned button,uint32_t now){
 if(button<2&&bank->assignment==BA_QLINK){if(in->jog_shift){input_mode_add(in,snapshot,button?1:-1,now);return;}if(bank->qlink_page!=button){bank->qlink_page=button;bank_drop(bank,now);input_discard(in);}return;}
 if(button<2&&bank->assignment==BA_EFFECT){
  const EffectsCopy *e=&snapshot->effects;if(bank->effects_slot==EFFECT_LIST||!snapshot->effects_available||bank->effects_slot>=EFFECT_SLOTS)return;
  unsigned pages=(e->slots[bank->effects_slot].presentation_count+7)/8;if(button&&bank->effects_page+1<pages)bank->effects_page++;else if(!button&&bank->effects_page)bank->effects_page--;return;
 }
 if(button<2&&bank->assignment==BA_SEND)return;
 if(button<2){unsigned before=bank->offset;bank_navigate(bank,button?1:-1,now);if(before!=bank->offset)input_bank_discard(in);}
 else input_channel(in,bank,snapshot,button==3?1:-1,now);
}
static void surface_general(Surface *surface,MirrorInput *in,const CopiedMirror *snapshot,unsigned note,uint32_t now){
 if(note>=54&&note<=61){input_global_add(in,snapshot,GLOBAL_PAGE_MAIN+note-54,0,now);requested_page=note-54;page_notice_until=now+2000;memset(surface->wire_valid,0,sizeof(surface->wire_valid));return;}
 if(note==52){surface->name_value=!surface->name_value;memset(surface->wire_valid,0,sizeof(surface->wire_valid));return;}
 if(note==100){surface->zoom_mode=!surface->zoom_mode;return;}
 unsigned op=note==95?GLOBAL_RECORD_TOGGLE:note==89?GLOBAL_CLICK_TOGGLE:note==86?GLOBAL_LOOP_TOGGLE:note==74||note==75||note==79?CF_AUTOMATION:note==80?GLOBAL_SAVE:note==81?(in->jog_shift?GLOBAL_REDO:GLOBAL_UNDO):note==82?GLOBAL_KEY_CANCEL:note==83?GLOBAL_KEY_ENTER:0;
 if(note>=96&&note<=99){
  static const unsigned ordinary[]={GLOBAL_KEY_UP,GLOBAL_KEY_DOWN,GLOBAL_KEY_LEFT,GLOBAL_KEY_RIGHT};
  static const unsigned zoom[]={GLOBAL_ZOOM_UP,GLOBAL_ZOOM_DOWN,GLOBAL_ZOOM_OUT,GLOBAL_ZOOM_IN};
  op=(surface->zoom_mode?zoom:ordinary)[note-96];
 }
 if(op)input_global_add(in,snapshot,op,note==74?1:note==75?2:0,now);
}
static void surface_general_press(Surface *surface,MirrorInput *in,const CopiedMirror *snapshot,unsigned note,int down,uint32_t now){
 if(down&&!surface->general_down[note])surface_general(surface,in,snapshot,note,now);
 surface->general_down[note]=down;
}
static int surface_pitch(Surface *s,MirrorInput *in,MirrorBank *bank,const CopiedMirror *snapshot,unsigned strip,int position,uint32_t now){
 if(stopping)return 1;
 if(bank->assignment==BA_QLINK&&bank->flip)input_qlink_pitch(in,bank,snapshot,strip,position,now);else if(bank->assignment==BA_EFFECT&&bank->flip)input_effect_pitch(in,bank,snapshot,strip,position,now);else input_pitch(in,bank,strip,position,now);motor_follow_report(&following.motors[strip],position,now,bank->assignment==BA_QLINK&&bank->flip?in->qlinks[bank->qlink_page*8+strip].pending:bank->assignment==BA_EFFECT&&bank->flip?in->effects[strip].pending:bank_field(bank,strip)<CF_COUNT&&in->desires[strip][bank_field(bank,strip)].pending);s->display_field[strip]=bank_field(bank,strip);s->display_until[strip]=now>UINT32_MAX-2000?UINT32_MAX:now+2000;
 return surface_servo(s,bank,strip,position,now);
}
#endif
static void surface_close(Surface *s,MirrorBank *bank){
#ifdef MIRROR_INPUT
 if(s->seq&&stop_adapter_exe&&s->stop_sink>=0){snd_seq_delete_simple_port(s->seq,s->stop_sink);s->stop_sink=-1;}
#endif
#ifdef MIRROR_INPUT
 if(s->seq&&(bank->view==BV_DRUM_PADS||bank->assignment==BA_SEND)&&!stopping)(void)parent_led_clear(s);
#endif
#ifdef MIRROR_INPUT
 input_discard(&input);input_jog_discard(&input);input_master_discard(&input);
#endif
#ifdef MIRROR_INPUT
 motor_follow_reset(&following);
#endif
 if(s->seq)snd_seq_close(s->seq);
 memset(s,0,sizeof(*s));bank_disconnect(bank);bank->fader_field=CF_VOLUME;bank->assignment=BA_TRACK;bank->flip=0;}
/* 1 drained, 0 connection/input continuity lost. No output is queued. */
#ifdef MIRROR_INPUT
static int surface_stop_route(Surface *s,const MirrorState *state){
 int adapter_pid=0,mpc_pid=0;
 if(!stop_adapter_exe||stop_find_port(s->seq,"mpclearn-controls","mpclearn-controls MIDI",stop_adapter_exe,SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,SND_SEQ_PORT_CAP_WRITE,&s->stop_adapter,&adapter_pid)!=1||stop_find_port(s->seq,"MPC","mpclearn-controls MIDI","/usr/bin/MPC",SND_SEQ_PORT_CAP_WRITE,SND_SEQ_PORT_CAP_READ,&s->stop_mpc,&mpc_pid)!=1||(unsigned)mpc_pid!=state->pid)return 0;
 snd_seq_port_subscribe_t *sub;snd_seq_port_subscribe_alloca(&sub);snd_seq_port_subscribe_set_sender(sub,&s->stop_adapter);snd_seq_port_subscribe_set_dest(sub,&s->stop_mpc);
 return snd_seq_get_port_subscription(s->seq,sub)>=0;
}
/* Called only for the exact adapter's private relay. Classify before sending;
 * no post-send source sample can reinterpret this same physical press. */
static int surface_stop_relay(Surface *s,MirrorInput *in,const CopiedMirror *snapshot,int copied,int down,uint32_t now){
 if(down&&in->stop_down)return 1;
 if(copied&&snapshot->ready&&snapshot->alive&&!snapshot->error)input_stop_button(in,snapshot,93,down,now);
 else{in->stop_home=0;input_jog_release(in,3);in->stop_down=down;}
 snd_seq_event_t out;snd_seq_ev_clear(&out);
 if(down)snd_seq_ev_set_noteon(&out,0,93,127);else snd_seq_ev_set_noteoff(&out,0,93,0);
 snd_seq_ev_set_source(&out,s->source);snd_seq_ev_set_dest(&out,s->stop_mpc.client,s->stop_mpc.port);snd_seq_ev_set_direct(&out);
 int rc=snd_seq_event_output_direct(s->seq,&out);
 if(rc<0){in->stop_home=0;input_jog_release(in,3);fprintf(stderr,"Relayed Stop output failed: %s; no replay\n",snd_strerror(rc));return 0;}
 return 1;
}
#endif
static int surface_drain(Surface *s,MirrorBank *bank,const MirrorState *state,int discard){
 if(stopping)return 1;
#ifdef MIRROR_INPUT
 CopiedMirror snapshot;int batch_valid=0,batch_pads=0;
#endif
 for(unsigned n=0;n<256;n++){
  snd_seq_event_t *event;int rc=snd_seq_event_input(s->seq,&event);if(rc==-EAGAIN)return !lost_events(s->seq);if(rc<0)return 0;
  if(event->source.client==SND_SEQ_CLIENT_SYSTEM&&event->source.port==SND_SEQ_PORT_SYSTEM_ANNOUNCE){
   if(event->type==SND_SEQ_EVENT_CLIENT_EXIT&&event->data.addr.client==s->full.client)return 0;
   if(event->type==SND_SEQ_EVENT_PORT_EXIT&&address_equal(event->data.addr,s->full))return 0;
   if(event->type==SND_SEQ_EVENT_PORT_UNSUBSCRIBED&&address_equal(event->data.connect.dest,s->ingress))return 0;
  }
#ifdef MIRROR_INPUT
  if(stop_adapter_exe&&event->dest.port==s->stop_sink){
   if(discard)continue;
   if(!surface_stop_route(s,state))return 0;
   if(!address_equal(event->source,s->stop_adapter)||(event->type!=SND_SEQ_EVENT_NOTEON&&event->type!=SND_SEQ_EVENT_NOTEOFF)||event->data.note.channel||event->data.note.note!=93||event->data.note.velocity>127)continue;
   uint32_t at=source_now(state);CopiedMirror before={0};int fresh=at==UINT32_MAX?0:fresh_copy(state,&before,&at);
   if(!surface_stop_relay(s,&input,&before,fresh>0&&!atomic_load(&input.commands->new_project_intent),event->type==SND_SEQ_EVENT_NOTEON&&event->data.note.velocity,at))return 0;
   if(fresh<0)return 0;
   continue;
  }
#endif
  unsigned kind,channel;int value,match=surface_event(event,s->full,&kind,&channel,&value);if(match<0)return 0;
  if(stopping)return 1;
  if(match&&!discard){
   uint32_t now=source_now(state);if(now==UINT32_MAX)return 0;
#ifdef MIRROR_INPUT
   int pads=bank->view==BV_DRUM_PADS;
   /* A bounded MIDI batch shares one source observation. Desires coalesce in
    * order; publication below takes a new snapshot and native execution still
    * checks each target/revision. A topology or view change invalidates it. */
   int copied=batch_valid;
   if(!batch_valid||batch_pads!=pads||!mirror_selection_current(state,&snapshot)||snapshot.revision!=atomic_load_explicit(&state->revision,memory_order_acquire)||snapshot.epoch!=atomic_load(&state->epoch)){
    copied=fresh_copy_view(state,&snapshot,&now,pads);if(copied<0)return 0;batch_valid=copied;batch_pads=pads;
   }
   uint32_t heartbeat=atomic_load_explicit(&state->heartbeat,memory_order_acquire);now=source_now(state);
   if(now==UINT32_MAX||!atomic_load(&state->alive)||atomic_load(&state->error)||now<heartbeat||now-heartbeat>=1000)return 0;
   if(copied&&(now<snapshot.heartbeat||now-snapshot.heartbeat>=1000))return 0;
   if(copied)bank_apply(bank,&snapshot,now);else bank_drop(bank,now);
   input_sync(&input,bank);
   if(!snapshot.ready||atomic_load(&input.commands->new_project_intent)){
    unsigned held_shift=input.jog_shift;input_discard(&input);input_jog_discard(&input);input.jog_shift=held_shift;input_master_invalidate(&input);
    if(kind==1)bank_touch(bank,channel,value,now);
    else if(kind==7){s->master_touch=value;s->master_tick=now;}
    else if(kind==3){bank->faders[channel].physical=value;motor_follow_report(&following.motors[channel],value,now,0);}
    else if(kind==10)motor_follow_report(&following.motors[8],value,now,0);
    else if(kind==9&&channel==70)input.jog_shift=value;
    else if(kind==6&&channel==50){if(value&&!s->general_down[channel]&&input.jog_shift)surface_assignment(s,&input,bank,&snapshot,channel,now);s->general_down[channel]=value;}
    else if(kind==5)s->button_down[channel]=value;
    else if(kind==6||kind==11)s->general_down[channel]=value;
    else if(kind==2)s->bank_down[channel]=value;
    continue;
   }
   if(kind==1){s->display_until[channel]=now>UINT32_MAX-2000?UINT32_MAX:now+2000;input_touch(&input,bank,&snapshot,channel,value,now);INPUT_LOG("TOUCH fader=%u %s; channel motion input=%u tick=%u\n",channel+1,value?"down":"up",input.gestures[channel].held,now);}
   else if(kind==7){s->master_touch=value;input_master_touch(&input,&snapshot,value);}
   else if(kind==10){input_master_pitch(&input,&snapshot,value,now);motor_follow_report(&following.motors[8],value,now,input.master.pending);}
   else if(kind==12)input_stop_button(&input,&snapshot,channel,value,now);
   else if(kind==8||kind==9)surface_jog(&input,&snapshot,kind,channel,value,now);
   else if(kind==11)surface_general_press(s,&input,&snapshot,channel,value,now);
   else if(kind==3){if(!surface_pitch(s,&input,bank,&snapshot,channel,value,now))return 0;}
   else if(kind==4)surface_encoder(s,&input,bank,&snapshot,channel,value,0,now);
   else if(kind==5){if(value&&!s->button_down[channel]){
    unsigned strip=channel%8;
    if(channel>=32)surface_encoder(s,&input,bank,&snapshot,strip,0,1,now);
    else{unsigned fields[4]={CF_ARM,CF_SOLO,CF_MUTE,CF_SELECTION};unsigned field=fields[channel/8];input_control(&input,bank,&snapshot,strip,field,0,1,now);s->display_field[strip]=field;s->display_until[strip]=now>UINT32_MAX-2000?UINT32_MAX:now+2000;}
   }s->button_down[channel]=value;}
   else if(kind==6){if(value&&!s->general_down[channel])surface_assignment(s,&input,bank,&snapshot,channel,now);s->general_down[channel]=value;}
   else{if(value&&!s->bank_down[channel])surface_navigation(&input,bank,&snapshot,channel,now);s->bank_down[channel]=(unsigned)value;}
#else
   if(kind==1){bank_touch(bank,channel,value,now);INPUT_LOG("TOUCH fader=%u %s state=%d tick=%u\n",channel+1,value?"down":"up",bank->faders[channel].touch,now);}
   else if(kind==2){if(value&&!s->bank_down[channel]){if(channel<2)bank_navigate(bank,channel?1:-1,now);else bank_channel(bank,channel==3?1:-1,now);BRIDGE_LOG("BANK first_playable=%u\n",bank->offset+1);}s->bank_down[channel]=(unsigned)value;}
#endif
  }
 }
 return snd_seq_event_input_pending(s->seq,1)==0&&!lost_events(s->seq);
}
static int surface_open(Surface *s,MirrorBank *bank,const MirrorState *state,snd_seq_t *discovery){
 /* Enumerate through a retained portless client. An absent controller must
  * not create/destroy ALSA clients and broadcast topology changes at4Hz. */
 snd_seq_addr_t found;
 if(!discover(discovery,&found))return 0;
#ifdef MIRROR_INPUT
 s->stop_sink=-1;
#endif
 if(snd_seq_open(&s->seq,"default",SND_SEQ_OPEN_DUPLEX,SND_SEQ_NONBLOCK)<0)return 0;
 if(snd_seq_set_client_name(s->seq,"mpclearn-mirror-motors")<0||!discover(s->seq,&s->full)||snd_seq_set_client_pool_input(s->seq,256)<0||snd_seq_set_client_pool_output(s->seq,16)<0)goto fail;
 s->sink=snd_seq_create_simple_port(s->seq,"private MCU touch and bank",SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT,SND_SEQ_PORT_TYPE_APPLICATION);
 s->source=snd_seq_create_simple_port(s->seq,"MCU eight motor output",SND_SEQ_PORT_CAP_READ,SND_SEQ_PORT_TYPE_APPLICATION);
 if(s->sink<0||s->source<0)goto fail;
 s->ingress=(snd_seq_addr_t){.client=snd_seq_client_id(s->seq),.port=s->sink};
 if(snd_seq_connect_from(s->seq,s->sink,s->full.client,s->full.port)<0||snd_seq_connect_from(s->seq,s->sink,SND_SEQ_CLIENT_SYSTEM,SND_SEQ_PORT_SYSTEM_ANNOUNCE)<0)goto fail;
#ifdef MIRROR_INPUT
 if(stop_adapter_exe&&!surface_stop_route(s,state))goto fail;
#endif
 if(!surface_drain(s,bank,state,1))goto fail;
#ifdef MIRROR_INPUT
 /* Publish takeover only after the startup discard. Until then the adapter
  * forwards ordinary Stop itself, so no routed press is intentionally dropped. */
 if(stop_adapter_exe){
  s->stop_sink=snd_seq_create_simple_port(s->seq,STOP_RELAY_PORT,SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT,SND_SEQ_PORT_TYPE_APPLICATION);
  if(s->stop_sink<0)goto fail;
 }
#endif
 bank_disconnect(bank);
#ifdef MIRROR_INPUT
 for(unsigned i=0;i<8;i++)if(inherited_holds&(1u<<i))bank->faders[i].touch=MT_DOWN;
 s->master_touch=(inherited_holds>>8)&1;inherited_holds=0;
#endif
 BRIDGE_LOG("CONNECTED X-Touch=%u:%u; automatic fresh-source sync unless touch-down observed; physical touch on connection unknown\n",s->full.client,s->full.port);return 1;
fail:surface_close(s,bank);return 0;
}
static void motor_event(snd_seq_event_t *event,const Surface *surface,unsigned channel,int position){snd_seq_ev_clear(event);snd_seq_ev_set_pitchbend(event,channel,position-8192);snd_seq_ev_set_source(event,surface->source);snd_seq_ev_set_dest(event,surface->full.client,surface->full.port);snd_seq_ev_set_direct(event);}
#ifdef MIRROR_INPUT
static int channel_send(Surface *s,snd_seq_event_t *e){if(stopping)return 0;snd_seq_ev_set_source(e,s->source);snd_seq_ev_set_dest(e,s->full.client,s->full.port);snd_seq_ev_set_direct(e);return snd_seq_event_output_direct(s->seq,e)>=0;}
/* TouchMCU9ea4881 protocol thresholds; Ardour e0d72575b1 confirms the
 * channel-pressure framing and per-strip enable. Scaling is host-specific.
 * Use max(L,R) native envelope amplitude, no invented held-overload latch. */
static unsigned meter_level(const CopiedMeter *m){
 float l,r;memcpy(&l,&m->left,4);memcpy(&r,&m->right,4);float peak=l>r?l:r;
 if(!(peak>0))return 0;
 float db=20.0f*log10f(peak);static const float threshold[]={-60,-50,-40,-30,-20,-14,-10,-8,-6,-4,-2,0};
 unsigned level=0;while(level<12&&db>=threshold[level])level++;
 return db>0?13:level;
}
static int meter_output(Surface *s,const MirrorBank *bank,const CopiedMirror *snapshot){
 if(!bank->ready&&!s->meter_valid)return 1;
 unsigned due=!s->meter_valid||snapshot->heartbeat<s->meter_tick||snapshot->heartbeat-s->meter_tick>=50;
 for(unsigned i=0;i<8;i++){
  const CopiedTrack *t=bank->ready?input_track(snapshot,&bank->strips[i]):NULL;
  unsigned enabled=t&&t->meter.available,id=enabled?t->meter.incarnation:0;
  snd_seq_event_t e;
  if(!s->meter_valid||s->meter_enabled[i]!=enabled||s->meter_identity[i]!=id){
   unsigned char bytes[]={0xf0,0,0,0x66,0x14,0x20,(unsigned char)i,enabled?7:0,0xf7};
   snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,sizeof(bytes),bytes);if(!channel_send(s,&e))return 0;
   snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,(i<<4)|0xf);if(!channel_send(s,&e))return 0;
   if(!enabled){snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,i<<4);if(!channel_send(s,&e))return 0;}
   s->meter_enabled[i]=enabled;s->meter_identity[i]=id;
  }
  /* Sustained native levels refresh hardware's own decay. A stale source is
   * disabled rather than refreshed with an invented zero or synthetic decay. */
  if(enabled&&due){snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,(i<<4)|meter_level(&t->meter));if(!channel_send(s,&e))return 0;}
 }
 s->meter_valid=1;if(due)s->meter_tick=snapshot->heartbeat;return 1;
}
static int chooser_output(Surface *s){
 if(s->chooser_cleared)return 1;
 snd_seq_event_t e;unsigned char bytes[15],blank[7];memset(blank,' ',7);
 /* Presentation only: no source values or native input are manufactured. */
 for(unsigned note=0;note<=103;note++){snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,note,0);if(!channel_send(s,&e))return 0;}
 for(unsigned note=113;note<=114;note++){snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,note,0);if(!channel_send(s,&e))return 0;}
 for(unsigned i=0;i<10;i++){snd_seq_ev_clear(&e);snd_seq_ev_set_controller(&e,0,0x49-i,' ');if(!channel_send(s,&e))return 0;}
 for(unsigned i=0;i<8;i++){
  for(unsigned row=0;row<2;row++){snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,channel_lcd(bytes,i,row,blank),bytes);if(!channel_send(s,&e))return 0;}
  snd_seq_ev_clear(&e);snd_seq_ev_set_controller(&e,0,0x30+i,0);if(!channel_send(s,&e))return 0;
  unsigned char off[]={0xf0,0,0,0x66,0x14,0x20,(unsigned char)i,0,0xf7};
  snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,sizeof(off),off);if(!channel_send(s,&e))return 0;
  snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,(i<<4)|15);if(!channel_send(s,&e))return 0;
  snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,i<<4);if(!channel_send(s,&e))return 0;
 }
 memset(s->colors,0,sizeof(s->colors));snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,channel_colors(bytes,s->colors),bytes);if(!channel_send(s,&e))return 0;
 memset(s->wire_valid,0,sizeof(s->wire_valid));memset(s->state_valid,0,sizeof(s->state_valid));
 s->colors_valid=s->mode_valid=s->position_valid=s->playing_valid=s->meter_valid=0;s->chooser_cleared=1;return 1;
}
static int motor_output(Surface *s,unsigned channel,const uint32_t key[16],int target,uint32_t now,int allowed){
 MotorFollow *m=following.motors+channel;motor_follow_target(m,key,target,now);
 int position=motor_follow_due(m,now,allowed&&following.enabled&&!stopping);if(position<0)return 1;
 snd_seq_event_t e;motor_event(&e,s,channel,position);if(!channel_send(s,&e))return 0;motor_follow_sent(m,position,now);return 1;
}
static int strip_motor_ready(MirrorBank *bank,unsigned channel,uint32_t now){
 uint32_t key[16]={0};MirrorFader *f=bank->faders+channel;
 memcpy(key,&f->identity,sizeof(MotorIdentity));if(f->effect[0])memcpy(key+11,f->effect,sizeof(f->effect));else{key[11]=f->field_incarnation;key[12]=bank_field(bank,channel);key[15]=bank->chooser?2:f->empty?3:1;}
 int target=bank_target(bank,channel,now);if(target>=0)motor_follow_target(&following.motors[channel],key,target,now);
 if(bank->assignment==BA_QLINK&&bank->flip){
  if(input_mode_busy(&input))return 0;
  unsigned index=bank->qlink_page*8+channel;
  InputQLink *g=input.qlinks+index;
  unsigned same=g->request.epoch==bank->epoch&&g->request.qlink_generation==f->effect[0]&&g->request.qlink_wrapper==f->effect[2]&&g->request.qlink_provider==f->effect[3]&&g->request.qlink_root==f->effect[4];
  if(same&&g->wait_tick&&f->qlink_kind)for(unsigned lane=0;lane<COMMAND_LANES;lane++)if(f->qlink_matches[lane]>g->barriers[lane]){g->wait_tick=0;break;}
  if(same&&(g->pending||g->wait_tick))return 0;
  for(unsigned i=0;i<COMMAND_SLOTS;i++){const InputFlight *tx=input.flights+i;if(tx->flight&&!tx->acknowledged&&command_qlink(tx->field)&&tx->qlink_request.qlink_index==index&&tx->qlink_request.qlink_wrapper==f->effect[2])return 0;}
 }
 if(bank->assignment==BA_EFFECT&&bank->flip){
  if(input.effects[channel].pending)return 0;
  for(unsigned i=0;i<COMMAND_SLOTS;i++){const InputFlight *tx=input.flights+i;if(tx->flight&&!tx->acknowledged&&tx->field==EFFECT_PARAMETER&&tx->effect_request.program_owner==f->identity.program_owner&&tx->effect_request.effect_slot==f->effect[1]&&tx->effect_request.effect_index==f->effect[2])return 0;}
 }
 return !input_blocked_field(&input,channel,bank_field(bank,channel))&&bank_due(bank,channel,now)>=0&&motor_follow_due(&following.motors[channel],now,following.enabled&&!stopping)>=0;
}
/* Called again after the production loop's immediate source/USB revalidation. */
static int strip_motor_output(Surface *s,MirrorBank *bank,unsigned channel,uint32_t now){
 if(!strip_motor_ready(bank,channel,now))return 0;
 int position=motor_follow_due(&following.motors[channel],now,following.enabled&&!stopping);if(position<0)return 0;
 snd_seq_event_t event;motor_event(&event,s,channel,position);
 if(snd_seq_event_output_direct(s->seq,&event)<0)return -1;
 motor_follow_sent(&following.motors[channel],position,now);
 MirrorFader *f=bank->faders+channel;f->last_sent=position;f->last_tick=now;
 /* A command invalidates the preceding physical observation. */
 f->physical=-1;return 1;
}
static int parent_led_clear(Surface *s){
 for(unsigned i=0;i<8;i++)if(!s->wire_valid[i]||s->wire[i].led[3]){
  snd_seq_event_t e;snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,24+i,0);if(!channel_send(s,&e))return 0;s->wire[i].led[3]=0;
 }
 return 1;
}
/* Each Send strip is a physical encoder/fader pair. Six readable characters
 * leave the normal LCD separator; neither Name/Value nor edits swap rows. */
static void send_pair_text(unsigned char out[7],const CopiedMirror *snapshot,const CopiedTrack *t,unsigned field,unsigned slot,int ret,int names){
 char text[24];CopiedField volume;const CopiedField *f=t?copied_field(snapshot,t,field,&volume):NULL;
 float value=0;if(f)memcpy(&value,&f->bits,4);
 if(!t||!mixable(t->vptr)||!f||!f->available||!isfinite(value)||value<0||value>1)snprintf(text,sizeof(text),"%c%u ---",ret?'R':'S',slot+1);
 else if(names)snprintf(text,sizeof(text),"%s%u",ret?"Rtn":"Send",slot+1);
 else if(!ret){if(value>=0.995f)snprintf(text,sizeof(text),"S%u100%%",slot+1);else snprintf(text,sizeof(text),"S%u %2.0f%%",slot+1,value*100.0f);}
 else{
  float db=value>0?20.0f*log10f(value*value*1.9952623844146729f):-INFINITY;
  if(db<=-96)snprintf(text,sizeof(text),"R%u-INF",slot+1);
  else if(db<=-9.95f)snprintf(text,sizeof(text),"R%u%.0f",slot+1,db);
  else snprintf(text,sizeof(text),"R%u%+.1f",slot+1,fabsf(db)<0.05f?0:db);
 }
 channel_ascii(out,text,strlen(text));
}
static int channel_output(Surface *s,const MirrorBank *bank,const CopiedMirror *snapshot,uint32_t now){
 if(bank->assignment!=BA_QLINK||bank->chooser||!bank->ready){mode_notice_until=mode_notice_controller=mode_notice_generation=mode_notice_epoch=0;}
 if(bank->chooser){
  if(!s->chooser_cleared){s->master_sent=-1;s->master_tick=bank->heartbeat;}
  if(!chooser_output(s))return 0;
  uint32_t key[16]={0};key[15]=2;
  if(!motor_output(s,8,key,0,now,!s->master_touch&&bank->heartbeat>s->master_tick&&bank->heartbeat>following.barrier))return 0;
  return 1;
 }
 s->chooser_cleared=0;
 if(!meter_output(s,bank,snapshot))return 0;
 if(!bank->ready){
  if((bank->view==BV_DRUM_PADS||bank->assignment==BA_SEND)&&!parent_led_clear(s))return 0;
  for(unsigned i=0;i<9;i++)following.motors[i].bound=0;
  return 1;
 } /* unavailable topology is not an empty bank */
 snd_seq_event_t global;
 unsigned char position[10];channel_position(position,snapshot);
 for(unsigned i=0;i<10;i++)if(!s->position_valid||position[i]!=s->position_text[i]){snd_seq_ev_clear(&global);snd_seq_ev_set_controller(&global,0,0x49-i,position[i]);if(!channel_send(s,&global))return 0;}
 unsigned beats=snapshot->position_available?127:0;
 if(!s->position_valid||s->mode_led[114]!=beats){for(unsigned note=113;note<=114;note++){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,note,note==114?beats:0);if(!channel_send(s,&global))return 0;}s->mode_led[113]=0;s->mode_led[114]=beats;}
 memcpy(s->position_text,position,10);s->position_valid=1;
 unsigned notes[]={40,41,42,43,44,50,51,62,63,64,65,66,67,68,69};
 unsigned modes[]={bank->assignment==BA_TRACK,bank->assignment==BA_SEND,bank->assignment==BA_PAN,bank->assignment==BA_EFFECT,bank->assignment==BA_QLINK,bank->flip,bank->view==BV_ALL,bank->view==BV_MIDI,bank->view==BV_INPUT||bank->assignment==BA_IO,bank->view==BV_AUDIO,bank->view==BV_INSTRUMENT,bank->view==BV_RETURN||bank->view==BV_DRUM_PADS,bank->view==BV_SUBMIX,bank->view==BV_OUTPUT,bank->view==BV_ALL};
 for(unsigned i=0;i<sizeof(notes)/sizeof(notes[0]);i++)if(!s->mode_valid||s->mode_led[notes[i]]!=modes[i]){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,notes[i],modes[i]?127:0);if(!channel_send(s,&global))return 0;s->mode_led[notes[i]]=modes[i];}
 s->mode_valid=1;
 if(snapshot->playing.available){unsigned playing=snapshot->playing.bits;if(!s->playing_valid||s->playing_value!=playing){for(unsigned i=0;i<2;i++){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,93+i,(i?playing:!playing)?127:0);if(!channel_send(s,&global))return 0;}s->playing_valid=1;s->playing_value=playing;}}
 else if(!s->playing_valid||s->playing_value!=2){for(unsigned i=0;i<2;i++){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,93+i,0);if(!channel_send(s,&global))return 0;}s->playing_valid=1;s->playing_value=2;}
 unsigned auto_led=snapshot->automation_detail_available&&!snapshot->automation_mixed?127:1;
 unsigned record=0;
 if(snapshot->record_mode.available){
  if(snapshot->record_mode.bits==3)record=1;
  else if(snapshot->record_mode.bits==1&&snapshot->playing.available)record=snapshot->playing.bits?127:1;
 }
 unsigned state_notes[7]={74,75,79,86,95,89,100},state_values[7]={snapshot->automation.available&&snapshot->automation.bits==1?auto_led:0,snapshot->automation.available&&snapshot->automation.bits==2?auto_led:0,snapshot->automation.available&&snapshot->automation.bits==0?auto_led:0,snapshot->loop.available&&snapshot->loop.bits?127:0,record,snapshot->click.available&&snapshot->click.bits?127:0,s->zoom_mode?127:0};
 for(unsigned i=0;i<7;i++)if(!s->mode_valid||s->mode_led[state_notes[i]]!=state_values[i]||!s->state_valid[i]){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,state_notes[i],state_values[i]);if(!channel_send(s,&global))return 0;s->mode_led[state_notes[i]]=state_values[i];s->state_valid[i]=1;}
 /* Master input/feedback belongs to the distinct rooted MpcMixer owner.
  * Pending native source settlement inhibits only its motor. */
 const CopiedField *master=&snapshot->master;
 if(s->master_owner!=master->owner_incarnation||s->master_incarnation!=master->incarnation){s->master_owner=master->owner_incarnation;s->master_incarnation=master->incarnation;s->master_sent=-1;}
 if(master->available){float v;memcpy(&v,&master->bits,4);if(isfinite(v)&&v>=0&&v<=1){uint32_t key[16]={0};key[0]=snapshot->epoch;key[1]=master->owner_incarnation;key[2]=master->incarnation;key[15]=1;
  if(!motor_output(s,8,key,(int)(v*16383.0f+0.5f),now,!s->master_touch&&!input_master_blocked(&input)&&bank->heartbeat>following.barrier))return 0;
 }}else following.motors[8].bound=0;

 if(bank->assignment!=BA_EFFECT){effect_notice_until=0;effect_notice_slot=EFFECT_LIST;}
 unsigned strip=s->next_wire++%8;const CopiedTrack *t=input_track(snapshot,&bank->strips[strip]);
 ChannelWire w=channel_wire(snapshot,t,s->display_field[strip]);
 if(bank->view==BV_DRUM_PADS&&!t){
  if(bank->offset+strip<snapshot->pad_count){char label[8];unsigned index=bank->offset+strip;snprintf(label,sizeof(label),"%c%02u",'A'+index/16,index%16+1);channel_ascii(w.name,label,strlen(label));channel_ascii(w.value,"------",6);w.color=7;}
 }
 const CopiedTrack *encoder=input_track(snapshot,bank_encoder_identity(bank,strip));
 unsigned ring_field=bank_encoder_field(bank,strip);w.ring=0;
 if(encoder&&ring_field<CF_COUNT){CopiedField volume;const CopiedField *rf=copied_field(snapshot,encoder,ring_field,&volume);if(rf&&rf->available&&(ring_field==CF_MIDI_VOLUME?midi_volume(encoder->vptr):mixable(encoder->vptr))){float x;memcpy(&x,&rf->bits,4);w.ring=(unsigned)(x*10.0f+0.5f)+1;if(ring_field==CF_PAN&&x==0.5f)w.ring|=64;}}
 if(bank->assignment==BA_EFFECT){
  const EffectsCopy *e=&snapshot->effects;if(!bank->flip)w.ring=0;memset(w.name,' ',7);memset(w.value,' ',7);
  int current=snapshot->effects_available&&e->serial==bank->selected.serial&&e->slot==bank->effects_slot&&e->page==bank->effects_page;
  if(bank->effects_slot==EFFECT_LIST){
   if(strip<4){char label[8];const EffectSlot *slot=e->slots+strip;const char *enabled=current&&slot->enable_valid&&slot->enable_tick<=now&&now-slot->enable_tick<EFFECT_FRESH_MS?(slot->enable_bits?"On":"Off"):"---";snprintf(label,sizeof(label),"%u %s",strip+1,enabled);channel_ascii(w.name,label,strlen(label));const char *name=current&&e->slots[strip].status==EF_READY?e->slots[strip].name:current&&e->slots[strip].status==EF_EMPTY?"Empty":e->serial==bank->selected.serial&&e->status>=EF_UNSUPPORTED?"N/A":"Wait";channel_label(w.value,name,strlen(name));}
   else if(strip==6){channel_ascii(w.name,"Select",6);channel_ascii(w.value,"Push",4);}
   else if(strip==7){const CopiedTrack *selected=input_track(snapshot,&bank->selected);channel_ascii(w.name,"Source",6);if(selected&&selected->fields[CF_NAME].available)channel_label(w.value,selected->fields[CF_NAME].text,selected->fields[CF_NAME].length);}
  }else if(current&&e->status==EF_READY){const EffectParameter *p=e->parameters+strip;if(p->status==EF_READY&&p->tick<=snapshot->heartbeat&&snapshot->heartbeat-p->tick<EFFECT_FRESH_MS){channel_label(w.name,p->name,strnlen(p->name,EFFECT_TEXT));channel_value(w.value,p->text,strnlen(p->text,EFFECT_TEXT));float value;memcpy(&value,&p->bits,4);if(!bank->flip&&value>=0&&value<=1)w.ring=(unsigned)(value*10+0.5f)+1;}else if(p->status!=EF_EMPTY){channel_ascii(w.name,p->status>=EF_UNSUPPORTED?"N/A":"Wait",p->status>=EF_UNSUPPORTED?3:4);}}
  else if(strip==0){channel_ascii(w.name,"Effect",6);channel_ascii(w.value,e->serial==bank->selected.serial&&e->status>=EF_UNSUPPORTED?"N/A":"Wait",e->serial==bank->selected.serial&&e->status>=EF_UNSUPPORTED?3:4);}
  if(current&&e->status==EF_READY&&e->page_count&&bank->effects_slot<EFFECT_SLOTS){
   if(effect_notice_epoch!=e->epoch||effect_notice_generation!=e->generation||effect_notice_serial!=e->serial||effect_notice_slot!=e->slot||effect_notice_page!=e->page){effect_notice_epoch=e->epoch;effect_notice_generation=e->generation;effect_notice_serial=e->serial;effect_notice_slot=e->slot;effect_notice_page=e->page;effect_notice_until=now+2000;}
   if(effect_notice_until>now&&!(page_notice_until>now)&&!(preference_notice_until>now)){
    if(strip<6){unsigned length=strnlen(e->page_name,EFFECT_TEXT),start=strip*6;channel_ascii(w.name,start<length?e->page_name+start:"",start<length?length-start:0);}
    else if(strip==6)channel_ascii(w.name,"Page",4);
    else{char page[16];snprintf(page,sizeof(page),"%u/%u",e->page+1,e->page_count);channel_ascii(w.name,page,strlen(page));}
   }
  }else{effect_notice_until=0;effect_notice_slot=EFFECT_LIST;}
  if(current&&e->chooser.serial==e->serial&&e->chooser.request&&e->chooser.status==EC_OPEN&&strip==6){channel_ascii(w.name,"Choose",6);channel_ascii(w.value,"On MPC",6);}
  w.color=memcmp(w.name,"       ",7)||memcmp(w.value,"       ",7)?7:0;
  /* Track faders/buttons retain their normal targets and temporary feedback. */
  if((now<s->display_until[strip]&&s->display_field[strip]<CF_COUNT)||(!bank->flip&&bank->faders[strip].touch==MT_DOWN)){ChannelWire track=channel_wire(snapshot,t,s->display_field[strip]);memcpy(w.name,track.name,7);memcpy(w.value,track.value,7);w.color=track.color;}
 }
 else if(bank->assignment==BA_IO){
  static const char *labels[]={"Monitr","InPort","InChan","SendTo","AudOut","OutPrt","OutChn","AudIn"};const IOField *f=snapshot->io.fields+strip;
  channel_ascii(w.name,labels[strip],strlen(labels[strip]));memset(w.value,' ',7);w.ring=0;w.color=7;
  if(snapshot->io_available&&snapshot->io.serial==bank->selected.serial&&f->status==IO_READY&&f->tick<=now&&now-f->tick<IO_FRESH_MS){channel_routing(w.value,f->text,strnlen(f->text,IO_TEXT));if(f->choice_count>1)w.ring=1+(10*f->choice_index)/(f->choice_count-1);}
  else{const char *text=snapshot->io_available&&f->status==IO_NOT_APPLICABLE?"--":"N/A";channel_ascii(w.value,text,strlen(text));}
  if((now<s->display_until[strip]&&s->display_field[strip]<CF_COUNT)||bank->faders[strip].touch==MT_DOWN){ChannelWire track=channel_wire(snapshot,t,s->display_field[strip]);memcpy(w.name,track.name,7);memcpy(w.value,track.value,7);w.color=track.color;}
 }
 else if(bank->assignment==BA_QLINK){
  effect_notice_slot=EFFECT_LIST;
  const QLinkCopy *q=&snapshot->qlinks;unsigned index=bank->qlink_page*8+strip;
  if(snapshot->qlinks_available&&q->mode_valid&&q->mode_tick<=now&&now-q->mode_tick<QLINK_FRESH_MS){
   if(mode_notice_controller!=q->mode_controller||mode_notice_generation!=q->mode_generation||mode_notice_epoch!=snapshot->epoch||mode_notice_id!=q->mode_id){mode_notice_controller=q->mode_controller;mode_notice_generation=q->mode_generation;mode_notice_epoch=snapshot->epoch;mode_notice_id=q->mode_id;mode_notice_until=now+2000;}
  }else mode_notice_until=0;
  memset(w.name,' ',7);memset(w.value,' ',7);if(!bank->flip)w.ring=0;
  const QLinkSlot *p=q->slots+index;
  int current=snapshot->qlinks_available&&q->epoch==snapshot->epoch&&q->status==QL_SAMPLED&&p->sampled&&p->normalized_valid&&p->tick<=now&&now-p->tick<QLINK_FRESH_MS;
  if(current){
   channel_qlink_label(w.name,p->name,strnlen(p->name,QLINK_TEXT),index);channel_value(w.value,p->text,strnlen(p->text,QLINK_TEXT));
   if(!bank->flip){float value;memcpy(&value,&p->bits,4);if(value>=0&&value<=1)w.ring=(unsigned)(value*10+0.5f)+1;}
  }else{char label[8];snprintf(label,sizeof(label),"Q%u",index+1);channel_ascii(w.name,label,strlen(label));channel_ascii(w.value,"Wait",4);}
  w.color=memcmp(w.name,"       ",7)||memcmp(w.value,"       ",7)?7:0;
  if((now<s->display_until[strip]&&s->display_field[strip]<CF_COUNT)||(!bank->flip&&bank->faders[strip].touch==MT_DOWN)){ChannelWire track=channel_wire(snapshot,t,s->display_field[strip]);memcpy(w.name,track.name,7);memcpy(w.value,track.value,7);w.color=track.color;}
 }
 else if(bank->assignment==BA_SEND){
  const CopiedTrack *selected=input_track(snapshot,&bank->selected);
  memset(w.name,' ',7);memset(w.value,' ',7);memset(w.led,0,sizeof(w.led));
  if(strip<4){
   unsigned char send[7],ret[7];
   unsigned names=s->name_value&&now>=s->display_until[strip]&&bank->faders[strip].touch!=MT_DOWN;
   send_pair_text(send,snapshot,selected,CF_SEND1+strip,strip,0,names);
   send_pair_text(ret,snapshot,t,CF_VOLUME,strip,1,names);
   memcpy(w.name,bank->flip?ret:send,7);memcpy(w.value,bank->flip?send:ret,7);
  }else{w.ring=0;if(strip==7){channel_ascii(w.name,"Source",6);if(selected&&selected->fields[CF_NAME].available)channel_label(w.value,selected->fields[CF_NAME].text,selected->fields[CF_NAME].length);else channel_ascii(w.value,"Ch +/-",6);}}
  w.color=selected&&selected->fields[CF_COLOR].available?channel_palette(selected->fields[CF_COLOR].bits):7;
 }else if(s->name_value&&now>=s->display_until[strip]&&bank->faders[strip].touch!=MT_DOWN&&t){
  unsigned field=s->display_field[strip];const char *name=field==CF_PAN?"Pan":field==CF_MUTE?"Mute":field==CF_SOLO?"Solo":field==CF_ARM?"Record":field==CF_SELECTION?"Select":"Volume";
  channel_ascii(w.value,name,strlen(name));
 }
 if(bank->assignment==BA_QLINK&&mode_notice_until>now&&strip<2&&!(page_notice_until>now)&&!(preference_notice_until>now)){
  const char *name=qlink_mode_name(mode_notice_id);unsigned length=strlen(name),start=strip*6;w.color=7;channel_ascii(w.name,strip?"Mode":"Q-Link",strip?4:6);channel_ascii(w.value,start<length?name+start:"",start<length?length-start:0);
 }
 if(page_notice_until){
  if(snapshot->heartbeat<page_notice_until){if(strip==0){static const char *labels[]={"Main","Arrange","Clip","Trk Mix","Pad Mix","TrkEdit","SmpEdit","StepSeq"};if(bank->assignment==BA_EFFECT)w.color=7;channel_ascii(w.name,"Open",4);channel_ascii(w.value,labels[requested_page],strlen(labels[requested_page]));}}
  else{page_notice_until=0;memset(s->wire_valid,0,sizeof(s->wire_valid));}
 }
 if(preference_notice_until){
  if(snapshot->heartbeat<preference_notice_until){if(strip==0){if(bank->assignment==BA_EFFECT)w.color=7;channel_ascii(w.name,"Motors",6);channel_ascii(w.value,preference_notice_error?"SaveErr":following.enabled?"ON":"OFF",preference_notice_error?7:following.enabled?2:3);}}
  else{preference_notice_until=0;memset(s->wire_valid,0,sizeof(s->wire_valid));}
 }
 /* Drum Mix and Send Select LEDs identify the source Track in its normal
  * fixed bank, independently of the pad or send/return strip bindings. */
 if(bank->view==BV_DRUM_PADS||bank->assignment==BA_SEND){
  w.led[3]=0;
  if(bank->ready&&snapshot->ready&&snapshot->alive&&!snapshot->error&&snapshot->selection.available&&now>=snapshot->heartbeat&&now-snapshot->heartbeat<1000){
   const CopiedTrack *parent=input_track(snapshot,&bank->selected);unsigned ordinal=0;
   for(unsigned i=0;i<snapshot->count;i++)if(bank_role(BV_TRACK,snapshot->tracks[i].vptr)){
    if(snapshot->tracks+i==parent){w.led[3]=ordinal%8==strip?127:0;break;}ordinal++;
   }
  }
 }
 ChannelWire *old=s->wire+strip;unsigned valid=s->wire_valid[strip];snd_seq_event_t e;unsigned char bytes[15];
 for(unsigned row=0;row<2;row++){unsigned char *line=row?w.value:w.name,*before=row?old->value:old->name;if(!valid||memcmp(line,before,7)){snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,channel_lcd(bytes,strip,row,line),bytes);if(!channel_send(s,&e))return 0;}}
 for(unsigned i=0;i<4;i++)if(!valid||w.led[i]!=old->led[i]){snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,i*8+strip,w.led[i]);if(!channel_send(s,&e))return 0;}
 if(!valid||w.ring!=old->ring){snd_seq_ev_clear(&e);snd_seq_ev_set_controller(&e,0,0x30+strip,w.ring);if(!channel_send(s,&e))return 0;}
 s->colors[strip]=w.color;
 if(!s->colors_valid||!valid||w.color!=old->color){snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,channel_colors(bytes,s->colors),bytes);if(!channel_send(s,&e))return 0;s->colors_valid=1;}
 *old=w;s->wire_valid[strip]=1;return 1;
}
#endif
static int source_identity(const char *path,int fd,const struct stat *file,const MirrorState *s,unsigned pid,uint64_t start,struct stat *exe){
 struct stat live,opened;
 return !lstat(path,&live)&&!fstat(fd,&opened)&&S_ISREG(live.st_mode)&&same_stat(file,&live)&&same_stat(file,&opened)&&live.st_size==(off_t)sizeof(*s)&&opened.st_size==live.st_size&&s->magic==MIRROR_MAGIC&&s->version==MIRROR_VERSION&&s->bytes==sizeof(*s)&&s->pid==pid&&s->capacity==MIRROR_CELLS&&start_time(pid)==start&&process_file(pid,exe,0);
}
#ifdef MIRROR_INPUT
static int input_file_current(const char *path,const MirrorState *state,uint64_t start){
 struct stat at,fd;
 return input_mapping!=MAP_FAILED&&!lstat(path,&at)&&!fstat(input_fd,&fd)&&S_ISREG(at.st_mode)&&same_stat(&at,&input_file)&&same_stat(&fd,&input_file)&&at.st_size==(off_t)sizeof(CommandState)&&fd.st_size==at.st_size&&input_mapping->magic==COMMAND_MAGIC&&input_mapping->version==COMMAND_VERSION&&input_mapping->bytes==sizeof(CommandState)&&input_mapping->capacity==COMMAND_SLOTS&&command_duration_valid(input_mapping->seconds)&&input_mapping->owner_token&&input_mapping->pid==state->pid&&input_mapping->start_lo==(uint32_t)start&&input_mapping->start_hi==(uint32_t)(start>>32)&&input_mapping->origin_sec==state->origin_sec&&input_mapping->origin_nsec==state->origin_nsec;
}
static int input_open_file(const char *path,const MirrorState *state,uint64_t start){
 input_fd=open(path,O_RDWR|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(input_fd<0||fstat(input_fd,&input_file)||!S_ISREG(input_file.st_mode)||input_file.st_size!=(off_t)sizeof(CommandState)||input_file.st_uid!=geteuid()||flock(input_fd,LOCK_EX|LOCK_NB))return 0;
 input_mapping=mmap(NULL,sizeof(*input_mapping),PROT_READ|PROT_WRITE,MAP_SHARED,input_fd,0);
 return input_file_current(path,state,start)&&input_begin(&input,input_mapping);
}
static void input_close_file(void){
 /* Native MPC kernel retains flock through mmap: explicitly unlock. */
 if(input_fd>=0)flock(input_fd,LOCK_UN);
 if(input_mapping!=MAP_FAILED)munmap(input_mapping,sizeof(*input_mapping));
 if(input_fd>=0)close(input_fd);
 input_mapping=MAP_FAILED;input_fd=-1;
}
#endif
/* -1 invalid/stale source, 0 bounded copy retry exhausted, 1 fresh snapshot. */
static int fresh_copy_view(const MirrorState *state,CopiedMirror *snapshot,uint32_t *now,int pads){
 int copied=copy_mirror_view(state,snapshot,pads);
 uint32_t heartbeat=atomic_load_explicit(&state->heartbeat,memory_order_acquire);
 *now=source_now(state); /* clock after publication reads, never before them */
 if(*now==UINT32_MAX||!atomic_load_explicit(&state->alive,memory_order_acquire)||atomic_load_explicit(&state->error,memory_order_acquire)||*now<heartbeat||*now-heartbeat>=1000)return -1;
 if(!copied)return 0;
 return snapshot->alive&&!snapshot->error&&*now>=snapshot->heartbeat&&*now-snapshot->heartbeat<1000?1:-1;
}

static inline int fresh_copy(const MirrorState *state,CopiedMirror *snapshot,uint32_t *now){return fresh_copy_view(state,snapshot,now,0);}

int main(int argc,char **argv){
#ifdef MIRROR_INPUT
 if(argc==3&&!strcmp(argv[1],"--surface-status")){unsigned enabled;if(!surface_preferences_read(argv[2],&enabled))return 1;puts(enabled?"on":"off");return 0;}
#endif
 setvbuf(stdout,NULL,_IOLBF,0);uint64_t seconds=120;
#ifdef MIRROR_INPUT
 int arguments=servo_arguments(argc,argv);
 if((arguments!=3&&arguments!=4)||argv[1][0]!='/'||argv[2][0]!='/'||(arguments==4&&strcmp(argv[3],"manual")&&!number(argv[3],900,&seconds))){fputs("mirror-input /absolute/volume.state /absolute/command.state [SECONDS(1..900), default120, or manual] [--servo=off|--servo=on, default off] [--verbose]\n",stderr);return 2;}
 if(arguments==4&&!strcmp(argv[3],"manual"))seconds=0;
 if(preferences_path){unsigned enabled;if(!surface_preferences_read(preferences_path,&enabled)||enabled!=following.enabled){fputs("Invalid or changed surface preference; no motors started\n",stderr);return 1;}}
 motor_follow_reset(&following);
#else
 if((argc!=2&&argc!=3)||argv[1][0]!='/'||(argc==3&&!number(argv[2],900,&seconds))){fputs("mirror-motor /absolute/volume.state [SECONDS(1..900), default120]\n",stderr);return 2;}
#endif
 unsigned pid=0;uint64_t start=0;
 snd_seq_t *discovery=NULL;int disconnected_idle=0;
 int result=1,fd=-1,lockfd=-1;const MirrorState *state=MAP_FAILED;Surface surface={0};MirrorBank bank;bank_init(&bank);unsigned sent=0;const char *reason="setup failure";
 signal(SIGINT,signal_stop);signal(SIGTERM,signal_stop);signal(SIGALRM,signal_stop);if(seconds)alarm((unsigned)seconds+15);
 /* Share the proven one-motor bridge's abstract exclusion key: the old
  * diagnostic and this bank cannot drive the same hardware concurrently. */
 lockfd=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC,0);struct sockaddr_un lock={.sun_family=AF_UNIX};memcpy(lock.sun_path+1,"mpclearn-motor-one",18);
 if(lockfd<0||bind(lockfd,(struct sockaddr*)&lock,sizeof(sa_family_t)+19))goto done;
 fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);struct stat file,exe;
 if(fd<0||fstat(fd,&file)||!S_ISREG(file.st_mode)||file.st_size!=(off_t)sizeof(MirrorState))goto done;
 state=mmap(NULL,sizeof(*state),PROT_READ,MAP_SHARED,fd,0);if(state==MAP_FAILED)goto done;
 if(state->magic!=MIRROR_MAGIC||state->version!=MIRROR_VERSION||state->bytes!=sizeof(*state)||state->capacity!=MIRROR_CELLS||!state->pid||state->pid>INT_MAX||state->origin_nsec>=1000000000)goto done;
 pid=state->pid;start=start_time(pid);long hz=sysconf(_SC_CLK_TCK);
 uint32_t began_sec=state->origin_sec,began_nsec=state->origin_nsec;
 if(!start||hz<=0||(uint64_t)began_sec*(unsigned long)hz+(uint64_t)began_nsec*(unsigned long)hz/1000000000<start||!process_file(pid,&exe,1)||!source_identity(argv[1],fd,&file,state,pid,start,&exe))goto done;
 CopiedMirror snapshot={0};uint32_t now;
 if(fresh_copy_view(state,&snapshot,&now,bank.view==BV_DRUM_PADS)<0){reason="mirror unavailable or stale at start";goto done;}
#ifdef MIRROR_INPUT
 if(!input_open_file(argv[2],state,start)){reason=input.error==C_BUSY?"command mailbox not idle at bridge start":input.error?"command producer unavailable at bridge start":"command mailbox identity, format or lock unavailable at bridge start";goto done;}
 BRIDGE_LOG("INPUT channel-bank source commands; continuous fader coalescing, independent bounded control transactions, fair owner-drain batches; first/touchless positions accepted; raw SERVO is not source settlement\n");
 BRIDGE_LOG("SERVO_MODE %s; MPC command/source settlement and authoritative motor policy unchanged\n",servo_disabled?"OFF X-Touch default":"ON diagnostic comparison");
#endif
 if(snd_seq_open(&discovery,"default",SND_SEQ_OPEN_DUPLEX,SND_SEQ_NONBLOCK)<0||snd_seq_set_client_name(discovery,"mpclearn-mcu-discovery")<0){reason="MIDI discovery unavailable";goto done;}
 uint64_t began_wall=monotonic_ms();if(began_wall==UINT64_MAX||began_wall>UINT64_MAX-seconds*1000){reason="clock overflow";goto done;}
 uint64_t until=seconds?began_wall+seconds*1000:UINT64_MAX,last_connect=0,last_identity=0,last_discovery=0;
#ifdef MIRROR_INPUT
 BRIDGE_LOG("BRIDGE_READY pid=%u\n",(unsigned)getpid());
#endif
 BRIDGE_LOG("SOURCE pid=%u start=%llu epoch=%u; automatic eight-playable-track volume bank\n",pid,(unsigned long long)start,snapshot.epoch);
 while(!stopping){
  uint64_t wall=monotonic_ms();if(wall==UINT64_MAX){reason="clock failure";break;}if(wall>=until){result=0;reason="finite duration";break;}
  if(state->origin_sec!=began_sec||state->origin_nsec!=began_nsec){reason="source origin changed";break;}
  if(wall-last_identity>=100){last_identity=wall;if(!source_identity(argv[1],fd,&file,state,pid,start,&exe)){reason="source/process identity changed";break;}
#ifdef MIRROR_INPUT
   if(!input_file_current(argv[2],state,start)){reason="mailbox identity changed";break;}
#endif
  }
  if(!surface.seq&&disconnected_idle){
   /* No hardware consumer and no unsettled command: do not clear/copy the
    * entire mixer at100Hz. Lifetime and freshness still fail closed. */
   uint32_t heartbeat=atomic_load_explicit(&state->heartbeat,memory_order_acquire);now=source_now(state);
   if(now==UINT32_MAX||!atomic_load_explicit(&state->alive,memory_order_acquire)||atomic_load_explicit(&state->error,memory_order_acquire)||now<heartbeat||now-heartbeat>=1000){reason="mirror failure or stale heartbeat while disconnected";break;}
#ifdef MIRROR_INPUT
   if(!input_health(&input)){reason="command source unavailable while disconnected";break;}
   if(!input_drained(&input))disconnected_idle=0;
#endif
   if(disconnected_idle){
    snd_seq_addr_t found;
    if(wall-last_connect<250){poll(NULL,0,10);continue;}
    last_connect=wall;
    if(!discover(discovery,&found)){poll(NULL,0,10);continue;}
    disconnected_idle=0;last_connect=0; /* normal fresh-copy path before attach */
   }
  }
  int copied=0;
  if(!surface.seq){
   copied=fresh_copy_view(state,&snapshot,&now,bank.view==BV_DRUM_PADS);
   if(copied<0){reason="mirror failure or stale heartbeat";break;}
   if(!copied){
#ifdef MIRROR_INPUT
    if(surface.seq&&(bank.view==BV_DRUM_PADS||bank.assignment==BA_SEND))(void)parent_led_clear(&surface);
#endif
    bank_drop(&bank,now);
#ifdef MIRROR_INPUT
    input_discard(&input);input_jog_discard(&input);input_master_invalidate(&input);
#endif
    poll(NULL,0,10);continue;}
   bank_apply(&bank,&snapshot,now);
#ifdef MIRROR_INPUT

   input_sync(&input,&bank);input_health(&input);
   if(input.error){reason="input rejected, ambiguous, expired or unavailable (see error code)";break;}
#endif
 #ifdef MIRROR_INPUT
   input_drain(&input,&bank,&snapshot,now); /* also settle while USB is absent */
   if(input.error){reason="published transaction drain failed";break;}
#endif
   if(stopping)break;

#ifdef MIRROR_INPUT
   if(!surface.seq){input_meter_interest(&input,&bank,&snapshot,now,0);input_io_interest(&input,&bank,&snapshot,now,0);input_effects_interest(&input,&bank,&snapshot,now,0);input_qlink_interest(&input,&bank,&snapshot,now,0);}
#endif
  }
  if(!surface.seq){
   if(wall-last_connect>=250){last_connect=wall;if(surface_open(&surface,&bank,state,discovery))last_identity=0;}
#ifdef MIRROR_INPUT
   disconnected_idle=!surface.seq&&input_drained(&input);
#else
   disconnected_idle=!surface.seq;
#endif
   poll(NULL,0,10);continue;
  }
  snd_seq_addr_t current;
  if(wall-last_discovery>=100){last_discovery=wall;if(!discover(surface.seq,&current)||!address_equal(current,surface.full)){BRIDGE_LOG("DISCONNECTED; pending output discarded\n");surface_close(&surface,&bank);continue;}}
  if(!surface_drain(&surface,&bank,state,0)){BRIDGE_LOG("INPUT continuity lost; all touch states UNKNOWN\n");surface_close(&surface,&bank);continue;}
#ifdef MIRROR_INPUT
  /* Observe disconnect/bank/touch input before publishing any queued desire. */
  copied=fresh_copy_view(state,&snapshot,&now,bank.view==BV_DRUM_PADS);if(copied<0){reason="source unavailable before input publication";break;}
  if(!copied){
#ifdef MIRROR_INPUT
   if(surface.seq&&(bank.view==BV_DRUM_PADS||bank.assignment==BA_SEND))(void)parent_led_clear(&surface);
#endif
   bank_drop(&bank,now);input_discard(&input);input_master_invalidate(&input);continue;}
  bank_apply(&bank,&snapshot,now);input_sync(&input,&bank);
  input_drain(&input,&bank,&snapshot,now);
  if(input.error){reason="published transaction drain failed";break;}
  if(stopping)break;
  input_meter_interest(&input,&bank,&snapshot,now,surface.seq!=NULL);input_io_interest(&input,&bank,&snapshot,now,surface.seq!=NULL);input_effects_interest(&input,&bank,&snapshot,now,surface.seq!=NULL);input_qlink_interest(&input,&bank,&snapshot,now,surface.seq!=NULL);
  input_pump(&input,&bank,&snapshot,now);
  if(input.error){reason="input rejected, ambiguous, expired or unavailable (see error code)";break;}
  if(!channel_output(&surface,&bank,&snapshot,now)){surface_close(&surface,&bank);continue;}
#else
  copied=fresh_copy_view(state,&snapshot,&now,bank.view==BV_DRUM_PADS);
  if(copied<0){reason="source unavailable before motor output";break;}
  if(!copied){bank_drop(&bank,now);continue;}
  bank_apply(&bank,&snapshot,now);
#endif
  for(unsigned channel=0;channel<MIRROR_BANK;channel++){
   /* The iteration already copied source state and serviced input. An idle
    * strip need not repeat full copies; a prospective send still takes every
    * original fresh source/USB/touch/command check below. */
#ifdef MIRROR_INPUT
   if(!strip_motor_ready(&bank,channel,now))continue;
#else
   if(bank_due(&bank,channel,now)<0)continue;
#endif
   /* Keep event continuity and touch suppression at the send boundary. Only
    * the target scalar/projection is revalidated; no full mixer or ALSA scan. */
   if(!surface_drain(&surface,&bank,state,0)||lost_events(surface.seq)){surface_close(&surface,&bank);break;}
   now=source_now(state);
   if(!mirror_motor_current(state,&snapshot,&bank,channel,now))continue;
#ifdef MIRROR_INPUT
   input_sync(&input,&bank);
   if(input_blocked_field(&input,channel,bank_field(&bank,channel)))continue;
   if(!input_health(&input)){reason="command source unavailable at motor boundary";goto done;}
#endif
#ifdef MIRROR_INPUT
   int emitted=strip_motor_output(&surface,&bank,channel,now);if(emitted<0){surface_close(&surface,&bank);break;}if(!emitted)continue;
   MirrorFader *f=bank.faders+channel;int position=f->last_sent;sent++;
#else
   int position=bank_due(&bank,channel,now);if(position<0)continue;
   snd_seq_event_t event;motor_event(&event,&surface,channel,position);
   if(stopping)break;
   if(snd_seq_event_output_direct(surface.seq,&event)<0){surface_close(&surface,&bank);break;}
   MirrorFader *f=bank.faders+channel;f->last_sent=position;f->last_tick=now;f->physical=-1;sent++;
#endif
#ifdef MIRROR_INPUT
   if(bridge_verbose)
#endif
   BRIDGE_LOG("MOTOR fader=%u position=%d epoch=%u track=%u binding=%u incarnation=%u volume_revision=%u heartbeat=%u empty=%d\n",channel+1,position,bank.epoch,f->identity.serial,f->identity.binding,f->identity.incarnation,f->revision,bank.heartbeat,f->empty);
  }
  poll(NULL,0,10);
 }
 if(stopping){result=0;reason="signal stop";}
done:
#ifdef MIRROR_INPUT
 /* Stop all hardware traffic first, retain exclusive mailbox writer until the
  * already published transaction is source-settled AND owner-reclaimed. */
 unsigned final_holds=0;for(unsigned i=0;i<8;i++)if(bank.faders[i].touch==MT_DOWN)final_holds|=1u<<i;if(surface.master_touch)final_holds|=256;
 if(surface.seq)fprintf(stderr,"TOUCH_FINAL pid=%ld mask=%u\n",(long)getpid(),final_holds);
 surface_close(&surface,&bank);input_discard(&input);
 if(input.commands&&state!=MAP_FAILED&&!input_drained(&input)){
  uint64_t deadline=monotonic_ms();
  if(deadline!=UINT64_MAX&&deadline<=UINT64_MAX-10000)deadline+=10000;
  else deadline=0;
  while(deadline&&!input.error&&monotonic_ms()<deadline){
   if(!source_identity(argv[1],fd,&file,state,pid,start,&exe)||!input_file_current(argv[2],state,start)||fresh_copy_view(state,&snapshot,&now,bank.view==BV_DRUM_PADS)!=1)break;
   bank_apply(&bank,&snapshot,now);input_drain(&input,&bank,&snapshot,now);
   if(input_drained(&input))break;
   poll(NULL,0,10);
  }
  if(!input_drained(&input)){result=1;if(input.submitted)reason="incomplete published transaction at bridge stop";}
 }
 if(input_mapping!=MAP_FAILED)fprintf(stderr,"MAILBOX published=%u consumed=%u reclaimed=%u pending=%u source_error=%u trace_error=%u\n",atomic_load(&input_mapping->published),atomic_load(&input_mapping->consumed),atomic_load(&input_mapping->reclaimed),command_pending(input_mapping),atomic_load(&input_mapping->error),atomic_load(&input_mapping->trace_error));
 fprintf(stderr,"INPUT STOP error=%u reason=%s submitted=%u settled=%u refused=%u pending_request=%u; timeout/return never acknowledged\n",input.error,command_error_name(input.error),input.submitted,input.settled,input.refused,input_pending(&input));input_close_file();
#endif
 if(discovery)snd_seq_close(discovery);
 surface_close(&surface,&bank);if(state!=MAP_FAILED)munmap((void*)state,sizeof(*state));if(fd>=0)close(fd);if(lockfd>=0)close(lockfd);
 fprintf(stderr,"STOP %s; motor_messages=%u; no reset sent\n",reason,sent);return result;
}
