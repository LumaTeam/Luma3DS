@   This file is part of Luma3DS
@   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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
@   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
@       * Requiring preservation of specified reasonable legal notices or
@         author attributions in that material or in the Appropriate Legal
@         Notices displayed by works containing it.
@       * Prohibiting misrepresentation of the origin of that material,
@         or requiring that modified versions of such material be marked in
@         reasonable ways as different from the original version.

.macro GEN_USUAL_HANDLER name, index
    \name\()Handler:
        ldr sp, =_regs
        stmia sp, {r0-r7}

        mov r0, #\index
        b _arm9ExceptionHandlerCommon
.endm

.section .arm9_exception_handlers.text, "ax", %progbits
.arm
.align 4

.global _arm9ExceptionHandlerCommon
.type   _arm9ExceptionHandlerCommon, %function
_arm9ExceptionHandlerCommon:
    mov r1, r0
    mov r0, sp
    mrs r2, spsr
    mrs r3, cpsr
    add r6, r0, #(8 * 4)

    orr r3, #0xc0              @ mask interrupts
    msr cpsr_cx, r3

    ands r4, r2, #0xf          @ get the mode that triggered the exception
    moveq r4, #0xf             @ usr => sys
    bic r5, r3, #0xf
    orr r5, r4
    msr cpsr_c, r5             @ change processor mode
    stmia r6!, {r8-lr}
    msr cpsr_c, r3             @ restore processor mode

    str lr, [r6], #4
    str r2, [r6]

    msr cpsr_cxsf, #0xdf       @ finally, switch to system mode, mask interrupts and clear flags (in case of double faults)
    ldr sp, =0x02000000
    b arm9ExceptionHandlerMain


.global FIQHandler
.type   FIQHandler, %function
GEN_USUAL_HANDLER FIQ, 0

.global undefinedInstructionHandler
.type   undefinedInstructionHandler, %function
GEN_USUAL_HANDLER undefinedInstruction, 1

.global prefetchAbortHandler
.type   prefetchAbortHandler, %function
prefetchAbortHandler:
    msr cpsr_cx, #0xd7                  @ mask interrupts (abort mode)
    mrs sp, spsr
    and sp, #0x3f
    cmp sp, #0x13
    bne _prefetchAbortNormalHandler

    ldr sp, =arm9ExceptionHandlerSvcBreakAddress
    ldr sp, [sp]
    cmp sp, #0
    beq _prefetchAbortNormalHandler
    add sp, #(1*4 + 4)
    cmp lr, sp
    bne _prefetchAbortNormalHandler

    mov sp, r8
    pop {r8-r11}
    ldr lr, [sp, #8]!
    ldr sp, [sp, #4]
    msr spsr_cxsf, sp
    tst sp, #0x20
    addne lr, #2                        @ adjust address for later

    GEN_USUAL_HANDLER _prefetchAbortNormal, 2

.global dataAbortHandler
.type   dataAbortHandler, %function
dataAbortHandler:
    msr cpsr_cx, #0xd7                  @ mask interrupts (abort mode)
    mrs sp, spsr
    and sp, #0x3f
    cmp sp, #0x1f
    bne _dataAbortNormalHandler

    sub lr, #8
    adr sp, safecpy
    cmp lr, sp
    blo _j_dataAbortNormalHandler
    adr sp, _safecpy_end
    cmp lr, sp
    bhs _j_dataAbortNormalHandler

    msr spsr_f, #(1 << 30)
    mov r12, #0
    adds pc, lr, #4

    _j_dataAbortNormalHandler:
    add lr, #8

    GEN_USUAL_HANDLER _dataAbortNormal, 3


.global safecpy
.type   safecpy, %function
safecpy:
    push {r4, lr}
    mov r3, #0
    movs r12, #1

    _safecpy_loop:
        ldrb r4, [r1, r3]
        cmp r12, #0
        beq _safecpy_loop_end
        strb r4, [r0, r3]
        add r3, #1
        cmp r3, r2
        blo _safecpy_loop

    _safecpy_loop_end:
    mov r0, r3
    pop {r4, pc}

_safecpy_end:

.section .arm9_exception_handlers.rodata, "a", %progbits
.align 4
.global arm9ExceptionHandlerAddressTable
arm9ExceptionHandlerAddressTable:
    .word   0                               @ IRQ
    .word   FIQHandler                      @ FIQ
    .word   0                               @ SVC
    .word   undefinedInstructionHandler     @ Undefined instruction
    .word   prefetchAbortHandler            @ Prefetch abort
    .word   dataAbortHandler                @ Data abort

.section .arm9_exception_handlers.bss, "aw", %nobits
.align 4

.global arm9ExceptionHandlerSvcBreakAddress
arm9ExceptionHandlerSvcBreakAddress:
    .skip 4

_regs: .skip (4 * 17)
