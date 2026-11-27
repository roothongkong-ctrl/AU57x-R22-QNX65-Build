/* SPDX-License-Identifier: GPL-3.0-only
 * ARM AAPCS variadic trace forwarder, review only.
 * The original receives the exact argument registers, stack and return
 * address; no C va_list is passed to a vendor variadic function.
 * A resolver returns the callable original target; no SDK operation here.
 * This is NOT the vendor symbol and is not automatically preloaded.
 */
.syntax unified
.arm
.text
.align 2
.global mu_bt_trace_tail
.type mu_bt_trace_tail,%function
mu_bt_trace_tail:
    push {r0-r3,r4,lr}
    mov r1, lr
    /* r0 = original Channel this, r1 = caller LR, r2 = format */
    bl mu_bt_trace_target
    mov r12, r0
    pop {r0-r3,r4,lr}
    cmp r12, #0
    bxeq lr
    bx r12
.size mu_bt_trace_tail, .-mu_bt_trace_tail
