.section .text.start
.align 4
.global _start
_start:

    @ Flush caches
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0  @ flush I-cache
    mcr p15, 0, r0, c7, c6, 0  @ flush D-cache
    mcr p15, 0, r0, c7, c10, 4 @ drain write buffer

    bl main

.die:
    b .die
