.nds

firm_size equ 0x000E9000
firm_addr equ 0x24000000
fopen equ 0x0805AF20
fread equ 0x0804D828
pxi_wait_recv equ 0x08054FB0

.macro svc, num
	.if isArm()
		.word 0xEF000000 | num
	.else
		.if num > 0xFF
			.error "bitch you crazu"
		.endif
		.halfword 0xDF00 | num
	.endif
.endmacro

.create "reboot1.bin", 0x080858E0
.org 0x080858E0
.arm
patch005:
	ldr r0, =0x2000E000
	mov r1, #0x200
	mov r2, #0
	add r1, r1, r0
@@memset_loop:
	str r2, [r0]
	add r0, r0, #4
	cmp r0, r1
	blt @@memset_loop
	ldr r0, =0x2000E000
	ldr r1, =firm_fname
	mov r2, #1
	blx fopen
	ldr r0, =0x2000E000
	ldr r1, =0x2000E100
	mov r2, #firm_addr
	mov r3, #firm_size
	blx fread

	ldr r4, =0x44846
	blx pxi_wait_recv
	cmp r0, r4
	bne patch005
	mov r2, #0
	mov r3, r2
	mov r1, r2
	mov r0, r2
	svc 0x7C
	ldr r0, =0x80FF4FC
	svc 0x7B

@@inf_loop:
	b @@inf_loop
.pool
firm_fname:
.close
