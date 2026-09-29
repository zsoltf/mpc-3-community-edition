#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "../controllers/program-observer/mpc-identity.h"
#include "runtime-identity.h"

#ifndef SCAN_INTERVAL_MS
#define SCAN_INTERVAL_MS 1000
#endif
#ifndef POLL_INTERVAL_MS
#define POLL_INTERVAL_MS 250
#endif
#ifndef USB_WRITE_INTERVAL_MS
#define USB_WRITE_INTERVAL_MS 1000
#endif
#ifndef CACHE_INTERVAL_MS
#define CACHE_INTERVAL_MS 30000
#endif
#ifndef QUERY_TIMEOUT_MS
#define QUERY_TIMEOUT_MS 500
#endif
#ifndef HASH_BYTES_PER_TICK
#define HASH_BYTES_PER_TICK (4u * 1024u * 1024u)
#endif
#ifndef HASH_START_INTERVAL_MS
#define HASH_START_INTERVAL_MS 10000
#endif
#ifndef PROC_ROOT
#define PROC_ROOT "/proc"
#endif
#ifndef DEV_INPUT_ROOT
#define DEV_INPUT_ROOT "/dev/input"
#endif
#ifndef MEDIA_ROOT
#define MEDIA_ROOT "/media"
#endif
#ifndef SYS_DEV_BLOCK_ROOT
#define SYS_DEV_BLOCK_ROOT "/sys/dev/block"
#endif
#ifndef SCRATCH_FILE
#define SCRATCH_FILE "/run/mpclearn-diagnostics/current.txt"
#endif

#ifndef REPORT_CAP
#define REPORT_CAP (256u * 1024u)
#endif
#define MAX_EPOCHS 8
#define MAX_EXE_CACHE 8
#define MAX_INPUTS 32
#define MAX_MOUNTS 64
#define MARKER "MPCLEARN-DIAGNOSTICS"
#define EXPECTED_MPC_SHA MPC_EXECUTABLE_SHA256
#define EXPECTED_OBSERVER_SHA MPCLEARN_OBSERVER_SHA256
#ifndef MPC_EXE_PATH
#define MPC_EXE_PATH "/usr/bin/MPC"
#endif
#ifndef OBSERVER_PATH
#define OBSERVER_PATH "/usr/share/mpclearn/mcu/command-observer.so"
#endif
#ifndef DEV_OBSERVER_PATH
#define DEV_OBSERVER_PATH "/data/mpclearn/dev/command-observer.so"
#endif
#ifndef PAYLOAD_DIR
#define PAYLOAD_DIR "/usr/share/mpclearn/mcu"
#endif
#define EXPECTED_PAYLOAD_MANIFEST_SHA MPCLEARN_PAYLOAD_MANIFEST_SHA256
#ifndef COMMAND_CLIENT
#define COMMAND_CLIENT "/usr/share/mpclearn/mcu/command-client"
#endif
#ifndef COMMAND_STATE
#define COMMAND_STATE "/run/mpclearn/state/command.state"
#endif
#ifndef MIRROR_STATE
#define MIRROR_STATE "/run/mpclearn/state/volume.state"
#endif
#ifndef SETTINGS_PATH
#define SETTINGS_PATH "/media/az01-internal/Settings/MPC/MPC.settings"
#endif
#ifndef SESSION_DIR
#define SESSION_DIR "/data/mpclearn/session"
#endif
#ifndef SYSTEMCTL_PATH
#define SYSTEMCTL_PATH "/bin/systemctl"
#endif
#ifndef DRM_DEBUG_ROOT
#define DRM_DEBUG_ROOT "/sys/kernel/debug/dri"
#endif
#define CURSOR_SLOT_RVA 0x06b220d4u
static volatile sig_atomic_t stopping;
static void stop_signal(int sig){(void)sig;stopping=1;}

typedef struct { uint32_t h[8]; uint64_t bytes; unsigned used; unsigned char block[64]; } Sha;
static uint32_t rr(uint32_t x,unsigned n){return x>>n|x<<(32-n);}
static void sha_block(Sha *s,const unsigned char *b){
 static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t w[64]; for(unsigned i=0;i<16;i++)w[i]=(uint32_t)b[4*i]<<24|(uint32_t)b[4*i+1]<<16|(uint32_t)b[4*i+2]<<8|b[4*i+3];
 for(unsigned i=16;i<64;i++){uint32_t a=w[i-15],c=w[i-2];w[i]=w[i-16]+(rr(a,7)^rr(a,18)^(a>>3))+w[i-7]+(rr(c,17)^rr(c,19)^(c>>10));}
 uint32_t a=s->h[0],c=s->h[1],d=s->h[2],e=s->h[3],f=s->h[4],g=s->h[5],h=s->h[6],j=s->h[7];
 for(unsigned i=0;i<64;i++){uint32_t t=j+(rr(f,6)^rr(f,11)^rr(f,25))+((f&g)^(~f&h))+k[i]+w[i];uint32_t u=(rr(a,2)^rr(a,13)^rr(a,22))+((a&c)^(a&d)^(c&d));j=h;h=g;g=f;f=e+t;e=d;d=c;c=a;a=t+u;}
 s->h[0]+=a;s->h[1]+=c;s->h[2]+=d;s->h[3]+=e;s->h[4]+=f;s->h[5]+=g;s->h[6]+=h;s->h[7]+=j;
}
static void sha_init(Sha *s){*s=(Sha){.h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};}
static void sha_add(Sha *s,const void *p,size_t n){const unsigned char *b=p;s->bytes+=n;while(n){unsigned m=64-s->used;if(m>n)m=(unsigned)n;memcpy(s->block+s->used,b,m);s->used+=m;b+=m;n-=m;if(s->used==64){sha_block(s,s->block);s->used=0;}}}
static void sha_end(Sha *s,unsigned char out[32]){uint64_t bits=s->bytes*8;unsigned char pad[128]={0x80};unsigned n=s->used<56?56-s->used:120-s->used;sha_add(s,pad,n);for(unsigned i=0;i<8;i++)pad[i]=(unsigned char)(bits>>(56-i*8));sha_add(s,pad,8);for(unsigned i=0;i<32;i++)out[i]=(unsigned char)(s->h[i/4]>>(24-8*(i%4)));}

typedef struct { char data[REPORT_CAP]; size_t used; bool truncated; } Report;
typedef struct {
 int id,parent; unsigned major,minor; char root[256],point[512],options[256],fstype[64],source[256];
 dev_t dev; ino_t root_ino,marker_ino; int root_fd,marker_fd; bool valid;
} Destination;
typedef struct {
 int fd; char node[32],name[128]; dev_t dev; ino_t ino; unsigned connections;
 uint64_t motion,left_down,left_up,wheel; uint64_t first_ms,last_ms;
} Input;
typedef struct {
 uint32_t object,plane,display_count,device_count; int32_t x,y; unsigned enabled; uint64_t at_ms;
} CursorSample;
typedef struct {
 pid_t pid; unsigned long long start; char sha[65]; bool exact,observer_mapped,state_command,state_mirror;
 dev_t exe_dev; ino_t exe_ino; off_t exe_size; time_t exe_mtime,exe_ctime; bool exe_key_valid; char verification[16]; uint64_t sequence;
 char observer_kind[16],observer_sha[65]; bool observer_supported;
 bool cursor_current_available,cursor_ever_available; CursorSample cursor_first,cursor_last;
 unsigned cursor_valid_samples,cursor_unavailable_samples,cursor_position_changes,cursor_enabled_changes;
 unsigned cursor_object_changes,cursor_plane_changes,cursor_device_count_changes;
 uint64_t first_ms,last_ms; unsigned samples;
} Epoch;
typedef struct {dev_t dev;ino_t ino;off_t size;time_t mtime,ctime;} FileKey;
typedef enum {VERIFY_PENDING,VERIFY_HASHING,VERIFY_EXACT,VERIFY_MISMATCH,VERIFY_UNAVAILABLE} VerifyState;
typedef struct {FileKey key;bool used;VerifyState state;pid_t pid;unsigned long long start;int fd;Sha sha;char digest[65];uint64_t last_used,last_attempt;} ExeVerification;
typedef struct {char data[32768];size_t used;bool ready,truncated;uint64_t observed_ms;} CachedText;

