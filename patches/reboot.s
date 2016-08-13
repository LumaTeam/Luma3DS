.arm.little

payload_addr equ 0x23F00000   ; Brahma payload address.
payload_maxsize equ 0x10000   ; Maximum size for the payload (maximum that CakeBrah supports).

.create "build/reboot.bin", 0
.arm
    ; Interesting registers and locations to keep in mind, set before this code is ran:
    ; - sp + 0x3A8 - 0x70: FIRM path in exefs.
    ; - r7 (which is sp + 0x3A8 - 0x198): Reserved space for file handle
    ; - *(sp + 0x3A8 - 0x198) + 0x28: fread function.

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

        mov r4, #0
        adr r1, bin_fname
        b open_payload

    fallback:
        mov r4, #1
        adr r1, dat_fname

    open_payload:
        ; Open file
        add r0, r7, #8
        mov r2, #1
        ldr r6, [fopen]
        orr r6, 1
        blx r6
        cmp r0, #0
        bne fallback ; If the .bin is not found, try the .dat.

    read_payload:
        ; Read file
        mov r0, r7
        adr r1, bytes_read
        ldr r2, =payload_addr
        cmp r4, #0
        movne r3, #0x12000 ; Skip the first 0x12000 bytes.
        moveq r3, payload_maxsize
        ldr r6, [sp, #0x3A8-0x198]
        ldr r6, [r6, #0x28]
        blx r6
        cmp r4, #0
        movne r4, #0
        bne read_payload ; Go read the real payload.

    ; Copy the low TID (in UTF-16) of the wanted firm to the 5th byte of the payload
    add r0, sp, #0x3A8 - 0x70
    add r0, 0x1A
    add r1, r0, #0x10
    ldr r2, =payload_addr + 4
    copy_TID_low:
        ldrh r3, [r0], #2
        strh r3, [r2], #2
        cmp r0, r1
        blo copy_TID_low

    ; Set kernel state
    mov r0, #0
    mov r1, #0
    mov r2, #0
    mov r3, #0
    swi 0x7C

    goto_reboot:
        ; Jump to reboot code
        ldr r0, =(kernelcode_start - goto_reboot - 12)
        add r0, pc
        swi 0x7B

    die:
        b die

bytes_read: .word 0
fopen: .ascii "OPEN"
.pool
bin_fname:  .dcw "sdmc:/arm9loaderhax.bin"
            .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
dat_fname: .dcw "sdmc:/Luma3DS.dat"
           .word 0

.align 4
    kernelcode_start:

    ; Disable MPU
    ldr r0, =0x42078  ; alt vector select, enable itcm
    mcr p15, 0, r0, c1, c0, 0

    ; Clean and flush data cache
    mov r1, #0                          ; segment counter
    outer_loop:
        mov r0, #0                      ; line counter

        inner_loop:
            orr r2, r1, r0                  ; generate segment and line address
            mcr p15, 0, r2, c7, c14, 2      ; clean and flush the line
            add r0, #0x20                   ; increment to next line
            cmp r0, #0x400
            bne inner_loop

        add r1, #0x40000000
        cmp r1, #0
        bne outer_loop

    mcr p15, 0, r1, c7, c10, 4              ; drain write buffer

    ; Flush instruction cache
    mcr p15, 0, r1, c7, c5, 0

    ; Jump to payload
    ldr r0, =payload_addr
    bx r0

.pool
.close
