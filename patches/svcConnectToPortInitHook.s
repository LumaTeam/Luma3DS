.arm.little

.create "build/svcConnectToPortInitHook.bin", 0
.arm
    b skip_vars
vars:
    orig: .word 0
    SleepThread: .word 0
skip_vars:
    push {r0-r4, lr}
    ldr r4, =0x1ff81108

    loop:
        ldrb r12, [r4]
        cmp r12, #0
        bne loop_end

        ldr r12, [SleepThread]
        ldr r0, =(10 * 1000 * 1000)
        mov r1, #0
        blx r12
        b loop

    loop_end:
    pop {r0-r4, lr}
    mov r12, #0x40000000
    add r12, #4
    bx r12

.pool

.close
