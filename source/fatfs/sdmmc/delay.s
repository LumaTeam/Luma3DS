.arm
.global waitcycles
.type   waitcycles STT_FUNC

@waitcycles ( u32 us )
waitcycles:
	PUSH    {R0-R2,LR}
	STR     R0, [SP,#4]
	waitcycles_loop:
		LDR     R3, [SP,#4]
		SUBS    R2, R3, #1
		STR     R2, [SP,#4]
		CMP     R3, #0
		BNE     waitcycles_loop
	POP     {R0-R2,PC}
