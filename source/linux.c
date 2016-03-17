#include "fs.h"
#include "i2c.h"
#include "linux.h"

void runLinux(void){
	__asm__(
	"mrc p15, 0, r0, c1, c0, 0\n"
	"bic r0, r0, #1\n"
	"mcr p15, 0, r0, c1, c0, 0\n"
	"mrs r0, cpsr\n"
	"orr r0, r0, #(0x80 | 0x40)\n"
	"msr cpsr_c, r0\n"
	);
	
	while(*((vu32 *)0x1FFFFFF8) != 0){}
	*((vu32 *)0x1FFFFFF8) = (u32)arm11;
	
	for(;;){
		__asm__(
			"mov r0, #0\n"
			"mcr p15, 0, r0, c7, c0, 4\n"
		);
	}
}

void shutdown(void){
	i2cWriteRegister(3, 0x20, 1);
}

void loadLinux(void){
	u32 size = fileSize(LINUXIMAGE_FILENAME);
	if(!size){
		shutdown();
	}
	fileRead((u8 *)ZIMAGE_ADDR, LINUXIMAGE_FILENAME, size);
	
	size = fileSize(DTB_FILENAME);
	if(!size){
		shutdown();
	}
	fileRead((u8 *)PARAMS_ADDR, DTB_FILENAME, size);
}

void arm11(void){
	// Clear the FB, removes "snow"
	u8* address = (u8*)0x18000000;
	while((u32)address <= 0x18151800){
		*address = 0;
		address++;
	}
	
	//TODO Make this C
	__asm__(
	".cpu mpcore\n"
	// Disable FIQs, IRQs, imprecise aborts
	// and enter SVC mode
	"CPSID aif, #0x13\n"
	
	// Invalidate Entire Instruction Cache,
	// also flushes the branch target cache
	"mov r0, #0\n"
	"mcr p15, 0, r0, c7, c5, 0\n"
	
	// Clear and Invalidate Entire Data Cache
	"mov r0, #0\n"
	"mcr p15, 0, r0, c7, c14, 0\n"
	
	// Data Synchronization Barrier
	"mov r0, #0\n"
	"mcr p15, 0, r0, c7, c10, 4\n"
	
	// Disable the MMU and data cache
	// (the MMU is already disabled)
	"mrc p15, 0, r1, c1, c0, 0\n"
	"bic r1, r1, #0b101\n"
	"mcr p15, 0, r1, c1, c0, 0\n"
	
	// Clear exclusive records
	"clrex\n"
	
	////////// Map Framebuffers //////////
	
	////// Top screen //////
	"ldr r0, =0x10400400\n"
	
	// Left eye
	"ldr r1, =0x18000000\n"
	"str r1, [r0, #(0x68 + 0)]\n"
	"ldr r1, =0x18046500\n"
	"str r1, [r0, #(0x68 + 4)]\n"
	
	// Right eye
	"ldr r1, =0x1808CA00\n"
	"str r1, [r0, #(0x94 + 0)]\n"
	"ldr r1, =0x180D2F00\n"
	"str r1, [r0, #(0x94 + 4)]\n"
	
	// Select framebuffer 0 and adjust format/stride
	"mov r1, #0\n"
	"str r1, [r0, #0x78]\n"
	"ldr r1, =0x80341\n"
	"str r1, [r0, #0x70]\n"
	"mov r1, #0x2D0\n"
	"str r1, [r0, #0x90]\n"
	
	////// Bottom screen //////
	"ldr r0, =0x10400500\n"
	
	"ldr r1, =0x18119400\n"
	"str r1, [r0, #(0x68 + 0)]\n"
	"ldr r1, =0x18151800\n"
	"str r1, [r0, #(0x68 + 4)]\n"
	
	// Select framebuffer 0
	"mov r1, #0\n"
	"str r1, [r0, #0x78]\n"
	
	////////// Jump to the kernel //////////
	
	// Setup the registers before
	// jumping to the kernel entry
	"mov r0, #0\n"
	"ldr r1, =0xFFFFFFFF\n"
	"ldr r2, =0x20000100\n"
	"ldr lr, =0x20008000\n"
	
	// Jump to the kernel!
	"bx lr\n"
	);
}