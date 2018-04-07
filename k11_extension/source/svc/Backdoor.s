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

.global Backdoor
.type   Backdoor, %function
Backdoor:
    @ Nintendo's code
    bic   r1, sp, #0xff
    orr   r1, r1, #0xf00
    add   r1, r1, #0x28     @ get user stack.

    ldr   r2, [r1]
    stmdb r2!, {sp, lr}
    mov   sp, r2            @ sp_svc = sp_usr. You'll get nice crashes if an interrupt or context switch occurs during svcBackdoor
    blx   r0
    pop   {r0, r1}
    mov   sp, r0
    bx    r1
