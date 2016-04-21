@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@   handlers.s                               @
@       by TuxSH                             @
@   Copyright (c) 2016 All Rights Reserved   @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

@ This is part of AuReiNand, see LICENSE.txt for details

.macro GEN_HANDLER name, addr_offset
    .global \name
    .type   \name, %function
\name:
    stmfd sp!, {r0-r7}      @ FIQ has its own r8-r14 regs
    ldr r0, =\addr_offset
    sub r0, lr, r0          @ address of instruction that triggered the exception; we will handle the undef+Thumb case later
    mrs r2, spsr
    
    mov r6, sp
    mrs r3, cpsr
    ands r4, r2, #0xf       @ get the mode that triggered the exception
    moveq r4, #0xf          @ usr => sys
    bic r5, r3, #0xf
    orr r5, r4
    msr cpsr_c, r5          @ change processor mode
    stmfd r6!, {r8-r14}
    msr cpsr_c, r3          @ restore processor mode
    mov sp, r6
    
    stmfd sp!, {r0,r2}      @ it's a bit of a mess, but we will fix that later
                            @ order of regs now: pc, spsr, r8-r14, r0-r7
    mov r0, sp
    ldr r1, =\@             @ macro expansion counter
    b mainHandler

    .size   \name, . - \name
.endm

    .text
    .arm
    .align  4

    GEN_HANDLER FIQHandler, 4
    GEN_HANDLER undefinedInstructionHandler, 4
    GEN_HANDLER prefetchAbortHandler, 4
    GEN_HANDLER dataAbortHandler, 8

.global setupStack
.type setupStack, %function
setupStack:
    cmp r0, #0
    moveq r0, #0xf          @ usr => sys
    mrs r2, cpsr
    bic r3, r2, #0xf
    orr r3, r0              @ processor mode
    msr cpsr_c, r3          @ change processor mode
    mov sp, r1
    msr cpsr_c, r2          @ restore processor mode
    bx lr
