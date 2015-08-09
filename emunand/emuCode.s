.nds

sdmmc equ 0x80D86F0
sdmmc_unk1 equ 0x080788C0
aes_unk equ 0x0805F9E4
aes_setkey equ 0x08057458
sdmmc_unk2 equ 0x080786A0
sdmmc_unk0 equ 0x08062890

.create "emunand.bin", 0x0801A4C0
.org 0x0801A4C0
.arm
EMU_WRITE:
	stmfd sp!, {r0-r3}
	mov r3, r0
	ldr r1, =orig_sector
	ldr r2, [r3,#4]
	str r2, [r1,#4]
	ldr r0, =sdmmc
	cmp r2, r0
	ldr r2, [r3,#8]
	str r2, [r1]
	beq @@orig_code
	ldr r1, =sdmmc
	str r1, [r3,#4]
	cmp r2, #0
	ldr r0, =nand_offset
	ldrne r0, [r0]
	addne r0, r2
	ldreq r0, [r0, #(ncsd_header_offset - nand_offset)]
	str r0, [r3,#8]
@@orig_code:
	ldmfd sp!, {r0-r3}
	movs r4, r0
	movs r5, r1
	movs r7, r2
	movs r6, r3
	movs r0, r1, lsl#23
	beq loc_801a534
	stmfd sp!, {r4}
	ldr r4, =(sdmmc_unk0 + 1)
	blx r4
	ldmfd sp!, {r4}
loc_801a534:
	ldr r0, [r4,#4]
	ldr r1, [r0]
	ldr r1, [r1,#0x18]
	blx r1
	ldr r1, [r4,#4]
	movs r3, r0
	ldr r0, [r1,#0x20]
	movs r2, r5, lsr#9
	mov r12, r0
	ldr r0, [r4,#8]
	str r7, [sp,#4]
	adds r0, r0, r2
	cmp r1, #0
	str r6, [sp,#8]
	str r0, [sp]
	beq loc_801a578
	adds r1, r1, #8
loc_801a578:
	movs r2, r4
	adds r2, r2, #0xc
	mov r0, r12
	ldr r5, =(sdmmc_unk1 + 1) ; called by the original function
	blx r5
	stmfd sp!, {r0-r3}
	ldr r2, =orig_sector
	ldr r1, [r2]
	str r1, [r4,#8]
	ldr r1, [r2,#4]
	str r1, [r4,#4]
	ldmfd sp!, {r0-r3}
	ldmfd sp!, {r1-r7,lr}
	bx lr

EMU_READ:
	stmfd sp!, {r0-r3}
	mov r3, r0
	ldr r1, =orig_sector
	ldr r2, [r3,#4]
	str r2, [r1,#4]
	ldr r0, =sdmmc
	cmp r2, r0
	ldr r2, [r3,#8]
	str r2, [r1]
	beq @@orig_code
	ldr r1, =sdmmc
	str r1, [r3,#4]
	cmp r2, #0
	ldr r0, =nand_offset
	ldrne r0, [r0]
	addne r0, r2
	ldreq r0, [r0, #(ncsd_header_offset - nand_offset)]
	str r0, [r3,#8]
@@orig_code:
	ldmfd sp!, {r0-r3}
	movs r4, r0
	movs r5, r1
	movs r7, r2
	movs r6, r3
	movs r0, r1, lsl#23
	beq loc_801a624
	stmfd sp!, {r4}
	ldr r4, =(sdmmc_unk0 + 1)
	blx r4
	ldmfd sp!, {r4}
loc_801a624:
	ldr r0, [r4,#4]
	ldr r1, [r0]
	ldr r1, [r1,#0x18]
	blx r1
	ldr r1, [r4,#4]
	movs r3, r0
	ldr r0, [r1,#0x20]
	movs r2, r5, lsr#9
	mov r12, r0
	ldr r0, [r4,#8]
	str r7, [sp,#4]
	adds r0, r0, r2
	cmp r1, #0
	str r6, [sp,#8]
	str r0, [sp]
	beq loc_801a668
	adds r1, r1, #8
loc_801a668:
	movs r2, r4
	adds r2, r2, #0xC
	mov r0, r12
	ldr r5, =(sdmmc_unk2 + 1)
	blx r5
	stmfd sp!, {r0-r3}
	ldr r2, =orig_sector
	ldr r1, [r2]
	str r1, [r4,#8]
	ldr r1, [r2,#4]
	str r1, [r4,#4]
	ldmfd sp!, {r0-r3}
	ldmfd sp!, {r1-r7,lr}
	bx lr

.pool
orig_sector:		.word 0x00000000
orig_ptr:			.word 0x00000000
nand_offset:		.ascii "NAND"       ; for rednand this should be 1
ncsd_header_offset:	.ascii "NCSD"       ; depends on nand manufacturer + emunand type (GW/RED)
;ncsd_header_offset: .word 0x1D7800
;ncsd_header_offset: .word 0x1DD000
slot0x25keyX:
    .word 0xABD8E7CE 
    .word 0xAE0DC030 
    .word 0xE3F50E85 
    .word 0xF35AAC82
.close
