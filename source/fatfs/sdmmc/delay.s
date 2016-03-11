.arm
.global waitcycles
.type   waitcycles STT_FUNC

@waitcycles ( u32 us )
waitcycles:
    waitcycles_loop:
        subs r0, #1
        bgt waitcycles_loop
    bx lr
