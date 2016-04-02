.section .text.start
.align 4
.global _start
_start:
    @ Fix payloads like Decrypt9
    mov r0, #0x5
    mcr p15, 0, r0, c3, c0, 0  @ data bufferable

    @ Flush caches
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0  @ flush I-cache
    mcr p15, 0, r0, c7, c6, 0  @ flush D-cache
    mcr p15, 0, r0, c7, c10, 4 @ drain write buffer

    b main
