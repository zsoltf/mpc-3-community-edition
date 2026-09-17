/* Duplicate Sequence in one press: GLOBAL_SEQ_DUPLICATE.
 *
 * The application has no one-press duplicate. Its pencil menu opens a
 * From/To/Name popup (popup id 0x3099, dialog ctor 0x02e71830) whose Confirm
 * button hands one properties block to
 * SeqEditProperties::CopySequence::Confirm, 0x01951810. That function is the
 * whole operation: it builds a CopySequenceCommand (vtable 0x068eeb4c, base
 * UndoableCommand with undo name id 58, "Copy Sequence") and submits it through
 * CommandManager::perform on *(Project+0xab0), which executes it synchronously
 * on the submitting thread and keeps it for Undo and Redo.
 *
 * This lane calls that same Confirm with a properties block it fills itself,
 * so the duplicate is the application's own command, undoable the same way,
 * and the copy becomes current the same way. No hook site is added:
 * 0x01951810 is called, exactly as 0x02c70f20 is called by the page and save
 * lanes, so PATCH_COUNT is unchanged.
 *
 * Confirm reads SIX words of the block and nothing else. Verified by
 * disassembling the whole range [0x01951810,0x01951bb8): the only
 * block-relative accesses in it are
 *
 *   0x0195183c  ldr r2,[r0,#0x38]   destination slot index
 *   0x01951850  ldr r3,[r0,#0x24]   source slot index
 *   0x01951864  ldr r2,[r4,#0x54]   the navigator handle
 *   0x01951868  ldr r3,[r4,#4]      the Project
 *   0x0195186c  ldr fp,[r4,#0x50]   the seventh constructor argument
 *   0x01951894  add r1,r4,#0x4c     the name String
 *
 * and it begins with `cmp r2,r3; beq` - a source equal to the destination is
 * the application's own no-op. The property objects at +0x14, +0x28 and +0x3c
 * that the dialog keeps around those values are never touched here, so the
 * block this lane builds is a plain aggregate, not a live property object.
 *
 * The command constructor 0x013fabdc is
 * (this, Project*, navigator, srcIndex, dstIndex, const String* name, dead):
 * it stores the navigator at +0x30 (0x013fac84 str r8,[r4,#0x30]), the source
 * at +0x34, the destination at +0x38, copy-constructs the name at +0x3c
 * (0x00925c88) and takes the undo snapshot of the destination at +0x40 from
 * GetSequence(project,dstIndex). The seventh word is dead: the constructor
 * loads only [sp,#48] and [sp,#52], never [sp,#56]. Its vptr is formed
 * pc-relative (0x013fac88 add r3,pc,r3 then add r3,r3,#676 = 0x068eeb4c), and
 * that vtable still carries execute at +0x20 (0x013fb584) and undo at +0x24
 * (0x013f8b8c); both are checked at recipe time in model-prepare.py, because
 * those words are relocated and carry the load bias at runtime.
 *
 * THE NAVIGATOR IS MANDATORY. Execute's tail makes the copy current with
 * 0x01bba0e0(cmd+0x30, dest+0x250), and that step is NOT gated on the name:
 * both the empty-name and the named path fall into the same
 * `cmp CurrentSequenceIndex(project), dest+0x250` compare at 0x013fb9f0, the
 * name is applied to a different field (dest+0x44), and the sequence copy
 * 0x025c3990 never writes +0x250. A duplicate into a free slot therefore
 * always reaches that call, and 0x01bba0e0 dereferences its first argument
 * immediately (`add r6,r0,#12; ldrex r3,[r6]`) with no null check. Undo
 * mirrors it at 0x013f8e38 when the +0x654 flag is set. A block with a null
 * navigator would fault, so this lane refuses instead.
 *
 * The handle is reachable from the editor pointer this lane is already given:
 *
 *   nav = *(*(*(editor + 0x37c) + 0x2e4) + 0x3d8)
 *
 * That is the literal chain the application's own generic action dispatcher
 * 0x02c70f20 uses in nine of its handlers, for example at 0x02c71b64
 * (`ldr r3,[r4,#0x37c]; ldr r3,[r3,#0x2e4]; ldr r0,[r3,#0x3d8]`). The first hop
 * is the editor root this lane already pins. The handle is allocated in its
 * owner's constructor (`mov r0,#72; bl 0x01bba540; str r4,[r5,#0x3d8]` at
 * 0x01374b48..0x01374b64) and that is the only store to the member anywhere in
 * the owner's own range. SCOPE OF THAT SEARCH: the executable segment holds 249
 * unconditional `str rX,[rY,#0x3d8]` encodings, one of them this one; the other
 * 248 belong to unrelated classes that happen to use the same offset, and no
 * whole-image proof says none of them can ever name this object. "Allocated
 * once and never reassigned" is therefore a claim about the owner, not about
 * the image. The lane does not depend on the stronger claim: it re-reads the
 * pointer from the live chain on every request and pins the handle's own vptr,
 * so a reassigned member would be seen, and the raw borrowed pointer the
 * command stores (no refcount) is the same one the application's own dispatcher
 * borrows. A freed-but-not-nulled handle would still pass `typed` - that is a
 * residual, shared with every other borrowed native pointer this observer uses.
 * Its vptr, formed pc-relative in 0x01bba540, is 0x0689fdac; the lane pins it
 * with `typed`, which adds the load bias, and every hop is null-checked.
 *
 * SOURCE AND DESTINATION. The source is the current sequence's own slot index,
 * Sequence+0x250. That reading was inferred, so this lane proves it at request
 * time instead of trusting it: it requires
 * Project->a70[current->0x250] == Project->e4. A concrete index, not the -1
 * "current" convention GetSequence also accepts, because Redo runs execute
 * again later and must copy the same source. The destination is
 * FirstUnusedSequence(&out,project) (0x0255c32c), the application's own
 * first-free-slot rule, whose out is {int index; bool found}; not found is the
 * refusal the application shows as "All Sequences are used!" and is reported
 * as C_NO_DESTINATION. The name is DefaultSequenceName(&name,dstIndex)
 * (0x025b9638), "Sequence %02d" with index+1, the same name the application
 * gives a sequence in that slot. Passing the real name means the rename is
 * inside the undoable command (execute applies it with
 * 0xe5fbe4(dest+0x44,&name)) rather than a separate edit outside it.
 *
 * THE DESTINATION GETS THE SAME ROUND TRIP AS THE SOURCE. Execute does not
 * navigate or record undo with the index the command carries at +0x38; it
 * re-reads the destination SEQUENCE OBJECT's own slot word:
 *
 *   0x013fb9f0  ldr r4,[r8,#0x250]    r8 = GetSequence(project, cmd+0x38)
 *   0x013fba10  str r0,[r6,#0x650]    the index undo navigates back to
 *   0x013fba1c  ldr r1,[r8,#0x250]    the index the navigation is given
 *
 * The copy itself is driven by the array index, so a destination whose +0x250
 * disagreed with its array position would still be filled correctly, but the
 * UI would jump to a different sequence and Undo would navigate back to a stale
 * one. So the lane requires slots[destination]+0x250 == destination, the mirror
 * of the source check above, and refuses otherwise.
 *
 * The copy becoming current is asynchronous: 0x01bba0e0 posts the navigation
 * through the handle's dispatcher and it lands on the message thread after
 * Confirm returns. This lane's completion is therefore the submission and the
 * synchronous execute, not the navigation, and CE_GLOBAL_END says exactly that.
 *
 * WHAT THE SUBMISSION REACHES, AND WHAT NO CHECK HERE COVERS.
 *
 * 1. Reentrancy. Confirm runs inside service() at the COMMAND_DRAIN site, so
 *    the whole submission - constructor, perform, execute, the sequence copy
 *    0x025c3990, the name write and the posted navigation - runs while
 *    command_in_hook is 1. Any hook site that reaches the gate in
 *    command-capture.c during that window is scored command_violation|C_REENTRY
 *    and clears observer_running, which kills the command lane for the life of
 *    the process. Direct-call reachability from the submission entries to depth
 *    3 reaches M_NAME, M_MUTE, M_FLOAT, MIRROR_PROPERTY_DESTROY, the IO commit
 *    pair, MI_SET and JG_PUBLISH, and every one of those returns earlier, in
 *    the channel_source branch or on its own io/midi/jog submission flag, all
 *    of which are clear for this lane. That scan does NOT follow `blx reg`, and
 *    a whole-sequence copy certainly makes virtual calls: a collection or
 *    replace/destroy site firing from deeper inside 0x025c3990 would hit the
 *    gate. No check here can settle that. The device settles it: after the
 *    first press, `command-client status` must show error=0, trace_error=0 and
 *    no violation, and a later unrelated command must still be accepted.
 * 2. A post-call refusal does not mean nothing happened. The submission is
 *    synchronous and irreversible by the time general-capture.inc re-checks
 *    identity after it; native_started is set, so a C_SOURCE_CHANGED there
 *    reports a refusal for an operation the application already performed.
 *    That mechanism is shared with every global lane, but this is the first one
 *    whose native call mutates the project, so read a device refusal as "check
 *    whether a sequence was created", not as "nothing happened".
 * 3. Neither exception behaviour (this lane has no handler, like every other
 *    native call the observer makes) nor the cost of a synchronous whole-
 *    sequence copy on the UI drain in a loaded project is covered anywhere.
 */
