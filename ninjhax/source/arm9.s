.arm
.align 4
.code 32
.text

@ default ARM9 payload, simply launches FIRM (reboots without clearing mem)
.global arm9_start
arm9_start:
	B               skipvars

	@ offs 4, will contain backup copy of FIRM ARM9
	@ entry point so execution can be returned to FIRM
	pa_arm9_entrypoint_backup: .long 0xFFFF0000
	
skipvars:
	STMFD           SP!, {R0-R12,LR}

	@ insert your funky stuff here
	LDMFD           SP!, {R0-R12,LR}
	
	LDR             PC, pa_arm9_entrypoint_backup
	
.global arm9_end
arm9_end:
