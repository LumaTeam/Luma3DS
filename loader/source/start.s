@   This file is part of Luma3DS
@   Copyright (C) 2017 Aurora Wright, TuxSH
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

.arm

.section .text.start
.align 4
.global _start
_start:
    ldr sp, =__stack_top__
    b main

.text
.balign 4
.global disableMpuAndJumpToEntrypoints
.type   disableMpuAndJumpToEntrypoints, %function
disableMpuAndJumpToEntrypoints:
    mov r4, r0
    mov r5, r1
    mov r6, r2
    mov r7, r3

    bl flushCaches

    @ Disable caches / MPU
    mrc p15, 0, r0, c1, c0, 0  @ read control register
    bic r0, #(1<<12)           @ - instruction cache disable
    bic r0, #(1<<2)            @ - data cache disable
    bic r0, #(1<<0)            @ - mpu disable
    mcr p15, 0, r0, c1, c0, 0  @ write control register

    @ Set the ARM11 entrypoint
    mov r0, #0x20000000
    str r7, [r0, #-4]

    @ Jump to the ARM9 entrypoint
    mov r0, r4
    mov r1, r5
    ldr r2, =0xBEEF
    bx r6
