.section .emunand_patch, "aw", %progbits
.arm
.align 4

@ Code originally by Normmatt

.global emunandPatch
emunandPatch:
    @ Original code that still needs to be executed
    mov r4, r0
    mov r5, r1
    mov r7, r2
    mov r6, r3
    @ End

    @ If we're already trying to access the SD, return
    ldr r2, [r0, #4]
    ldr r1, emunandPatchSdmmcStructPtr
    cmp r2, r1
    beq out

    str r1, [r0, #4] @ Set object to be SD
    ldr r2, [r0, #8] @ Get sector to read
    cmp r2, #0 @ For GW compatibility, see if we're trying to read the ncsd header (sector 0)

    ldr r3, emunandPatchNandOffset
    add r2, r3 @ Add the offset to the NAND in the SD

    ldreq r3, emunandPatchNcsdHeaderOffset
    addeq r2, r3 @ If we're reading the ncsd header, add the offset of that sector

    str r2, [r0, #8] @ Store sector to read

    out:
        @ Restore registers.
        mov r1, r5
        mov r2, r7
        mov r3, r6

        @ Return 4 bytes behind where we got called,
        @ due to the offset of this function being stored there
        mov r0, lr
        add r0, #4
        bx r0

.pool

.global emunandPatchSdmmcStructPtr
.global emunandPatchNandOffset
.global emunandPatchNcsdHeaderOffset

emunandPatchSdmmcStructPtr:     .word   0 @ Pointer to sdmmc struct
emunandPatchNandOffset:         .word   0 @ For rednand this should be 1
emunandPatchNcsdHeaderOffset:   .word   0 @ Depends on nand manufacturer + emunand type (GW/RED)

.pool
.balign 4

_emunandPatchEnd:

.global emunandPatchSize
emunandPatchSize:
    .word _emunandPatchEnd - emunandPatch
