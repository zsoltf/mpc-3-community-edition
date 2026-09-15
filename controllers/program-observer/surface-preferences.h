/* One bridge-owned surface preference; atomic replacement, no MPC state. */
#ifndef MPC_SURFACE_PREFERENCES_H
#define MPC_SURFACE_PREFERENCES_H
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#define SURFACE_PREFIX "MCU-SURFACE1 motors="
#define SURFACE_BYTES (sizeof(SURFACE_PREFIX)+1u)
static int surface_preferences_read(const char *path,unsigned *enabled){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0){if(errno==ENOENT){*enabled=1;return 1;}return 0;}
 struct stat st;char b[32];ssize_t n=read(fd,b,sizeof(b));int ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_uid==geteuid()&&(st.st_mode&0777)==0600&&st.st_size==(off_t)SURFACE_BYTES&&n==(ssize_t)SURFACE_BYTES;
 close(fd);if(!ok||memcmp(b,SURFACE_PREFIX,sizeof(SURFACE_PREFIX)-1)||b[SURFACE_BYTES-1]!='\n'||(b[SURFACE_BYTES-2]!='0'&&b[SURFACE_BYTES-2]!='1'))return 0;
 *enabled=b[SURFACE_BYTES-2]=='1';return 1;
}
static int surface_preferences_write(const char *path,unsigned enabled){
 char directory[PATH_MAX],temporary[64];const char *slash=strrchr(path,'/');
 if(!slash||slash==path||strcmp(slash+1,"surface-preferences")||(size_t)(slash-path)>=sizeof(directory))return 0;
 memcpy(directory,path,(size_t)(slash-path));directory[slash-path]=0;
 int dir=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(dir<0)return 0;
 struct stat st;unsigned current;int ok=!fstat(dir,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==geteuid()&&(st.st_mode&0777)==0700&&surface_preferences_read(path,&current);
 if(!ok){close(dir);return 0;}
 snprintf(temporary,sizeof(temporary),".surface-preferences.%ld",(long)getpid());
 int fd=openat(dir,temporary,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0){close(dir);return 0;}
 const char *text=enabled?"MCU-SURFACE1 motors=1\n":"MCU-SURFACE1 motors=0\n";
 ok=write(fd,text,SURFACE_BYTES)==(ssize_t)SURFACE_BYTES&&!fsync(fd);if(close(fd))ok=0;
 if(ok)ok=!renameat(dir,temporary,dir,"surface-preferences");
 if(!ok)unlinkat(dir,temporary,0);else if(fsync(dir))fprintf(stderr,"Surface preference renamed; directory sync failed: %d\n",errno);
 close(dir);return ok;
}
#endif
