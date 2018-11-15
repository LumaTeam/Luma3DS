.section .data
.balign 4
.arm

.global gamePatchFunc
.type   gamePatchFunc, %function
gamePatchFunc:
    stmfd   sp!, {r0-r12, lr}
    mrs     r0, cpsr
    stmfd   sp!, {r0}
    adr     r0, g_savedGameInstr
    ldr     r1, =0x00100000
    ldr     r2, [r0]
    str     r2, [r1]
    ldr     r2, [r0, #4]
    str     r2, [r1, #4]
    svc     0x92
    svc     0x94

startplugin:
    ldr     r5, =0x07000100
    blx     r5

exit:
    ldmfd   sp!, {r0}
    msr     cpsr, r0
    ldmfd   sp!, {r0-r12, lr}
    ldr     lr, =0x00100000
    mov     pc, lr

.global g_savedGameInstr
g_savedGameInstr:
    .word 0, 0
