.arm.little

payload_addr equ 0x23F00000   ; Brahma payload address.
payload_maxsize equ 0x20000   ; Maximum size for the payload (200 KB will do).

.create "reboot.bin", 0
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
        movne r3, #0 ; Skip 0 bytes.
        moveq r3, payload_maxsize
	ldr r6, [sp, #0x3A8-0x198]
	ldr r6, [r6, #0x28]
	blx r6
        cmp r4, #0
        movne r4, #0
        bne read_payload ; Go read the real payload.

    ; Copy the last digits of the wanted firm to the 5th byte of the payload
    add r2, sp, #0x3A8 - 0x70
    ldr r0, [r2, #0x27]
    ldr r1, =payload_addr + 4
    str r0, [r1]
    ldr r0, [r2, #0x2B]
    str r0, [r1, #4]

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
bin_fname:     .dcw "sdmc:/homebrew/SaltFW.bin"
	       .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
dat_fname:     .dcw "sdmc:/homebrew/boot.bin"
	       .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

.align 4
    kernelcode_start:
        ; Set MPU settings
        mrc p15, 0, r0, c2, c0, 0  ; dcacheable
        mrc p15, 0, r12, c2, c0, 1  ; icacheable
        mrc p15, 0, r1, c3, c0, 0  ; write bufferable
        mrc p15, 0, r2, c5, c0, 2  ; daccess
        mrc p15, 0, r3, c5, c0, 3  ; iaccess
        ldr r4, =0x18000035  ; 0x18000000 128M
        bic r2, r2, #0xF0000  ; unprotect region 4
        bic r3, r3, #0xF0000  ; unprotect region 4
        orr r0, r0, #0x10  ; dcacheable region 4
        orr r2, r2, #0x30000  ; region 4 r/w
        orr r3, r3, #0x30000  ; region 4 r/w
        orr r12, r12, #0x10  ; icacheable region 4
        orr r1, r1, #0x10  ; write bufferable region 4
        mcr p15, 0, r0, c2, c0, 0
        mcr p15, 0, r12, c2, c0, 1
        mcr p15, 0, r1, c3, c0, 0  ; write bufferable
        mcr p15, 0, r2, c5, c0, 2  ; daccess
        mcr p15, 0, r3, c5, c0, 3  ; iaccess
        mcr p15, 0, r4, c6, c4, 0  ; region 4 (hmmm)

        mrc p15, 0, r0, c2, c0, 0  ; dcacheable
        mrc p15, 0, r1, c2, c0, 1  ; icacheable
        mrc p15, 0, r2, c3, c0, 0  ; write bufferable
        orr r0, r0, #0x20  ; dcacheable region 5
        orr r1, r1, #0x20  ; icacheable region 5
        orr r2, r2, #0x20  ; write bufferable region 5
        mcr p15, 0, r0, c2, c0, 0  ; dcacheable
        mcr p15, 0, r1, c2, c0, 1  ; icacheable
        mcr p15, 0, r2, c3, c0, 0  ; write bufferable

    ; Flush cache
    mov r2, #0
    mov r1, r2
    flush_cache:
        mov r0, #0
        mov r3, r2, lsl #30
        flush_cache_inner_loop:
            orr r12, r3, r0, lsl#5
            mcr p15, 0, r1, c7, c10, 4  ; drain write buffer
            mcr p15, 0, r12, c7, c14, 2  ; clean and flush dcache entry (index and segment)
            add r0, #1
            cmp r0, #0x20
            bcc flush_cache_inner_loop
        add r2, #1
        cmp r2, #4
        bcc flush_cache

    ; Enable MPU
    ldr r0, =0x42078  ; alt vector select, enable itcm
    mcr p15, 0, r0, c1, c0, 0
    mcr p15, 0, r1, c7, c5, 0  ; flush dcache
    mcr p15, 0, r1, c7, c6, 0  ; flush icache
    mcr p15, 0, r1, c7, c10, 4  ; drain write buffer

    ; Jump to payload
    ldr r0, =payload_addr
    bx r0

.pool
.close
