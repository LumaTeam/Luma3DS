.section .text.start
.align 4
.global _start
_start:
    @ Disable interrupts
    CPSID aif

    b main
