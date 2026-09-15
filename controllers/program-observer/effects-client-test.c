/* Actual CLI mapping/lease/request path, with process hash, copied producer and
 * clock substituted to advance two seconds during full-hash admission. */
#define _GNU_SOURCE
#define COMMAND_CLIENT
#include "mirror-motor-core.h"
#include "mirror-input-core.h"
#include "command-process.h"
static MirrorState *producer;
static unsigned file_checks,snapshot_checks;static int settling;
static int bounded_file(uint32_t pid,int hash){(void)pid;(void)hash;return ++file_checks<3;}
static uint32_t test_ms=100;static int hash_ok=1;static unsigned hash_calls;
static int delayed_hash(uint32_t pid){(void)pid;hash_calls++;test_ms+=2000;return hash_ok;}
static uint64_t fixed_start(uint32_t pid){(void)pid;return 123;}
static int test_clock(clockid_t clock,struct timespec *t){(void)clock;t->tv_sec=test_ms/1000;t->tv_nsec=(test_ms%1000)*1000000;return 0;}
static int fresh_copy(const MirrorState *m,CopiedMirror *s){
 (void)m;if(settling){snapshot_checks++;atomic_store(&producer->heartbeat,test_ms);return 0;}memset(s,0,sizeof(*s));atomic_store(&producer->heartbeat,test_ms);s->heartbeat=test_ms;s->alive=s->ready=1;s->selection.available=1;s->selected_serial=7;s->count=1;s->tracks[0].serial=7;
 s->effects_available=1;s->effects.status=EF_READY;s->effects.serial=7;s->effects.slot=0;s->effects.slots[0].status=EF_READY;s->effects.slots[0].count=1;s->effects.parameters[0].status=EF_READY;s->effects.parameters[0].tick=test_ms;return 1;
}
#define command_process_file bounded_file
#define command_process_exact delayed_hash
#define command_process_start fixed_start
#define clock_gettime test_clock
#define copy_mirror fresh_copy
#define main effects_client_entry_not_called
#include "command-client.c"
#undef main
#undef command_process_file
#undef command_process_exact
#undef command_process_start
#undef copy_mirror
static void need(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
int main(int argc,char **argv){
 (void)command_process_file;(void)command_process_exact;(void)command_process_start;(void)copy_mirror;
 if(argc!=4)return 2;
 int cfd=open(argv[1],O_CREAT|O_EXCL|O_RDWR,0600),mfd=open(argv[2],O_CREAT|O_EXCL|O_RDWR,0600);need(cfd>=0&&mfd>=0&&!ftruncate(cfd,sizeof(CommandState))&&!ftruncate(mfd,sizeof(MirrorState)),"exclusive fixture files");
 CommandState *c=mmap(NULL,sizeof(*c),PROT_READ|PROT_WRITE,MAP_SHARED,cfd,0);MirrorState *m=mmap(NULL,sizeof(*m),PROT_READ|PROT_WRITE,MAP_SHARED,mfd,0);need(c!=MAP_FAILED&&m!=MAP_FAILED,"fixture maps");
 c->magic=COMMAND_MAGIC;c->version=COMMAND_VERSION;c->bytes=sizeof(*c);c->capacity=COMMAND_SLOTS;c->pid=1;c->start_lo=123;c->owner_token=1;atomic_store(&c->alive,1);
 producer=m;m->magic=MIRROR_MAGIC;m->version=MIRROR_VERSION;m->bytes=sizeof(*m);m->pid=1;atomic_store(&m->alive,1);
 need(!effects_interest_cli(argv[1],argv[2],"list","0","30"),"interest accepts heartbeat advanced during full hash");need(atomic_load(&c->effects_interest.until)==test_ms+30000&&hash_calls==1,"interest lease starts after full hash");
 need(!effects_prepare_cli(argv[2],argv[3],"0","0.5"),"prepare accepts heartbeat advanced during full hash");
 CommandRequest r;int fd=open(argv[3],O_RDONLY);need(fd>=0&&read(fd,&r,sizeof(r))==sizeof(r),"actual request readback");close(fd);need(r.created==test_ms&&r.expires==test_ms+120000&&r.serial==7&&r.reserved==EFFECT_PARAMETER&&hash_calls==2,"request dates follow full admission and actual selected field");unlink(argv[3]);
 hash_ok=0;unsigned rev=atomic_load(&c->effects_interest.revision);need(effects_interest_cli(argv[1],argv[2],"list","0","30")==1&&atomic_load(&c->effects_interest.revision)==rev,"interest still rejects failed full hash without write");need(effects_prepare_cli(argv[2],argv[3],"0","0.5")==1&&access(argv[3],F_OK)<0,"prepare still rejects failed full hash without file");
 hash_ok=1;settling=1;file_checks=snapshot_checks=0;unsigned hashes_before=hash_calls;atomic_store(&c->published,1);atomic_store(&c->slots[0].published,1);
 need(settle(argv[1],argv[2])==1,"settle rejects changed process/file identity during snapshot contention");
 need(hash_calls==hashes_before+1&&file_checks==3&&snapshot_checks==2&&!atomic_load(&c->slots[0].settled)&&!atomic_load(&c->slots[0].reclaimed),"settle hashes once at admission, retains bounded recurring identity checks, never acknowledges on identity loss");
 puts("PASS actual settle loop hashes only at admission; two contended snapshots then file-identity loss retain the pending request (process identity/copy substituted)");
 munmap(c,sizeof(*c));munmap(m,sizeof(*m));close(cfd);close(mfd);unlink(argv[1]);unlink(argv[2]);puts("PASS actual Effects CLI admits fresh post-hash snapshot/clock and rejects hash failure; two-second hash delay, producer snapshot and process identity substituted");return 0;
}
