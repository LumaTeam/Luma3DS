	.arm
	.text

#include "rop.h"

	.global	_start
@---------------------------------------------------------------------------------
_start:
	rop_memcpy(BUFFER_LOC, ROP_LOC+arm_code, arm_code_end-arm_code)
	rop_flush_data_cache(BUFFER_LOC, arm_code_end-arm_code)
	rop_gx_texture_copy(BUFFER_LOC, CODE_TARGET, arm_code_end-arm_code)
	rop_sleep(1000000000)
	rop_jump_arm
.align 4, GARBAGE
arm_code:
#ifdef ARM_CODE
	#define S_(x)	#x
	#define S(x)	S_(x)
	.incbin S(ARM_CODE)
#endif
arm_code_end:
