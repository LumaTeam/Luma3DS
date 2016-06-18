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
        ldr sp, =#0x02000000    @ We make the (full descending) stack point to the end of ITCM for our exception handlers. 
                                @ It doesn't matter if we're overwriting stuff here, since we're going to reboot.
                                
        stmfd sp!, {r0-r7}      @ FIQ has its own r8-r14 regs
        ldr r1, =\@             @ macro expansion counter
        b _commonHandler

    .size   \name, . - \name
.endm

.text
.arm
.align 4

.global _commonHandler
.type   _commonHandler, %function
_commonHandler:
    mrs r2, spsr
    mov r6, sp
    mrs r3, cpsr
    orr r3, #0x1c0          @ disable Imprecise Aborts, IRQ and FIQ (AIF)
    ands r4, r2, #0xf       @ get the mode that triggered the exception
    moveq r4, #0xf          @ usr => sys
    bic r5, r3, #0xf
    orr r5, r4
    msr cpsr_c, r5          @ change processor mode
    stmfd r6!, {r8-lr}
    msr cpsr_cx, r3         @ restore processor mode
    mov sp, r6
    
    stmfd sp!, {r2,lr}      @ it's a bit of a mess, but we will fix that later
                            @ order of saved regs now: cpsr, pc + (2/4/8), r8-r14, r0-r7
                            
    mov r0, sp
    b mainHandler

GEN_HANDLER FIQHandler
GEN_HANDLER undefinedInstructionHandler
GEN_HANDLER prefetchAbortHandler
GEN_HANDLER dataAbortHandler
