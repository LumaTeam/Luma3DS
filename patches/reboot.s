; Code originally from delebile and mid-kid

.arm.little

payload_addr equ 0x23F00000   ; Brahma payload address
payload_maxsize equ 0x100000  ; Maximum size for the payload (maximum that CakeBrah supports)

.create "build/reboot.bin", 0
.arm
    ; Interesting registers and locations to keep in mind, set just before this code is ran:
    ; - r1: FIRM path in exefs.
    ; - r7: pointer to file object
    ;   - *r7: vtable
    ;       - *(vtable + 0x28): fread function 
    ;   - *(r7 + 8): file handle

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

    mov r4, #2

    open_payload:
        ; Open file
        add r0, r7, #8
        adr r1, fname
        mov r2, #1
        ldr r6, [fopen]
        orr r6, 1
        blx r6
        cmp r0, #0
        beq read_payload
        subs r4, r4, #1
        beq panic
        adr r0, fname
        adr r1, nand_mount
        mov r2, #8
        bl memcpy16
        b open_payload

    read_payload:
        ; Read file
        mov r0, r7
        adr r1, bytes_read
        ldr r2, =payload_addr
        ldr r3, =payload_maxsize
        ldr r6, [r7]
        ldr r6, [r6, #0x28]
        blx r6

    ; Copy the low TID (in UTF-16) of the wanted firm to the 5th byte of the payload
    ldr r0, =payload_addr + 4
    add r1, r8, #0x1A
    mov r2, #0x10
    bl memcpy16

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
        add r2, r0, r2
        copy_loop:
            ldrh r3, [r1], #2
            strh r3, [r0], #2
            cmp r0, r2
            blo copy_loop
        bx lr

    panic:
        mov r1, r0 ; unused register
        mov r0, #0
        swi 0x3C ; svcBreak(USERBREAK_PANIC)
        b die

bytes_read: .word 0
fopen: .ascii "OPEN"
.pool
fname: .dcw "sdmc:/arm9loaderhax.bin"
       .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.pool
nand_mount: .dcw "nand"

.align 4
    kernelcode_start:

    ; Disable MPU
    ldr r0, =0x42078 ; alt vector select, enable itcm
    mcr p15, 0, r0, c1, c0, 0

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

    ; Jump to payload
    ldr r0, =payload_addr
    bx r0

.pool
.close