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

.section    .text.alignedseqmemcpy, "ax", %progbits
.arm
@ Align on cache line boundaries & make sure the loops don't cross them.
.align      5
.global     alignedseqmemcpy
.type       alignedseqmemcpy, %function
alignedseqmemcpy:
    @ src=r1 and dst=r0 are expected to be 4-byte-aligned
    push    {r4-r10, lr}

    lsrs    r12, r2, #5
    sub     r2, r2, r12, lsl #5
    beq     2f

1:
    ldmia   r1!, {r3-r10}
    stmia   r0!, {r3-r10}
    subs    r12, #1
    bne     1b

2:
    lsrs    r12, r2, #2
    sub     r2, r2, r12, lsl #2
    beq     4f

3:
    ldr     r3, [r1], #4
    str     r3, [r0], #4
    subs    r12, #1
    bne     3b

4:
    tst     r2, #2
    ldrneh  r3, [r1], #2
    strneh  r3, [r0], #2

    tst     r2, #1
    ldrneb  r3, [r1], #1
    strneb  r3, [r0], #1

    pop     {r4-r10, pc}
