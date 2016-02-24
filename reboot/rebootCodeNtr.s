.arm.little

firm_size equ 0x000EB000
firm_addr equ 0x24000000
fopen equ 0x0805B180
fread equ 0x0804D9B0
pxi_wait_recv equ 0x08055178

.create "reboot1.bin", 0x080859C8
.org 0x080859C8
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
	swi 0x7C
	ldr r0, =0x80FF4FC
	swi 0x7B

@@inf_loop:
	b @@inf_loop
.pool
firm_fname:
.close
