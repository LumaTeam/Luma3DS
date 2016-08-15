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

; This is mainly Subv's code, big thanks to him.

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
    ; SP + 0x80 - 0x7C: Pointer to the memory location where the NCCH text was loaded

    ; Save the value of sp
    mov r0, sp
    ; Save the value of all registers
    push {r0-r12}

    ldr r0, [r0, #(0x80 - 0x7C)] ; Load the .text address
    ldr r7, [r4]
    ldr r8, [r7, #0x200]         ; Load the low title id of the current NCCH
    ldr r2, [r7, #0x18]
    mov r5, r0
    add r11, r5, r2              ; Max bounds of the memory region

    ldr r9, =0x00001002  ; Low title id of the sm module
    cmp r8, r9           ; Compare the low title id to the id of the sm module
    bne fs_patch         ; Skip if they're not the same

    ldr r7, =0xE5901024   ; mov r6, r2
    ldr r8, =0xE1B02001   ; mov r7, #0
    ldr r9, =0x0A00000A   ; ldr r1, [r2, #0x24]
    ldr r10, =0xE5915014  ; movs r2, r1
    
    loop_sm: ; patch adapted from BootNTR
        cmp r5, r11
        bhs out
        ldr r6, [r5]
        cmp r6, r7
        bne loop_sm_continue
        ldr r6, [r5, #4]
        cmp r6, r8
        bne loop_sm_continue
        ldr r6, [r5, #8]
        cmp r6, r9
        bne loop_sm_continue
        ldr r6, [r5, #12]
        cmp r6, r10
        bne loop_sm_continue
            ldr r9, =0xE3A00002   ; mov r0, #2
            ldr r10, =0xE12FFF1E  ; bx lr
            str r9, [r5, #-8]
            str r10, [r5, #-4]
            b out
        loop_sm_continue:
        add r5, r5, #4
        b loop_sm


    fs_patch: ; patch adapted from BootNTR
    ldr r9, =0x00001102  ; Low title id of the fs module
    cmp r8, r9           ; Compare the low title id to the id of the fs module
    bne out              ; Skip if they're not the same

    ldr r7, =0x4618     ; mov r0, r3
    ldr r8, =0x3481     ; add r4, #0x81
    loop_fs:
        cmp r5, r11
        bhs out
        ldrh r6, [r5]
        cmp r6, r7
        bne loop_fs_continue
        ldrh r6, [r5, #2]
        cmp r6, r8
        bne loop_fs_continue
            ldr r9, =0x2001     ; mov r0, #1
            ldr r10, =0x4770    ; bx lr
            strh r9, [r5, #-8]
            strh r10, [r5, #-6]
            b out
        loop_fs_continue:
        add r5, #2
        b loop_fs
    
    out:
    pop {r0-r12}               ; Restore the registers we used

    ldr r0, [r4]               ; Execute the instruction we overwrote in our detour

    bx lr                      ; Jump back to whoever called us

.pool
.close