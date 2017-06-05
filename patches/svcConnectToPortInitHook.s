.arm.little

.create "build/svcConnectToPortInitHook.bin", 0
.arm
    push {r0-r4, lr}
    adr r0, jumpAddress
    bl convertVAToPA
    orr r4, r0, #(1 << 31)

    loop:
        ldr r12, [r4]
        cmp r12, #0
        bne loop_end
        ldr r12, [SleepThread]
        ldr r0, =(10 * 1000 * 1000)
        mov r1, #0
        blx r12
        b loop

    loop_end:
    pop {r0-r4, lr}
    bx r12

convertVAToPA:
    mov r1, #0x1000
    sub r1, #1
    and r2, r0, r1
    bic r0, r1
    mcr p15, 0, r0, c7, c8, 0    ; VA to PA translation with privileged read permission check
    mrc p15, 0, r0, c7, c4, 0    ; read PA register
    tst r0, #1                   ; failure bit
    bic r0, r1
    addeq r0, r2
    movne r0, #0
    bx lr

.pool
_base: .ascii "base"
jumpAddressOrig: .ascii "orig"
SleepThread: .ascii "SlpT"
jumpAddress: .word 0

.close
