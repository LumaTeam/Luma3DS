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

.text
.arm
.balign 4

.macro GEN_GETINFO_WRAPPER, name
    .global Get\name\()InfoHookWrapper
    .type   Get\name\()InfoHookWrapper, %function
    Get\name\()InfoHookWrapper:
        push {r12, lr}
        sub sp, #8
        mov r0, sp
        bl Get\name\()InfoHook
        pop {r1, r2, r12, pc}
.endm

GEN_GETINFO_WRAPPER Handle
GEN_GETINFO_WRAPPER System
GEN_GETINFO_WRAPPER Process
GEN_GETINFO_WRAPPER Thread

.macro GEN_OUT1_WRAPPER, name
    .global \name\()Wrapper
    .type   \name\()Wrapper, %function
    \name\()Wrapper:
        push {lr}
        sub sp, #4
        mov r0, sp
        bl \name
        pop {r1, pc}
.endm

GEN_OUT1_WRAPPER ConnectToPortHook
GEN_OUT1_WRAPPER CopyHandle
GEN_OUT1_WRAPPER TranslateHandle

.global ControlMemoryHookWrapper
.type   ControlMemoryHookWrapper, %function
ControlMemoryHookWrapper:
    push {lr}
    sub sp, #12
    stmia sp, {r0, r4}
    add r0, sp, #8
    bl ControlMemoryHook
    ldr r1, [sp, #8]
    add sp, #12
    pop {pc}


.global ExitProcessHookWrapper
.type   ExitProcessHookWrapper, %function
ExitProcessHookWrapper:
    push {lr}
    bl ExitProcessHook
    pop {pc}

.global ControlMemoryEx
.type   ControlMemoryEx, %function
ControlMemoryEx:
    push {lr}
    sub sp, #8
    cmp r5, #0
    movne r5, #1
    push {r0, r4, r5}
    add r0, sp, #12
    ldr r12, =ControlMemory
    ldr r12, [r12]
    blx r12
    ldr r1, [sp, #12]
    add sp, #20
    pop {pc}

.global ControlMemoryUnsafeWrapper
.type   ControlMemoryUnsafeWrapper, %function
ControlMemoryUnsafeWrapper:
    push {lr}
    str r4, [sp, #-4]!
    bl ControlMemoryUnsafe
    add sp, #4
    pop {pc}

.global MapProcessMemoryExWrapper
.type   MapProcessMemoryExWrapper, %function
MapProcessMemoryExWrapper:
    push {lr}
    str r4, [sp, #-4]!
    bl MapProcessMemoryEx
    add sp, #4
    pop {pc}
