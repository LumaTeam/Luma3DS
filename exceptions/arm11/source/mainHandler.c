/*
*   mainHandler.c
*       by TuxSH
*
*   This is part of Luma3DS, see LICENSE.txt for details
*/

#include "handlers.h"

#define FINAL_BUFFER    0xE5000000 //0x25000000

#define REG_DUMP_SIZE   (4*23)
#define CODE_DUMP_SIZE  48

#define CODESET_OFFSET  0xBEEFBEEF

void __attribute__((noreturn)) mainHandler(u32 regs[REG_DUMP_SIZE / 4], u32 type, u32 cpuId)
{
    ExceptionDumpHeader dumpHeader;
    
    u32 registerDump[REG_DUMP_SIZE / 4];
    u8 codeDump[CODE_DUMP_SIZE];
    vu32 *final = (vu32 *)FINAL_BUFFER;

    while(final[0] == 0xDEADC0DE && final[1] == 0xDEADCAFE && (final[3] == 9 || (final[3] & 0xFFFF) == 11));
    
    dumpHeader.magic[0] = 0xDEADC0DE;
    dumpHeader.magic[1] = 0xDEADCAFE;
    dumpHeader.versionMajor = 1;
    dumpHeader.versionMinor = 1;
    
    dumpHeader.processor = 11;
    dumpHeader.core = cpuId & 0xF;
    dumpHeader.type = type;
    
    dumpHeader.registerDumpSize = REG_DUMP_SIZE;
    dumpHeader.codeDumpSize = CODE_DUMP_SIZE;
    dumpHeader.additionalDataSize = 0;

    //Dump registers
    //Current order of saved regs: dfsr, ifsr, far, fpexc, fpinst, fpinst2, cpsr, pc, r8-r12, sp, lr, r0-r7
    u32 cpsr = regs[6];
    u32 pc   = regs[7] - ((type < 3) ? (((cpsr & 0x20) != 0 && type == 1) ? 2 : 4) : 8);

    registerDump[15] = pc;
    registerDump[16] = cpsr;
    for(u32 i = 0; i < 6; i++) registerDump[17 + i] = regs[i];
    for(u32 i = 0; i < 7; i++) registerDump[8 + i]  = regs[8 + i];
    for(u32 i = 0; i < 8; i++) registerDump[i]      = regs[15 + i]; 
    
    dumpHeader.stackDumpSize = 0x1000 - (registerDump[13] & 0xfff);
    
    //Dump code
    vu8 *instr = (vu8 *)pc + ((cpsr & 0x20) ? 2 : 4) - dumpHeader.codeDumpSize; //Doesn't work well on 32-bit Thumb instructions, but it isn't much of a problem
    for(u32 i = 0; i < dumpHeader.codeDumpSize; i++)
        codeDump[i] = instr[i];
        
    //Copy register dump and code dump 
    final = (vu32 *)(FINAL_BUFFER + sizeof(ExceptionDumpHeader));
    
    for(u32 i = 0; i < dumpHeader.registerDumpSize / 4; i++)
        *final++ = registerDump[i];
    
    for(u32 i = 0; i < dumpHeader.codeDumpSize / 4; i++)
        *final++ = *((u32 *)codeDump + i);
        
    //Dump stack in place
    vu32 *sp = (vu32 *)registerDump[13];
    for(u32 i = 0; i < dumpHeader.stackDumpSize / 4; i++)
        *final++ = sp[i];


    vu8 *currentKProcess = *(vu8 **)0xFFFF9004;
    vu8 *currentKCodeSet = (currentKProcess != NULL) ? *(vu8 **)(currentKProcess + CODESET_OFFSET) : NULL;
    if(currentKCodeSet != NULL)
    {
        vu32 *additionalData = final;
        dumpHeader.additionalDataSize = 16;
        
        additionalData[0] = *(vu32 *)(currentKCodeSet + 0x50); //Process name
        additionalData[1] = *(vu32 *)(currentKCodeSet + 0x54);
        
        additionalData[2] = *(vu32 *)(currentKCodeSet + 0x5C); //Title ID
        additionalData[3] = *(vu32 *)(currentKCodeSet + 0x60);  
    }
    else
        dumpHeader.additionalDataSize = 16;

    //Copy header (actually optimized by the compiler)
    final = (vu32 *)FINAL_BUFFER;
    dumpHeader.totalSize = sizeof(ExceptionDumpHeader) + dumpHeader.registerDumpSize + dumpHeader.codeDumpSize + dumpHeader.stackDumpSize + dumpHeader.additionalDataSize;
    *(ExceptionDumpHeader *)final = dumpHeader;

    
    cleanInvalidateDCacheAndDMB();
    mcuReboot(); //Also contains DCache-cleaning code
}
