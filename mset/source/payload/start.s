.section .text.start
.align 4
.global _start
_start:
    @ Change the stack pointer
    mov sp, #0x27000000

    @ Sets MPU permissions and cache settings
    ldr r0, =0xFFFF001D			@ ffff0000 32k
    ldr r1, =0x01FF801D			@ 01ff8000 32k
    ldr r2, =0x08000027			@ 08000000 1M
    ldr r3, =0x10000021			@ 10000000 128k
    ldr r4, =0x10100025			@ 10100000 512k
    ldr r5, =0x20000035			@ 20000000 128M
    ldr r6, =0x2800801B			@ 28008000 16k
    ldr r7, =0x1800002D			@ 18000000 8M
    ldr r8, =0x33333336
    ldr r9, =0x60600666
    mov r10, #0x25
    mov r11, #0x25
    mov r12, #0x25
    mcr p15, 0, r0, c6, c0, 0
    mcr p15, 0, r1, c6, c1, 0
    mcr p15, 0, r2, c6, c2, 0
    mcr p15, 0, r3, c6, c3, 0
    mcr p15, 0, r4, c6, c4, 0
    mcr p15, 0, r5, c6, c5, 0
    mcr p15, 0, r6, c6, c6, 0
    mcr p15, 0, r7, c6, c7, 0
    mcr p15, 0, r8, c5, c0, 2	@ Enable data r/w for all regions
    mcr p15, 0, r9, c5, c0, 3	@ Enable inst read for 0, 1, 2, 5, 7
    mcr p15, 0, r10, c3, c0, 0	@ Write bufferable 0, 2, 5
    mcr p15, 0, r11, c2, c0, 0	@ Data cacheable 0, 2, 5
    mcr p15, 0, r12, c2, c0, 1	@ Inst cacheable 0, 2, 5

    @ Enables all the settings we specified above
    ldr r0, =0x5307D
    mcr p15, 0, r0, c1, c0, 0	@ cp15 ctl register enable mpu, enable cache and use alt vector table

    @ Undocumented: Fixes mounting of SDMC
    ldr r0, =0x10000020
    mov r1, #0x340
    str r1, [r0]

    bl main

.die:
    b .die
