#include <stdio.h>
#include <stdlib.h>
#include "hui-codec.h"

static void need(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}

int main(void){
 HuiInput input={0};HuiIntent intent;
 const unsigned char fader_msb[]={0xb0,3,0x12},fader_lsb[]={0xb0,0x23,0x34};
 need(!hui_decode(&input,fader_msb,100,&intent),"fader MSB remains a bounded partial");
 need(hui_decode(&input,fader_lsb,101,&intent)==1&&intent.kind==HUI_INTENT_FADER&&intent.strip==3&&intent.value==0x934,"paired 14-bit fader vector");
 need(!hui_decode(&input,fader_lsb,102,&intent),"orphan fader LSB ignored");
 need(!hui_decode(&input,fader_msb,200,&intent)&&!hui_decode(&input,fader_lsb,200+HUI_PARTIAL_MS+1,&intent),"expired fader partial ignored");
 const unsigned char zone[]={0xb0,0x0f,5},select[]={0xb0,0x2f,0x41};
 need(!hui_decode(&input,zone,500,&intent)&&hui_decode(&input,select,501,&intent)==1&&intent.kind==HUI_INTENT_STRIP_BUTTON&&intent.strip==5&&intent.control==HUI_BUTTON_SELECT&&intent.value,"zone/port strip switch vector");
 const unsigned char transport_zone[]={0xb0,0x0f,0x0e},play_up[]={0xb0,0x2f,4};
 need(!hui_decode(&input,transport_zone,600,&intent)&&hui_decode(&input,play_up,601,&intent)==1&&intent.kind==HUI_INTENT_TRANSPORT&&intent.control==HUI_TRANSPORT_PLAY&&!intent.value,"transport release vector");
 const unsigned char clockwise[]={0xb0,0x47,0x43},counterclockwise[]={0xb0,0x40,2};
 need(hui_decode(&input,clockwise,700,&intent)==1&&intent.kind==HUI_INTENT_ENCODER&&intent.strip==7&&intent.value==3,"clockwise encoder magnitude");
 need(hui_decode(&input,counterclockwise,701,&intent)==1&&intent.kind==HUI_INTENT_ENCODER&&!intent.strip&&intent.value==-2,"counterclockwise encoder magnitude");
 const unsigned char reply[]={0x90,0,0x7f};need(hui_decode(&input,reply,800,&intent)==1&&intent.kind==HUI_INTENT_HEARTBEAT_REPLY,"standard heartbeat reply vector");
 HuiWire heartbeat=hui_heartbeat(),led=hui_led(2,HUI_BUTTON_MUTE,1),ring=hui_ring(6,11);
 const unsigned char expected_heartbeat[]={0x90,0,0},expected_led[]={0xb0,0x0c,2,0xb0,0x2c,0x42},expected_ring[]={0xb0,0x16,11};
 need(heartbeat.length==3&&!memcmp(heartbeat.bytes,expected_heartbeat,3),"standard host heartbeat encoding");
 need(led.length==6&&!memcmp(led.bytes,expected_led,6),"paired HUI LED encoding");
 need(ring.length==3&&!memcmp(ring.bytes,expected_ring,3),"standard ring encoding");
 hui_input_reset(&input);for(unsigned i=0;i<HUI_STRIPS;i++)need(!input.fader_valid[i],"reset clears fader partials");need(!input.zone_valid,"reset clears zone partial");
 puts("PASS independent standard HUI codec vectors: bounded fader/zone pairs, switches, transport, encoders, heartbeat, LED and ring encoding");return 0;
}
