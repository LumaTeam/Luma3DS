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

.global svc0x2F
.type svc0x2F, %function
svc0x2F:
    @ custom backdoor before kernel ext. is installed
    svc 0x2F
    bx lr

.global convertVAToPA
.type   convertVAToPA, %function
convertVAToPA:
    @ needs to be executed in supervisor mode
    mov r1, #0x1000
    sub r1, #1
    and r2, r0, r1
    bic r0, r1
    mcr p15, 0, r0, c7, c8, 0    @ VA to PA translation with privileged read permission check
    mrc p15, 0, r0, c7, c4, 0    @ read PA register
    tst r0, #1                   @ failure bit
    bic r0, r1
    addeq r0, r2
    movne r0, #0
    bx lr

.section .data

.p2align 12
.global kernel_extension
kernel_extension: .incbin "build/kernel_extension.bin"
.p2align 12
kernel_extension_end:

.global kernel_extension_size
kernel_extension_size: .word kernel_extension_end - kernel_extension
