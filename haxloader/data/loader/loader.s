.section .text

start_:

	@ Initial setup
	ldr	r1, =0x24F7FFFC @  void *r1 = (void*) 0x24F7FFFC;
	ldr	r2, =0x23EFFFFC @  void *r2 = (void*) 0x23EFFFFC;
	ldr	r3, =0x24EFFFFC @  void *r3 = (void*) 0x24EFFFFC;

copy_loop:
	ldr	r0, [r3, #4]! @ u32 r0 = *((u32*)(r3 + 4)); r3 += 4;
	str	r0, [r2, #4]! @ *((u32*)(r2 + 4)) = r0; r2 += 4;

	cmp	r3, r1    @ if r3 != 0x24F7FFFC
	bne	copy_loop @ goto copy_loop;


	@ Cache flush routine by gemarcano (aka Gelex)

	mov r1, #0

	outer_loop:
		mov r0, #0

			inner_loop:
			orr r2, r1, r0
			mcr p15, 0, r2, c7, c14, 2
			add r0, r0, #0x20
			cmp r0, #0x400
			bne inner_loop

		add r1, r1, #0x40000000
		cmp r1, #0x0
		bne outer_loop 

	mov r0, #0 
	mcr p15, 0, r0, c7, c5, 0 
	mcr p15, 0, r0, c7, c10, 4 

	ldr	r3, =0x23F00000
	bx	r3
