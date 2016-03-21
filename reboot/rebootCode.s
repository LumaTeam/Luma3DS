.arm.little

byteswritten equ 0x2000E000
kernelCode equ 0x080F0000
buffer equ 0x24000000
fileOpen equ 0x4E45504F ;dummy

.create "reboot.bin", 0
.arm
//Code jumps here right after the sprintf call
process9Reboot:
	doPxi:
        ldr r4, =0x44846
		ldr r0, =0x10008000
		readPxiLoop1:
			ldrh r1, [r0,#4]
			.word 0xE1B01B81	//lsls r1, r1, #0x17
			bmi readPxiLoop1
			ldr r0, [r0,#0xC]		
        cmp r0, r4
        bne doPxi
	
	GetFirmPath:
		add r0, sp, #0x3A8-0x70+0x24
		ldr r1, [r0], #4
		ldr r2, =0x00300030
		cmp r1, r2
		ldreq r1, [r0], #4
		ldreq r2, =0x002F0032
		cmpeq r1, r2
		
	OpenFirm:
		ldreq r1, =(FileName - OpenFirm - 12)
		addeq r1, pc
		addne r1, sp, #0x3A8-0x70
		moveq r2, #1
		movne r2, #0
		str r2, [externalFirm]
		mov r2, #1
		add r0, r7, #8
		ldr r6, =fileOpen
		blx r6
	
    SeekFirm:
		ldr r0, [externalFirm]
		cmp r0, #1
		moveq r0, r7
        ldreq r1, =byteswritten
        ldreq r2, =buffer
        ldreq r3, =0x0
		ldreq r6, [sp,#0x3A8-0x198]
		ldreq r6, [r6,#0x28]	//fread function stored here
		blxeq r6
		
	ReadFirm:
		mov r0, r7
        ldr r1, =byteswritten
        ldr r2, =buffer
        ldr r3, =0x200000
		ldr r6, [sp,#0x3A8-0x198]
		ldr r6, [r6,#0x28]	//fread function stored here
		blx r6

    KernelSetState:
        mov r2, #0
        mov r3, r2
        mov r1, r2
        mov r0, r2
        .word 0xEF00007C    //SVC 0x7C

    GoToReboot:
        ldr r0, =(KernelCodeStart - GoToReboot - 12)
		add r0, pc
		ldr r1, =kernelCode
		ldr r2, =0x300
		bl Memcpy
		
		ldr r0, =kernelCode
        .word 0xEF00007B    //SVC 0x7B

    InfiniteLoop:
        b InfiniteLoop

Memcpy:
	MOV     R12, LR
	STMFD   SP!, {R0-R4}
	ADD     R2, R2, R0

	memcpyLoop:
		LDR     R3, [R0],#4
		STR     R3, [R1],#4
		CMP     R0, R2
		BLT     memcpyLoop
		LDMFD   SP!, {R0-R4}
		MOV     LR, R12
		BX      LR

FileName:
	.dcw "sdmc:/aurei/patched_firmware_sys.bin"
	.word 0x0

externalFirm:
	.word 0x2000A000

.pool

// Kernel Code
.align 4
KernelCodeStart:
	memorySetting:
		MRC     p15, 0, R0,c2,c0, 0
        MRC     p15, 0, R12,c2,c0, 1
        MRC     p15, 0, R1,c3,c0, 0
        MRC     p15, 0, R2,c5,c0, 2
        MRC     p15, 0, R3,c5,c0, 3
        LDR     R4, =0x18000035
        BIC     R2, R2, #0xF0000
        BIC     R3, R3, #0xF0000
        ORR     R0, R0, #0x10
        ORR     R2, R2, #0x30000
        ORR     R3, R3, #0x30000
        ORR     R12, R12, #0x10
        ORR     R1, R1, #0x10
        MCR     p15, 0, R0,c2,c0, 0
        MCR     p15, 0, R12,c2,c0, 1
        MCR     p15, 0, R1,c3,c0, 0
        MCR     p15, 0, R2,c5,c0, 2
        MCR     p15, 0, R3,c5,c0, 3
        MCR     p15, 0, R4,c6,c4, 0
        MRC     p15, 0, R0,c2,c0, 0
        MRC     p15, 0, R1,c2,c0, 1
        MRC     p15, 0, R2,c3,c0, 0
        ORR     R0, R0, #0x20
        ORR     R1, R1, #0x20
        ORR     R2, R2, #0x20
        MCR     p15, 0, R0,c2,c0, 0
        MCR     p15, 0, R1,c2,c0, 1
        MCR     p15, 0, R2,c3,c0, 0

    copyFirmPartitions:
        LDR     R4, =buffer
        ADD     R3, R4, #0x40
        LDR     R0, [R3]
        ADD     R0, R0, R4
        LDR     R1, [R3,#4]
        LDR     R2, [R3,#8] 
		bl KernelMemcpy
		
        ADD     R3, R4, #0x70
        LDR     R0, [R3]
        ADD     R0, R0, R4
        LDR     R1, [R3,#4]
        LDR     R2, [R3,#8]
        bl KernelMemcpy
			
        ADD     R3, R4, #0xA0
        LDR     R0, [R3]
        ADD     R0, R0, R4
        LDR     R1, [R3,#4]
        LDR     R2, [R3,#8]
        bl KernelMemcpy
			
		ADD     R3, R4, #0xD0
        LDR     R0, [R3]
		CMP		R0, #0
		BEQ		invalidateDataCache
        ADD     R0, R0, R4
        LDR     R1, [R3,#4]
        LDR     R2, [R3,#8]
        bl KernelMemcpy
		
    invalidateDataCache:
        MOV     R2, #0
        MOV     R1, R2
        loc_809460C:
        MOV     R0, #0
        MOV     R3, R2,LSL#30
        loc_8094614:
        ORR     R12, R3, R0,LSL#5
        MCR     p15, 0, R1,c7,c10, 4
        MCR     p15, 0, R12,c7,c14, 2
        ADD     R0, R0, #1
        CMP     R0, #0x20
        BCC     loc_8094614
        ADD     R2, R2, #1
        CMP     R2, #4
        BCC     loc_809460C

    jumpToEntrypoint:
        MCR     p15, 0, R1,c7,c10, 4
        LDR     R0, =0x42078
        MCR     p15, 0, R0,c1,c0, 0
        MCR     p15, 0, R1,c7,c5, 0
        MCR     p15, 0, R1,c7,c6, 0
        MCR     p15, 0, R1,c7,c10, 4
		LDR     R4, =buffer
        MOV     R1, #0x1FFFFFFC
		LDR     R2, [R4,#8]
		STR     R2, [R1]
		LDR     R0, [R4,#0xC]
		BX      R0
.pool

KernelMemcpy:
	MOV     R12, LR
	STMFD   SP!, {R0-R4}
	ADD     R2, R2, R0

	kmemcpyLoop:
		LDR     R3, [R0],#4
		STR     R3, [R1],#4
		CMP     R0, R2
		BLT     kmemcpyLoop
		LDMFD   SP!, {R0-R4}
		MOV     LR, R12
		BX      LR
.pool

KernelCodeEnd:

.close
