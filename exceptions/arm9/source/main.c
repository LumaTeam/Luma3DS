/*
*   main.c
*       by TuxSH
*   Copyright (c) 2016 All Rights Reserved
*/

// This is part of AuReiNand, see LICENSE.txt for details

#include "types.h"
#include "i2c.h"
#include "handlers.h"

#define TEMP_BUFFER (void *)0x1FF80000 // we choose AXIWRAM as tmp buffer since it's usually arm11 payloads running there
#define FINAL_BUFFER (void *)0x25000000
#define STACK_DUMP_SIZE 0x2000

#define SP  (TEMP_BUFFER + 4*STACK_DUMP_SIZE)
void __attribute__((noreturn)) mainHandler(u32 regs[17], int type)
{
    vu32* dump = (vu32 *)TEMP_BUFFER;   // AXIWRAM
    
    dump[0] = 0xDEADC0DE;               // magic
    dump[1] = 0xDEADCAFE;               //
    dump[2] = (1 << 16) | 0;            // dump format version number
    dump[3] = 9;                        // processor
    dump[4] = (u32)type;                // exception type
    dump[5] = 0;                        // total size
    dump[6] = 4*17;                     // register dump size (r0-r12, sp, lr, pc, cpsr)
    dump[7] = 40;                       // code dump size (10 ARM instructions, up to 20 Thumb instructions).
    dump[8] = STACK_DUMP_SIZE;          // stack dump size
    dump[9] = 0;                        // other data size
    
    // Dump registers
    // Current order of regs: pc, spsr, r8-r12, sp, lr, r0-r7
    vu32* regdump = dump + 10;
    
    u32 cpsr = regs[1];
    u32 pc   = regs[0] + (((cpsr & 0x20) != 0 && type == 1) ? 2 : 0);
    regdump[15] = pc;
    regdump[16] = cpsr;
    
    for(int i = 0; i < 7; ++i)
        regdump[8+i] = regs[2+i];
    
    for(int i = 0; i < 8; ++i)
        regdump[i] = regs[9+i]; 
        
    // Dump code
    vu16* codedump = (vu16 *)(regdump + dump[6]/4);
    vu16* instr = (vu16 *)pc - dump[7]/2 + 1;
    for(u32 i = 0; i < dump[7]/2; ++i)
        codedump[i] = instr[i];

    // Dump stack
    vu32* sp = (vu32 *)regdump[13];
    vu32* stackdump = (vu32 *)((vu8 *)codedump + dump[7]);
    // Homebrew/CFW set their stack at 0x27000000, but we'd better not make any assumption here
    // as it breaks things it seems
    for(u32 i = 0; i < dump[8]/4; ++i)
        stackdump[i] = sp[i];
        
    u32 size = dump[6]+dump[7]+dump[8]+dump[9]+40;
    dump[5] = size;
    vu32* final = (vu32 *)FINAL_BUFFER;
    for(u32 i = 0; i < size/4; ++i)
        final[i] = dump[i];

    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2); // Reboot
    for(;;);
}

 
void main(void)
{
    setupStack(1, SP);  // FIQ
    setupStack(7, SP);  // Abort
    setupStack(11, SP); // Undefined
    
    *(vu32 *)0x08000004 = 0xE51FF004;   
    *(vu32 *)0x08000008 = (u32)&FIQHandler;
    *(vu32 *)0x08000014 = 0xE51FF004;   
    *(vu32 *)0x08000018 = (u32)&undefinedInstructionHandler;
    *(vu32 *)0x0800001C = 0xE51FF004;   
    *(vu32 *)0x08000020 = (u32)&prefetchAbortHandler;
    *(vu32 *)0x08000028 = 0xE51FF004;
    *(vu32 *)0x0800002C = (u32)&dataAbortHandler;
}