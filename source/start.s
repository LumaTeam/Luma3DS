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

@   Thanks to the numerous people who took part in writing this file

.section .text.start
.align 4
.global _start
_start:
    b start

.global launchedFirmTidLow
launchedFirmTidLow:
    .hword 0, 0, 0, 0, 0, 0, 0, 0

start:
    @ Disable interrupts
    mrs r0, cpsr
    orr r0, #0x1C0
    msr cpsr_cx, r0

    @ Change the stack pointer
    mov sp, #0x27000000

    @ Disable caches / MPU
    mrc p15, 0, r0, c1, c0, 0  @ read control register
    bic r0, #(1<<12)           @ - instruction cache disable
    bic r0, #(1<<2)            @ - data cache disable
    bic r0, #(1<<0)            @ - mpu disable
    mcr p15, 0, r0, c1, c0, 0  @ write control register

    @ Flush caches
    bl flushEntireDCache
    bl flushEntireICache

    @ Give read/write access to all the memory regions
    ldr r0, =0x3333333
    mcr p15, 0, r0, c5, c0, 2 @ write data access
    mcr p15, 0, r0, c5, c0, 3 @ write instruction access

    @ Set MPU permissions and cache settings
    ldr r0, =0xFFFF001D @ ffff0000 32k  | bootrom (unprotected part)
    ldr r1, =0x01FF801D @ 01ff8000 32k  | itcm
    ldr r2, =0x08000029 @ 08000000 2M   | arm9 mem (O3DS / N3DS)
    ldr r3, =0x10000029 @ 10000000 2M   | io mem (ARM9 / first 2MB)
    ldr r4, =0x20000037 @ 20000000 256M | fcram (O3DS / N3DS)
    ldr r5, =0x1FF00027 @ 1FF00000 1M   | dsp / axi wram
    ldr r6, =0x1800002D @ 18000000 8M   | vram (+ 2MB)
    mov r7, #0
    mov r8, #0x15
    mcr p15, 0, r0, c6, c0, 0
    mcr p15, 0, r1, c6, c1, 0
    mcr p15, 0, r2, c6, c2, 0
    mcr p15, 0, r3, c6, c3, 0
    mcr p15, 0, r4, c6, c4, 0
    mcr p15, 0, r5, c6, c5, 0
    mcr p15, 0, r6, c6, c6, 0
    mcr p15, 0, r7, c6, c7, 0
    mcr p15, 0, r8, c3, c0, 0   @ Write bufferable 0, 2, 4
    mcr p15, 0, r8, c2, c0, 0   @ Data cacheable 0, 2, 4
    mcr p15, 0, r8, c2, c0, 1   @ Inst cacheable 0, 2, 4

    @ Enable caches / MPU / ITCM
    mrc p15, 0, r0, c1, c0, 0  @ read control register
    orr r0, r0, #(1<<18)       @ - ITCM enable
    orr r0, r0, #(1<<13)       @ - alternate exception vectors enable
    orr r0, r0, #(1<<12)       @ - instruction cache enable
    orr r0, r0, #(1<<2)        @ - data cache enable
    orr r0, r0, #(1<<0)        @ - mpu enable
    mcr p15, 0, r0, c1, c0, 0  @ write control register

    @ Fix mounting of SDMC
    ldr r0, =0x10000020
    mov r1, #0x340
    str r1, [r0]

    @ Clear BSS
    ldr r0, =__bss_start
    mov r1, #0
    ldr r2, =__bss_end
    sub r2, r0
    bl memset32

    b main
