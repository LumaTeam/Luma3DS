// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

.arm
.global ioDelay
.type   ioDelay STT_FUNC

@ioDelay ( u32 us )
ioDelay:
	ldr r1, =0x18000000  @ VRAM
1:
	@ Loop doing uncached reads from VRAM to make loop timing more reliable
	ldr r2, [r1]
	subs r0, #1
	bgt 1b
	bx lr
