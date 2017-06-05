.arm.little

.create "build/mmuHook.bin", 0
.arm
    ; r2 = L1 table
    ; Thanks @Dazzozo for giving me that idea
    ; Maps physmem so that, if addr is in physmem(0, 0x30000000), it can be accessed uncached&rwx as addr|(1<<31)
    ; Save the value of all registers

    push {r0-r1, r3-r7}
    mov r0, #0
    mov r1, #0x30000000 ; end address
    ldr r3, =#0x40C02   ; supersection (rwx for all) of strongly ordered memory, shared
    loop:
        orr r4, r0, #0x80000000
        orr r5, r0, r3

        mov r6, #0 ;
        loop2:
            add r7, r6, r4,lsr #20
            str r5, [r2, r7,lsl #2]
            add r6, #1
            cmp r6, #16
            blo loop2

        add r0, #0x01000000
        cmp r0, r1
        blo loop
    pop {r0-r1, r3-r7}

    mov r3, #0xe0000000 ; instruction that has been patched
    bx lr


.pool
.close
