@---------------------------------------------------------------------------------
	.section ".init"
@---------------------------------------------------------------------------------
	.global _start
	.extern main
	.align	4
	.arm
@---------------------------------------------------------------------------------
		b _start
		NOP
		NOP
		NOP
_start:
		MSR     CPSR_c, #0xDF
        LDR     R0, =0x2078
        MCR     p15, 0, R0,c1,c0, 0
        LDR     R0, =0xFFFF001D
		LDR     R1, =0x1FF801D
    	LDR     R2, =0x8000027
        LDR     R3, =0x10000021
    	LDR     R4, =0x10100025
    	LDR     R5, =0x20000035
        LDR     R6, =0x2800801B
        LDR     R7, =0x1800002D
        LDR     R8, =0x33333336
        LDR     R9, =0x60600666
        MOV     R10, #0x25
		MOV     R11, #0x25
		MOV     R12, #0x25
		MCR     p15, 0, R0,c6,c0, 0
		MCR     p15, 0, R1,c6,c1, 0
		MCR     p15, 0, R2,c6,c2, 0
     	MCR     p15, 0, R3,c6,c3, 0
      	MCR     p15, 0, R4,c6,c4, 0
       	MCR     p15, 0, R5,c6,c5, 0
   		MCR     p15, 0, R6,c6,c6, 0
     	MCR     p15, 0, R7,c6,c7, 0
   		MCR     p15, 0, R8,c5,c0, 2
       	MCR     p15, 0, R9,c5,c0, 3
       	MCR     p15, 0, R10,c3,c0, 0
        MCR     p15, 0, R11,c2,c0, 0
        MCR     p15, 0, R12,c2,c0, 1
        LDR     R0, =0x2800800C
     	MCR     p15, 0, R0,c9,c1, 0
        MOV     R0, #0x1E
      	MCR     p15, 0, R0,c9,c1, 1

		MOV     R12, #0
		loc_9D3D54:
			MOV     R0, #0
	        MOV     R2, R12,LSL#30
		loc_9D3D5C:
			ORR     R1, R2, R0,LSL#5
			MCR     p15, 0, R1,c7,c14, 2
			ADD     R0, R0, #1
			CMP     R0, #0x20
 			BCC     loc_9D3D5C
			ADD     R12, R12, #1
 			CMP     R12, #4
  			BCC     loc_9D3D54
  			MOV     R0, #0
  			MCR     p15, 0, R0,c7,c10, 4

		MOV     R0, #0
		MCR     p15, 0, R0,c7,c5, 0
		
     	LDR     R0, =0x5307D
		MCR     p15, 0, R0,c1,c0, 0

		ldr	r3, =main;
		blx r3

InfiniteLoop:
	b InfiniteLoop
.pool
