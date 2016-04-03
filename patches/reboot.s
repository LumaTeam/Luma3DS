.arm.little

firm_addr equ 0x24000000  ; Temporary location where we'll load the FIRM to
firm_maxsize equ 0x200000  ; Random value that's bigger than any of the currently known firm's sizes.
kernel_code equ 0x080F0000

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

    ; Convert 2 bytes of the path string
    ; This will be the method of getting the lower 2 bytes of the title ID
    ;   until someone bothers figuring out where the value is derived from.
    mov r0, #0  ; Result
    add r1, sp, #0x3A8 - 0x70
    add r1, #0x22  ; The significant bytes
    mov r2, #4  ; Maximum loops (amount of bytes * 2)

    hex_string_to_int_loop:
        ldr r3, [r1], #2  ; 2 because it's a utf-16 string.
        and r3, #0xFF

        ; Check if it"s a number
        cmp r3, #'0'
        blo hex_string_to_int_end
        sub r3, #'0'
        cmp r3, #9
        bls hex_string_to_int_calc

        ; Check if it"s a capital letter
        cmp r3, #'A' - '0'
        blo hex_string_to_int_end
        sub r3, #'A' - '0' - 0xA  ; Make the correct value: 0xF >= al >= 0xA
        cmp r3, #0xF
        bls hex_string_to_int_calc

        ; Incorrect value: x > "A"
        bhi hex_string_to_int_end

        hex_string_to_int_calc:
            orr r0, r3, r0, lsl #4
            subs r2, #1
            bne hex_string_to_int_loop
    hex_string_to_int_end:

    ; Get the FIRM path
    cmp r0, #0x0002  ; NATIVE_FIRM
    adreq r1, firm_fname
    beq load_firm
    ldr r5, =0x0102  ; TWL_FIRM
    cmp r0, r5
    adreq r1, twlfirm_fname
    beq load_firm
    ldr r5, =0x0202  ; AGB_FIRM
    cmp r0, r5
    adreq r1, agbfirm_fname
    beq load_firm
    bne fallback  ; TODO: Stubbed

    fallback:
        ; Fallback: Load specified FIRM from exefs
        add r1, sp, #0x3A8-0x70  ; Location of exefs string.
        b load_firm

    load_firm:
        ; Open file
        add r0, r7, #8
        mov r2, #1
        ldr r6, [fopen]
        orr r6, 1
        blx r6

        cmp r0, #0  ; Check if we were able to load the FIRM
        bne fallback  ; Otherwise, try again with the FIRM from exefs.
        ; This will loop indefinitely if the exefs FIRM fails to load, but whatever.

        ; Read file
		mov r0, r7
        adr r1, bytes_read
        mov r2, firm_addr
        mov r3, firm_maxsize
		ldr r6, [sp, #0x3A8-0x198]
		ldr r6, [r6, #0x28]
		blx r6

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
        ldr r1, =kernel_code
        ldr r2, =0x300
        bl memcpy32
        ldr r0, =kernel_code
        swi 0x7B

    die:
        b die

memcpy32:  ; memcpy32(void *src, void *dst, unsigned int size)
    add r2, r0
    memcpy32_loop:
        ldmia r0!, {r3}
        stmia r1!, {r3}
        cmp r0, r2
        blo memcpy32_loop
    bx lr

bytes_read: .word 0
fopen: .ascii "OPEN"
.pool
firm_fname: .dcw "sdmc:/aurei/patched_firmware_sys.bin"
	    .word 0x0
.pool
twlfirm_fname: .dcw "sdmc:/aurei/patched_firmware_twl.bin"
	    .word 0x0
.pool
agbfirm_fname: .dcw "sdmc:/aurei/patched_firmware_agb.bin"
	    .word 0x0

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

        ; Copy the firmware
        mov r4, firm_addr
        add r5, r4, #0x40  ; Start of loop
        add r6, r5, #0x30 * 3  ; End of loop (scan 4 entries)

    copy_firm_loop:
        ldr r0, [r5]
        cmp r0, #0
        addne r0, r4  ; src
        ldrne r1, [r5, #4]  ; dest
        ldrne r2, [r5, #8]  ; size
        blne kernelmemcpy32

        cmp r5, r6
        addlo r5, #0x30
        blo copy_firm_loop

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
    mov r0, firm_addr

    ; Boot FIRM
    mov r1, #0x1FFFFFFC
    ldr r2, [r0, #8]  ; arm11 entry
    str r2, [r1]
    ldr r0, [r0, #0xC]  ; arm9 entry
    bx r0
.pool

kernelmemcpy32:  ; memcpy32(void *src, void *dst, unsigned int size)
    add r2, r0
    kmemcpy32_loop:
        ldmia r0!, {r3}
        stmia r1!, {r3}
        cmp r0, r2
        blo kmemcpy32_loop
    bx lr
.close
