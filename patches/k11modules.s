;
;   This file is part of Luma3DS
;   Copyright (C) 2016 Aurora Wright, TuxSH
;
;   This program is free software: you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, either version 3 of the License, or
;   (at your option) any later version.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program.  If not, see <http://www.gnu.org/licenses/>.
;
;   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
;   reasonable legal notices or author attributions in that material or in the Appropriate Legal
;   Notices displayed by works containing it.
;

; Code originally from Subv

.arm.little

.create "build/k11modules.bin", 0
.arm
    ; This code searches the sm module for a specific byte pattern and patches some of the instructions
    ; in the code to disable service access checks when calling srv:GetServiceHandle

    ; It also searches the fs module for archive access check code

    ; Save the registers we'll be using
    ; Register contents:
    ; r4: Pointer to a pointer to the exheader of the current NCCH
    ; r6: Constant 0
    ; SP + 4: Pointer to the memory location where the NCCH text was loaded

    ; Execute the instruction we overwrote in our detour
    ldr r0, [r4]

    ; Save the value of the register we use
    push {r0-r4}

    ldr r1, [sp, #24]    ; Load the .text address
    ldr r2, [r0, #0x200] ; Load the low title id of the current NCCH
    ldr r0, [r0, #0x18]  ; Load the size of the .text
    add r0, r1, r0       ; Max bounds of the memory region

    ldr r3, =0x1002 ; Low title id of the sm module
    cmp r2, r3 ; Compare the low title id to the id of the sm module
    bne fs_patch ; Skip if they're not the same

    ldr r2, =0xE1A01006 ; mov r1, r6

    loop:
        cmp r0, r1
        blo die ; Check if we didn't go past the bounds of the memory region
        ldr r3, [r1]
        cmp r3, r2
        ldreqh r3, [r1, #4]
        cmpeq r3, #5
        addne r1, #4
        bne loop

    ; r1 now contains the start address of the pattern we found
    ldr r0, =0xE3A00001 ; mov r0, #1
    str r0, [r1, #8] ; Patch the bl
    b out

    fs_patch: ; patch adapted from BootNTR
        ldr r3, =0x1102 ; Low title id of the fs module
        cmp r2, r3 ; Compare the low title id to the id of the sm module
        bne out ; Skip if they're not the same

        ldr r2, =0x7401 ; strb r1, [r0, #16]
        ldr r3, =0x2000 ; movs r0, #0

        loop_fs:
            cmp r0, r1
            blo die
            ldrh r4, [r1]
            cmp r4, r2
            ldreqh r4, [r1, #2]
            cmpeq r4, r3
            addeq r1, #8
            addne r1, #2
            bne loop_fs

        ; r1 now contains the start address of the pattern we found
        ldr r0, =0x2001 ; mov r0, #1
        ldr r2, =0x4770 ; bx lr
        strh r0, [r1]
        strh r2, [r1, #2]

    out:
        pop {r0-r4} ; Restore the registers we used
        bx lr ; Jump back to whoever called us

    die:
        b die

.pool
.close