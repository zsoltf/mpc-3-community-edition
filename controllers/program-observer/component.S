.syntax unified
.arm
.fpu neon
.text
.global exercise,capture_a,capture_b
exercise:
 push {r4-r11,lr}
 vpush {d8-d15}
 sub sp,sp,#68
 ldr r3,=exercise_adjust
 ldr r3,[r3]
 sub sp,sp,r3
#if defined(MODEL_CAPTURE) || defined(VOLUME_MIRROR)
 /* Deterministic wrapper scratch lets epilogue replay compare every register. */
 mov r3,sp
 mov ip,#128
9:
 str ip,[r3],#4
 subs ip,ip,#1
 bne 9b
#endif
 ldr r3,=saved_sp
 str sp,[r3]
 ldr r3,=result_ptr
 str r2,[r3]
 ldr r3,=target_word
 str r1,[r3]
 mov r3,r0
 vldmia r3!,{d0-d15}
 vldmia r3!,{d16-d31}
 ldmia r3!,{r0,r1}
 vmsr fpscr,r1
 msr APSR_nzcvqg,r0
 ldmia r3,{r0-r12,lr}
 sub sp,sp,#4
 push {r0}
 ldr r0,=target_word
 ldr r0,[r0]
 str r0,[sp,#4]
 pop {r0,pc}
capture_a:
 push {r0,r1}
 ldr r0,=route
 mov r1,#1
 str r1,[r0]
 pop {r0,r1}
 b capture
capture_b:
 push {r0,r1}
 ldr r0,=route
 mov r1,#2
 str r1,[r0]
 pop {r0,r1}
capture:
 push {r0,r1}
 add r0,sp,#8
 ldr r1,=captured_sp
 str r0,[r1]
 ldr r1,[r0]
 push {r1}
 ldr r1,[r0,#4]
 ldr r0,=captured_pair
 str r1,[r0,#4]
 pop {r1}
 str r1,[r0]
 pop {r0,r1}
 push {r0-r12,lr}
 mrs r0,apsr
 vmrs r1,fpscr
 push {r0,r1}
 vpush {d16-d31}
 vpush {d0-d15}
 mov r1,sp
 ldr r0,=result_ptr
 ldr r0,[r0]
 mov r2,#320
 bl memcpy
 bl component_listener
 ldr r0,=saved_sp
 ldr sp,[r0]
 ldr r1,=exercise_adjust
 ldr r1,[r1]
 add sp,sp,r1
 add sp,sp,#68
 vpop {d8-d15}
 pop {r4-r11,pc}
/* Synthetic existing virtual callback: count once, preserving all context. */
.global component_dispatch
component_dispatch:
 push {r0,r1}
 ldr r0,=dispatch_count
 ldr r1,[r0]
 add r1,r1,#1
 str r1,[r0]
 pop {r0,r1}
 bx lr
.data
.balign 4
target_word: .word 0
.global saved_sp,captured_sp,captured_pair
saved_sp: .word 0
captured_sp: .word 0
captured_pair: .word 0,0
result_ptr: .word 0
.section .note.GNU-stack,"",%progbits
