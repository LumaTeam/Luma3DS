.arm
.align 4
.code 32
.text

.global InvalidateEntireInstructionCache
.type InvalidateEntireInstructionCache, %function
InvalidateEntireInstructionCache:
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0
	bx lr

.global CleanEntireDataCache
.type CleanEntireDataCache, %function
CleanEntireDataCache:
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 0
	bx lr

.global dsb
.type dsb, %function
dsb:
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 4
	bx lr

.global DisableInterrupts
.type DisableInterrupts, %function
DisableInterrupts:
	mrs r0, cpsr
	CPSID I
	bx lr

.global EnableInterrupts
.type EnableInterrupts, %function
EnableInterrupts:
	msr cpsr_cx, r0
	bx lr
