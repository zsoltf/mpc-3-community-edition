static struct timespec probe_test_time={100,123456789};
static int probe_test_clock_error;
static int probe_test_clock(struct timespec *t){*t=probe_test_time;return probe_test_clock_error;}
__attribute__((naked)) static void probe_test_callee(void){
 __asm__ volatile("push {r0,r1}\n movw r0,#:lower16:dispatch_count\n movt r0,#:upper16:dispatch_count\n ldr r1,[r0]\n add r1,r1,#1\n str r1,[r0]\n pop {r0,r1}\n bx lr");
}
static void native_wheel_probe_checks(void){
 component_probe_clock=probe_test_clock;
 command_state=calloc(1,sizeof(*command_state));mirror_state=calloc(1,sizeof(*mirror_state));
 require(command_state&&mirror_state,"probe component shared-state substitutes");
 command_owner=token()+1;observer_image_bias=0;observer_running=1;
 native_wheel_probe_initialize();
 /* Native BLX continuation retained. Only the callee and following return are
  * substituted; original and patched executions receive identical fixtures. */
 unsigned site=0;while(site<PATCH_COUNT&&anchor[site]!=0x331ea38)site++;
 require(site<PATCH_COUNT&&capture_after_pair[site],"exact counted panel site after its pair");
 static uint32_t table[9],controller[60];table[8]=ptr(probe_test_callee);
 Context in=context(),before,after;in.r[3]=ptr(table);in.r[5]=ptr(controller);in.r[1]=(uint32_t)-3;in.r[2]=1;
 uint32_t *entry=(uint32_t*)(uintptr_t)anchor[site];memcpy(entry,expected[site],8);entry[2]=0xe12fff33;
 uint32_t *tail=entry+3;jump(&tail,ptr(capture_a));__builtin___clear_cache((char*)entry,(char*)tail);
 dispatch_count=0;exercise(&in,entry,&before);require(dispatch_count==1,"original native BLX continuation once");
 unsigned char *stub=mmap(NULL,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 require(stub!=MAP_FAILED,"probe gate fixture");size_t bytes=build_stub(stub,site,0);__builtin___clear_cache((char*)stub,(char*)stub+bytes);
 uint32_t *patched=entry;jump(&patched,ptr(stub));__builtin___clear_cache((char*)entry,(char*)entry+8);
 dispatch_count=0;errno=E2BIG;exercise(&in,entry,&after);
 require(dispatch_count==1&&!memcmp(&before,&after,sizeof(before))&&errno==E2BIG,"passive actual gate preserves GPR/APSR/FPSCR/d0-d31, errno and exactly one native BLX continuation");
 require(atomic_load(&native_wheel_probe.published)==1&&native_wheel_probe.events[0].r[0]==ptr(controller)&&native_wheel_probe.events[0].r[1]==(uint32_t)-3&&native_wheel_probe.events[0].r[2]==1&&native_wheel_probe.events[0].r[3]==ptr(probe_test_callee),"probe records after-pair arguments without callee substitution");
 munmap(stub,4096);
 /* A foreign thread's register can be aligned yet unreadable. */
 void *unreadable=mmap(NULL,4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);require(unreadable!=MAP_FAILED,"foreign unreadable fixture");
 Context c=context(),unchanged;c.r[0]=ptr(unreadable);c.r[1]=127;c.r[2]=1;c.r[3]=WHEEL_ROTATE_RVA;unchanged=c;
 native_wheel_probe_initialize();native_wheel_probe_capture(&c);
 require(atomic_load(&native_wheel_probe.published)==1&&native_wheel_probe.events[0].flags==NWP_CALLEE&&!memcmp(&c,&unchanged,sizeof(c)),"foreign call recorded without UI dereference or Context mutation");
 NativeWheelProbeEvent first=native_wheel_probe.events[0];
 for(unsigned i=1;i<140;i++)native_wheel_probe_capture(&c);
 require(atomic_load(&native_wheel_probe.published)==128&&atomic_load(&native_wheel_probe.closed)==NWP_CAP&&!memcmp(&first,&native_wheel_probe.events[0],sizeof(first)),"128 immutable records, no wrap or overflow");
 native_wheel_probe_initialize();atomic_store(&native_wheel_probe_guard.busy,1);native_wheel_probe_capture(&c);atomic_store(&native_wheel_probe_guard.busy,0);
 require(atomic_load(&native_wheel_probe.busy_loss)&&!atomic_load(&native_wheel_probe.published),"busy admission reports loss without waiting");
 claim_test_guard=&native_wheel_probe_guard.busy;claim_test_losses=4;native_wheel_probe_capture(&c);claim_test_guard=NULL;claim_test_losses=0;
 require(atomic_load(&native_wheel_probe.reservation_loss)&&!atomic_load(&native_wheel_probe.published)&&!atomic_load(&command_state->error),"four forced native STREX losses remain diagnostic-only");
 native_wheel_probe_capture(&c);require(atomic_load(&native_wheel_probe.published)==1,"loss does not disable runtime or later probe capture");
 native_wheel_probe_initialize();probe_test_time.tv_sec+=180;native_wheel_probe_capture(&c);
 require(!atomic_load(&native_wheel_probe.published)&&atomic_load(&native_wheel_probe.closed)==NWP_EXPIRED,"exact monotonic deadline excludes expired event");
 native_wheel_probe_initialize();probe_test_clock_error=1;native_wheel_probe_capture(&c);probe_test_clock_error=0;
 require(atomic_load(&native_wheel_probe.closed)==NWP_CLOCK&&!atomic_load(&native_wheel_probe.published),"clock failure is explicit diagnostic closure");
 uint32_t slot_page=FOCUS_SLOT_RVA&~4095u;
 void *mapped=mmap((void*)(uintptr_t)slot_page,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 require(mapped!=MAP_FAILED,"native focus slot fixture");
 static uint32_t grid[28];controller[0]=wheel_focus_vtables[0];controller[WHEEL_FOCUSED_OFFSET/4]=ptr(grid);grid[0]=NOTE_GRID_VTABLE;grid[WHEEL_FLAGS_OFFSET/4]=2;put(FOCUS_SLOT_RVA,ptr(controller));
 c.r[0]=ptr(controller);command_owner=token();native_wheel_probe_initialize();native_wheel_probe_capture(&c);
 require(native_wheel_probe.events[0].flags==63&&native_wheel_probe.events[0].focused==ptr(grid),"owner-thread typed visible Grid qualification");
 require(!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error),"probe does not change command health");
 munmap(mapped,4096);munmap(unreadable,4096);free(command_state);free(mirror_state);command_state=NULL;mirror_state=NULL;
 puts("PASS passive wheel: actual ARM gate and BLX continuation; 128-record cap, expiry, explicit busy/reservation loss, foreign unreadable pointer, owner focus, unchanged Context/errno/runtime health (clock/callee/focus substituted)");
}
