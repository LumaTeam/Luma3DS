/*
*   mainHandler.c
*       by TuxSH
*
*   This is part of Luma3DS, see LICENSE.txt for details
*/

#include "types.h"
#include "handlers.h"
#define NULL            0

#define FINAL_BUFFER    0xE5000000 //0x25000000

#define REG_DUMP_SIZE   (4*23)
#define CODE_DUMP_SIZE  48

#define CODESET_OFFSET  0xBEEFBEEF

void __attribute__((noreturn)) mainHandler(u32 regs[REG_DUMP_SIZE / 4], u32 type, u32 cpuId)
{
    u32 dump[(40 + REG_DUMP_SIZE + CODE_DUMP_SIZE) / 4];
    vu32 *final = (vu32 *)FINAL_BUFFER;

    while(final[0] == 0xDEADC0DE && final[1] == 0xDEADCAFE && ((final[3] & 0xFFFF) == 9 || (final[3] & 0xFFFF) == 11));
    
    dump[0] = 0xDEADC0DE;                                                               //Magic
    dump[1] = 0xDEADCAFE;                                                               //Magic
    dump[2] = (1 << 16) | 1;                                                            //Dump format version number
    dump[3] = ((cpuId & 0xf) << 16) | 11;                                               //Processor
    dump[4] = type;                                                                     //Exception type
    dump[6] = REG_DUMP_SIZE;                                                            //Register dump size (r0-r12, sp, lr, pc, cpsr, fpexc)
    dump[7] = CODE_DUMP_SIZE;                                                           //Code dump size (10 ARM instructions, up to 20 Thumb instructions).

    //Dump registers
    //Current order of saved regs: dfsr, ifsr, far, fpexc, fpinst, fpinst2, cpsr, pc, r8-r12, sp, lr, r0-r7
    u32 *regdump = dump + 10;

    u32 cpsr = regs[6];
    u32 pc   = regs[7] - ((type < 3) ? (((cpsr & 0x20) != 0 && type == 1) ? 2 : 4) : 8);

    regdump[15] = pc;
    regdump[16] = cpsr;
    for(u32 i = 0; i < 6; i++) regdump[17 + i] = regs[i];
    for(u32 i = 0; i < 7; i++) regdump[8 + i]  = regs[8 + i];
    for(u32 i = 0; i < 8; i++) regdump[i]      = regs[15 + i]; 
    
    //Dump code
    u8 *codedump = (u8 *)regdump + dump[6];
    vu8 *instr = (vu8 *)pc + ((cpsr & 0x20) ? 2 : 4) - dump[7]; //Doesn't work well on 32-bit Thumb instructions, but it isn't much of a problem
    for(u32 i = 0; i < dump[7]; i++)
        codedump[i] = instr[i];

    //Dump stack in place
    dump[8] = 0x1000 - (regdump[13] & 0xfff);                                           //Stack dump size (max. 0x1000 bytes)
    vu32 *sp = (vu32 *)regdump[13];
    vu32 *stackdump = (vu32 *)((vu8 *)FINAL_BUFFER + 40 + REG_DUMP_SIZE + CODE_DUMP_SIZE);
    for(u32 i = 0; i < dump[8] / 4; i++)
        stackdump[i] = sp[i];

    vu8 *currentKProcess = *(vu8 **)0xFFFF9004;
    vu8 *currentKCodeSet = (currentKProcess != NULL) ? *(vu8 **)(currentKProcess + CODESET_OFFSET) : NULL;
    if(currentKCodeSet != NULL)
    {
        dump[9] = 16;                                                                   //Additional data size
        vu32 *additionalData = (vu32 *)((vu8 *)FINAL_BUFFER + 40 + REG_DUMP_SIZE + CODE_DUMP_SIZE + dump[8]);
        
        additionalData[0] = *(vu32 *)(currentKCodeSet + 0x50); //Process name
        additionalData[1] = *(vu32 *)(currentKCodeSet + 0x54);
        
        additionalData[2] = *(vu32 *)(currentKCodeSet + 0x5C); //Title ID
        additionalData[3] = *(vu32 *)(currentKCodeSet + 0x60);  
    }
    else
        dump[9] = 0;
    
    dump[5] = 40 + REG_DUMP_SIZE + CODE_DUMP_SIZE + dump[8] + dump[9];          //Total size
    for(u32 i = 0; i < (40 + REG_DUMP_SIZE + CODE_DUMP_SIZE) / 4; i++)
        final[i] = dump[i];
        
    clearDCacheAndDMB();
    mcuReboot();
}
