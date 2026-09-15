#ifndef MPC_TRANSPORT_QUEUE_H
#define MPC_TRANSPORT_QUEUE_H
#include <stdint.h>
typedef void (*TransportAudioCallback)(void *);
unsigned transport_queue_submit(uint32_t image_bias,uint32_t owner,void *job,TransportAudioCallback callback);
#ifdef COMMAND_COMPONENT
void transport_queue_component_reset(void);
void transport_queue_component_fail(int);
unsigned transport_queue_component_pending(void);
unsigned transport_queue_component_manager_count(unsigned);
unsigned transport_queue_component_trivial(void);
void transport_queue_component_run(void);
#endif
#endif
