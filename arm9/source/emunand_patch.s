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

_emunandPatchEnd:

.global emunandProtoPatch
emunandProtoPatch:
    @ Save registers
    push {r0-r3}

    @ If we're already trying to access the SD, return
    ldr r2, [r0, #4]
    ldr r1, emunandPatchSdmmcStructPtr
    cmp r2, r1
    beq _out

    ldrb r2, [r1, #0xc] @ Get sdmc->m_isInitialised
    cmp r2, #0 @ Is initialised?
    beq _pastSdmc @ if not, use "NAND" object, patched elsewhere to access SD
    str r1, [r0, #4] @ Set object to be SD
    _pastSdmc:
    ldr r2, [r0, #8] @ Get sector to read
    cmp r2, #0 @ For GW compatibility, see if we're trying to read the ncsd header (sector 0)

    ldr r3, emunandPatchNandOffset
    add r2, r3 @ Add the offset to the NAND in the SD

    ldreq r3, emunandPatchNcsdHeaderOffset
    addeq r2, r3 @ If we're reading the ncsd header, add the offset of that sector

    str r2, [r0, #8] @ Store sector to read

    _out:
        @ Restore registers
        pop {r0-r3}
        @ Execute original code that got patched.
        cmp r1, #3
        mov r12, r2
        add r0, r0, #4
        movne r1, #0
        moveq r1, #1
        @ r2 about to be overwritten, so it's free to use here.
        @ Save off our return address and restore lr.
        mov r2, lr
        pop {lr}
        @ r2+0 is return address (patched movne r1, #0)
        @ r2+4 is moveq r1, #1
        @ r2+8 is the following instruction (mov r2, r3)
        add r2, #8
        bx r2

.global emunandProtoCidPatch
emunandProtoCidPatch:
    @ If we're already trying to access the SD, return
    ldr r4, emunandPatchSdmmcStructPtr
    cmp r0, r4
    beq _cid_return
    
    @ Trying to access nand, so copy the NAND cid into r1
    adr r4, emunandPatchNandCid
    ldr r2, [r4, #0]
    ldr r3, [r4, #4]
    ldr r5, [r4, #8]
    ldr r6, [r4, #0xc]
    str r2, [r1, #0]
    str r3, [r1, #4]
    str r5, [r1, #8]
    str r6, [r1, #0xc]
    @ And return from whence we came
    mov r0, #0
    pop {r4-r6, pc}
    
    _cid_return:
        @ Execute original code that got patched.
        mov r4, r0
        ldr r0, [r0]
        mov r5, r1
        @ lr+0 is return address (patched mov r5, r1)
        @ lr+4 is following instruction (ldr r1, [r0,#8])
        add lr, #4
        bx lr

.global emunandProtoPatch238
emunandProtoPatch238:
    @ Save registers
    push {r0-r3}

    @ If we're already trying to access the SD, return
    ldr r2, [r4, #4]
    ldr r1, emunandPatchSdmmcStructPtr
    cmp r2, r1
    beq _out238

    ldr r2, [r1, #0x24] @ Get sdmc->m_someObjInitedLater
    cmp r2, #0 @ Is initialised?
    beq _pastSdmc238 @ if not, use "NAND" object, patched elsewhere to access SD
    str r1, [r4, #4] @ Set object to be SD
    _pastSdmc238:

    ldr r2, [r4, #8] @ Get sector to read
    cmp r2, #0 @ For GW compatibility, see if we're trying to read the ncsd header (sector 0)

    ldr r3, emunandPatchNandOffset
    add r2, r3 @ Add the offset to the NAND in the SD

    ldreq r3, emunandPatchNcsdHeaderOffset
    addeq r2, r3 @ If we're reading the ncsd header, add the offset of that sector

    str r2, [r4, #8] @ Store sector to read

    _out238:
        @ Restore registers
        pop {r0-r3}
        @ Execute original code that got patched.
        cmp r0, #3
        movne r0, #0
        moveq r0, #1
        @ r1 about to be overwritten, so it's free to use here.
        @ Save off our return address.
        mov r1, lr
        @ r1+0 is return address (patched moveq r1, #1)
        @ r1+4 is tst r0, #0xff or sub sp, sp, #0xc
        add r1, #4
        bx r1

.pool

.global emunandPatchSdmmcStructPtr
.global emunandPatchNandOffset
.global emunandPatchNcsdHeaderOffset
.global emunandPatchNandCid

_emunandPatchBssStart:
emunandPatchSdmmcStructPtr:     .word   0 @ Pointer to sdmmc struct
emunandPatchNandOffset:         .word   0 @ For rednand this should be 1
emunandPatchNcsdHeaderOffset:   .word   0 @ Depends on nand manufacturer + emunand type (GW/RED)
emunandPatchNandCid:                      @ Store emmc cid here, to override "sdmc's" when trying to read emmc's
    .word 0,0,0,0 
_emunandPatchBssEnd:

.pool
.balign 4

.global emunandPatchSize
emunandPatchSize:
    .word _emunandPatchEnd - emunandPatch

.global emunandPatchBssSize
emunandPatchBssSize:
    .word _emunandPatchBssEnd - _emunandPatchBssStart