#define SEQ_DUP_CONFIRM_RVA 0x1951810u
#define SEQ_DUP_CONFIRM_LENGTH 0x3a8u
/* One folded .ARM.exidx range holds GetSequence 0x0255c304, FirstUnusedSequence
 * 0x0255c32c, CountUsedSequences 0x0255c378 and CurrentSequenceIndex
 * 0x0255c3b0. The lane calls the second; the others are provenance. */
#define SEQ_DUP_ACCESSORS_RVA 0x255c304u
#define SEQ_DUP_ACCESSORS_LENGTH 0xbcu
#define SEQ_DUP_FIRST_UNUSED_RVA 0x255c32cu
#define SEQ_DUP_DEFAULT_NAME_RVA 0x25b9638u
#define SEQ_DUP_DEFAULT_NAME_LENGTH 0xf0u
/* The String destructor the creation site itself uses on its temporary
 * (0x019518dc), inside its folded range. */
#define SEQ_DUP_STRING_FREE_RVA 0x911f58u
#define SEQ_DUP_STRING_GROUP_RVA 0x911e40u
#define SEQ_DUP_STRING_GROUP_LENGTH 0x270u
#define SEQ_DUP_NAV_VTABLE_RVA 0x689fdacu
_Static_assert(SEQ_DUP_CONFIRM_RVA+SEQ_DUP_CONFIRM_LENGTH<=0x6875d04u,"CopySequence confirm inside the executable segment");
_Static_assert(SEQ_DUP_ACCESSORS_RVA+SEQ_DUP_ACCESSORS_LENGTH<=0x6875d04u,"sequence accessors inside the executable segment");
_Static_assert(SEQ_DUP_DEFAULT_NAME_RVA+SEQ_DUP_DEFAULT_NAME_LENGTH<=0x6875d04u,"default sequence name inside the executable segment");
_Static_assert(SEQ_DUP_STRING_GROUP_RVA+SEQ_DUP_STRING_GROUP_LENGTH<=0x6875d04u,"String destructor inside the executable segment");
/* The six words Confirm reads, and nothing else; the block is zeroed first, so
 * the seventh constructor argument at +0x50 goes in as 0. */
