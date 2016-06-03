@
@   handlers.s
@       by TuxSH
@
@   This is part of Luma3DS, see LICENSE.txt for details
@

.macro GEN_HANDLER name
    .global \name
    .type   \name, %function
    \name:
        ldr sp, =#0xffff3000
        stmfd sp!, {r0-r7}
        mov r1, #\@         @ macro expansion counter
        b _commonHandler

    .size   \name, . - \name
.endm

.text
.arm
.align 4

.global _commonHandler
.type   _commonHandler, %function
_commonHandler:
    clrex
    cpsid aif
    mrs r2, spsr
    mov r6, sp
    mrs r3, cpsr
    
    ands r4, r2, #0xf       @ get the mode that triggered the exception
    moveq r4, #0xf          @ usr => sys
    bic r5, r3, #0xf
    orr r5, r4
    msr cpsr_c, r5          @ change processor mode
    stmfd r6!, {r8-lr}
    msr cpsr_c, r3          @ restore processor mode
    mov sp, r6
    vmrs r3, fpexc

    cmp r1, #1
    bne noFPUInit
    tst r5, #0x20
    bne noFPUInit
    
    ldr r4, [lr, #-4]
    lsl r4, #4
    sub r4, #0xc0000000
    cmp r4, #0x30000000
    bcs noFPUInit
    tst r3, #0x40000000
    bne noFPUInit
    
    sub lr, #4
    srsfd sp!, #0x13
    add sp, #28             @ restore context
    ldmfd sp!, {r0-r7}
    cps #0x13               @ FPU init
    stmfd sp, {r0-r3, r11-lr}^
    sub sp, #0x20
    bl .                    @ will be replaced
    ldmfd sp, {r0-r3, r11-lr}^
    add sp, #0x20
    rfefd sp!    
    
    noFPUInit:
    stmfd sp!, {r2,lr}      @ it's a bit of a mess, but we will fix that later
                            @ order of saved regs now: cpsr, pc + (2/4/8), r8-r14, r0-r7

    mov r0, sp
    mrc p15,0,r2,c0,c0,5    @ CPU ID register

    b mainHandler

GEN_HANDLER FIQHandler
GEN_HANDLER undefinedInstructionHandler
GEN_HANDLER prefetchAbortHandler
GEN_HANDLER dataAbortHandler

.global mcuReboot
.type   mcuReboot, %function
mcuReboot:
    b .                     @ will be replaced
    
.global clearDCacheAndDMB
.type   clearDCacheAndDMB, %function
clearDCacheAndDMB:
    mov r0, #0
    mcr p15,0,r0,c7,c14,0   @ Clean and Invalidate Entire Data Cache
    mcr p15,0,r0,c7,c10,4   @ Drain Memory Barrier
    bx lr
