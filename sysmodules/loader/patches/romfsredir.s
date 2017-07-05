.arm.little
.create "build/romfsredir.bin", 0

.macro addr, reg, func
    add reg, pc, #func-.-8
.endmacro
.macro load, reg, func
    ldr reg, [pc, #func-.-8]
.endmacro

; Patch by delebile

.arm
_start:

    ; Jumps here before the fsOpenFileDirectly call
    _mountArchive:
        b       mountArchive
        .word   0xdead0000  ; Substituted opcode
        .word   0xdead0001  ; Branch to hooked function

    ; Jumps here before every iFileOpen call
    _fsRedir:
        b       fsRedir
        .word   0xdead0002  ; Substituted opcode
        .word   0xdead0003  ; Branch to hooked function

    ; Mounts the archive and registers it as 'lf:'
    mountArchive:
        cmp     r3, #3
        bne     _mountArchive + 4
        stmfd   sp!, {r0-r4, lr}
        sub     sp, sp, #4
        load    r1, archiveId
        mov     r0, sp
        load    r4, fsMountArchive
        blx     r4
        mov     r3, #0
        mov     r2, #0
        ldr     r1, [sp]
        addr    r0, archiveName
        load    r4, fsRegisterArchive
        blx     r4
        add     sp, sp, #4
        ldmfd   sp!, {r0-r4, lr}
        b       _mountArchive + 4

    ; Check the path passed to iFileOpen.
    ; If it is trying to access a RomFS file, we try to
    ; open it from the LayeredFS folder.
    ; If the file cannot be opened, we just open
    ; it from its original archive like nothing happened
    fsRedir:
        stmfd   sp!, {r0-r12, lr}
        addr    r3, romFsMount
        bl      compare
        addne   r3, pc, #updateRomFsMount-.-8
        blne    compare
        bne     endRedir
        sub     sp, sp, #0x400
        pathRedir:
            stmfd   sp!, {r0-r3}
            add     r0, sp, #0x10
            load    r3, customPath
            pathRedir_1:
                ldrb    r2, [r3], #1
                cmp     r2, #0
                strneh  r2, [r0], #2
                bne     pathRedir_1
            pathRedir_2:
                ldrh    r2, [r1], #2
                cmp     r2, #0x3A ; ':'
                bne     pathRedir_2
            ; Skip a slash if there are two after the mountpoint,
            ; as some games mistakenly have those
            ldrh    r3, [r1, #2]
            cmp     r3, #0x2F ; '/'
            pathRedir_3:
                ldrh    r2, [r1], #2
                strneh  r2, [r0], #2
                cmp     r2, #0
                bne     pathRedir_3
            ldmfd   sp!, {r0-r3}
        mov     r1, sp
        bl      _fsRedir + 4
        add     sp, sp, #0x400
        cmp     r0, #0

    endRedir:
        ldmfd   sp!, {r0-r12, lr}
        moveq   r0, #0
        bxeq    lr
        b       _fsRedir + 4

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
.align 4
    archiveName       : .dcb "lf:", 0
    fsMountArchive    : .word 0xdead0005
    fsRegisterArchive : .word 0xdead0006
    archiveId         : .word 0xdead0007
    romFsMount        : .dcb "rom:"
    updateRomFsMount  : .word 0xdead0008
    customPath        : .word 0xdead0004

.close
