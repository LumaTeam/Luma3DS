.arm.little

.create "build/k11MainHook.bin", 0
.arm

bindSGI0:
    ; hook __kernel_main to bind SGI0 for own purposes
    push {r0-r4, lr}
    sub sp, #16                  ; 3 args passed through the stack + alignment
    ldr r0, [interruptManager]
    adr r1, interruptEvent
    mov r2, #0
    mrc p15, 0, r3, c0, c0, 5
    and r3, #3
    mov r4, #0
    str r4, [sp]
    str r4, [sp, #4]
    str r4, [sp, #8]

    ldr r12, [InterruptManager_mapInterrupt]
    blx r12
    cmp r0, #0
    blt .

    add sp, #16
    pop {r0-r4, pc}

executeCustomHandler:
    push {r4, lr}
    mrs r4, cpsr
    adr r0, customHandler
    bl convertVAToPA
    orr r0, #(1 << 31)
    ldr r12, [r0]

    blx r12

    mov r0, #0
    msr cpsr_cx, r4
    pop {r4, pc}

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

; Result InterruptManager::mapInterrupt(InterruptManager *this, InterruptEvent *iEvent, u32 interruptID, u32 coreID, s32 priority, bool willBeMasked, bool isLevelHighActive);
InterruptManager_mapInterrupt: .ascii "bind"

_vtable: .word executeCustomHandler
interruptEvent: .word _vtable

parameters:
customHandler: .ascii "hdlr"
interruptManager: .word 0
L2MMUTable: .word 0
funcs: .word 0,0,0
TTBCR: .word 0
L1MMUTableAddrs: .word 0,0,0,0
kernelVersion: .word 0
CFWInfo: .word 0,0,0,0

.close
