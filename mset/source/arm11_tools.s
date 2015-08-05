.global invalidate_data_cache
invalidate_data_cache:
    mov r0, #0
    mcr p15, 0, r0, c7, c14, 0 @ Clean and Invalidate Entire Data Cache
    mcr p15, 0, r0, c7, c10, 4 @ Data Synchronization Barrier
    bx lr

.global invalidate_instruction_cache
invalidate_instruction_cache:
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0
    mcr p15, 0, r0, c7, c5, 4
    mcr p15, 0, r0, c7, c5, 6
    mcr p15, 0, r0, c7, c10, 4
    bx lr

.global asm_memcpy
asm_memcpy:
    add r2, r1

    .memcpy_loop:
        ldmia r1!, {r3}
        stmia r0!, {r3}
        cmp r1, r2
        bcc .memcpy_loop

    bx lr