#define SEQ_DUP_BLOCK_BYTES 0x58u
#define SEQ_DUP_BLOCK_PROJECT 0x04u
#define SEQ_DUP_BLOCK_FROM 0x24u
#define SEQ_DUP_BLOCK_TO 0x38u
#define SEQ_DUP_BLOCK_NAME 0x4cu
#define SEQ_DUP_BLOCK_SPARE 0x50u
#define SEQ_DUP_BLOCK_NAV 0x54u
/* The navigator chain, and the Project members the plan reads. */
#define SEQ_DUP_EDITOR_ROOT 0x37cu
#define SEQ_DUP_ROOT_AUDIO 0x2e4u
#define SEQ_DUP_AUDIO_NAVIGATOR 0x3d8u
#define SEQ_DUP_PROJECT_SEQUENCES 0xa70u
#define SEQ_DUP_PROJECT_COUNT 0xa78u
#define SEQ_DUP_PROJECT_CURRENT 0xe4u
#define SEQ_DUP_PROJECT_MANAGER 0xab0u
#define SEQ_DUP_SEQUENCE_SLOT 0x250u
#define SEQ_DUP_SEQUENCE_USED 0x258u
/* This lane's own sanity cap on the project's sequence count; a count past it
 * is a failed pin, not a larger project. The application has no such bound:
 * 0x0255c32c walks Project+0xa78 whatever it holds (cmp r12,#0; ble; then a
 * plain index loop to r12), so this number is the observer's, not the app's. */
