/* Existing ALSA clients only. Resolve on Stop/connection, never a render poll. */
#ifndef MPC_STOP_ROUTE_H
#define MPC_STOP_ROUTE_H
#include <sys/stat.h>
#define STOP_RELAY_PORT "mpclearn Stop relay v1"
static inline int stop_executable(int pid,const char *path){
 char proc[64];struct stat running,expected;if(pid<=0||!path||path[0]!='/')return 0;
 snprintf(proc,sizeof(proc),"/proc/%d/exe",pid);
 return !stat(proc,&running)&&!stat(path,&expected)&&S_ISREG(expected.st_mode)&&expected.st_uid==0&&running.st_dev==expected.st_dev&&running.st_ino==expected.st_ino;
}
static inline int stop_find_port(snd_seq_t *seq,const char *client_name,const char *port_name,const char *exe,unsigned need,unsigned forbid,snd_seq_addr_t *address,int *pid){
 snd_seq_client_info_t *ci;snd_seq_port_info_t *pi;snd_seq_client_info_alloca(&ci);snd_seq_port_info_alloca(&pi);snd_seq_client_info_set_client(ci,-1);int n=0,rc;
 while((rc=snd_seq_query_next_client(seq,ci))>=0){
  if(strcmp(snd_seq_client_info_get_name(ci),client_name))continue;
  int owner=snd_seq_client_info_get_pid(ci);if(snd_seq_client_info_get_type(ci)!=SND_SEQ_USER_CLIENT||!stop_executable(owner,exe))return -1;
  snd_seq_port_info_set_client(pi,snd_seq_client_info_get_client(ci));snd_seq_port_info_set_port(pi,-1);int pr;
  while((pr=snd_seq_query_next_port(seq,pi))>=0){
   if(strcmp(snd_seq_port_info_get_name(pi),port_name))continue;
   unsigned caps=snd_seq_port_info_get_capability(pi);if((caps&need)!=need||(caps&forbid))return -1;
   *address=*snd_seq_port_info_get_addr(pi);*pid=owner;n++;
  }if(pr!=-ENOENT)return -1;
 }return rc==-ENOENT&&n<=1?n:-1;
}
/* No retry after successful delivery: the receiver owns that event. */
static inline int stop_deliver(snd_seq_t *seq,snd_seq_event_t *event,int source,int matched,snd_seq_addr_t relay){
 snd_seq_ev_set_source(event,source);snd_seq_ev_set_direct(event);
 if(matched==1){
  snd_seq_ev_set_dest(event,relay.client,relay.port);int rc=snd_seq_event_output_direct(seq,event);
  if(rc!=-ENOENT&&rc!=-ENXIO)return rc; /* Only definite nondelivery permits fallback. */
 }
 snd_seq_ev_set_subs(event);return snd_seq_event_output_direct(seq,event);
}
#endif
