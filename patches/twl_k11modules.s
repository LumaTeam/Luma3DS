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

.arm.little

.create "build/twl_k11modules.bin", 0

.align 4
.arm

patch:
    ; r4: Pointer to a pointer to the exheader of the current NCCH
    ; sp + 0xb0 - 0xa4: Pointer to the memory location where the NCCH text was loaded

    add r3, sp, #(0xb0 - 0xa4)
    add r1, sp, #(0xb0 - 0xac)

    push {r0-r11, lr}
    
    ldr r9, [r3]            ; load the address of the code section
    ldr r8, [r4]            ; load the address of the exheader

    ldr r7, [r8, #0x200]    ; low titleID
    ldr r6, =#0x000001ff
    cmp r7, r6
    bne end

    ldr r7, [launcher]      ; offset of the dev launcher (will be replaced later)
    add r7, r9

    adr r5, patchesStart
    add r6, r5, #(patchesEnd - patchesStart)
    
    patchLoop:
        ldrh r0, [r5, #4]
        cmp r0, #0
        moveq r4, r9
        movne r4, r7

        ldrh r2, [r5, #6]
        add r1, r5, #8
        ldr r0, [r5]
        add r0, r4
        blx memcmp
        cmp r0, #0
        bne skipPatch

        ldrh r2, [r5, #6]
        add r1, r5, #0x08
        add r1, r2
        ldr r0, [r5]
        add r0, r4
        blx memcpy
        
        skipPatch:

        ldrh r0, [r5, #6]
        add r5, r5, #0x08
        add r5, r0,lsl#1
        cmp r5, r6
        blo patchLoop

    end:

    pop {r0-r11, pc}

launcher: .ascii "LAUN"  

.align 2
.thumb

memcmp:
    push {r4-r7, lr}
    mov r4, #0
    cmp_loop:
        cmp r4, r2
        bhs cmp_loop_end
        ldrb r6, [r0, r4]
        ldrb r7, [r1, r4]
        add r4, #1
        sub r6, r7
        cmp r6, #0
        beq cmp_loop
    
    cmp_loop_end:
    mov r0, r6
    pop {r4-r7, pc}

memcpy:
    push {r4-r5, lr}
    mov r4, #0

    copy_loop:
        cmp r4, r2
        bhs copy_loop_end
        ldrb r5, [r1, r4]
        strb r5, [r0, r4]
        add r4, #1
        b copy_loop

    copy_loop_end:
    pop {r4-r5, pc}

.align 4

; Available space for patches: 152 bytes on N3DS, 666 on O3DS

patchesStart:
    ; SCFG_EXT bit31 patches, based on https://github.com/ahezard/twl_firm_patcher (credits where they're due)
    
    .word 0x07368                 ; offset
    .halfword 1                   ; type (0: relative to the start of TwlBg's code; 1: relative to the start of the dev SRL launcher)
    .halfword 4                   ; size (must be a multiple of 4)
    .byte 0x94, 0x09, 0xfc, 0xed  ; expected data (decrypted = 0x08, 0x60, 0x87, 0x05)
    .byte 0x24, 0x09, 0xbc, 0xe9  ; patched data (decrypted = 0xb8, 0x60, 0xc7, 0x01)

    .word 0xa5888
    .halfword 1
    .halfword 8
    .byte 0x83, 0x30, 0x2e, 0xa4, 0xb0, 0xe2, 0xc2, 0xd6 ; (decrypted = 0x02, 0x01, 0x1a, 0xe3, 0x08, 0x60, 0x87, 0x05)
    .byte 0x83, 0x50, 0xf2, 0xa4, 0xb0, 0xe2, 0xc2, 0xd6 ; (decrypted = 0x02, 0x61, 0xc6, 0xe3, 0x08, 0x60, 0x87, 0xe5)

patchesEnd:

.pool

.close