#define SEQ_DUP_SLOT_LIMIT 1024u
typedef struct {uint32_t project,navigator,manager,source,destination;} SequenceDuplicatePlan;
#ifdef COMMAND_COMPONENT
/* The three native calls and the String release, substituted one for one so the
 * regression reads the real block, its six words and the call order rather than
 * a restatement of them. */
static void (*component_sequence_first_unused)(uint32_t out,uint32_t project);
static void (*component_sequence_default_name)(uint32_t name,int index);
static void (*component_sequence_confirm)(uint32_t block);
static void (*component_sequence_string_free)(uint32_t name);
#endif
static unsigned char sequence_duplicate_byte(uint32_t a){return *(const volatile unsigned char*)(uintptr_t)a;}
static void sequence_duplicate_store(uint32_t a,uint32_t v){*(volatile uint32_t*)(uintptr_t)a=v;}
/* Every hop null-checked and the handle's own vptr pinned. Returns 0 when any
 * step fails, and the operation is then refused rather than submitted with a
 * navigator the application would dereference without a guard. */
static uint32_t sequence_duplicate_navigator(uint32_t editor,uint32_t editor_root){
 uint32_t owner=word(editor+SEQ_DUP_EDITOR_ROOT);
 if(!pointer(owner)||owner!=editor_root||owner>UINT32_MAX-(SEQ_DUP_ROOT_AUDIO+4u))return 0;
 uint32_t audio=word(owner+SEQ_DUP_ROOT_AUDIO);
 if(!pointer(audio)||audio>UINT32_MAX-(SEQ_DUP_AUDIO_NAVIGATOR+4u))return 0;
 uint32_t navigator=word(audio+SEQ_DUP_AUDIO_NAVIGATOR);
 if(!typed(navigator,SEQ_DUP_NAV_VTABLE_RVA)||navigator>UINT32_MAX-0x48u)return 0;
 return navigator;
}
static void sequence_duplicate_first_unused(uint32_t out,uint32_t project){
#ifdef COMMAND_COMPONENT
 component_sequence_first_unused(out,project);
#else
 ((void (*)(void*,void*))(uintptr_t)(observer_image_bias+SEQ_DUP_FIRST_UNUSED_RVA))((void*)(uintptr_t)out,(void*)(uintptr_t)project);
#endif
}
static void sequence_duplicate_default_name(uint32_t name,int index){
#ifdef COMMAND_COMPONENT
 component_sequence_default_name(name,index);
#else
 ((void (*)(void*,int))(uintptr_t)(observer_image_bias+SEQ_DUP_DEFAULT_NAME_RVA))((void*)(uintptr_t)name,index);
#endif
}
static void sequence_duplicate_confirm(uint32_t block){
#ifdef COMMAND_COMPONENT
 component_sequence_confirm(block);
#else
 ((void (*)(void*))(uintptr_t)(observer_image_bias+SEQ_DUP_CONFIRM_RVA))((void*)(uintptr_t)block);
#endif
}
static void sequence_duplicate_string_free(uint32_t name){
#ifdef COMMAND_COMPONENT
 component_sequence_string_free(name);
#else
 ((void (*)(void*))(uintptr_t)(observer_image_bias+SEQ_DUP_STRING_FREE_RVA))((void*)(uintptr_t)name);
#endif
}
/* Resolve the whole operation before anything is built or called: the Project
 * and its sequence array, the current sequence and its slot index proved by a
 * round trip through the application's own array, the navigator handle, the
 * CommandManager the submission needs, and the first unused slot. Returns 0
 * with the plan filled, or the refusal code. Nothing is written and nothing is
 * called except the application's own first-unused-slot rule. */
