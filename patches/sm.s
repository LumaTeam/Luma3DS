.arm.little

.create "build/sm.bin", 0
.arm
    ; This code searches the sm module for a specific byte pattern and patches some of the instructions
    ; in the code to disable service access checks when calling srv:GetServiceHandle

    ; Save the registers we'll be using
    ; Register contents:
    ; r4: Pointer to a pointer to the exheader of the current NCCH
    ; r6: Constant 0
    ; SP + 0x80 - 0x7C: Pointer to the memory location where the NCCH text was loaded

    ; Save the value of sp
    mov r0, sp
    ; Save the value of all registers
    push {r0-r12}

    ; Clear all the caches, just to be safe
    mcr p15, 0, r6, c7, c14, 0
    mcr p15, 0, r6, c7, c5, 0

    ldr r9, =0x00001002  ; Low title id of the sm module
    ldr r7, [r4]
    ldr r8, [r7, #0x200] ; Load the low title id of the current NCCH
    cmp r8, r9           ; Compare the low title id to the id of the sm module
    bne out              ; Exit if they're not the same

    ldr r0, [r0, #(0x80 - 0x7C)] ; Load the .text address
    ldr r2, [r7, #0x18]          ; Load the size of the .text
    mov r5, r0
    add r11, r5, r2       ; Max bounds of the memory region
    ldr r7, =0xE1A01006   ; mov r1, r6
    ldr r8, =0xE1A00005   ; mov r0, r5
    ldr r9, =0xE3500000   ; cmp r0, #0
    ldr r10, =0xE2850004  ; add r0, r5, #4
    
    loop:
        cmp r11, r5
        blo out         ; Check if we didn't go past the bounds of the memory region
        ldr r6, [r5]
        cmp r6, r7
        ldreq r6, [r5, #4]
        cmpeq r6, r8
        ldreq r6, [r5, #12]
        cmpeq r6, r9
        ldreq r6, [r5, #24]
        cmpeq r6, r10
        moveq r8, r5
        addne r5, r5, #4
        bne loop

    ; r8 now contains the start address of the pattern we found

    ; Write NOPs to the four instructions we want to patch
    ldr r9, =0xE320F000   ; nop
    str r9, [r8, #8]      ; Patch the bl
    str r9, [r8, #12]     ; Patch the cmp
    str r9, [r8, #16]     ; Patch the ldreq
    str r9, [r8, #20]     ; Patch the beq

    out:
    pop {r0-r12}               ; Restore the registers we used

    ; Clear all the caches again, just to be safe
    mcr p15, 0, r6, c7, c14, 0
    mcr p15, 0, r6, c7, c5, 0

    ldr r0, [r4]               ; Execute the instruction we overwrote in our detour

    bx lr                      ; Jump back to whoever called us

.pool
.close