/* Explicitly bound AAPCS32 review-only trampoline. Save r5 startup object,
 * preserve all original core arguments and stack alignment, then tail-call
 * original variadic logCV with original stack. Not an automatic hook. */
.syntax unified
.arm
.text
.align 2
.global r132_trace_thunk
.type r132_trace_thunk,%function
r132_trace_thunk:
    push {r0-r5,r12,lr}
    mov r0,sp
    bl r132_trace_dispatch
    str r0,[sp,#24]
    pop {r0-r5,r12,lr}
    cmp r12,#0
    bxeq lr
    bx r12
.size r132_trace_thunk,.-r132_trace_thunk
.section .note.GNU-stack,"",%progbits
