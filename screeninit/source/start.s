.section .text.start
.align 4
.global _start
_start:
    @ Disable interrupts
    CPSID aif

    bl main

.die:
    b .die
