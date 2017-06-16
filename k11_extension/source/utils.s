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

.global convertVAToPA
.type   convertVAToPA, %function
convertVAToPA:
    mov r3, r1
    mov r1, #0xf00
    orr r1, #0xff
    and r2, r0, r1
    bic r0, r1
    cmp r3, #1
    beq _convertVAToPA_write_check
    _convertVAToPA_read_check:
        mcr p15, 0, r0, c7, c8, 0    @ VA to PA translation with privileged read permission check
        b _convertVAToPA_end_check
    _convertVAToPA_write_check:
        mcr p15, 0, r0, c7, c8, 1    @ VA to PA translation with privileged write permission check
    _convertVAToPA_end_check:
    mrc p15, 0, r0, c7, c4, 0    @ read PA register
    tst r0, #1                   @ failure bit
    bic r0, r1
    addeq r0, r2
    movne r0, #0
    bx lr

.global flushEntireDataCache
.type   flushEntireDataCache, %function
flushEntireDataCache:
    mvn r1, #0              @ this is translated to a full cache flush
    ldr r12, =flushDataCacheRange
    ldr r12, [r12]
    bx r12

.global invalidateEntireInstructionCache
.type   invalidateEntireInstructionCache, %function
invalidateEntireInstructionCache:
    mvn r1, #0              @ this is translated to a full cache flush
    ldr r12, =invalidateInstructionCacheRange
    ldr r12, [r12]
    bx r12

.global KObjectMutex__Acquire
.type   KObjectMutex__Acquire, %function
KObjectMutex__Acquire:
    ldr r1, =0xFFFF9000     @ current thread addr

    ldr r1, [r1]
    ldrex r2, [r0]
    cmp r2, #0
    strexeq r3, r1, [r0]    @ store current thread
    strexne r3, r2, [r0]    @ stored thread != NULL, no change
    cmpeq r3, #0
    bxeq lr

    ldr r12, =KObjectMutex__WaitAndAcquire
    ldr r12, [r12]
    bx r12

.global KObjectMutex__Release
.type   KObjectMutex__Release, %function
KObjectMutex__Release:
    mov r1, #0
    str r1, [r0]
    ldrh r1, [r0, #4]
    cmp r1, #0
    bxle lr

    ldr r12, =KObjectMutex__ErrorOccured
    ldr r12, [r12]
    blx r12
    bx lr

.global safecpy
.type   safecpy, %function
safecpy:
    push {r4, lr}
    mov r3, #0
    movs r12, #1

    _safecpy_loop:
        ldrb r4, [r1, r3]
        cmp r12, #0
        beq _safecpy_loop_end
        strb r4, [r0, r3]
        add r3, #1
        cmp r3, r2
        blo _safecpy_loop

    _safecpy_loop_end:
    mov r0, r3
    pop {r4, pc}

_safecpy_end:

.section .rodata

.global safecpy_sz
safecpy_sz: .word _safecpy_end - safecpy

.bss
.balign 4

.global SGI0Handler
SGI0Handler: .word 0  @ see synchronization.c

.balign 4

.section .data
.balign 4

_customInterruptEventObj: .word SGI0Handler
.global customInterruptEvent
customInterruptEvent: .word _customInterruptEventObj
