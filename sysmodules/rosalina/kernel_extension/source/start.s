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

.section .text.start
.balign 4
.global _start
_start:
    push {r4, lr}

    mrc p15, 0, r4, c0, c0, 5   @ CPUID register
    and r4, #3
    cmp r4, #1
    beq _core1_only

    _waitLoop:
        wfe
        ldr r0, =_setupFinished
        ldr r0, [r0]
        cmp r0, #0
        beq _waitLoop
    b end

    _core1_only:
        bl main
        ldr r0, =_setupFinished
        str r4, [r0]
        sev

    end:
    pop {r4, pc}

.bss
.balign 4
_setupFinished: .word 0
