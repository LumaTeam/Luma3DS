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

.text
.arm
.align 4

.global flushCaches
.type flushCaches, %function
flushCaches:
    @ Clean and flush data cache
    @ Adpated from http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0155a/ch03s03s05.html ,
    @ and https://github.com/gemarcano/libctr9_io/blob/master/src/ctr_system_ARM.c#L39 as well
    @ Note: ARM's example is actually for a 8KB DCache (which is what the 3DS has)
    @ Implemented in bootROM at address 0xffff0830

    mov r1, #0                          @ segment counter
    outer_loop:
        mov r0, #0                      @ line counter

        inner_loop:
            orr r2, r1, r0                  @ generate segment and line address
            mcr p15, 0, r2, c7, c14, 2      @ clean and flush the line
            add r0, #0x20                   @ increment to next line
            cmp r0, #0x400
            bne inner_loop

        add r1, #0x40000000
        cmp r1, #0
        bne outer_loop

    mcr p15, 0, r1, c7, c10, 4              @ drain write buffer

    @ Flush instruction cache
    mcr p15, 0, r1, c7, c5, 0

    bx lr
