#define _GNU_SOURCE
/* Original isolated wrong-executable preload rejection check. MIT. */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "config.h"
#define main command_client_entry
#include "command-client.c"
#undef main
#include <assert.h>
int main(void){
 alarm(5);
 if(getenv("LD_PRELOAD")||access(OBSERVER_LOG,F_OK)==0)return 1;
 puts("PASS exact own preload consumed; wrong executable left inert without output");
 char cp[]="/tmp/mpclearn-init-command.XXXXXX",mp[]="/tmp/mpclearn-init-mirror.XXXXXX";int cf=mkstemp(cp),mf=mkstemp(mp);assert(cf>=0&&mf>=0);
 assert(session(cp,mp,0)==5);assert(!ftruncate(cf,sizeof(CommandState))&&!ftruncate(mf,sizeof(MirrorState)));assert(session(cp,mp,0)==5);
 uint32_t wrong=COMMAND_VERSION-1;assert(pwrite(cf,&wrong,4,4)==4);assert(session(cp,mp,0)==1);wrong=0;assert(pwrite(cf,&wrong,4,4)==4);
 uint32_t error=C_TRACE_LOSS;assert(pwrite(cf,&error,4,offsetof(CommandState,error))==4);assert(session(cp,mp,0)==1);
 assert(!ftruncate(mf,0));assert(session(cp,mp,0)==1); /* zero peer must not mask terminal source */
 error=0;assert(pwrite(cf,&error,4,offsetof(CommandState,error))==4);assert(!ftruncate(mf,sizeof(MirrorState)));
 assert(!ftruncate(cf,1));assert(session(cp,mp,0)==1);close(cf);close(mf);assert(!unlink(cp)&&!unlink(mp));
 puts("PASS actual status consumer recognizes typed allocation/header initialization; wrong version, malformed size and terminal source are never treated as initialization (no MPC process or wrapper launch)");
 MirrorState *m=calloc(1,sizeof(*m));assert(m);struct timespec clock;assert(!clock_gettime(CLOCK_MONOTONIC,&clock));m->origin_sec=clock.tv_sec-2;m->origin_nsec=clock.tv_nsec;atomic_store(&m->alive,1);atomic_store(&m->heartbeat,command_now(m->origin_sec,m->origin_nsec));atomic_store(&m->revision,1);
 MIRROR_COPY(snapshot);uint32_t sampled;const char *reason="none";assert(settlement_snapshot(m,&snapshot,&sampled,&reason)==0);
 atomic_store(&m->revision,2);assert(settlement_snapshot(m,&snapshot,&sampled,&reason)==1&&sampled>=snapshot.heartbeat);
 atomic_store(&m->heartbeat,UINT32_MAX);assert(settlement_snapshot(m,&snapshot,&sampled,&reason)==-1&&!strcmp(reason,"source stopped, failed or stale"));
 atomic_store(&m->heartbeat,command_now(m->origin_sec,m->origin_nsec));atomic_store(&m->error,3);assert(settlement_snapshot(m,&snapshot,&sampled,&reason)==-1);free(m);
 puts("PASS CLI settlement retries bounded concurrent snapshot publication; clock is sampled after reads and future/error source remains rejected (observer-owned synthetic state)");
 return 0;
}
