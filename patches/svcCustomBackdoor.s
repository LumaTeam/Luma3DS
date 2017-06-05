.arm.little

.create "build/svcCustomBackdoor.bin", 0
.arm

; Result svcCustomBackdoor(void *func, ... <up to 3 args>)
svcCustomBackdoor:
    b skip_orig
orig: .word 0
skip_orig:
    push {r4, lr}
    mov r4, r0
    mov r0, r1
    mov r1, r2
    mov r2, r3
    blx r4
    pop {r4, pc}

.pool
.close