static unsigned sequence_duplicate_plan(uint32_t editor,SequenceDuplicatePlan *plan){
 memset(plan,0,sizeof(*plan));
 uint32_t here=atomic_load_explicit(&project,memory_order_acquire),editor_root=atomic_load_explicit(&root,memory_order_acquire);
 if(!pointer(here)||here>UINT32_MAX-(SEQ_DUP_PROJECT_MANAGER+4u)||!typed(here,0x6933c4c))return C_NATIVE_MEMBERSHIP;
 if(!pointer(editor_root)||editor_root<0xd0||word(editor_root+0x24c)!=here)return C_NATIVE_MEMBERSHIP;
 uint32_t navigator=sequence_duplicate_navigator(editor,editor_root);
 if(!navigator)return C_NATIVE_MEMBERSHIP;
 uint32_t manager=word(here+SEQ_DUP_PROJECT_MANAGER);
 if(!pointer(manager))return C_NATIVE_MEMBERSHIP;
 uint32_t count=word(here+SEQ_DUP_PROJECT_COUNT),slots=word(here+SEQ_DUP_PROJECT_SEQUENCES);
 if(!count||count>SEQ_DUP_SLOT_LIMIT||!pointer(slots)||slots>UINT32_MAX-4u*count)return C_NATIVE_MEMBERSHIP;
 uint32_t current=word(here+SEQ_DUP_PROJECT_CURRENT);
 if(!pointer(current)||current>UINT32_MAX-(SEQ_DUP_SEQUENCE_USED+4u))return C_NATIVE_MEMBERSHIP;
 uint32_t source=word(current+SEQ_DUP_SEQUENCE_SLOT);
 /* The round trip that turns the inferred meaning of Sequence+0x250 into a
  * fact for this project: the slot it names must hold this very sequence, and
  * that sequence must be in use. */
 if(source>=count||word(slots+4u*source)!=current||!sequence_duplicate_byte(current+SEQ_DUP_SEQUENCE_USED))return C_NATIVE_MEMBERSHIP;
 struct {uint32_t index;unsigned char found,pad[3];} first;
 memset(&first,0,sizeof(first));
 sequence_duplicate_first_unused((uint32_t)(uintptr_t)&first,here);
 if(!first.found)return C_NO_DESTINATION;
 uint32_t destination=first.index;
 if(destination>=count||destination==source)return C_NATIVE_MEMBERSHIP;
 uint32_t target=word(slots+4u*destination);
 if(!pointer(target)||target>UINT32_MAX-(SEQ_DUP_SEQUENCE_USED+4u)||sequence_duplicate_byte(target+SEQ_DUP_SEQUENCE_USED))return C_NATIVE_MEMBERSHIP;
 /* The same round trip on the destination. Execute navigates and records undo
  * with the destination object's own +0x250 (0x013fb9f0, 0x013fba1c), not with
  * the index the command carries, so a slot word that does not name its own
  * array position would leave the copy right and the navigation wrong. */
 if(word(target+SEQ_DUP_SEQUENCE_SLOT)!=destination)return C_NATIVE_MEMBERSHIP;
 plan->project=here;plan->navigator=navigator;plan->manager=manager;plan->source=source;plan->destination=destination;
 return C_OK;
}
/* The block, the name and the one call. The block is this lane's own zeroed
 * aggregate: Confirm reads six words out of it and writes none, and the
 * command copy-constructs the name, so nothing of it outlives the call. The
 * name String is released afterwards exactly as the creation site releases its
 * own temporary. */
static void sequence_duplicate_submit(const SequenceDuplicatePlan *plan){
 union {uint64_t align;unsigned char bytes[SEQ_DUP_BLOCK_BYTES];} block;
 memset(&block,0,sizeof(block));
 uint32_t at=(uint32_t)(uintptr_t)block.bytes;
 sequence_duplicate_store(at+SEQ_DUP_BLOCK_PROJECT,plan->project);
 sequence_duplicate_store(at+SEQ_DUP_BLOCK_FROM,plan->source);
 sequence_duplicate_store(at+SEQ_DUP_BLOCK_TO,plan->destination);
 sequence_duplicate_store(at+SEQ_DUP_BLOCK_SPARE,0);
 sequence_duplicate_store(at+SEQ_DUP_BLOCK_NAV,plan->navigator);
 sequence_duplicate_default_name(at+SEQ_DUP_BLOCK_NAME,(int)plan->destination);
 sequence_duplicate_confirm(at);
 sequence_duplicate_string_free(at+SEQ_DUP_BLOCK_NAME);
}
