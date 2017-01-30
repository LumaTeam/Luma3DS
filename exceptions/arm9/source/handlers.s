@   This file is part of Luma3DS
@   Copyright (C) 2016 Aurora Wright, TuxSH
@
@   This program is free software: you can redistribute it and/or modify
@   it under the terms of the GNU General Public License as published by
@   the Free Software Foundation, either version 3 of the License, or
@   (at your option) any later version.
@
@   This program is distributed in the hope that it will be useful,
@   but WITHOUT ANY WARRANTY; without even the implied warranty of
@   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
@   GNU General Public License for more details.
@
@   You should have received a copy of the GNU General Public License
@   along with this program.  If not, see <http://www.gnu.org/licenses/>.
@
@   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
@   reasonable legal notices or author attributions in that material or in the Appropriate Legal
@   Notices displayed by works containing it.

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

    orr r3, #0x1c0              @ disable Imprecise Aborts, IRQ and FIQ (equivalent to "cpsid aif" on arm11)
    msr cpsr_cx, r3

    tst r2, #0x20
    bne noSvcBreak
    cmp r1, #2
    bne noSvcBreak

    sub r0, lr, #4              @ calling cannotAccessAddress cause more problems that it actually solves... (I've to save a lot of regs and that's a pain tbh)
    lsr r0, #20                 @ we'll just do some address checks (to see if it's in ARM9 internal memory)
    cmp r0, #0x80
    bne noSvcBreak
    ldr r4, [lr, #-4]
    ldr r5, =#0xe12fff7f
    cmp r4, r5
    bne noSvcBreak
    bic r5, r3, #0xf
    orr r5, #0x3
    msr cpsr_c, r5             @ switch to supervisor mode
    ldmfd sp, {r8-r11}^
    ldr r2, [sp, #0x1c]        @ implementation details of the official svc handler
    ldr r4, [sp, #0x18]
    msr cpsr_c, r3             @ restore processor mode
    tst r2, #0x20
    addne lr, r4, #2           @ adjust address for later
    moveq lr, r4

    noSvcBreak:
    ands r4, r2, #0xf          @ get the mode that triggered the exception
    moveq r4, #0xf             @ usr => sys
    bic r5, r3, #0xf
    orr r5, r4
    msr cpsr_c, r5             @ change processor mode
    stmfd r6!, {r8-lr}
    msr cpsr_c, r3             @ restore processor mode
    mov sp, r6

    stmfd sp!, {r2,lr}         @ it's a bit of a mess, but we will fix that later
                               @ order of saved regs now: cpsr, pc + (2/4/8), r8-r14, r0-r7

    mov r0, sp

    b mainHandler

GEN_HANDLER FIQHandler
GEN_HANDLER undefinedInstructionHandler
GEN_HANDLER prefetchAbortHandler
GEN_HANDLER dataAbortHandler

.global readMPUConfig
.type   readMPUConfig, %function
readMPUConfig:
    stmfd sp!, {r4-r8, lr}
    mrc p15,0,r1,c6,c0,0
    mrc p15,0,r2,c6,c1,0
    mrc p15,0,r3,c6,c2,0
    mrc p15,0,r4,c6,c3,0
    mrc p15,0,r5,c6,c4,0
    mrc p15,0,r6,c6,c5,0
    mrc p15,0,r7,c6,c6,0
    mrc p15,0,r8,c6,c7,0
    stmia r0, {r1-r8}
    mrc p15,0,r0,c5,c0,2       @ read data access permission bits
    ldmfd sp!, {r4-r8, pc}
