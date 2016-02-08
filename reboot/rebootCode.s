.nds

firm_size equ 0x000EA000
firm_addr equ 0x24000000
fopen equ 0x08059D10
fread equ 0x0804CC54
pxi_wait_recv equ 0x08054134

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

.create "reboot1.bin", 0x080849DC
.org 0x080849DC
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

.create "reboot2.bin", 0x080933CC
.org 0x080933CC
.arm
	stmfd sp!, {r4-r11,lr}
	sub sp, sp, #0x3C
	mrc p15, 0, r0, c2, c0, 0	; dcacheable
	mrc p15, 0, r12, c2, c0, 1	; icacheable
	mrc p15, 0, r1, c3, c0, 0	; write bufferable
	mrc p15, 0, r2, c5, c0, 2	; daccess
	mrc p15, 0, r3, c5, c0, 3	; iaccess
	ldr r4, =0x18000035			; 0x18000000 128M
	bic r2, r2, #0xF0000		; unprotect region 4
	bic r3, r3, #0xF0000		; unprotect region 4
	orr r0, r0, #0x10			; dcacheable region 4
	orr r2, r2, #0x30000		; region 4 r/w
	orr r3, r3, #0x30000		; region 4 r/w
	orr r12, r12, #0x10			; icacheable region 4
	orr r1, r1, #0x10			; write bufferable region 4
	mcr p15, 0, r0, c2, c0, 0
	mcr p15, 0, r12, c2, c0, 1
	mcr p15, 0, r1, c3, c0, 0	; write bufferable
	mcr p15, 0, r2, c5, c0, 2	; daccess
	mcr p15, 0, r3, c5, c0, 3	; iaccess
	mcr p15, 0, r4, c6, c4, 0	; region 4 (hmmm)

	mrc p15, 0, r0, c2, c0, 0	; dcacheable
	mrc p15, 0, r1, c2, c0, 1	; icacheable
	mrc p15, 0, r2, c3, c0, 0	; write bufferable
	orr r0, r0, #0x20			; dcacheable region 5
	orr r1, r1, #0x20			; icacheable region 5
	orr r2, r2, #0x20			; write bufferable region 5
	mcr p15, 0, r0, c2, c0, 0	; dcacheable
	mcr p15, 0, r1, c2, c0, 1	; icacheable
	mcr p15, 0, r2, c3, c0, 0	; write bufferable

	mov r4, #firm_addr
	add r3, r4, #0x40
	ldr r0, [r3]				; offset
	add r0, r0, r4				; src
	ldr r1, [r3,#4]				; dst
	ldr r2, [r3,#8]				; size
	bl memcpy32
	add r3, r4, #0x70
	ldr r0, [r3]
	add r0, r0, r4				; src
	ldr r1, [r3,#4]				; dst
	ldr r2, [r3,#8]				; size
	bl memcpy32
	add r3, r4, #0xA0
	ldr r0, [r3]
	add r0, r0, r4				; src
	ldr r1, [r3,#4]				; dst
	ldr r2, [r3,#8]				; size
	bl memcpy32
	mov r2, #0
	mov r1, r2
@flush_cache:
	mov r0, #0
	mov r3, r2, lsl#30
@flush_cache_inner_loop:
	orr r12, r3, r0, lsl#5
	mcr p15, 0, r1, c7, c10, 4	; drain write buffer
	mcr p15, 0, r12, c7, c14, 2	; clean and flush dcache entry (index and segment)
	add r0, r0, #1
	cmp r0, #0x20
	bcc @flush_cache_inner_loop
	add r2, r2, #1
	cmp r2, #4
	bcc @flush_cache
	mcr p15, 0, r1, c7, c10, 4	; drain write buffer
@mpu_enable:
	ldr r0, =0x42078			; alt vector select, enable itcm
	mcr p15, 0, r0, c1, c0, 0
	mcr p15, 0, r1, c7, c5, 0	; flush dcache
	mcr p15, 0, r1, c7, c6, 0	; flush icache
	mcr p15, 0, r1, c7, c10, 4	; drain write buffer
	mov r0, #firm_addr
	mov r1, 0X1FFFFFFC
	ldr r2, [r0,#8]				; arm11 entry
	str r2, [r1]
	ldr r0, [r0,#0xC]			; arm9 entry
	add sp, sp, #0x3C
	ldmfd sp!, {r4-r11,lr}
	bx r0
.pool
memcpy32: ; memcpy32(void *src, void *dst, unsigned int size)
	mov r12, lr
	stmfd sp!, {r0-r4}
	add r2, r2, r0
@memcpy_loop:
	ldr r3, [r0], #4
	str r3, [r1], #4
	cmp r0, r2
	blt @memcpy_loop
	ldmfd sp!, {r0-r4}
	mov lr, r12
	bx lr
.pool
.close
