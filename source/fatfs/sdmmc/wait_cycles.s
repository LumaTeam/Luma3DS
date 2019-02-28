.arm

.section    .text.wait_cycles, "ax", %progbits
.align      2
.global     wait_cycles
.type       wait_cycles, %function
wait_cycles:
    subs    r0, #2
    nop
    bgt     wait_cycles
    bx      lr
