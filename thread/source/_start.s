.arm
.global _start
_start:
	push {r0-r12 , lr}
	ldr r0, =0x08000c00
	mov r1, #0xA00
	mov r2, #0x0
	thread_stack_loop:
		str r2, [r0], #0x4
		subs r1, r1, #4
		bgt thread_stack_loop
	mov r0, #0x3F @ thread priority
	ldr r1, =thread @ thread_addr
	mov r2, #0x0 @ arg
	ldr r3, =0x08000c00 @ StackTop
	ldr r4, =0xFFFFFFFE
	svc 0x8
	pop {r0-r12 , lr}
	ldr r0, =0x080E3408
	ldr pc, =0x0808519C