@   This file is part of Luma3DS
@   Copyright (C) 2016-2017 Aurora Wright, TuxSH
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

.text
.arm
.balign 4

.global svcHandler
.type   svcHandler, %function
svcHandler:
    srsdb sp!, #0x13
    stmfd sp, {r8-r11, sp, lr}^
    sub sp, #0x18
    mrs r9, spsr
    ands r9, #0x20
    ldrneb r9, [lr, #-2]
    ldreqb r9, [lr, #-4]

    mov lr, #0              @ do stuff as if the "allow debug" flag is always set
    push {r0-r7, r12, lr}
    mov r10, #1
    strb r9, [sp, #0x58+3]  @ page end - 0xb8 + 3: svc being handled
    strb r10, [sp, #0x58+1] @ page end - 0xb8 + 1: "allow debug" flag

    @ sp = page end - 0x110
    add r0, sp, #0x110       @ page end
    bl svcHook
    cpsid i
    mov r8, r0
    ldmfd sp, {r0-r7, r12, lr}
    cmp r8, #0
    beq _fallback            @ invalid svc, or svc 0xff (stop point)

    _handled_svc:            @ unused label, just here for formatting
        push {r0-r12, lr}
        add r0, sp, #0x148
        cpsie i
        bl signalSvcEntry
        cpsid i
        pop {r0-r12, lr}

        cpsie i
        blx r8
        cpsid i

        ldrb lr, [sp, #0x58+0] @ page end - 0xb8 + 0: scheduling flags
        b _fallback_end

    _fallback:
        mov r0, r9
        ldr r8, =svcFallbackHandler
        ldr r8, [r8]
        blx r8
        mov lr, #0

    _fallback_end:

    ldr r8, [sp, #0x24]     @ page_end - 0xec: saved lr (see above) : should reload regs
    cmp r8, #0
    addeq sp, #0x24
    popne {r0-r7, r12}
    add sp, #4

    cmp r9, #0xff
    beq _no_signal_return

    push {r0-r7, r12, lr}
    add r0, sp, #0x110      @ page end
    cpsie i
    bl signalSvcReturn
    cpsid i
    pop {r0-r7, r12}
    add sp, #4

    _no_signal_return:

    mov r8, #0
    strb r8, [sp, #0x30+3]  @ page_end - 0xb8 + 3: svc being handled

    _svc_finished:

    ldmfd sp, {r8-r11, sp, lr}^
    cmp lr, #0
    bne _postprocess_svc

    _svc_return:
        add sp, #0x18
        rfefd sp!           @ return to user mode

    _postprocess_svc:
        mov lr, #0
        push {r0-r7, r12, lr}

        push {r0-r3}
        bl postprocessSvc
        pop {r0-r3}

        ldrb lr, [sp, #0x58+0] @ page end - 0xb8 + 0: scheduling flags
        ldr r8, [sp, #0x24]    @ page_end - 0xec: saved lr (see above) : should reload regs
        cmp r8, #0
        addeq sp, #0x24
        popne {r0-r7, r12}
        add sp, #4
        b _svc_finished
