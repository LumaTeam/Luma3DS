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
 
.section .text.start
.align 4
.global _start
.type   _start, %function
_start:
b start

.global operation
operation:
    .word 0

start:
    cpsid aif
    ldr sp, =__stack_top__
    b main

.global prepareForFirmlaunch
.type   prepareForFirmlaunch, %function
prepareForFirmlaunch:
    mov r0, #0x20000000

    _wait_for_core0_entrypoint_loop:
        ldr r1, [r0, #-4]   @ check if core0's entrypoint is 0
        cmp r1, #0
        beq _wait_for_core0_entrypoint_loop

    bx r1                   @ jump to core0's entrypoint
prepareForFirmlaunchEnd:

.global prepareForFirmlaunchSize
prepareForFirmlaunchSize: .word prepareForFirmlaunchEnd - prepareForFirmlaunch
