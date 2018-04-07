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
    b start
    b startPhys

    b _bindSGI0Hook
    b configHook

    b undefinedInstructionHandler
    b svcHandler
    b prefetchAbortHandler
    b dataAbortHandler

    .word __end__
    .word kExtParameters
    .word 1 @ enableUserExceptionHandlersForCPUExc

    b KThread__DebugReschedule
start:
    @ Only core0 executes this, the other cores are running coreBarrier

    @ Skipped instruction:
    str r1, [r4, #0x8]

    push {r0-r12, lr}

    sub r0, r4, #8
    sub r1, r8, #0x8000
    bl main

    pop {r0-r12, pc}

startPhys:
    push {r0-r12, lr}
    mrc p15, 0, r0, c0, c0, 5   @ CPUID register
    and r0, #3
    mov r1, r2
    bl relocateAndSetupMMU
    pop {r0-r12, lr}
    mov r12, #0x20000000        @ instruction that has been patched
    bx lr

_bindSGI0Hook:
    push {r0-r12, lr}
    bl bindSGI0Hook
    pop {r0-r12, pc}
