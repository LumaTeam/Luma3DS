// Patch by delebile

.section .data.romfsRedirPatch, "aw", %progbits
.align 4

.global romfsRedirMountArchive
.type   romfsRedirMountArchive, %function
romfsRedirMountArchive:
    b       mountArchive

.global romfsRedirFileOpen
.type   romfsRedirFileOpen, %function
romfsRedirFileOpen:
    b       fsRedir

.global romfsRedirOriginalMountArchive
romfsRedirOriginalMountArchive:
    .skip 4*4
.global romfsRedirOriginalFileOpen
romfsRedirOriginalFileOpen:
    .skip 4*4

mountArchive:
    push    {r4-r12, lr}
    sub     sp, sp, #4
    adr     r4, romfsRedirContext
    str     r4, [sp]
    add     sp, sp, #4
    bl      doRomfsRedirMountArchive
    pop     {r4-r12, pc}

fsRedir:
    push    {r4-r12, lr}
    sub     sp, sp, #4
    adr     r3, romfsRedirContext
    bl      doRomfsRedirFsRedir
    pop     {r4-r12, pc}

.pool

.align      5
.global     romfsRedirContext
romfsRedirContext:
    .skip   0x400 // lots of space just in case

/*

romfsRedirPatchArchiveName       : .ascii "lf:\0"
romfsRedirPatchFsMountArchive    : .word 0xdead0005
romfsRedirPatchFsRegisterArchive : .word 0xdead0006
romfsRedirPatchArchiveId         : .word 0xdead0007
romfsRedirPatchRomFsMount        : .ascii "rom:"
romfsRedirPatchUpdateRomFsMount  : .word 0xdead0008
romfsRedirPatchCustomPath        : .word 0xdead0004
*/

// Mounts the archive and registers it as 'lf:'
mountArchive:
    cmp     r3, #3
    bne     romfsRedirPatchSubstituted1
    push    {r0-r4, lr}
    sub     sp, sp, #8
    ldr     r1, romfsRedirPatchArchiveId
    str     r1, [sp, #4]
    add     r1, sp, #4
    mov     r0, sp
    ldr     r4, romfsRedirPatchFsMountArchive
    blx     r4
    mov     r3, #0
    mov     r2, #0
    ldr     r1, [sp]
    ldr     r0, romfsRedirPatchArchiveName
    str     r0, [sp, #4]
    add     r0, sp, #4
    ldr     r4, romfsRedirPatchFsRegisterArchive
    blx     r4
    add     sp, sp, #8
    pop     {r0-r4, lr}
    b       romfsRedirPatchSubstituted1

// Check the path passed to iFileOpen.
// If it is trying to access a RomFS file, we try to
// open it from the LayeredFS folder.
// If the file cannot be opened, we just open
// it from its original archive like nothing happened
fsRedir:
    push    {r0-r12, lr}
    adr     r3, romfsRedirPatchRomFsMount
    bl      compare
    adrne   r3, romfsRedirPatchUpdateRomFsMount
    blne    compare
    bne     endRedir
    sub     sp, sp, #0x400
    pathRedir:
        push    {r0-r3}
        add     r0, sp, #0x10
        ldr     r3, romfsRedirPatchCustomPath
        pathRedir_1:
            ldrb    r2, [r3], #1
            cmp     r2, #0
            strneh  r2, [r0], #2
            bne     pathRedir_1
        pathRedir_2:
            ldrh    r2, [r1], #2
            cmp     r2, #0x3A // ':'
            bne     pathRedir_2
        // Skip a slash if there are two after the mountpoint,
        // as some games mistakenly have those
        ldrh    r3, [r1, #2]
        cmp     r3, #0x2F // '/'
        pathRedir_3:
            ldrh    r2, [r1], #2
            strneh  r2, [r0], #2
            cmp     r2, #0
            bne     pathRedir_3
        pop     {r0-r3}
    mov     r1, sp
    bl      romfsRedirPatchSubstituted2
    add     sp, sp, #0x400
    cmp     r0, #0

endRedir:
    pop     {r0-r12, lr}
    moveq   r0, #0
    bxeq    lr
    b       romfsRedirPatchSubstituted2

compare:
    mov     r9, r1
    add     r10, r3, #4
    loop:
        ldrb    r12, [r3], #1
        ldrb    r11, [r9], #2
        cmp     r11, r12
        bxne    lr
        cmp     r10, r3
        bne     loop
    bx lr

.pool
.balign 4

.global romfsRedirPatchArchiveName
.global romfsRedirPatchFsMountArchive
.global romfsRedirPatchFsRegisterArchive
.global romfsRedirPatchArchiveId
.global romfsRedirPatchRomFsMount
.global romfsRedirPatchUpdateRomFsMount
.global romfsRedirPatchCustomPath



_romfsRedirPatchEnd:

.global romfsRedirPatchSize
romfsRedirPatchSize:
    .word _romfsRedirPatchEnd - romfsRedirPatch
