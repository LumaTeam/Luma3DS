.arm
.align 4
.code 32
.text

.global jump_table
jump_table:
    b func_patch_hook
    b reboot_function

func_patch_hook:
    push {r0-r12, lr}

    mov r0, #0
    bl pxi_send
    bl pxi_sync

    mov r0, #0x10000
    bl pxi_send
    bl pxi_recv
    bl pxi_recv
    bl pxi_recv

    ldr r1, jt_pdn_regs
    mov r0, #2
    strb r0, [r1, #0x230]
    mov r0, #0x10
    bl busy_spin
    mov r0, #0
    strb r0, [r1, #0x230]
    mov r0, #0x10
    bl busy_spin

    pop {r0-r12, lr}

    ldr r0, =0x44836
    str r0, [r1]
    ldr pc, jt_return

reboot_function:
    adr r0, arm11_reboot_hook
    adr r1, arm11_reboot_hook_end
    ldr r2, =0x1FFFFC00
    mov r4, r2
    bl copy_mem
    bx r4

copy_mem:
    sub r3, r1, r0
    mov r1, r3,asr#2
    cmp r1, #0
    ble copy_mem_ret
    movs r1, r3,lsl#29
    sub r0, r0, #4
    sub r1, r2,	#4
    bpl copy_mem_loc1
    ldr r2, [r0,#4]!
    str r2, [r1,#4]!

    copy_mem_loc1:
        movs r2, r3,asr#3
        beq copy_mem_ret

    copy_mem_loc2:
        ldr r3, [r0,#4]
        subs r2, r2, #1
        str r3, [r1,#4]
        ldr	r3, [r0,#8]!
        str	r3, [r1,#8]!
        bne	copy_mem_loc2

    copy_mem_ret:
        bx lr

.pool

arm11_reboot_hook:
    ldr r0, pxi_regs
    ldr r1, pxi_command
    str r1, [r0]

    ldr r8, io_mem
    ldr r9, arm9_payload
    ldr r10, firm_header

    wait_arm9_loop:
        ldrb r0, [r8]
        ands r0, r0, #1
        bne wait_arm9_loop

    str r9, [r10, #0xC]

    mov r0, #0x1FFFFFF8
    wait_arm11_loop:
        ldr r1, [r0]
        cmp r1, #0
        beq wait_arm11_loop

    bx r1

pxi_regs: .long 0x10163008
pxi_command: .long 0x44846
io_mem: .long 0x10140000
arm9_payload: .long 0x23F00000
firm_header: .long 0x24000000

arm11_reboot_hook_end:

.pool

busy_spin:
    subs r0, #2
    nop
    bgt busy_spin
    bx lr

pxi_send:
    ldr r1, jt_pxi_regs
    pxi_send_l1:
        ldrh r2, [r1,#4]
        tst r2, #2
        bne pxi_send_l1
    str r0, [r1,#8]
    bx lr

pxi_sync:
    ldr r0, jt_pxi_regs
    ldrb r1, [r0,#3]
    orr r1, #0x40
    strb r1, [r0,#3]
    bx lr

pxi_recv:
    ldr r0, jt_pxi_regs
    pxi_recv_l1:
        ldrh r1, [r0,#4]
        tst r1, #0x100
        bne pxi_recv_l1
    ldr r0, [r0,#0xC]
    bx lr

.global jt_pdn_regs
jt_pdn_regs: .long 0
.global jt_pxi_regs
jt_pxi_regs: .long 0
.global jt_return
jt_return: .long 0

.global jump_table_end
jump_table_end:
