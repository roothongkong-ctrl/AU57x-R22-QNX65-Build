/* Original MU1005 SI event producer, review only.
 * No calls to OEM services, no authentication decisions. */
.syntax unified
.arm
.text
.macro SAVE
    push {r0-r3,r12,lr}
    mrs r12,cpsr
    push {r12,lr}
.endm
.macro RESTORE
    pop {r12,lr}
    msr cpsr_f,r12
    pop {r0-r3,r12,lr}
.endm
/* Atomic count, bounded retry. On contention/overflow mark observation invalid. */
.macro COUNT offset
    ldr r0,=EVENT_STATE
    add r1,r0,#\offset
    mov r3,#8
1:
    ldrex r2,[r1]
    adds r2,r2,#1
    beq 2f
    strex r12,r2,[r1]
    cmp r12,#0
    beq 3f
    subs r3,r3,#1
    bne 1b
2:
    clrex
    mov r2,#1
    str r2,[r0,#20]
3:
    dmb ish
.endm
.global mu_usb_remove,mu_usb_insert,mu_si_reset
mu_usb_remove:
    mov r2,r1
    SAVE
    COUNT 0
    RESTORE
    ldr pc,=0x15d4d4
mu_usb_insert:
    mov r2,r1
    SAVE
    COUNT 4
    RESTORE
    ldr pc,=0x15d4e8
mu_si_reset:
    SAVE
    ldr r0,[r4,#0x498]
    /* Replay original store before publishing the reset observation. */
    str sl,[r4,#0x498]
    ldr r1,=EVENT_STATE
    add r12,r1,#16
    ldrex r2,[r12]
    cmp r2,#0
    bne 4f
    mov r3,#1
    strex r2,r3,[r12]
    cmp r2,#0
    bne 4f
    dmb ish
    ldr r2,[r1,#8]
    adds r2,r2,#1
    beq 5f
    str r0,[r1,#12]
    dmb ish
    str r2,[r1,#8]
    b 6f
5:
    mov r2,#1
    str r2,[r1,#20]
6:
    dmb ish
    mov r2,#0
    str r2,[r12]
    b 7f
4:
    clrex
    mov r2,#1
    str r2,[r1,#20]
7:
    dmb ish
    RESTORE
    ldr pc,=0x188034
.ltorg
