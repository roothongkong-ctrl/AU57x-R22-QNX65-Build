/* Exact MU1005 original instruction replay. No vehicle deployment. */
.syntax unified
.arm
.section .text.entry,"ax"
.global mu_started,mu_cleanup,mu_eir
mu_started:
    strb r3,[r5,#0x1b0]
    push {r0-r3,r12,lr}
    mrs r2,apsr
    push {r2,r3}
    mov r0,r5
    bl mu_phase1_start
    pop {r2,r3}
    msr APSR_nzcvq,r2
    pop {r0-r3,r12,lr}
    ldr pc,=0x11ec9c
mu_cleanup:
    push {r0-r3,r12,lr}
    mrs r2,apsr
    push {r2,r3}
    bl mu_phase1_cleanup
    pop {r2,r3}
    msr APSR_nzcvq,r2
    pop {r0-r3,r12,lr}
    ldrb r3,[r0,#0x16]
    ldr pc,=0x27a618
mu_eir:
    push {r1,r3,r12,lr}
    mrs r3,apsr
    push {r1,r3}
    mov r0,r7
    mov r1,r2
    mov r2,r6
    bl mu_phase1_eir
    mov r2,r0
    pop {r1,r3}
    msr APSR_nzcvq,r3
    pop {r1,r3,r12,lr}
    strb r0,[r6,#0x40c]
    ldr pc,=0x12a6a4
.ltorg
