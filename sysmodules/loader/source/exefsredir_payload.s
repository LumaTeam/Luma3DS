@ HOME Menu ExeFS overrides. Called in place of a BL to svcSendSyncRequest
@ inside FSUSER_OpenFileDirectly; r0 is the FS session, lr is the return site.
@ All scratch storage is per-call, including paths used by synchronous IPC.

.section .rodata.exefsRedirPatch, "a", %progbits
.arm
.balign 4
.global exefsRedirPatch
exefsRedirPatch:
    push    {r0-r12, lr}
    mrc     p15, 0, r4, c13, c0, 3
    add     r4, r4, #0x80
    ldr     r1, [r4]
    ldr     r2, =0x08030204
    cmp     r1, r2
    bne     original
    ldr     r1, [r4, #4]
    cmp     r1, #0                  @ No transaction
    bne     original
    ldr     r1, [r4, #8]
    ldr     r2, =0x2345678A         @ Title content archive
    cmp     r1, r2
    bne     original
    ldr     r1, [r4, #12]
    cmp     r1, #2                  @ PATH_BINARY
    ldreq   r1, [r4, #16]
    cmpeq   r1, #16
    ldreq   r1, [r4, #20]
    cmpeq   r1, #2
    ldreq   r1, [r4, #24]
    cmpeq   r1, #20
    ldreq   r1, [r4, #28]
    cmpeq   r1, #1                  @ Read only
    ldreq   r1, [r4, #32]
    cmpeq   r1, #0                  @ No attributes
    bne     original
    ldr     r1, [r4, #36]
    ldr     r2, =0x40802            @ Archive static buffer: 16 bytes, id 2
    cmp     r1, r2
    ldreq   r1, [r4, #44]
    ldreq   r2, =0x50002            @ File static buffer: 20 bytes, id 0
    cmpeq   r1, r2
    bne     original
    ldr     r7, [r4, #40]
    ldr     r6, [r4, #48]
    cmp     r7, #0
    cmpne   r6, #0
    beq     original
    orr     r1, r6, r7
    tst     r1, #3
    bne     original
    ldr     r1, [r7, #4]
    bic     r1, r1, #2              @ CTR applications and demos only
    ldr     r2, =0x00040000
    cmp     r1, r2
    bne     original
    ldr     r1, [r7, #8]
    cmp     r1, #2                  @ NAND, SD, or gamecard
    bhi     original
    ldr     r1, [r7, #12]
    cmp     r1, #0
    ldreq   r1, [r6]
    cmpeq   r1, #0                  @ High-level NCCH access
    ldreq   r1, [r6, #4]
    cmpeq   r1, #0                  @ Main content only
    ldreq   r1, [r6, #8]
    cmpeq   r1, #2                  @ System Menu Data
    bne     original
    ldr     r1, [r6, #12]
    ldr     r2, =0x6E6F6369         @ icon\0\0\0\0
    cmp     r1, r2
    bne     checkBanner
    ldr     r1, [r6, #16]
    cmp     r1, #0
    bne     original
    b       redirect
checkBanner:
    ldr     r2, =0x6E6E6162         @ banner\0\0
    cmp     r1, r2
    ldreq   r1, [r6, #16]
    ldreq   r2, =0x00007265
    cmpeq   r1, r2
    bne     original

redirect:
    mov     r5, r0
    sub     sp, sp, #104            @ 52-byte request, padding, 44-byte path
    mov     r1, #0
saveRequest:
    ldr     r2, [r4, r1]
    str     r2, [sp, r1]
    add     r1, r1, #4
    cmp     r1, #52
    bne     saveRequest
    add     r8, sp, #56
    adr     r2, pathPrefix
    mov     r1, #0
copyPrefix:
    ldrb    r3, [r2, r1]
    strb    r3, [r8, r1]
    add     r1, r1, #1
    cmp     r1, #13
    bne     copyPrefix
    add     r8, r8, #13
    ldr     r2, [r7, #4]
    bl      hexWord
    ldr     r2, [r7]
    bl      hexWord
    adr     r2, pathSuffix
    mov     r1, #0
copySuffix:
    ldrb    r3, [r2, r1]
    strb    r3, [r8, r1]
    add     r1, r1, #1
    cmp     r1, #7
    bne     copySuffix
    add     r8, r8, #7
    add     r6, r6, #12
copyName:
    ldrb    r3, [r6], #1
    strb    r3, [r8], #1
    cmp     r3, #0
    bne     copyName

    mov     r1, #9                  @ ARCHIVE_SDMC
    str     r1, [r4, #8]
    mov     r1, #1                  @ PATH_EMPTY
    str     r1, [r4, #12]
    str     r1, [r4, #16]
    mov     r1, #3                  @ PATH_ASCII
    str     r1, [r4, #20]
    add     r2, sp, #56
    sub     r1, r8, r2              @ Includes terminator
    str     r1, [r4, #24]
    str     r2, [r4, #48]
    mov     r1, r1, lsl #14
    orr     r1, r1, #2
    str     r1, [r4, #44]
    ldr     r1, =0x4802             @ Empty archive static buffer
    str     r1, [r4, #36]
    sub     r1, r8, #1              @ Point at the path's NUL byte
    str     r1, [r4, #40]
    mov     r0, r5
    svc     0x32
    cmp     r0, #0
    blt     fallback
    ldr     r1, [r4, #4]
    cmp     r1, #0
    bge     redirected
fallback:
    mov     r1, #0
restoreRequest:
    ldr     r2, [sp, r1]
    str     r2, [r4, r1]
    add     r1, r1, #4
    cmp     r1, #52
    bne     restoreRequest
    add     sp, sp, #104
original:
    pop     {r0-r12, lr}
    svc     0x32
    bx      lr
redirected:
    add     sp, sp, #108            @ Discard scratch and saved r0
    pop     {r1-r12, lr}
    bx      lr

hexWord:
    mov     r9, #8
hexDigit:
    mov     r3, r2, lsr #28
    cmp     r3, #10
    addlo   r3, r3, #'0'
    addhs   r3, r3, #('A' - 10)
    strb    r3, [r8], #1
    mov     r2, r2, lsl #4
    subs    r9, r9, #1
    bne     hexDigit
    bx      lr

.pool
pathPrefix: .ascii "/luma/titles/"
.balign 4
pathSuffix: .ascii "/exefs/"
.balign 4
.global exefsRedirPatchEnd
exefsRedirPatchEnd:
.global exefsRedirPatchSize
exefsRedirPatchSize:
    .word exefsRedirPatchEnd - exefsRedirPatch
