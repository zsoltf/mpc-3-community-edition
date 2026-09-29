#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "native-preferences.h"

static const char *profile_name(unsigned profile){return profile==1?"xtouch":profile==2?"xtouch-mini":profile==3?"generic":"none";}
static const char *status_name(unsigned status){
 static const char *const names[]={"idle","pending","applying","active","unavailable","ambiguous","invalid","write-failed","restart-failed"};
 return status<sizeof(names)/sizeof(names[0])?names[status]:"unknown";
}

int main(int argc,char **argv){
 if(argc!=2||argv[1][0]!='/'){fprintf(stderr,"native-preferences-read /absolute/native-preferences.state\n");return 2;}
 int fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0){perror("open");return 1;}struct stat st={};
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=NATIVE_PREFERENCES_BYTES){fputs("invalid state file\n",stderr);close(fd);return 1;}
 const NativePreferencesState *s=mmap(NULL,sizeof(*s),PROT_READ,MAP_SHARED,fd,0);close(fd);if(s==MAP_FAILED){perror("mmap");return 1;}
 NativePreferencesState copy;unsigned before=0,after=0,attempt=0;
 do{before=__atomic_load_n(&s->result_revision,__ATOMIC_ACQUIRE);if(before&1u)continue;memcpy(&copy,s,sizeof(copy));after=__atomic_load_n(&s->result_revision,__ATOMIC_ACQUIRE);}while((before!=after||(after&1u))&&++attempt<100);
 munmap((void*)s,sizeof(*s));if(before!=after||(after&1u)){fputs("changing state\n",stderr);return 1;}
 if(copy.magic!=NATIVE_PREFERENCES_MAGIC||copy.version!=NATIVE_PREFERENCES_VERSION||copy.bytes!=sizeof(copy)){fputs("invalid state header\n",stderr);return 1;}
 printf("pid=%u hook_entries=%u tabs=%u/%u buttons=%u/%u clicks=%u live_tabs=%u failures=%u last_failure=%u overlay=%08x tab=%08x button=%08x choices=%08x,%08x,%08x request=%u/%s claimed=%u completed=%u status=%s saved=%s active=%s detail=%u endpoint=%.*s/%.*s\n",
  copy.pid,copy.hook_entries,copy.tabs_created,copy.tabs_destroyed,copy.buttons_created,copy.buttons_destroyed,copy.clicks,copy.live_tabs,copy.failures,copy.last_failure,copy.last_overlay,copy.last_tab,copy.last_button,
  copy.buttons[0],copy.buttons[1],copy.buttons[2],copy.request_sequence,profile_name(copy.request_profile),copy.claimed_sequence,copy.completed_sequence,status_name(copy.status),profile_name(copy.saved_profile),profile_name(copy.active_profile),copy.result_detail,
  (int)copy.endpoint_client_length,(const char*)copy.endpoint_client,(int)copy.endpoint_port_length,(const char*)copy.endpoint_port);
 return copy.failures?1:0;
}
