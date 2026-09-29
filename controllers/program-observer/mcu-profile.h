#ifndef MPCLEARN_MCU_PROFILE_H
#define MPCLEARN_MCU_PROFILE_H

#include <string.h>

enum {
 MCU_CAP_MASTER_INPUT=1u<<0,
 MCU_CAP_GLOBAL=1u<<1,
 MCU_CAP_JOG=1u<<2,
 MCU_CAP_LCD=1u<<3,
 MCU_CAP_TIME=1u<<4,
 MCU_CAP_METERS=1u<<5,
 MCU_CAP_COLOR=1u<<6,
 MCU_CAP_STRIP_FADER=1u<<7,
 MCU_CAP_STRIP_MOTOR=1u<<8,
 MCU_CAP_MASTER_MOTOR=1u<<9,
 MCU_CAP_TOUCH=1u<<10
};

enum {MCU_PROTOCOL_MCU,MCU_PROTOCOL_HUI};

typedef struct {
 const char *name;
 unsigned capabilities;
 unsigned raw_fader_echo;
 const char *default_client;
 const char *default_port;
 unsigned physical_only;
 unsigned master_input_max;
 unsigned protocol;
} McuProfile;

typedef struct {
 const McuProfile *profile;
 const char *client;
 const char *port;
 unsigned physical_only;
} McuEndpoint;

static const McuProfile mcu_profile_xtouch={
 .name="xtouch",
 .capabilities=MCU_CAP_MASTER_INPUT|MCU_CAP_GLOBAL|MCU_CAP_JOG|MCU_CAP_LCD|MCU_CAP_TIME|MCU_CAP_METERS|MCU_CAP_COLOR|MCU_CAP_STRIP_FADER|MCU_CAP_STRIP_MOTOR|MCU_CAP_MASTER_MOTOR|MCU_CAP_TOUCH,
 .raw_fader_echo=0,
 .default_client="X-Touch",
 .default_port="X-TOUCH_INT",
 .physical_only=1,
 .master_input_max=16383
};

/* Standard MCU hosts echo a received fader position to close the controller's
 * servo loop. Ardour strip.cc and Tracktion MackieMCU.cpp both implement that
 * policy. The vendor color SysEx is deliberately absent. Meters stay disabled:
 * the X-Touch's separate-meter enable value is not a generic layout contract. */
static const McuProfile mcu_profile_generic={
 .name="generic",
 .capabilities=MCU_CAP_MASTER_INPUT|MCU_CAP_GLOBAL|MCU_CAP_JOG|MCU_CAP_LCD|MCU_CAP_TIME|MCU_CAP_STRIP_FADER|MCU_CAP_STRIP_MOTOR|MCU_CAP_MASTER_MOTOR|MCU_CAP_TOUCH,
 .raw_fader_echo=1,
 .default_client=NULL,
 .default_port=NULL,
 .physical_only=1,
 .master_input_max=16383
};

/* The Mini exposes standard MCU V-Pots, buttons and a master input but no
 * motor, touch sensing, jog wheel or extended display. Its captured physical
 * master endpoints are E8 00 00..E8 00 7F, raw 0..16256. */
static const McuProfile mcu_profile_xtouch_mini={
 .name="xtouch-mini",
 .capabilities=MCU_CAP_MASTER_INPUT|MCU_CAP_GLOBAL,
 .raw_fader_echo=0,
 .default_client="X-TOUCH MINI",
 .default_port="X-TOUCH MINI MIDI 1",
 .physical_only=1,
 .master_input_max=16256
};

/* Standard HUI is an explicit, exactly named bidirectional endpoint. Device
 * endpoint and heartbeat-loss policy remain unselected until physical capture. */
static const McuProfile mcu_profile_hui={
 .name="hui",
 .capabilities=MCU_CAP_GLOBAL|MCU_CAP_STRIP_FADER|MCU_CAP_TOUCH,
 .raw_fader_echo=0,
 .default_client=NULL,
 .default_port=NULL,
 .physical_only=1,
 .master_input_max=0,
 .protocol=MCU_PROTOCOL_HUI
};

static inline const McuProfile *mcu_profile_named(const char *name){
 if(!strcmp(name,"xtouch"))return &mcu_profile_xtouch;
 if(!strcmp(name,"generic"))return &mcu_profile_generic;
 if(!strcmp(name,"xtouch-mini"))return &mcu_profile_xtouch_mini;
 if(!strcmp(name,"hui"))return &mcu_profile_hui;
 return NULL;
}

static inline int mcu_endpoint_valid(const McuEndpoint *endpoint){
 if(!endpoint||!endpoint->profile||!endpoint->client||!endpoint->port)return 0;
 if(endpoint->profile->default_client||endpoint->profile->default_port)
  return endpoint->profile->default_client&&endpoint->profile->default_port&&endpoint->physical_only&&!strcmp(endpoint->client,endpoint->profile->default_client)&&!strcmp(endpoint->port,endpoint->profile->default_port);
 return endpoint->client&&*endpoint->client&&endpoint->port&&*endpoint->port;
}

static inline int mcu_has(const McuEndpoint *endpoint,unsigned capability){
 return endpoint&&endpoint->profile&&(endpoint->profile->capabilities&capability)!=0;
}

static inline int mcu_master_input_position(const McuEndpoint *endpoint,unsigned raw,unsigned *position){
 if(!endpoint||!endpoint->profile||!position||!endpoint->profile->master_input_max||raw>endpoint->profile->master_input_max)return 0;
 unsigned maximum=endpoint->profile->master_input_max;
 *position=maximum==16383?raw:(raw*16383u+maximum/2)/maximum;
 return 1;
}

enum {MCU_ENDPOINT_READ=1u<<0,MCU_ENDPOINT_SUBS_READ=1u<<1,MCU_ENDPOINT_WRITE=1u<<2};
static inline int mcu_endpoint_match(const McuEndpoint *endpoint,const char *client,unsigned kernel,const char *port,unsigned capabilities){
 unsigned need=MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE;
 return mcu_endpoint_valid(endpoint)&&(!endpoint->physical_only||kernel)&&!strcmp(endpoint->client,client)&&!strcmp(endpoint->port,port)&&(capabilities&need)==need;
}

#endif
