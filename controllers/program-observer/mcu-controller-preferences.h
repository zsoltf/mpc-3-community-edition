/* Session-owned controller selection. Fixed-size atomic file; no MPC state. */
#ifndef MPCLEARN_MCU_CONTROLLER_PREFERENCES_H
#define MPCLEARN_MCU_CONTROLLER_PREFERENCES_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mcu-profile.h"

#define MCU_CONTROLLER_NAME_MAX 128u

typedef struct {
 char magic[8];
 uint32_t profile,physical_only,client_length,port_length,reserved;
 unsigned char client[MCU_CONTROLLER_NAME_MAX],port[MCU_CONTROLLER_NAME_MAX];
 uint32_t checksum;
} McuControllerPreferenceFile;

typedef struct {
 McuEndpoint endpoint;
 char client[MCU_CONTROLLER_NAME_MAX+1],port[MCU_CONTROLLER_NAME_MAX+1];
} McuControllerPreference;

_Static_assert(sizeof(McuControllerPreferenceFile)==288,"fixed controller preference file");

static uint32_t mcu_controller_checksum(const McuControllerPreferenceFile *file){
 const unsigned char *p=(const unsigned char*)file;uint32_t value=2166136261u;
 for(size_t i=0;i<sizeof(*file)-sizeof(file->checksum);i++){value^=p[i];value*=16777619u;}
 return value;
}

static unsigned mcu_controller_profile_id(const McuProfile *profile){
 if(profile==&mcu_profile_xtouch)return 1;
 if(profile==&mcu_profile_xtouch_mini)return 2;
 if(profile==&mcu_profile_generic)return 3;
 if(profile==&mcu_profile_hui)return 4;
 return 0;
}

static const McuProfile *mcu_controller_profile(unsigned id){
 return id==1?&mcu_profile_xtouch:id==2?&mcu_profile_xtouch_mini:id==3?&mcu_profile_generic:id==4?&mcu_profile_hui:NULL;
}

static void mcu_controller_default(McuControllerPreference *selection){
 memset(selection,0,sizeof(*selection));
 selection->endpoint=(McuEndpoint){.profile=&mcu_profile_xtouch,.client=mcu_profile_xtouch.default_client,.port=mcu_profile_xtouch.default_port,.physical_only=1};
}

/* Missing means the unchanged X-Touch default. Any existing malformed file is
 * rejected, so the owner can keep the instrument usable and expose the error. */
static int mcu_controller_preferences_read(const char *path,McuControllerPreference *selection){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0){if(errno==ENOENT){mcu_controller_default(selection);return 1;}return 0;}
 McuControllerPreferenceFile file={0};struct stat st;ssize_t n=read(fd,&file,sizeof(file));unsigned char extra;
 int ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_uid==geteuid()&&(st.st_mode&0777)==0600&&st.st_size==(off_t)sizeof(file)&&n==(ssize_t)sizeof(file)&&read(fd,&extra,1)==0;
 close(fd);
 const McuProfile *profile=mcu_controller_profile(file.profile);
 ok=ok&&!memcmp(file.magic,"MCUCFG1",8)&&!file.reserved&&file.checksum==mcu_controller_checksum(&file)&&profile&&file.physical_only<=1&&file.client_length<=MCU_CONTROLLER_NAME_MAX&&file.port_length<=MCU_CONTROLLER_NAME_MAX;
 if(!ok)return 0;
 memset(selection,0,sizeof(*selection));
 memcpy(selection->client,file.client,file.client_length);memcpy(selection->port,file.port,file.port_length);
 if(profile->default_client){
  if(!file.physical_only||file.client_length||file.port_length)return 0;
  selection->endpoint=(McuEndpoint){.profile=profile,.client=profile->default_client,.port=profile->default_port,.physical_only=1};
 }else{
  if(!file.client_length||!file.port_length)return 0;
  selection->endpoint=(McuEndpoint){.profile=profile,.client=selection->client,.port=selection->port,.physical_only=file.physical_only};
 }
 return mcu_endpoint_valid(&selection->endpoint);
}

static int mcu_controller_preferences_write(const char *path,const McuEndpoint *endpoint){
 unsigned profile=mcu_controller_profile_id(endpoint?endpoint->profile:NULL);size_t client=0,port=0;
 if(!profile||endpoint->physical_only>1||!mcu_endpoint_valid(endpoint))return 0;
 if(!endpoint->profile->default_client){client=strlen(endpoint->client);port=strlen(endpoint->port);if(!client||!port||client>MCU_CONTROLLER_NAME_MAX||port>MCU_CONTROLLER_NAME_MAX)return 0;}
 char directory[PATH_MAX];const char *temporary=".controller-preferences.new",*slash=strrchr(path,'/');
 if(!slash||slash==path||strcmp(slash+1,"controller-preferences")||(size_t)(slash-path)>=sizeof(directory))return 0;
 memcpy(directory,path,(size_t)(slash-path));directory[slash-path]=0;
 int dir=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(dir<0)return 0;
 struct stat st,old;int exists=fstatat(dir,"controller-preferences",&old,AT_SYMLINK_NOFOLLOW)==0,old_error=errno;
 int ok=!fstat(dir,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==geteuid()&&(st.st_mode&0777)==0700&&(!exists||(S_ISREG(old.st_mode)&&old.st_uid==geteuid()&&(old.st_mode&0777)==0600));
 if(!exists&&old_error!=ENOENT)ok=0;
 if(!ok){close(dir);return 0;}
 McuControllerPreferenceFile file={.magic="MCUCFG1",.profile=profile,.physical_only=endpoint->physical_only,.client_length=(uint32_t)client,.port_length=(uint32_t)port};
 if(client)memcpy(file.client,endpoint->client,client);
 if(port)memcpy(file.port,endpoint->port,port);
 file.checksum=mcu_controller_checksum(&file);
 struct stat pending;
 if(!fstatat(dir,temporary,&pending,AT_SYMLINK_NOFOLLOW)){
  if(!S_ISREG(pending.st_mode)||pending.st_uid!=geteuid()||(pending.st_mode&0777)!=0600||unlinkat(dir,temporary,0)){close(dir);return 0;}
 }else if(errno!=ENOENT){close(dir);return 0;}
 int fd=openat(dir,temporary,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0){close(dir);return 0;}
 ok=write(fd,&file,sizeof(file))==(ssize_t)sizeof(file)&&!fsync(fd);if(close(fd))ok=0;
 if(ok)ok=!renameat(dir,temporary,dir,"controller-preferences");
 if(!ok)unlinkat(dir,temporary,0);else if(fsync(dir))fprintf(stderr,"Controller preference renamed; directory sync failed: %d\n",errno);
 close(dir);return ok;
}

#endif
