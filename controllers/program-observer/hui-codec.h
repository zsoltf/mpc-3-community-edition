/* Standard HUI wire families. Original MIT implementation from the protocol
 * facts recorded in docs/mcu-controller-support.md; no device policy here. */
#ifndef MPCLEARN_HUI_CODEC_H
#define MPCLEARN_HUI_CODEC_H

#include <stdint.h>
#include <string.h>

#define HUI_STRIPS 8u
#define HUI_PARTIAL_MS 250u

enum {
 HUI_INTENT_NONE,
 HUI_INTENT_FADER,
 HUI_INTENT_TOUCH,
 HUI_INTENT_STRIP_BUTTON,
 HUI_INTENT_ENCODER,
 HUI_INTENT_NAVIGATION,
 HUI_INTENT_TRANSPORT,
 HUI_INTENT_HEARTBEAT_REPLY
};

enum {HUI_BUTTON_SELECT=1,HUI_BUTTON_MUTE=2,HUI_BUTTON_SOLO=3,HUI_BUTTON_ARM=7};
enum {HUI_TRANSPORT_REWIND=1,HUI_TRANSPORT_FORWARD=2,HUI_TRANSPORT_STOP=3,HUI_TRANSPORT_PLAY=4,HUI_TRANSPORT_RECORD=5};

typedef struct {
 unsigned kind,strip,control;
 int value;
} HuiIntent;

typedef struct {
 uint32_t fader_tick[HUI_STRIPS],zone_tick;
 unsigned char fader_msb[HUI_STRIPS],fader_valid[HUI_STRIPS],zone,zone_valid;
} HuiInput;

typedef struct {unsigned char bytes[6];unsigned length;} HuiWire;

static inline void hui_input_reset(HuiInput *input){memset(input,0,sizeof(*input));}

static inline int hui_partial_current(uint32_t now,uint32_t then){return now>=then&&now-then<=HUI_PARTIAL_MS;}

/* Return 1 for a complete normalized intent, 0 for a valid partial/ignored
 * message, and -1 for malformed bytes in one of the supported families. */
static inline int hui_decode(HuiInput *input,const unsigned char bytes[3],uint32_t now,HuiIntent *intent){
 if(!input||!bytes||!intent)return -1;
 memset(intent,0,sizeof(*intent));
 unsigned status=bytes[0],d1=bytes[1],d2=bytes[2];
 if(d1>127||d2>127)return -1;
 if(status==0x90&&d1==0&&d2==0x7f){intent->kind=HUI_INTENT_HEARTBEAT_REPLY;return 1;}
 if(status!=0xb0)return 0;
 if(d1<HUI_STRIPS){input->fader_msb[d1]=(unsigned char)d2;input->fader_tick[d1]=now;input->fader_valid[d1]=1;return 0;}
 if(d1>=0x20&&d1<0x20+HUI_STRIPS){
  unsigned strip=d1-0x20;if(!input->fader_valid[strip])return 0;
  unsigned current=hui_partial_current(now,input->fader_tick[strip]);input->fader_valid[strip]=0;if(!current)return 0;
  intent->kind=HUI_INTENT_FADER;intent->strip=strip;intent->value=(input->fader_msb[strip]<<7)|d2;return 1;
 }
 if(d1==0x0f){input->zone=(unsigned char)d2;input->zone_tick=now;input->zone_valid=1;return 0;}
 if(d1==0x2f){
  if(!input->zone_valid)return 0;
  unsigned current=hui_partial_current(now,input->zone_tick),zone=input->zone;input->zone_valid=0;if(!current)return 0;
  if(d2&0xb0)return -1;
  unsigned port=d2&0x0f,down=(d2&0x40)!=0;
  if(zone<HUI_STRIPS&&(port==0||port==HUI_BUTTON_SELECT||port==HUI_BUTTON_MUTE||port==HUI_BUTTON_SOLO||port==HUI_BUTTON_ARM)){
   intent->kind=port?HUI_INTENT_STRIP_BUTTON:HUI_INTENT_TOUCH;intent->strip=zone;intent->control=port;intent->value=(int)down;return 1;
  }
  if(zone==0x0a&&port<=3){intent->kind=HUI_INTENT_NAVIGATION;intent->control=port;intent->value=(int)down;return 1;}
  if(zone==0x0e&&port>=1&&port<=5){intent->kind=HUI_INTENT_TRANSPORT;intent->control=port;intent->value=(int)down;return 1;}
  return 0;
 }
 if(d1>=0x40&&d1<0x40+HUI_STRIPS){
  unsigned magnitude=d2&0x3f;if(!magnitude)return 0;
  intent->kind=HUI_INTENT_ENCODER;intent->strip=d1-0x40;intent->value=(d2&0x40)?(int)magnitude:-(int)magnitude;return 1;
 }
 return 0;
}

static inline HuiWire hui_heartbeat(void){return (HuiWire){.bytes={0x90,0,0},.length=3};}
static inline HuiWire hui_led(unsigned zone,unsigned port,unsigned on){
 HuiWire wire={{0xb0,0x0c,(unsigned char)zone,0xb0,0x2c,(unsigned char)((on?0x40:0)|(port&0x0f))},6};return wire;
}
static inline HuiWire hui_ring(unsigned strip,unsigned position){
 HuiWire wire={{0xb0,(unsigned char)(0x10+strip),(unsigned char)position},3};return wire;
}

#endif