static uint64_t mono_ms(void){struct timespec t;if(clock_gettime(CLOCK_BOOTTIME,&t))return 0;return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void sat_u64(uint64_t *v){if(*v<UINT64_MAX)(*v)++;}
static void sat_u32(unsigned *v){if(*v<UINT_MAX)(*v)++;}
static void safe_text(char *s){for(;*s;s++)if((unsigned char)*s<0x20||(unsigned char)*s>0x7e)*s='?';}
static void report_add(Report *r,const char *fmt,...){
 if(r->truncated||r->used>=sizeof(r->data))return;
 va_list ap;va_start(ap,fmt);int n=vsnprintf(r->data+r->used,sizeof(r->data)-r->used,fmt,ap);va_end(ap);
 if(n<0){r->truncated=true;return;} if((size_t)n>=sizeof(r->data)-r->used){r->used=sizeof(r->data)-1;r->truncated=true;return;}r->used+=(size_t)n;
}
static void cache_add(CachedText *r,const char *fmt,...){
 if(r->truncated||r->used>=sizeof(r->data))return;
 va_list ap;va_start(ap,fmt);int n=vsnprintf(r->data+r->used,sizeof(r->data)-r->used,fmt,ap);va_end(ap);
 if(n<0){r->truncated=true;return;}if((size_t)n>=sizeof(r->data)-r->used){r->used=sizeof(r->data)-1;r->truncated=true;return;}r->used+=(size_t)n;
}
static bool regular_nofollow(const char *path,struct stat *st){return !lstat(path,st)&&S_ISREG(st->st_mode)&&!S_ISLNK(st->st_mode);}
static bool hash_file(const char *path,char out[65]){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;Sha s;sha_init(&s);unsigned char b[32768];ssize_t n;bool ok=true;
 while((n=read(fd,b,sizeof(b)))!=0){if(n<0){if(errno==EINTR)continue;ok=false;break;}sha_add(&s,b,(size_t)n);}if(close(fd))ok=false;if(!ok)return false;
 unsigned char d[32];sha_end(&s,d);for(unsigned i=0;i<32;i++)sprintf(out+2*i,"%02x",d[i]);out[64]=0;return true;
}
static const char *verify_release_payload(void){
 char manifest[512],digest[65];snprintf(manifest,sizeof(manifest),"%s/payload.sha256",PAYLOAD_DIR);if(!hash_file(manifest,digest))return "unavailable";if(strcmp(digest,EXPECTED_PAYLOAD_MANIFEST_SHA))return "manifest_mismatch";
 FILE *f=fopen(manifest,"re");if(!f)return "unavailable";char line[512];unsigned count=0;bool ok=true;
 while(fgets(line,sizeof(line),f)){char expected[65],name[256],extra;if(sscanf(line,"%64[0-9a-f]  %255s %c",expected,name,&extra)!=2||strlen(expected)!=64||strchr(name,'/')||strstr(name,"..")){ok=false;break;}char path[768],actual[65];snprintf(path,sizeof(path),"%s/%s",PAYLOAD_DIR,name);if(!hash_file(path,actual)||strcmp(actual,expected)){ok=false;break;}count++;}
 if(ferror(f))ok=false;
 fclose(f);return ok&&count==14?"verified":"file_mismatch";
}
static bool write_all(int fd,const void *data,size_t size){const unsigned char *p=data;while(size){ssize_t n=write(fd,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=(size_t)n;}return true;}
static bool internal_write(const void *data,size_t size){
 const char *tmp=SCRATCH_FILE ".tmp";struct stat st;if(!lstat(tmp,&st)){if(!S_ISREG(st.st_mode)||unlink(tmp))return false;}else if(errno!=ENOENT)return false;
 int fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return false;bool ok=write_all(fd,data,size)&&!fsync(fd);if(close(fd))ok=false;
 if(ok&&rename(tmp,SCRATCH_FILE))ok=false;
 if(!ok)unlink(tmp);
 return ok;
}
static bool unescape_mount(const char *src,char *dst,size_t cap){
 size_t n=0;for(size_t i=0;src[i];i++){unsigned char c=(unsigned char)src[i];if(c=='\\'&&src[i+1]>='0'&&src[i+1]<='7'&&src[i+2]>='0'&&src[i+2]<='7'&&src[i+3]>='0'&&src[i+3]<='7'){c=(unsigned char)((src[i+1]-'0')*64+(src[i+2]-'0')*8+src[i+3]-'0');i+=3;}if(!c||n+1>=cap)return false;dst[n++]=(char)c;}dst[n]=0;return true;
}
static bool under_media(const char *p){size_t n=strlen(MEDIA_ROOT);if(strncmp(p,MEDIA_ROOT,n)||p[n]!='/')return false;const char *leaf=p+n+1;if(!*leaf||strchr(leaf,'/'))return false;return strcmp(leaf,"az01-internal")&&strcmp(leaf,"az01-internal-sd");}
static bool option_rw(const char *s){size_t n=strlen(s);return (!strncmp(s,"rw,",3)||!strcmp(s,"rw")||(n>=3&&!strcmp(s+n-3,",rw"))||strstr(s,",rw,"));}
static bool usb_block_device(unsigned maj,unsigned min){
 char link[512],resolved[1024];snprintf(link,sizeof(link),"%s/%u:%u",SYS_DEV_BLOCK_ROOT,maj,min);if(!realpath(link,resolved))return false;
 for(char *p=resolved;*p;){while(*p=='/')p++;char *end=strchr(p,'/');size_t n=end?(size_t)(end-p):strlen(p);if(n>3&&!strncmp(p,"usb",3)){bool digits=true;for(size_t i=3;i<n;i++)if(!isdigit((unsigned char)p[i]))digits=false;if(digits)return true;}if(!end)break;p=end;}
 return false;
}
static bool marker_identity(Destination *m,int marker){
 struct stat rs,ms;struct statvfs sv;
 if(fstat(m->root_fd,&rs)||fstat(marker,&ms)||!S_ISDIR(rs.st_mode)||!S_ISDIR(ms.st_mode)||rs.st_dev!=m->dev||rs.st_ino!=m->root_ino||ms.st_dev!=m->dev||fstatvfs(marker,&sv)||(sv.f_flag&ST_RDONLY)||faccessat(marker,".",W_OK,AT_EACCESS))return false;
 m->marker_fd=marker;m->marker_ino=ms.st_ino;m->valid=true;return true;
}
/* Discovery opens and classifies mounts, but never creates the marker. */
static int discover_destinations(Destination candidates[MAX_MOUNTS]){
 char path[256];snprintf(path,sizeof(path),"%s/self/mountinfo",PROC_ROOT);FILE *f=fopen(path,"re");if(!f)return -1;char *line=NULL;size_t cap=0;int found=0;bool failed=false;
 while(getline(&line,&cap,f)>0){
  char *save=NULL,*tok=strtok_r(line," ",&save);char *fields[6];int nf=0;while(tok&&nf<6){fields[nf++]=tok;tok=strtok_r(NULL," ",&save);}if(nf<6)continue;
  Destination d={.root_fd=-1,.marker_fd=-1};char *end=NULL;long id=strtol(fields[0],&end,10);if(*end||id<=0)continue;d.id=(int)id;long par=strtol(fields[1],&end,10);if(*end||par<0)continue;d.parent=(int)par;
  if(sscanf(fields[2],"%u:%u",&d.major,&d.minor)!=2||!unescape_mount(fields[3],d.root,sizeof(d.root))||!unescape_mount(fields[4],d.point,sizeof(d.point)))continue;
  snprintf(d.options,sizeof(d.options),"%s",fields[5]);
  char *dash=NULL;while(tok){if(!strcmp(tok,"-")){dash=tok;break;}tok=strtok_r(NULL," ",&save);}if(!dash)continue;char *fs=strtok_r(NULL," ",&save),*source=strtok_r(NULL," ",&save);if(!fs||!source)continue;snprintf(d.fstype,sizeof(d.fstype),"%s",fs);if(!unescape_mount(source,d.source,sizeof(d.source)))continue;
  Destination *m=&d;if(!under_media(m->point)||!option_rw(m->options)||strcmp(m->root,"/")||!usb_block_device(m->major,m->minor))continue;
  int rootfd=open(m->point,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(rootfd<0)continue;struct stat rs;struct statvfs sv;
  bool ok=!fstat(rootfd,&rs)&&S_ISDIR(rs.st_mode)&&major(rs.st_dev)==m->major&&minor(rs.st_dev)==m->minor&&!fstatvfs(rootfd,&sv)&&!(sv.f_flag&ST_RDONLY)&&faccessat(rootfd,".",W_OK,AT_EACCESS)==0;
  if(!ok){close(rootfd);continue;}m->dev=rs.st_dev;m->root_ino=rs.st_ino;m->root_fd=rootfd;
  int marker=openat(rootfd,MARKER,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(marker>=0&&!marker_identity(m,marker))close(marker);
  /* Only eligible destinations consume slots; never admit a partial list. */
  if(found==MAX_MOUNTS){if(m->marker_fd>=0)close(m->marker_fd);close(rootfd);failed=true;break;}
  candidates[found++]=*m;
 }
 if(ferror(f))failed=true;
 free(line);fclose(f);
 if(failed){for(int i=0;i<found;i++){if(candidates[i].marker_fd>=0)close(candidates[i].marker_fd);close(candidates[i].root_fd);}return -1;}
 return found;
}
static bool destination_same(const Destination *a,const Destination *b){return a->id==b->id&&a->parent==b->parent&&a->major==b->major&&a->minor==b->minor&&a->dev==b->dev&&a->root_ino==b->root_ino&&a->marker_ino==b->marker_ino&&!strcmp(a->root,b->root)&&!strcmp(a->point,b->point)&&!strcmp(a->fstype,b->fstype)&&!strcmp(a->source,b->source);}
static void close_destination(Destination *d){if(d->marker_fd>=0)close(d->marker_fd);if(d->root_fd>=0)close(d->root_fd);d->marker_fd=d->root_fd=-1;}
static void close_candidates(Destination *c,int n,int keep){for(int i=0;i<n;i++)if(i!=keep)close_destination(&c[i]);}
static bool create_marker(Destination *d){
#ifdef DIAGNOSTICS_TEST
 if(getenv("MPCLEARN_TEST_MKDIR_FAIL")){errno=EIO;return false;}
#endif
 if(d->valid||d->root_fd<0||mkdirat(d->root_fd,MARKER,0700))return false;
 int marker=openat(d->root_fd,MARKER,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(marker<0)return false;
 if(!marker_identity(d,marker)){close(marker);return false;}if(fsync(d->root_fd)){close(marker);d->marker_fd=-1;d->marker_ino=0;d->valid=false;return false;}return true;
}
enum{ADMISSION_FAILED=-1,ADMISSION_NONE=0,ADMISSION_READY=1,ADMISSION_AMBIGUOUS=2};
static int admit_destination(Destination *out){
 Destination c[MAX_MOUNTS];int n=discover_destinations(c);if(n<=0)return ADMISSION_NONE;int marked=0,chosen=-1;
 for(int i=0;i<n;i++)if(c[i].valid){marked++;chosen=i;}
 if(marked>1){close_candidates(c,n,-1);return ADMISSION_AMBIGUOUS;}
 if(marked==1){*out=c[chosen];close_candidates(c,n,chosen);return ADMISSION_READY;}
 if(n>1){close_candidates(c,n,-1);return ADMISSION_AMBIGUOUS;}
 if(!create_marker(&c[0])){close_candidates(c,n,-1);return ADMISSION_FAILED;}
 *out=c[0];return ADMISSION_READY;
}
static bool destination_current(const Destination *d){
 Destination c[MAX_MOUNTS];int n=discover_destinations(c),marked=0,chosen=-1;if(n<0)return false;for(int i=0;i<n;i++)if(c[i].valid){marked++;chosen=i;}bool same=marked==1&&destination_same(d,&c[chosen]);close_candidates(c,n,-1);struct stat rs,ms;if(fstat(d->root_fd,&rs)||rs.st_dev!=d->dev||rs.st_ino!=d->root_ino||fstat(d->marker_fd,&ms)||ms.st_dev!=d->dev||ms.st_ino!=d->marker_ino)same=false;return same;
}
static bool clear_previous_report(const Destination *d){
 if(!destination_current(d))return false;
 struct stat st;if(fstatat(d->marker_fd,"report.txt",&st,AT_SYMLINK_NOFOLLOW)){if(errno==ENOENT)return true;return false;}if(!S_ISREG(st.st_mode))return false;if(unlinkat(d->marker_fd,"report.txt",0)||fsync(d->marker_fd))return false;return destination_current(d);
}
static bool clear_owned_tmp(const Destination *d){
 struct stat st;if(fstatat(d->marker_fd,".mpclearn-report.tmp",&st,AT_SYMLINK_NOFOLLOW)){return errno==ENOENT;}if(!S_ISREG(st.st_mode)||unlinkat(d->marker_fd,".mpclearn-report.tmp",0)||fsync(d->marker_fd))return false;return true;
}
static bool atomic_external(const Destination *d,const void *data,size_t size){
 if(size>REPORT_CAP||!destination_current(d))return false;
 const char *tmp=".mpclearn-report.tmp";if(!clear_owned_tmp(d))return false;int fd=openat(d->marker_fd,tmp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return false;
 bool ok=true,renamed=false;
#ifdef DIAGNOSTICS_TEST
 char failbuf[32]="";const char *fail=getenv("MPCLEARN_TEST_FAIL_STAGE"),*failpath=getenv("MPCLEARN_TEST_FAIL_FILE");if(failpath){int tfd=open(failpath,O_RDONLY|O_CLOEXEC);if(tfd>=0){ssize_t n=read(tfd,failbuf,sizeof(failbuf)-1);close(tfd);if(n>0){failbuf[n]=0;fail=failbuf;}}}if(fail&&!strcmp(fail,"write")){errno=ENOSPC;ok=false;}
#endif
 if(ok)ok=write_all(fd,data,size);
 if(ok)ok=!fsync(fd);
 if(close(fd))ok=false;
 if(ok&&!destination_current(d))ok=false;
#ifdef DIAGNOSTICS_TEST
 if(ok&&fail&&!strcmp(fail,"rename")){errno=EIO;ok=false;}
#endif
 if(ok){if(renameat(d->marker_fd,tmp,d->marker_fd,"report.txt"))ok=false;else renamed=true;}
#ifdef DIAGNOSTICS_TEST
 if(ok&&fail&&!strcmp(fail,"dirsync")){errno=EIO;ok=false;}
#endif
 if(ok)ok=!fsync(d->marker_fd);
 if(!ok&&!renamed)unlinkat(d->marker_fd,tmp,0);
#ifdef DIAGNOSTICS_TEST
 if(renamed){const char *log=getenv("MPCLEARN_TEST_EXPORT_LOG");if(log){int lfd=open(log,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC,0600);if(lfd>=0){char line[64];int n=snprintf(line,sizeof(line),"%llu\n",(unsigned long long)mono_ms());if(n>0)write_all(lfd,line,(size_t)n);close(lfd);}}}
#endif
 return ok;
}

static bool proc_start(pid_t pid,unsigned long long *start){
 char path[128],buf[4096];snprintf(path,sizeof(path),"%s/%ld/stat",PROC_ROOT,(long)pid);int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return false;ssize_t n=read(fd,buf,sizeof(buf)-1);close(fd);if(n<=0)return false;buf[n]=0;char *p=strrchr(buf,')');if(!p||p[1]!=' ')return false;p+=2;
 unsigned field=3;char *save=NULL,*tok=strtok_r(p," ",&save);while(tok&&field<22){tok=strtok_r(NULL," ",&save);field++;}if(!tok||field!=22)return false;char *end;errno=0;unsigned long long v=strtoull(tok,&end,10);if(errno||*end)return false;*start=v;return true;
}
static bool exact_mpc_exe(pid_t pid){char p[128],link[512],deleted[640];snprintf(p,sizeof(p),"%s/%ld/exe",PROC_ROOT,(long)pid);ssize_t n=readlink(p,link,sizeof(link)-1);if(n<=0)return false;link[n]=0;snprintf(deleted,sizeof(deleted),"%s (deleted)",MPC_EXE_PATH);return !strcmp(link,MPC_EXE_PATH)||!strcmp(link,deleted);}
static FileKey file_key(const struct stat *st){return (FileKey){st->st_dev,st->st_ino,st->st_size,st->st_mtime,st->st_ctime};}
static bool same_key(const FileKey *a,const FileKey *b){return a->dev==b->dev&&a->ino==b->ino&&a->size==b->size&&a->mtime==b->mtime&&a->ctime==b->ctime;}
static bool process_exe_key(pid_t pid,unsigned long long start,FileKey *key){
 unsigned long long confirmed=0;if(!proc_start(pid,&confirmed)||confirmed!=start||!exact_mpc_exe(pid))return false;char path[128];snprintf(path,sizeof(path),"%s/%ld/exe",PROC_ROOT,(long)pid);int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return false;struct stat st;bool ok=!fstat(fd,&st)&&S_ISREG(st.st_mode);close(fd);if(ok)*key=file_key(&st);return ok;
}
static ExeVerification *verification_find(ExeVerification cache[MAX_EXE_CACHE],const FileKey *key){for(int i=0;i<MAX_EXE_CACHE;i++)if(cache[i].used&&same_key(&cache[i].key,key))return &cache[i];return NULL;}
static ExeVerification *verification_request(ExeVerification cache[MAX_EXE_CACHE],const FileKey *key,pid_t pid,unsigned long long start,uint64_t now){
 ExeVerification *v=verification_find(cache,key);if(v){v->pid=pid;v->start=start;v->last_used=now;if(v->state==VERIFY_UNAVAILABLE&&now-v->last_attempt>=HASH_START_INTERVAL_MS)v->state=VERIFY_PENDING;return v;}
 int slot=-1;uint64_t oldest=UINT64_MAX;for(int i=0;i<MAX_EXE_CACHE;i++){if(!cache[i].used){slot=i;break;}if(cache[i].state!=VERIFY_HASHING&&cache[i].state!=VERIFY_EXACT&&cache[i].last_used<oldest){oldest=cache[i].last_used;slot=i;}}
 if(slot<0)return NULL;
 if(cache[slot].used&&cache[slot].fd>=0)close(cache[slot].fd);
 cache[slot]=(ExeVerification){.key=*key,.used=true,.state=VERIFY_PENDING,.pid=pid,.start=start,.fd=-1,.last_used=now};
 return &cache[slot];
}
static const char *verification_name(VerifyState state){switch(state){case VERIFY_PENDING:case VERIFY_HASHING:return "pending";case VERIFY_EXACT:return "exact";case VERIFY_MISMATCH:return "mismatch";default:return "unavailable";}}
static bool verification_step(ExeVerification cache[MAX_EXE_CACHE],uint64_t now,uint64_t *next_start,uint64_t *hashes_started){
 ExeVerification *active=NULL;for(int i=0;i<MAX_EXE_CACHE;i++)if(cache[i].used&&cache[i].state==VERIFY_HASHING){active=&cache[i];break;}
 if(!active&&now>=*next_start){uint64_t newest=0;for(int i=0;i<MAX_EXE_CACHE;i++)if(cache[i].used&&cache[i].state==VERIFY_PENDING&&(!active||cache[i].last_used>=newest)){active=&cache[i];newest=cache[i].last_used;}if(active){char path[128];snprintf(path,sizeof(path),"%s/%ld/exe",PROC_ROOT,(long)active->pid);unsigned long long confirmed=0;struct stat st;active->last_attempt=now;active->fd=open(path,O_RDONLY|O_CLOEXEC);if(active->fd<0||fstat(active->fd,&st)||!same_key(&active->key,&(FileKey){st.st_dev,st.st_ino,st.st_size,st.st_mtime,st.st_ctime})||!proc_start(active->pid,&confirmed)||confirmed!=active->start){if(active->fd>=0)close(active->fd);active->fd=-1;active->state=VERIFY_UNAVAILABLE;return true;}sha_init(&active->sha);active->state=VERIFY_HASHING;sat_u64(hashes_started);*next_start=now+HASH_START_INTERVAL_MS;
#ifdef DIAGNOSTICS_TEST
   const char *log=getenv("MPCLEARN_TEST_HASH_LOG");if(log){int lfd=open(log,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC,0600);if(lfd>=0){write_all(lfd,"hash\n",5);close(lfd);}}
#endif
  }}
 if(!active||active->state!=VERIFY_HASHING)return false;
 unsigned char block[32768];size_t total=0;while(total<HASH_BYTES_PER_TICK){size_t want=sizeof(block);if(want>HASH_BYTES_PER_TICK-total)want=HASH_BYTES_PER_TICK-total;ssize_t n=read(active->fd,block,want);if(n<0&&errno==EINTR)continue;if(n<0){close(active->fd);active->fd=-1;active->state=VERIFY_UNAVAILABLE;return true;}if(!n){unsigned char d[32];struct stat st;bool stable=!fstat(active->fd,&st)&&same_key(&active->key,&(FileKey){st.st_dev,st.st_ino,st.st_size,st.st_mtime,st.st_ctime});close(active->fd);active->fd=-1;if(!stable){active->state=VERIFY_UNAVAILABLE;return true;}sha_end(&active->sha,d);for(unsigned i=0;i<32;i++)sprintf(active->digest+2*i,"%02x",d[i]);active->digest[64]=0;active->state=!strcmp(active->digest,EXPECTED_MPC_SHA)?VERIFY_EXACT:VERIFY_MISMATCH;return true;}sha_add(&active->sha,block,(size_t)n);total+=(size_t)n;}
 return false;
}
static int find_mpc(pid_t *pid,unsigned long long *start){
 DIR *d=opendir(PROC_ROOT);if(!d)return -1;struct dirent *e;int count=0;pid_t found=0;unsigned long long tick=0;
 while((e=readdir(d))){if(!isdigit((unsigned char)e->d_name[0]))continue;char *end;long p=strtol(e->d_name,&end,10);if(*end||p<=1)continue;unsigned long long t;if(!exact_mpc_exe((pid_t)p)||!proc_start((pid_t)p,&t))continue;found=(pid_t)p;tick=t;count++;}
 closedir(d);if(count==1){*pid=found;*start=tick;}return count;
}
static bool map_identity(pid_t pid,const char *wanted,bool *mapped,unsigned long *bias){
 *mapped=false;if(bias)*bias=0;struct stat ws;if(stat(wanted,&ws))return false;char path[128],line[1024];snprintf(path,sizeof(path),"%s/%ld/maps",PROC_ROOT,(long)pid);FILE *f=fopen(path,"re");if(!f)return false;
 while(fgets(line,sizeof(line),f)){unsigned long lo,hi,off,inode;unsigned maj,min;char perms[8],name[512]="";int got=sscanf(line,"%lx-%lx %7s %lx %x:%x %lu %511[^\n]",&lo,&hi,perms,&off,&maj,&min,&inode,name);if(got<7)continue;
  if((dev_t)makedev(maj,min)==ws.st_dev&&(ino_t)inode==ws.st_ino){*mapped=true;if(bias&&off==0)*bias=lo;}
 }
 fclose(f);return true;
}
static bool map_exe_key(pid_t pid,const FileKey *key,bool *mapped,unsigned long *bias){
 *mapped=false;if(bias)*bias=0;char path[128],line[1024];snprintf(path,sizeof(path),"%s/%ld/maps",PROC_ROOT,(long)pid);FILE *f=fopen(path,"re");if(!f)return false;
 while(fgets(line,sizeof(line),f)){unsigned long lo,hi,off,inode;unsigned maj,min;char perms[8];int got=sscanf(line,"%lx-%lx %7s %lx %x:%x %lu",&lo,&hi,perms,&off,&maj,&min,&inode);if(got<7)continue;if((dev_t)makedev(maj,min)==key->dev&&(ino_t)inode==key->ino){*mapped=true;if(bias&&off==0)*bias=lo;}}
 fclose(f);return true;
}
static FileKey epoch_key(const Epoch *e){return (FileKey){e->exe_dev,e->exe_ino,e->exe_size,e->exe_mtime,e->exe_ctime};}
static bool epoch_identity_current(const Epoch *e){if(!e->exe_key_valid)return false;FileKey current,key=epoch_key(e);return process_exe_key(e->pid,e->start,&current)&&same_key(&current,&key);}
static bool sync_epoch_verification(Epoch *e,ExeVerification cache[MAX_EXE_CACHE]){
 FileKey key=epoch_key(e);ExeVerification *v=e->exe_key_valid?verification_find(cache,&key):NULL;const char *name=v?verification_name(v->state):"unavailable";char digest[65]="unavailable";bool exact=false;if(v&&(v->state==VERIFY_EXACT||v->state==VERIFY_MISMATCH)){snprintf(digest,sizeof(digest),"%s",v->digest);exact=v->state==VERIFY_EXACT;}else if(v&&(v->state==VERIFY_PENDING||v->state==VERIFY_HASHING))snprintf(digest,sizeof(digest),"pending");
 bool changed=e->exact!=exact||strcmp(e->verification,name)||strcmp(e->sha,digest);e->exact=exact;snprintf(e->verification,sizeof(e->verification),"%s",name);snprintf(e->sha,sizeof(e->sha),"%s",digest);return changed;
}
static bool read_remote(pid_t pid,uintptr_t at,void *out,size_t n){char path[128];snprintf(path,sizeof(path),"%s/%ld/mem",PROC_ROOT,(long)pid);int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;ssize_t got=pread(fd,out,n,(off_t)at);close(fd);return got==(ssize_t)n;}
static bool cursor_sample(Epoch *e,uint64_t elapsed){
 CursorSample sample={.at_ms=elapsed};bool ok=false;
 if(e->exact){bool mapped=false;unsigned long bias=0;FileKey key=epoch_key(e);if(epoch_identity_current(e)&&map_exe_key(e->pid,&key,&mapped,&bias)&&mapped&&bias&&epoch_identity_current(e)&&read_remote(e->pid,bias+CURSOR_SLOT_RVA,&sample.object,sizeof(sample.object))&&sample.object){unsigned char b[0x3c];if(epoch_identity_current(e)&&read_remote(e->pid,sample.object,b,sizeof(b))){uint32_t begin,end;memcpy(&sample.plane,b+0x0c,4);memcpy(&sample.display_count,b+0x20,4);memcpy(&begin,b+0x24,4);memcpy(&end,b+0x28,4);memcpy(&sample.x,b+0x30,4);memcpy(&sample.y,b+0x34,4);sample.enabled=b[0x38];if(end>=begin&&end-begin<=4096&&(end-begin)%4==0){sample.device_count=(end-begin)/4;ok=true;}}}}
 bool changed=e->cursor_current_available!=ok;e->cursor_current_available=ok;if(!ok){sat_u32(&e->cursor_unavailable_samples);return changed;}
 if(!e->cursor_ever_available){e->cursor_first=sample;changed=true;}else{CursorSample *last=&e->cursor_last;if(last->x!=sample.x||last->y!=sample.y){sat_u32(&e->cursor_position_changes);changed=true;}if(last->enabled!=sample.enabled){sat_u32(&e->cursor_enabled_changes);changed=true;}if(last->object!=sample.object){sat_u32(&e->cursor_object_changes);changed=true;}if(last->plane!=sample.plane){sat_u32(&e->cursor_plane_changes);changed=true;}if(last->device_count!=sample.device_count){sat_u32(&e->cursor_device_count_changes);changed=true;}}
 e->cursor_ever_available=true;e->cursor_last=sample;sat_u32(&e->cursor_valid_samples);return changed;
}
static bool epoch_refresh(Epoch *e,uint64_t elapsed){
 bool changed=false;
 char previous[16];snprintf(previous,sizeof(previous),"%s",e->observer_kind);bool release=false,dev=false;if(map_identity(e->pid,OBSERVER_PATH,&release,NULL)&&release){e->observer_mapped=true;snprintf(e->observer_kind,sizeof(e->observer_kind),"release");}else if(map_identity(e->pid,DEV_OBSERVER_PATH,&dev,NULL)&&dev){e->observer_mapped=true;snprintf(e->observer_kind,sizeof(e->observer_kind),"development");}else{e->observer_mapped=false;snprintf(e->observer_kind,sizeof(e->observer_kind),"none");}
 if(strcmp(previous,e->observer_kind)){changed=true;e->observer_sha[0]=0;e->observer_supported=false;if(e->observer_mapped){const char *observer=!strcmp(e->observer_kind,"release")?OBSERVER_PATH:DEV_OBSERVER_PATH;if(hash_file(observer,e->observer_sha))e->observer_supported=!strcmp(e->observer_sha,EXPECTED_OBSERVER_SHA);else snprintf(e->observer_sha,sizeof(e->observer_sha),"unavailable");}else snprintf(e->observer_sha,sizeof(e->observer_sha),"unavailable");}
 struct stat st;bool command=regular_nofollow(COMMAND_STATE,&st),mirror=regular_nofollow(MIRROR_STATE,&st);if(command!=e->state_command||mirror!=e->state_mirror)changed=true;e->state_command=command;e->state_mirror=mirror;if(cursor_sample(e,elapsed))changed=true;e->last_ms=elapsed;sat_u32(&e->samples);return changed;
}

static bool bit(const unsigned long *b,unsigned n){return b[n/(8*sizeof(unsigned long))]&(1ul<<(n%(8*sizeof(unsigned long))));}
static void close_input(Input *in){if(in->fd>=0)close(in->fd);in->fd=-1;}
static bool input_path_stat(const char *node,struct stat *st){char p[512];snprintf(p,sizeof(p),"%s/%s",DEV_INPUT_ROOT,node);return !lstat(p,st)&&!S_ISLNK(st->st_mode);}
static bool refresh_input_fds(Input inputs[MAX_INPUTS],int count){bool changed=false;for(int i=0;i<count;i++)if(inputs[i].fd>=0){struct stat path,openst;if(!input_path_stat(inputs[i].node,&path)||fstat(inputs[i].fd,&openst)||path.st_dev!=openst.st_dev||path.st_ino!=openst.st_ino){close_input(&inputs[i]);changed=true;}}return changed;}
static bool open_pointer(Input *in,const char *node){
 char p[512];snprintf(p,sizeof(p),"%s/%s",DEV_INPUT_ROOT,node);struct stat before,after;if(lstat(p,&before)||S_ISLNK(before.st_mode))return false;
#ifndef DIAGNOSTICS_TEST
 if(!S_ISCHR(before.st_mode))return false;
#endif
 int fd=open(p,O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);if(fd<0||fstat(fd,&after)||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino){if(fd>=0)close(fd);return false;}
 unsigned long ev[(EV_MAX+8*sizeof(unsigned long))/(8*sizeof(unsigned long))];unsigned long rel[(REL_MAX+8*sizeof(unsigned long))/(8*sizeof(unsigned long))];unsigned long keys[(KEY_MAX+8*sizeof(unsigned long))/(8*sizeof(unsigned long))];memset(ev,0,sizeof(ev));memset(rel,0,sizeof(rel));memset(keys,0,sizeof(keys));
 bool pointer=ioctl(fd,EVIOCGBIT(0,sizeof(ev)),ev)>=0&&bit(ev,EV_REL)&&ioctl(fd,EVIOCGBIT(EV_REL,sizeof(rel)),rel)>=0&&bit(rel,REL_X)&&bit(rel,REL_Y)&&bit(ev,EV_KEY)&&ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(keys)),keys)>=0&&bit(keys,BTN_LEFT);
#ifdef DIAGNOSTICS_TEST
 if(!pointer&&getenv("MPCLEARN_TEST_EVDEV")&&!strcmp(node,"event-fixture"))pointer=true;
#endif
 if(!pointer){close(fd);return false;}in->fd=fd;in->dev=after.st_dev;in->ino=after.st_ino;
#ifdef DIAGNOSTICS_TEST
 if(getenv("MPCLEARN_TEST_SATURATE")){in->connections=UINT_MAX-1;in->motion=UINT64_MAX-1;in->left_down=UINT64_MAX-1;in->left_up=UINT64_MAX-1;in->wheel=UINT64_MAX-1;}
#endif
 sat_u32(&in->connections);snprintf(in->node,sizeof(in->node),"%.31s",node);const char *fallback="unavailable";
#ifdef DIAGNOSTICS_TEST
 if(getenv("MPCLEARN_TEST_EVDEV"))fallback="fixture-pointer";
#endif
 if(ioctl(fd,EVIOCGNAME(sizeof(in->name)),in->name)<0)snprintf(in->name,sizeof(in->name),"%s",fallback);
 safe_text(in->name);return true;
}
static int discover_inputs(Input inputs[MAX_INPUTS],int count,bool *changed){
 if(refresh_input_fds(inputs,count))*changed=true;
 DIR *d=opendir(DEV_INPUT_ROOT);if(!d)return count;struct dirent *e;while((e=readdir(d))){if(strncmp(e->d_name,"event",5))continue;int known=-1;for(int i=0;i<count;i++)if(!strcmp(inputs[i].node,e->d_name)){known=i;break;}if(known>=0){Input *in=&inputs[known];if(in->fd>=0)continue;struct stat st;if(!input_path_stat(in->node,&st)||(st.st_dev==in->dev&&st.st_ino==in->ino))continue;if(open_pointer(in,e->d_name))*changed=true;continue;}if(count>=MAX_INPUTS)continue;Input fresh={.fd=-1};if(open_pointer(&fresh,e->d_name)){inputs[count++]=fresh;*changed=true;}
 }
 closedir(d);return count;
}
static bool drain_input(Input *in,uint64_t elapsed){
 bool changed=false;enum{MAX_EVENTS_PER_TICK=256};struct input_event ev[64];unsigned handled=0;while(in->fd>=0&&handled<MAX_EVENTS_PER_TICK){size_t want=MAX_EVENTS_PER_TICK-handled;if(want>64)want=64;ssize_t n=read(in->fd,ev,want*sizeof(ev[0]));if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return changed;if(n<=0){close_input(in);return true;}size_t count=(size_t)n/sizeof(ev[0]);if(!count){close_input(in);return true;}handled+=(unsigned)count;for(size_t i=0;i<count;i++){bool used=false;if(ev[i].type==EV_REL&&(ev[i].code==REL_X||ev[i].code==REL_Y)){sat_u64(&in->motion);used=true;}else if(ev[i].type==EV_REL&&(ev[i].code==REL_WHEEL||ev[i].code==REL_HWHEEL||ev[i].code==REL_WHEEL_HI_RES||ev[i].code==REL_HWHEEL_HI_RES)){sat_u64(&in->wheel);used=true;}else if(ev[i].type==EV_KEY&&ev[i].code==BTN_LEFT){if(ev[i].value)sat_u64(&in->left_down);else sat_u64(&in->left_up);used=true;}if(used){changed=true;uint64_t stamp=elapsed?elapsed:1;if(!in->first_ms)in->first_ms=stamp;in->last_ms=stamp;}}}return changed;
}

static void drain_child_pipe(int fd,char *output,size_t cap,size_t *used,unsigned max_reads){char buf[512];for(unsigned i=0;i<max_reads;i++){ssize_t n=read(fd,buf,sizeof(buf));if(n>0){if(output&&cap&&*used+1<cap){size_t keep=(size_t)n;if(keep>cap-*used-1)keep=cap-*used-1;memcpy(output+*used,buf,keep);*used+=keep;}continue;}if(n<0&&errno==EINTR){i--;continue;}break;}}
static int child_status(char *const argv[],unsigned timeout_ms,char *output,size_t cap){
 int pipefd[2];if(pipe2(pipefd,O_CLOEXEC|O_NONBLOCK))return -1;pid_t p=fork();if(p<0){close(pipefd[0]);close(pipefd[1]);return -1;}if(!p){dup2(pipefd[1],STDOUT_FILENO);dup2(pipefd[1],STDERR_FILENO);close(pipefd[0]);close(pipefd[1]);execv(argv[0],argv);_exit(127);}close(pipefd[1]);size_t used=0;uint64_t end=mono_ms()+timeout_ms;int status=0;bool done=false;
 while(mono_ms()<end){drain_child_pipe(pipefd[0],output,cap,&used,8);pid_t w=waitpid(p,&status,WNOHANG);if(w==p){done=true;break;}if(w<0&&errno!=EINTR)break;struct pollfd pf={pipefd[0],POLLIN,0};poll(&pf,1,25);}
 if(!done){kill(p,SIGKILL);while(waitpid(p,&status,0)<0&&errno==EINTR){}}drain_child_pipe(pipefd[0],output,cap,&used,64);if(output&&cap)output[used]=0;close(pipefd[0]);if(!done)return -2;if(WIFEXITED(status))return WEXITSTATUS(status);return -3;
}
static void unit_report(CachedText *r,const char *unit){
 char *argv[]={SYSTEMCTL_PATH,"--no-pager","show",(char *)unit,"-p","ActiveState","-p","SubState","-p","Result","-p","ExecMainStatus",NULL};char out[1024];int code=child_status(argv,QUERY_TIMEOUT_MS,out,sizeof(out));char active[64]="unavailable",sub[64]="unavailable",result[64]="unavailable",status[32]="unavailable";
 if(code==0){char *save=NULL;for(char *line=strtok_r(out,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){char *eq=strchr(line,'=');if(!eq)continue;*eq++=0;bool safe=*eq;for(char *p=eq;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='_'&&*p!='-')safe=false;if(!safe)continue;if(!strcmp(line,"ActiveState"))snprintf(active,sizeof(active),"%s",eq);else if(!strcmp(line,"SubState"))snprintf(sub,sizeof(sub),"%s",eq);else if(!strcmp(line,"Result"))snprintf(result,sizeof(result),"%s",eq);else if(!strcmp(line,"ExecMainStatus"))snprintf(status,sizeof(status),"%s",eq);}}
 cache_add(r,"unit name=%s active=%s sub=%s result=%s exec_status=%s query_exit=%d\n",unit,active,sub,result,status,code);
}
static int command_health(void){struct stat a,b;if(!regular_nofollow(COMMAND_CLIENT,&a)||!regular_nofollow(COMMAND_STATE,&a)||!regular_nofollow(MIRROR_STATE,&b))return -4;char *argv[]={COMMAND_CLIENT,"session-status",COMMAND_STATE,MIRROR_STATE,NULL};return child_status(argv,QUERY_TIMEOUT_MS,NULL,0);}
typedef struct {unsigned card,id,fb,crtc_w,crtc_h,src_w,src_h;char name[32],crtc[32],format[16];} DrmPlane;
static void drm_report(CachedText *r){
 enum{MAX_DRM_PLANES=32};DrmPlane planes[MAX_DRM_PLANES];memset(planes,0,sizeof(planes));unsigned cards=0,readable=0,count=0,total=0;bool truncated=false;
 for(unsigned card=0;card<16;card++){char path[256],line[256];snprintf(path,sizeof(path),"%s/%u/name",DRM_DEBUG_ROOT,card);FILE *f=fopen(path,"re");if(!f)continue;if(!fgets(line,sizeof(line),f)){fclose(f);continue;}fclose(f);for(char *p=line;*p;p++)*p=(char)tolower((unsigned char)*p);if(!strstr(line,"rockchip"))continue;cards++;snprintf(path,sizeof(path),"%s/%u/state",DRM_DEBUG_ROOT,card);f=fopen(path,"re");if(!f)continue;readable++;int current=-1;
  while(fgets(line,sizeof(line),f)){unsigned id=0;char name[32];if(sscanf(line,"plane[%u]: %31s",&id,name)==2){total++;if(count<MAX_DRM_PLANES){current=(int)count;planes[count]=(DrmPlane){.card=card,.id=id};snprintf(planes[count].name,sizeof(planes[count].name),"%s",name);count++;}else{current=-1;truncated=true;}continue;}if(line[0]&&!isspace((unsigned char)line[0])){current=-1;continue;}if(current<0)continue;char *v=line;while(isspace((unsigned char)*v))v++;DrmPlane *p=&planes[current];if(sscanf(v,"fb=%u",&p->fb)==1)continue;if(sscanf(v,"crtc-pos=%ux%u",&p->crtc_w,&p->crtc_h)==2)continue;if(sscanf(v,"src-pos=%u.%*ux%u.%*u",&p->src_w,&p->src_h)==2)continue;if(!strncmp(v,"crtc=",5))sscanf(v+5,"%31s",p->crtc);else if(!strncmp(v,"format=",7))sscanf(v+7,"%15s",p->format);}
  fclose(f);
 }
 cache_add(r,"drm rockchip_cards=%u debug_state_readable=%u planes=%u planes_total=%u planes_truncated=%s cursor_classification=unknown\n",cards,readable,count,total,truncated?"yes":"no");for(unsigned i=0;i<count;i++){DrmPlane *p=&planes[i];safe_text(p->name);safe_text(p->crtc);safe_text(p->format);cache_add(r,"drm_plane card=%u id=%u name=%s classification=unknown active=%s fb=%u crtc=%s format=%s crtc_size=%ux%u src_size=%ux%u\n",p->card,p->id,p->name[0]?p->name:"unavailable",p->fb&&p->crtc[0]&&strcmp(p->crtc,"null")?"yes":"no",p->fb,p->crtc[0]?p->crtc:"unavailable",p->format[0]?p->format:"unavailable",p->crtc_w,p->crtc_h,p->src_w,p->src_h);}
}
static bool cache_commit(CachedText *cache,CachedText *next,uint64_t observed){bool changed=!cache->ready||cache->used!=next->used||memcmp(cache->data,next->data,next->used)||cache->truncated!=next->truncated;next->ready=true;next->observed_ms=observed;*cache=*next;return changed;}
static bool status_refresh(CachedText *cache,bool supported,uint64_t observed){CachedText next={0};unit_report(&next,"mpclearn-provision.service");unit_report(&next,"mpclearn-boot.service");if(supported)cache_add(&next,"command_session_status_exit=%d (0=ready,3=healthy_awaiting_project)\n",command_health());else cache_add(&next,"command_session_status_exit=unavailable reason=loaded_observer_unsupported_or_absent\n");return cache_commit(cache,&next,observed);}
static bool drm_refresh(CachedText *cache,uint64_t observed){CachedText next={0};drm_report(&next);return cache_commit(cache,&next,observed);}

typedef struct {
 uint64_t begun,capture_id,snapshot_sequence,hashes_started,total_epochs,dropped_epochs,usb_replacements;
 char payload_status[32],observer_sha[65],fast_facts[512];bool observer_hash,observer_exact;
 Destination dest;bool admitted,export_valid,export_invalidated,ever_absent,ever_ambiguous;
 Input inputs[MAX_INPUTS];int input_count;Epoch epochs[MAX_EPOCHS];int epoch_count;bool epochs_truncated;
 pid_t current_pid;unsigned long long current_start;ExeVerification verifications[MAX_EXE_CACHE];uint64_t next_hash_start;
 CachedText status_cache,drm_cache;uint64_t next_status_refresh,next_drm_refresh;
} Collector;

static void fast_facts(char out[512]){
 struct stat st;char settings[192];if(lstat(SETTINGS_PATH,&st))snprintf(settings,sizeof(settings),"settings=absent errno=%d",errno);else if(S_ISLNK(st.st_mode))snprintf(settings,sizeof(settings),"settings=unsupported_link");else if(S_ISREG(st.st_mode))snprintf(settings,sizeof(settings),"settings=present size=%lld mode=%03o mtime=%lld",(long long)st.st_size,(unsigned)(st.st_mode&0777),(long long)st.st_mtime);else snprintf(settings,sizeof(settings),"settings=unsupported_type");
 bool ready=!lstat(SESSION_DIR "/generation.ready",&st)&&S_ISREG(st.st_mode),failed=!lstat(SESSION_DIR "/generation.failed",&st)&&S_ISREG(st.st_mode);snprintf(out,512,"%s\nsession generation_ready=%s generation_failed=%s\n",settings,ready?"yes":"no",failed?"yes":"no");
}
static bool current_observer_supported(const Collector *c){if(!c->epoch_count)return false;const Epoch *e=&c->epochs[c->epoch_count-1];return e->pid==c->current_pid&&e->start==c->current_start&&e->exact&&e->observer_mapped&&e->observer_supported;}
static void mark_dirty(bool *dirty,uint64_t *since,uint64_t now){if(!*dirty)*since=now;*dirty=true;}
static bool add_epoch(Collector *c,pid_t pid,unsigned long long start,uint64_t elapsed,uint64_t now){
 if(c->epoch_count==MAX_EPOCHS){memmove(&c->epochs[0],&c->epochs[1],(MAX_EPOCHS-1)*sizeof(c->epochs[0]));c->epoch_count--;c->epochs_truncated=true;sat_u64(&c->dropped_epochs);}
 Epoch *e=&c->epochs[c->epoch_count++];memset(e,0,sizeof(*e));e->pid=pid;e->start=start;e->first_ms=e->last_ms=elapsed;sat_u64(&c->total_epochs);e->sequence=c->total_epochs;snprintf(e->sha,sizeof(e->sha),"unavailable");snprintf(e->verification,sizeof(e->verification),"unavailable");snprintf(e->observer_kind,sizeof(e->observer_kind),"none");snprintf(e->observer_sha,sizeof(e->observer_sha),"unavailable");
 FileKey key;if(process_exe_key(pid,start,&key)){e->exe_key_valid=true;e->exe_dev=key.dev;e->exe_ino=key.ino;e->exe_size=key.size;e->exe_mtime=key.mtime;e->exe_ctime=key.ctime;ExeVerification *v=verification_request(c->verifications,&key,pid,start,now);if(v)sync_epoch_verification(e,c->verifications);}
 return true;
}
static bool refresh_epoch(Collector *c,uint64_t elapsed,uint64_t now){
 pid_t pid=0;unsigned long long start=0;int found=find_mpc(&pid,&start);bool changed=false;
 bool executable_changed=false;if(found==1&&pid==c->current_pid&&start==c->current_start&&c->epoch_count){Epoch *last=&c->epochs[c->epoch_count-1];FileKey current,key=epoch_key(last);if(process_exe_key(pid,start,&current)&&(!last->exe_key_valid||!same_key(&current,&key)))executable_changed=true;}
 if(found==1&&(pid!=c->current_pid||start!=c->current_start||executable_changed)){c->current_pid=pid;c->current_start=start;changed=add_epoch(c,pid,start,elapsed,now);}
 else if(found!=1){if(c->current_pid||c->current_start)changed=true;if(c->epoch_count&&c->epochs[c->epoch_count-1].pid==c->current_pid&&c->epochs[c->epoch_count-1].start==c->current_start&&c->epochs[c->epoch_count-1].cursor_current_available){c->epochs[c->epoch_count-1].cursor_current_available=false;changed=true;}c->current_pid=0;c->current_start=0;}
 if(c->epoch_count){Epoch *e=&c->epochs[c->epoch_count-1];if(e->pid==c->current_pid&&e->start==c->current_start){if(e->exe_key_valid){FileKey key=epoch_key(e);verification_request(c->verifications,&key,e->pid,e->start,now);}if(sync_epoch_verification(e,c->verifications))changed=true;if(epoch_refresh(e,elapsed))changed=true;}}
 return changed;
}
static void report_cache(Report *r,const char *name,const CachedText *cache,uint64_t elapsed){if(!cache->ready){report_add(r,"%s_cache=pending\n",name);return;}uint64_t age=elapsed>=cache->observed_ms?elapsed-cache->observed_ms:0;report_add(r,"%s_cache=available observed_ms=%llu age_ms=%llu truncated=%s\n",name,(unsigned long long)cache->observed_ms,(unsigned long long)age,cache->truncated?"yes":"no");report_add(r,"%.*s",(int)cache->used,cache->data);}
static void finish_report(Report *r){
 const char *tail="truncated=no\nresult=current\ncollector_running_at_write=yes\n";report_add(r,"%s",tail);if(r->truncated){tail="\ntruncated=yes\nresult=current\ncollector_running_at_write=yes\n";size_t n=strlen(tail);size_t at=REPORT_CAP-1-n;r->used=at;memcpy(r->data+at,tail,n);r->used+=n;}
}
static void build_report(Collector *c,Report *r,uint64_t now){
 memset(r,0,sizeof(*r));uint64_t elapsed=now-c->begun;time_t wall=time(NULL);unsigned connected=0;for(int i=0;i<c->input_count;i++)if(c->inputs[i].fd>=0)connected++;
 report_add(r,"MPCLEARN USB debug report\nformat=2\ncollector=usb-diagnostics-v2\ncapture_id=%llu\nsnapshot_sequence=%llu\nsnapshot_unix_seconds=%lld\nsnapshot_ms_since_start=%llu\n",(unsigned long long)c->capture_id,(unsigned long long)c->snapshot_sequence,(long long)wall,(unsigned long long)elapsed);
 report_add(r,"release_payload=%s\nobserver installed_sha256=%s expected=%s\nexecutable_hashes_started=%llu\n",c->payload_status,c->observer_hash?c->observer_sha:"unavailable",c->observer_exact?"yes":"no",(unsigned long long)c->hashes_started);
 report_add(r,"destination admitted=%s export_valid=%s invalidated=%s absent_seen=%s ambiguity_seen=%s prior_usb_replacements=%llu",c->admitted?"yes":"no",c->export_valid?"yes":"no",c->export_invalidated?"yes":"no",c->ever_absent?"yes":"no",c->ever_ambiguous?"yes":"no",(unsigned long long)c->usb_replacements);if(c->admitted)report_add(r," mount_id=%d device=%u:%u fstype=%s\n",c->dest.id,c->dest.major,c->dest.minor,c->dest.fstype);else report_add(r,"\n");
 report_add(r,"%s",c->fast_facts);report_cache(r,"status",&c->status_cache,elapsed);
 report_add(r,"mpc_epochs=%d total_epochs=%llu dropped_epochs=%llu epochs_truncated=%s current_pid=%ld current_start_ticks=%llu\n",c->epoch_count,(unsigned long long)c->total_epochs,(unsigned long long)c->dropped_epochs,c->epochs_truncated?"yes":"no",(long)c->current_pid,c->current_start);
 for(int i=0;i<c->epoch_count;i++){Epoch *e=&c->epochs[i];report_add(r,"epoch index=%d sequence=%llu pid=%ld start_ticks=%llu first_ms=%llu last_ms=%llu exe_sha256=%s exe_verification=%s exact_build=%s observer_mapped=%s loaded_package=%s observer_sha256=%s observer_supported=%s command_state=%s mirror_state=%s samples=%u\n",i+1,(unsigned long long)e->sequence,(long)e->pid,e->start,(unsigned long long)e->first_ms,(unsigned long long)e->last_ms,e->sha,e->verification,e->exact?"yes":"no",e->observer_mapped?"yes":"no",e->observer_kind,e->observer_sha,e->observer_supported?"yes":"no",e->state_command?"present":"absent",e->state_mirror?"present":"absent",e->samples);report_add(r,"cursor epoch=%d current_available=%s ever_available=%s valid_samples=%u unavailable_samples=%u position_changes=%u enabled_changes=%u object_changes=%u plane_changes=%u device_count_changes=%u\n",i+1,e->cursor_current_available?"yes":"no",e->cursor_ever_available?"yes":"no",e->cursor_valid_samples,e->cursor_unavailable_samples,e->cursor_position_changes,e->cursor_enabled_changes,e->cursor_object_changes,e->cursor_plane_changes,e->cursor_device_count_changes);if(e->cursor_ever_available){CursorSample *first=&e->cursor_first,*last=&e->cursor_last;report_add(r,"cursor_first epoch=%d at_ms=%llu object=0x%08x enabled=%u plane=0x%08x display_count=%u device_count=%u x=%d y=%d\n",i+1,(unsigned long long)first->at_ms,first->object,first->enabled,first->plane,first->display_count,first->device_count,first->x,first->y);report_add(r,"cursor_last epoch=%d at_ms=%llu object=0x%08x enabled=%u plane=0x%08x display_count=%u device_count=%u x=%d y=%d\n",i+1,(unsigned long long)last->at_ms,last->object,last->enabled,last->plane,last->display_count,last->device_count,last->x,last->y);}else report_add(r,"cursor_unavailable epoch=%d reason=%s\n",i+1,e->exact?"memory_access_or_object_unavailable":e->verification);}
 report_add(r,"pointer_devices=%d connected_at_write=%u zero_counts_require_user_motion=yes\n",c->input_count,connected);for(int i=0;i<c->input_count;i++)report_add(r,"input node=%s name=%s connections=%u motion=%llu left_down=%llu left_up=%llu wheel=%llu first_ms=%llu last_ms=%llu\n",c->inputs[i].node,c->inputs[i].name,c->inputs[i].connections,(unsigned long long)c->inputs[i].motion,(unsigned long long)c->inputs[i].left_down,(unsigned long long)c->inputs[i].left_up,(unsigned long long)c->inputs[i].wheel,(unsigned long long)c->inputs[i].first_ms,(unsigned long long)c->inputs[i].last_ms);
 report_cache(r,"drm",&c->drm_cache,elapsed);finish_report(r);
}

int main(void){
 signal(SIGINT,stop_signal);signal(SIGTERM,stop_signal);umask(077);Collector c={0};c.begun=mono_ms();c.capture_id=c.begun^(uint64_t)getpid();c.dest.root_fd=c.dest.marker_fd=-1;c.export_valid=true;for(int i=0;i<MAX_EXE_CACHE;i++)c.verifications[i].fd=-1;
 snprintf(c.payload_status,sizeof(c.payload_status),"%s",verify_release_payload());c.observer_hash=hash_file(OBSERVER_PATH,c.observer_sha);c.observer_exact=c.observer_hash&&!strcmp(c.observer_sha,EXPECTED_OBSERVER_SHA);fast_facts(c.fast_facts);c.next_status_refresh=c.begun+SCAN_INTERVAL_MS;c.next_drm_refresh=c.begun+2*SCAN_INTERVAL_MS;
 bool dirty=true;uint64_t dirty_since=c.begun,next_scan=c.begun,last_snapshot=0,last_external=0;bool status_supported=false;
 while(!stopping){uint64_t now=mono_ms(),elapsed=now-c.begun;
  if(now>=next_scan){next_scan=now+SCAN_INTERVAL_MS;
   if(!c.admitted&&!c.export_invalidated){Destination next={.root_fd=-1,.marker_fd=-1};int admission=admit_destination(&next);if(admission==ADMISSION_READY){c.dest=next;c.admitted=true;if(!clear_previous_report(&c.dest)){c.export_valid=false;c.export_invalidated=true;}mark_dirty(&dirty,&dirty_since,now);}else{if((admission==ADMISSION_NONE||admission==ADMISSION_FAILED)&&!c.ever_absent){c.ever_absent=true;mark_dirty(&dirty,&dirty_since,now);}if(admission==ADMISSION_AMBIGUOUS&&!c.ever_ambiguous){c.ever_ambiguous=true;mark_dirty(&dirty,&dirty_since,now);}if(admission==ADMISSION_FAILED){c.export_valid=false;c.export_invalidated=true;mark_dirty(&dirty,&dirty_since,now);}}}
   else if(c.admitted&&c.export_valid&&!destination_current(&c.dest)){c.export_valid=false;c.export_invalidated=true;mark_dirty(&dirty,&dirty_since,now);}
   bool input_changed=false;c.input_count=discover_inputs(c.inputs,c.input_count,&input_changed);if(input_changed)mark_dirty(&dirty,&dirty_since,now);
   if(refresh_epoch(&c,elapsed,now))mark_dirty(&dirty,&dirty_since,now);
   char facts[512];fast_facts(facts);if(strcmp(facts,c.fast_facts)){snprintf(c.fast_facts,sizeof(c.fast_facts),"%s",facts);mark_dirty(&dirty,&dirty_since,now);}
   bool supported=current_observer_supported(&c);if(supported!=status_supported){status_supported=supported;c.next_status_refresh=now;}
  }
  if(verification_step(c.verifications,now,&c.next_hash_start,&c.hashes_started)){for(int i=0;i<c.epoch_count;i++)if(sync_epoch_verification(&c.epochs[i],c.verifications))mark_dirty(&dirty,&dirty_since,now);}
  if(now>=c.next_status_refresh){if(status_refresh(&c.status_cache,status_supported,elapsed))mark_dirty(&dirty,&dirty_since,now);c.next_status_refresh=mono_ms()+CACHE_INTERVAL_MS;}
  else if(now>=c.next_drm_refresh){if(drm_refresh(&c.drm_cache,elapsed))mark_dirty(&dirty,&dirty_since,now);c.next_drm_refresh=mono_ms()+CACHE_INTERVAL_MS;}
  struct pollfd pf[MAX_INPUTS];for(int i=0;i<c.input_count;i++)pf[i]=(struct pollfd){c.inputs[i].fd,POLLIN,0};int ready=poll(pf,(nfds_t)c.input_count,POLL_INTERVAL_MS);now=mono_ms();if(ready>0)for(int i=0;i<c.input_count;i++){if(pf[i].revents&POLLIN&&drain_input(&c.inputs[i],now-c.begun))mark_dirty(&dirty,&dirty_since,now);if(c.inputs[i].fd>=0&&(pf[i].revents&(POLLERR|POLLHUP|POLLNVAL))){close_input(&c.inputs[i]);mark_dirty(&dirty,&dirty_since,now);}}
  bool write_due=dirty&&(!last_snapshot||now-last_snapshot>=USB_WRITE_INTERVAL_MS);if(write_due){sat_u64(&c.snapshot_sequence);Report report;build_report(&c,&report,now);bool internal=internal_write(report.data,report.used);bool external=true;if(c.admitted&&c.export_valid){if(last_external&&now-last_external<USB_WRITE_INTERVAL_MS)external=false;else if(atomic_external(&c.dest,report.data,report.used)){last_external=now;sat_u64(&c.usb_replacements);}else{c.export_valid=false;c.export_invalidated=true;external=false;}}
   if(internal&&external){dirty=false;last_snapshot=now;}else if(internal&&!c.export_valid){last_snapshot=now;dirty=false;mark_dirty(&dirty,&dirty_since,now);}else if(!internal)mark_dirty(&dirty,&dirty_since,dirty_since);}
 }
 for(int i=0;i<c.input_count;i++)close_input(&c.inputs[i]);
 for(int i=0;i<MAX_EXE_CACHE;i++)if(c.verifications[i].used&&c.verifications[i].fd>=0)close(c.verifications[i].fd);
 if(c.admitted)close_destination(&c.dest);
 return 0;
}
