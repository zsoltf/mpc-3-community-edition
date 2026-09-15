.syntax unified
.arm
.fpu neon
.section .rodata
.balign 4
.global gate_begin,gate_end,gate_handler
.hidden gate_begin,gate_end,gate_handler
gate_begin:
 push {r0-r12,lr}
 mrs r0,apsr
 vmrs r1,fpscr
 push {r0,r1}
 vpush {d16-d31}
 vpush {d0-d15}
 mov r4,sp
 bic sp,sp,#7
 mov r0,r4
 /* r1 is filled by a generated MOV immediately before this template? The
  * hook id instead lives beside the handler literal, avoiding register loss. */
 ldr r1,gate_id
 ldr ip,gate_handler
 blx ip
 mov sp,r4
 vpop {d0-d15}
 vpop {d16-d31}
 pop {r0,r1}
 vmsr fpscr,r1
 msr APSR_nzcvqg,r0
 pop {r0-r12,lr}
 b gate_end
gate_handler: .word 0
gate_id: .word 0
gate_end:
.balign 4
.global fast_begin,fast_end,fast_running,fast_filter,fast_skip,fast_target
.hidden fast_begin,fast_end,fast_running,fast_filter,fast_skip,fast_target
fast_begin:
 push {r0-r3,ip,lr}
 mrs ip,apsr
fast_target:
 mov r3,r5
 ldr r0,fast_running
 ldr r0,[r0]
 dmb ish
 cmp r0,#0
 beq 2f
 ldr r0,fast_filter
 mov r2,#64
1:
 ldr r1,[r0],#4
 dmb ish
 cmp r1,r3
 beq 3f
 subs r2,r2,#1
 bne 1b
2:
 msr APSR_nzcvqg,ip
 pop {r0-r3,ip,lr}
 ldr pc,fast_skip
3:
 msr APSR_nzcvqg,ip
 pop {r0-r3,ip,lr}
 b fast_end
fast_running: .word 0
fast_filter: .word 0
fast_skip: .word 0
fast_end:
.section .note.GNU-stack,"",%progbits
