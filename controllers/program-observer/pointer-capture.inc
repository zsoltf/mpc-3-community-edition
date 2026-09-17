/* USB mouse support on MPC 3.9.1 Gen1.
 *
 * The firmware already implements pointer input and a DRM hardware cursor: the
 * JUCE LinuxFbComponentPeer adds libinput devices with CAP_POINTER, dispatches
 * every pointer event type, and draws a cursor plane. All of it is dormant
 * behind one enable byte at cursor-state +0x38, which only the Remote Screen
 * listener sets on this firmware, and that listener also starts a web server.
 * See docs/mpc-app-map.md#mouse-pointer-gate.
 *
 * This hook never writes that byte. It calls the application's own setter
 * (ba6658) so MPC performs its normal cursor setup, exactly as the Remote
 * Screen path would, and nothing here touches the web server.
 *
 * Site: ba6884 is the single join of the peer's device handler (ba6780). Both
 * the DEVICE_ADDED and DEVICE_REMOVED paths reach it after the pointer-device
 * vector has been updated, with the live cursor-state object in r6, and the
 * app itself tail-calls the cursor show/move routine (c48034) from ba68a8 two
 * instructions later. The call therefore inherits the execution context MPC
 * already uses for the same routine, and adds no new thread to it.
 */
#define POINTER_ENABLE_RVA 0xba6658u
#define POINTER_STATE_RVA 0x6b220d4u /* peer static table 6b220a8, cursor slot +0x2c */
/* Exact 3.9.1 program headers: RX segment [0,6875d04), RW segment
 * [6877010,6bd6078). module_ranges carries p_filesz only, so the cursor slot,
 * which lives past the writable segment's file image, is qualified here. */
_Static_assert(POINTER_ENABLE_RVA+8<=0x6875d04u,"enable setter inside the executable segment");
_Static_assert(POINTER_STATE_RVA>=0x6877010u&&POINTER_STATE_RVA+4<=0x6bd6078u,"cursor slot inside the writable segment");
static _Atomic uint32_t pointer_enabled;
#ifdef COMMAND_COMPONENT
static void (*component_pointer_enable)(unsigned);
#endif
/* One call, on the thread that owns the observer's command work, and only while
 * a pointer-capable device is actually present: an empty device vector means no
 * mouse is plugged in, and then nothing happens at all. Unplugging later leaves
 * the pointer enabled; the app's own handlers hide the cursor. */
static void pointer_device_enable(Context *c){
 if(atomic_load_explicit(&pointer_enabled,memory_order_acquire))return;
 if(token()!=command_owner)return;
 uint32_t object=c->r[6];
 if(!pointer(object)||object>UINT32_MAX-0x3c)return;
 /* The peer's own static slot must still name this receiver. */
 if(word(observer_image_bias+POINTER_STATE_RVA)!=object)return;
 uint32_t begin=word(object+0x24),end=word(object+0x28);
 if(!pointer(begin)||!pointer(end)||end<=begin||(end-begin)&3)return;
 if(atomic_exchange_explicit(&pointer_enabled,1,memory_order_acq_rel))return;
#ifdef COMMAND_COMPONENT
 if(component_pointer_enable){component_pointer_enable(1);return;}
#endif
 ((void (*)(unsigned))(uintptr_t)(observer_image_bias+POINTER_ENABLE_RVA))(1);
}
