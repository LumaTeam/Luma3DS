; Code originally from delebile and mid-kid

.arm.little

copy_launch_stub_stack_top      equ 0x01FFB800
copy_launch_stub_stack_bottom   equ 0x01FFA800
copy_launch_stub_addr           equ 0x01FF9000

argv_addr                       equ (copy_launch_stub_stack_bottom - 0x100)
fname_addr                      equ (copy_launch_stub_stack_bottom - 0x200)
low_tid_addr                    equ (copy_launch_stub_stack_bottom - 0x300)

firm_addr                       equ 0x20001000
firm_maxsize                    equ 0x07FFF000

.create "build/reboot.bin", 0
.arm
    ; Interesting registers and locations to keep in mind, set just before this code is ran:
    ; - r1: FIRM path in exefs.
    ; - r7 (or r8): pointer to file object
    ;   - *r7: vtable
    ;       - *(vtable + 0x28): fread function
    ;   - *(r7 + 8): file handle

    sub r7, r0, #8
    mov r8, r1

    pxi_wait_recv:
        ldr r2, =0x44846
        ldr r0, =0x10008000
        readPxiLoop1:
            ldrh r1, [r0, #4]
            lsls r1, #0x17
            bmi readPxiLoop1
            ldr r0, [r0, #0xC]
        cmp r0, r2
        bne pxi_wait_recv

    ; Open file
    add r0, r7, #8
    adr r1, fname
    mov r2, #1
    ldr r6, [fopen]
    orr r6, 1
    blx r6
    cmp r0, #0
    bne panic

    ; Read file
    mov r0, r7
    adr r1, bytes_read
    ldr r2, =firm_addr
    ldr r3, =firm_maxsize
    ldr r6, [r7]
    ldr r6, [r6, #0x28]
    blx r6

    ; Copy the low TID (in UTF-16) of the wanted firm
    ldr r0, =low_tid_addr
    add r1, r8, #0x1A
    mov r2, #0x10
    bl memcpy16

    ; Copy argv[0]
    ldr r0, =fname_addr
    adr r1, fname
    mov r2, #82
    bl memcpy16

    ldr r0, =argv_addr
    ldr r1, =fname_addr
    ldr r2, =low_tid_addr
    stmia r0, {r1, r2}

    ; Set kernel state
    mov r0, #0
    mov r1, #0
    mov r2, #0
    mov r3, #0
    swi 0x7C

    goto_reboot:
        ; Jump to reboot code
        ldr r0, =(kernelcode_start - goto_reboot - 12)
        add r0, pc ; pc is two instructions ahead of the instruction being executed (12 = 2*4 + 4)
        swi 0x7B

    die:
        b die

    memcpy16:
        cmp r2, #0
        bxeq lr
        add r2, r0, r2
        copy_loop16:
            ldrh r3, [r1], #2
            strh r3, [r0], #2
            cmp r0, r2
            blo copy_loop16
        bx lr

    panic:
        mov r1, r0 ; unused register
        mov r0, #0
        swi 0x3C ; svcBreak(USERBREAK_PANIC)
        b die

bytes_read: .word 0
fopen: .ascii "OPEN"
.pool

.area 82, 0
fname: .ascii "FILE"
.endarea

.align 4
    kernelcode_start:

    mrs r0, cpsr  ; disable interrupts
    orr r0, #0xC0
    msr cpsr, r0

    ldr sp, =copy_launch_stub_stack_top

    ldr r0, =copy_launch_stub_addr
    adr r1, copy_launch_stub
    mov r2, #(copy_launch_stub_end - copy_launch_stub)
    bl memcpy32

    ; Disable MPU
    ldr r0, =0x42078 ; alt vector select, enable itcm
    mcr p15, 0, r0, c1, c0, 0

    bl flushCaches

    ldr r0, =copy_launch_stub_addr
    bx r0

    copy_launch_stub:

    ldr r4, =firm_addr

    mov r5, #0
    load_section_loop:
        ; Such checks. Very ghetto. Wow.
        add r3, r4, #0x40
        add r3, r5,lsl #5
        add r3, r5,lsl #4
        ldmia r3, {r6-r8}
        cmp r8, #0
        movne r0, r7
        addne r1, r4, r6
        movne r2, r8
        blne memcpy32
        add r5, #1
        cmp r5, #4
        blo load_section_loop

    mov r0, #2 ; argc
    ldr r1, =argv_addr ; argv
    ldr r2, =0xBABE    ; magic word

    mov r5, #0x20000000
    ldr r6, [r4, #0x08]
    str r6, [r5, #-4]   ; store arm11 entrypoint

    ldr lr, [r4, #0x0c]
    bx lr

    memcpy32:
    add r2, r0, r2
    copy_loop32:
        ldr r3, [r1], #4
        str r3, [r0], #4
        cmp r0, r2
        blo copy_loop32
    bx lr

    .pool

    copy_launch_stub_end:

    flushCaches:

    ; Clean and flush data cache
    mov r1, #0 ; segment counter
    outer_loop:
        mov r0, #0 ; line counter

        inner_loop:
            orr r2, r1, r0 ; generate segment and line address
            mcr p15, 0, r2, c7, c14, 2 ; clean and flush the line
            add r0, #0x20 ; increment to next line
            cmp r0, #0x400
            bne inner_loop

        add r1, #0x40000000
        cmp r1, #0
        bne outer_loop

    ; Drain write buffer
    mcr p15, 0, r1, c7, c10, 4

    ; Flush instruction cache
    mcr p15, 0, r1, c7, c5, 0

    bx lr

.close
