/* Actual ARM MMV7 consumer/policy/ALSA encoding. No ALSA device is opened. */
#define main bridge_entry_not_called
#include "mirror-motor.c"
#undef main
#include "mirror-poison.h"
static void need(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static void ready_snapshot(CopiedMirror *s,unsigned n,uint32_t epoch){
 memset(s,0,sizeof(*s));s->count=n;s->epoch=epoch;s->ready=s->alive=1;s->heartbeat=100;s->revision=2;
 for(unsigned i=0;i<n;i++)s->tracks[i]=(CopiedTrack){.serial=i+1,.track=0x10000+i*0x1000,.program=0x90000+i*0x1000,.incarnation=i+1,.binding=i+1,.vptr=0x6930c00,.revision=2,.bits=0x3f000000,.seed=1,.available=1};
}
static void release_all(MirrorBank *bank,uint32_t now){for(unsigned i=0;i<8;i++){bank_touch(bank,i,1,now);bank_touch(bank,i,0,now+1);}}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 alarm(15);
 need(relative_time(118490,1800000,118489,985631730)==17,"same-origin signed fractional second boundary");
 need(relative_time(118489,985631729,118489,985631730)==UINT32_MAX,"negative age rejected");
 /* Read the actual mirror component's file through the production copy owner. */
 int fd=open(argv[1],O_RDONLY);struct stat st;need(fd>=0&&!fstat(fd,&st)&&st.st_size==(off_t)sizeof(MirrorState),"real shared fixture file");
 const MirrorState *state=mmap(NULL,sizeof(*state),PROT_READ,MAP_SHARED,fd,0);close(fd);need(state!=MAP_FAILED,"read-only source map");
 MIRROR_COPY(snapshot);need(copy_mirror(state,&snapshot)&&snapshot.count==12&&snapshot.ready,"actual MMV7 copied fixture topology");
 /* Reused snapshot capacity is deliberately unspecified outside the counts.
  * Poison it to expose accidental reliance on the former whole-buffer clear. */
 CopiedMirror *poisoned=calloc(1,sizeof(*poisoned));need(poisoned!=NULL,"snapshot allocation");
 mirror_poison(poisoned,0xa5);need(copy_mirror(state,poisoned),"copy into poisoned capacity");
 need(poisoned->count==snapshot.count&&poisoned->pad_count==snapshot.pad_count,"poisoned metadata reset");
 need(!memcmp(poisoned->tracks,snapshot.tracks,snapshot.count*sizeof(CopiedTrack)),"all active fields initialized");
 need(poisoned->tracks[12].serial==0xa5a5a5a5,"unused capacity is not cleared");
 MirrorState *empty=mmap(NULL,sizeof(*empty),PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);need(empty!=MAP_FAILED,"empty source allocation");
 atomic_store(&empty->revision,2);atomic_store(&empty->ready,1);
 need(copy_mirror(empty,poisoned)&&!poisoned->count&&!poisoned->pad_count&&!poisoned->selected_serial&&!poisoned->selection.available&&!poisoned->position_available&&!poisoned->effects_available&&!poisoned->io_available&&!poisoned->qlinks_available,"shrinking to empty removes prior metadata");
 MirrorBank empty_bank;bank_init(&empty_bank);bank_apply(&empty_bank,poisoned,100);need(!empty_bank.faders[0].identity.serial,"old rows cannot populate an empty bank");
 atomic_store(&empty->revision,3);need(!copy_mirror(empty,poisoned)&&!poisoned->count&&!poisoned->ready,"unstable topology clears metadata and rejects copy");
 munmap(empty,sizeof(*empty));free(poisoned);
 /* Exercise final output validation against a private copy of a real producer
  * file, with only heartbeat/liveness restored for this component check. */
 int live_fd=open(argv[1],O_RDONLY);need(live_fd>=0,"private producer descriptor");
 MirrorState *live=mmap(NULL,sizeof(*live),PROT_READ|PROT_WRITE,MAP_PRIVATE,live_fd,0);close(live_fd);
 need(live!=MAP_FAILED,"private producer state");atomic_store(&live->alive,1);atomic_store(&live->heartbeat,100);
 MIRROR_COPY(current);need(copy_mirror_view(live,&current,0),"scope-aware fresh copy");
 MirrorBank checked;bank_init(&checked);bank_apply(&checked,&current,100);
 need(mirror_motor_current(live,&current,&checked,0,100),"current target passes narrow motor barrier");
 need(checked.faders[0].identity.incarnation&&checked.faders[0].identity.incarnation<=MIRROR_CELLS,"bound volume cell for barrier check");
 MirrorCell *cell=live->cells+checked.faders[0].identity.incarnation-1;unsigned rev=atomic_load(&cell->revision);
 atomic_store(&cell->revision,rev+2);need(!mirror_motor_current(live,&current,&checked,0,100),"changed target deferred without whole snapshot copy");atomic_store(&cell->revision,rev);
 atomic_fetch_add(&live->revision,2);need(!mirror_motor_current(live,&current,&checked,0,100),"topology change blocks old motor target");atomic_fetch_sub(&live->revision,2);
 need(!mirror_motor_current(live,&current,&checked,0,1100),"stale batch cannot drive motor");
 atomic_store(&live->alive,0);need(!mirror_motor_current(live,&current,&checked,0,100),"closed producer blocks motor");munmap(live,sizeof(*live));
 /* Closed source file is read unchanged; policy clock/alive below is synthetic. */
 MirrorBank bank;bank_init(&bank);snapshot.alive=1;snapshot.heartbeat=100;bank_apply(&bank,&snapshot,100);
 for(unsigned i=0;i<8;i++){need(bank.faders[i].identity.serial==i+1,"automatic first eight identities");need(bank_due(&bank,i,100)<0,"automatic initial output waits fresh source barrier");}
 need(bank_due(&bank,0,103)<0,"startup waits for post-binding source heartbeat");snapshot.heartbeat=110;bank_apply(&bank,&snapshot,110);
 Surface surface={.source=3,.full={32,0}};
 for(unsigned i=0;i<8;i++){
  int p=bank_due(&bank,i,110);need(p==8192,"actual fixture value reaches motor policy");snd_seq_event_t event;motor_event(&event,&surface,i,p);
  need(event.type==SND_SEQ_EVENT_PITCHBEND&&event.data.control.channel==i&&event.data.control.value==0&&event.dest.client==32&&event.dest.port==0&&event.source.port==3,"exact eight-channel direct pitchbend encoding");
  bank.faders[i].last_sent=p;bank.faders[i].last_tick=110;need(bank_due(&bank,i,150)<0,"unchanged targets coalesced");
 }
 snapshot.tracks[0].bits=0x3f400000;snapshot.tracks[0].revision=4;snapshot.heartbeat=119;bank_apply(&bank,&snapshot,119);
 need(bank_due(&bank,0,119)<0,"changed actual source waits minimum10ms motor interval");
 need(bank_due(&bank,0,120)==12287,"changed actual source becomes due at10ms without interpolation");
 bank.faders[0].last_sent=12287;bank.faders[0].last_tick=120;
 need(bank_due(&bank,0,130)<0,"10ms output cadence still deduplicates identical source target");
 snapshot.tracks[0].bits=0x3f000000;snapshot.tracks[0].revision=6;
 bank_touch(&bank,3,1,120);snapshot.tracks[3].bits=0x3f400000;snapshot.tracks[3].revision=4;snapshot.heartbeat=160;bank_apply(&bank,&snapshot,160);need(bank_due(&bank,3,160)<0,"held fader never follows source changes");
 bank_touch(&bank,3,0,161);snapshot.heartbeat=170;bank_apply(&bank,&snapshot,170);need(bank_due(&bank,3,170)==12287,"release catches up to latest retained value");
 bank_navigate(&bank,1,180);need(!bank.ready,"bank button discards old targets before new copy");snapshot.heartbeat=190;bank_apply(&bank,&snapshot,190);
 need(bank.offset==8&&bank.faders[0].identity.serial==9&&!bank.faders[4].available,"second bank has exact remaining tracks and empty slots");need(bank_due(&bank,0,190)<0,"new binding waits post-remap heartbeat");snapshot.heartbeat=200;bank_apply(&bank,&snapshot,200);need(bank_due(&bank,0,200)==8192,"known released physical fader can follow new bank after barrier");
 bank_touch(&bank,0,1,205);bank_navigate(&bank,-1,206);snapshot.heartbeat=210;bank_apply(&bank,&snapshot,210);snapshot.heartbeat=220;bank_apply(&bank,&snapshot,220);need(bank_due(&bank,0,220)<0&&bank.faders[0].touch==MT_DOWN,"bank change cannot forget a held physical fader");
 bank_touch(&bank,0,0,221);snapshot.heartbeat=230;bank_apply(&bank,&snapshot,230);need(bank_due(&bank,0,230)==8192,"new-bank release uses new binding");
 snapshot.epoch=2;for(unsigned i=0;i<snapshot.count;i++){snapshot.tracks[i].serial+=100;snapshot.tracks[i].binding+=100;}snapshot.heartbeat=240;bank_apply(&bank,&snapshot,240);
 need(bank.offset==0&&bank.faders[0].identity.epoch==2&&bank.faders[0].identity.serial==101&&bank.faders[0].identity.incarnation==1&&bank_due(&bank,0,240)<0,"reload invalidates Track identity even when Program incarnation is retained");
 snapshot.heartbeat=250;bank_apply(&bank,&snapshot,250);need(bank_due(&bank,0,250)==8192,"reload rebuilds current volume after fresh barrier");
 bank_disconnect(&bank);bank_apply(&bank,&snapshot,250);snapshot.heartbeat=260;bank_apply(&bank,&snapshot,260);need(bank_due(&bank,0,260)==8192&&bank.faders[0].touch==MT_UNKNOWN&&bank.faders[0].physical==-1,"reconnect automatically follows fresh state without inventing observed physical position");
 release_all(&bank,270);snapshot.heartbeat=280;bank_apply(&bank,&snapshot,280);need(bank_due(&bank,0,280)==8192,"reconnect recovers copied latest state without a new edit");
 need(bank_due(&bank,0,1280)<0,"one-second stale heartbeat inhibits");snapshot.tracks[0].available=0;bank_apply(&bank,&snapshot,280);need(bank.ready&&bank_due(&bank,0,280)<0,"unknown/closed volume inhibits only its fader");
 snapshot.ready=0;bank_apply(&bank,&snapshot,280);need(!bank.ready&&!bank.faders[0].identity.serial,"loading discards targets");
 /* Independent synthetic topology exercises offsets > one bank. */
 ready_snapshot(&snapshot,24,3);bank_apply(&bank,&snapshot,300);bank_navigate(&bank,1,310);bank_apply(&bank,&snapshot,310);bank_navigate(&bank,1,320);bank_apply(&bank,&snapshot,320);bank_navigate(&bank,1,330);need(bank.offset==16,"bank right clamps to last nonempty bank");
 snapshot.count=3;bank_apply(&bank,&snapshot,340);need(bank.offset==0&&!bank.faders[3].available,"removal clamps bank and clears empty faders");
 snapshot.heartbeat=350;bank_apply(&bank,&snapshot,350);need(bank.faders[3].empty&&bank_due(&bank,3,350)==0,"fresh known absent strip parks at bottom");
 bank.faders[3].last_sent=0;bank.faders[3].last_tick=350;need(bank_due(&bank,3,400)<0,"empty parking coalesces unchanged bottom target");
 bank.faders[3].last_sent=12000;bank_touch(&bank,3,1,400);snapshot.heartbeat=410;bank_apply(&bank,&snapshot,410);need(bank_due(&bank,3,410)<0,"empty parking cannot fight observed held cap");
 bank_touch(&bank,3,0,411);snapshot.heartbeat=420;bank_apply(&bank,&snapshot,420);need(bank_due(&bank,3,420)==0,"released empty strip parks after fresh barrier");
 snapshot.tracks[0].available=0;bank_apply(&bank,&snapshot,420);need(!bank.faders[0].empty&&bank_due(&bank,0,420)<0,"unavailable populated Track is not an empty slot");
 need(bank_due(&bank,3,1420)<0,"stale source never authorizes empty parking");snapshot.ready=0;bank_apply(&bank,&snapshot,430);need(!bank.faders[3].empty&&bank_due(&bank,3,430)<0,"invalid topology removes empty-slot authority");
 snapshot.ready=1;snapshot.count=0;snapshot.heartbeat=440;bank_apply(&bank,&snapshot,440);snapshot.heartbeat=450;bank_apply(&bank,&snapshot,450);for(unsigned i=0;i<8;i++)need(bank.faders[i].empty&&bank_due(&bank,i,450)==0,"fresh valid zero-track bank parks all unheld strips");
 snd_seq_event_t event;unsigned kind=0,channel=0;int value=0;
 for(unsigned i=0;i<8;i++){snd_seq_ev_clear(&event);event.source=surface.full;snd_seq_ev_set_noteon(&event,0,104+i,127);need(surface_event(&event,surface.full,&kind,&channel,&value)==1&&kind==1&&channel==i&&value,"all eight physical touch notes");event.data.note.velocity=0;need(surface_event(&event,surface.full,&kind,&channel,&value)==1&&!value,"zero velocity touch release");}
 for(unsigned i=0;i<2;i++){snd_seq_ev_set_noteon(&event,0,46+i,127);need(surface_event(&event,surface.full,&kind,&channel,&value)==1&&kind==2&&channel==i&&value,"documented MCU bank button mapping");}
 event.data.note.channel=1;need(!surface_event(&event,surface.full,&kind,&channel,&value),"foreign channel ignored");event.data.note.channel=0;event.source.client++;need(!surface_event(&event,surface.full,&kind,&channel,&value),"foreign controller ignored");
 event.source=surface.full;snd_seq_ev_set_pitchbend(&event,0,500);need(!surface_event(&event,surface.full,&kind,&channel,&value),"input fader values never echoed or written to MPC");
 munmap((void*)state,sizeof(*state));
 puts("PASS actual ARM MMV7 file-to-eight-motor policy/encoding, automatic startup/reconnect, observed-touch suppression/coalescing, bank/reload/retained Program/reconnect and freshness; no ALSA device or physical motor used");return 0;
}
