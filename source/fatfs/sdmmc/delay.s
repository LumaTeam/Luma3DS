.text
.arm
.align 4

.global waitcycles
.type waitcycles, %function
waitcycles:
    push {r0-r2, lr}
    str r0, [sp, #4]
    waitcycles_loop:
        ldr r3, [sp, #4]
        subs r2, r3, #1
        str r2, [sp, #4]
        cmp r3, #0
        bne waitcycles_loop
    pop {r0-r2, pc}
