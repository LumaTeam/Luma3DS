; Code from delebile

.arm.little
.create "build/romfsredir.bin", 0

.macro load, reg, func
    ldr reg, [pc, #func-.-8]
.endmacro

.arm
    ; fsOpenFileDirectly function will be redirected here.
    ; If the requested archive is not ROMFS, we'll return
    ; to the original function.
    openFileDirectlyHook:
        cmp r3, #3
        beq openRomfs
        load r12, fsOpenFileDirectly
        nop ; Will be replaced with the original function opcode
        bx r12

    ; We redirect ROMFS file opening by changing the parameters and call
    ; the fsOpenFileDirectly function recursively. The parameter format:
    ; r0          : fsUserHandle
    ; r1          : Output FileHandle
    ; r2          : Transaction (usually 0)
    ; r3          : Archive ID
    ; [sp, #0x00] : Archive PathType
    ; [sp, #0x04] : Archive DataPointer
    ; [sp, #0x08] : Archive PathSize
    ; [sp, #0x0C] : File PathType
    ; [sp, #0x10] : File DataPointer
    ; [sp, #0x14] : File PathSize
    ; [sp, #0x18] : File OpenFlags
    ; [sp, #0x1C] : Attributes (usually 0)
    openRomfs:
        sub sp, sp, #0x50
        stmfd sp!, {r0, r1, lr}
        add sp, sp, #0x5C
        str r3, [sp, #0x0C] ; File PathType (ASCII = 3)
        load r12, romfsFileName
        str r12, [sp, #0x10] ; File DataPointer
        load r12, romfsFileNameSize
        str r12, [sp, #0x14] ; File PathSize
        load r3, archive
        bl openFileDirectlyHook
        sub sp, sp, #0x5C
        ldmfd sp!, {r0, r1, lr}
        add sp, sp, #0x50
        mov r0, r1 ; Substitute fsUserHandle with the fileHandle

    ; Once we have the sd romfs file opened, we'll open a subfile
    ; in order to skip the useless data.
        stmfd sp!, {r1, r3-r11}
        mrc p15, 0, r4, c13, c0, 3
        add r4, r4, #0x80
        mov r1, r4
        add r3, pc, #fsOpenSubFileCmd-.-8
        ldmia r3!, {r5-r9}
        stmia r1!, {r5-r9}
        ldr r0, [r0]
        swi 0x32
        ldr r0, [r4, #0x0C]
        ldmfd sp!, {r1, r3-r11}
        str r0, [r1]
        mov r0, #0
        bx lr

.pool
.align 4
; Part of these symbols will be set from outside
    fsOpenFileDirectly : .word 0
    fsOpenSubFileCmd   : .word 0x08010100
                         .word 0 ; File Offset
                         .word 0
                         .word 0 ; File Size
                         .word 0
    archive            : .word 0
    romfsFileNameSize  : .word 0
    romfsFileName      : .word 0 ; File DataPointer
.close