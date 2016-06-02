/*
*   mainHandler.c
*       by TuxSH
*
*   This is part of Luma3DS, see LICENSE.txt for details
*/

#include "types.h"
#include "handlers.h"

#define FINAL_BUFFER    0xE5000000 //0x25000000

#define REG_DUMP_SIZE   (4*18)
#define CODE_DUMP_SIZE  48
#define STACK_DUMP_SIZE 0x2000
#define OTHER_DATA_SIZE 0

void __attribute__((noreturn)) mainHandler(u32 regs[18], u32 type, u32 cpuId, u32 fpexc)
{
    u32 dump[(40 + REG_DUMP_SIZE + CODE_DUMP_SIZE) / 4];
    vu32 *final = (vu32 *)FINAL_BUFFER;

    while(final[0] == 0xDEADC0DE && final[1] == 0xDEADCAFE && ((final[3] & 0xFFFF) == 9 || (final[3] & 0xFFFF) == 11));
    
    dump[0] = 0xDEADC0DE;                                                               //Magic
    dump[1] = 0xDEADCAFE;                                                               //Magic
    dump[2] = (1 << 16) | 0;                                                            //Dump format version number
    dump[3] = ((cpuId & 0xf) << 16) | 11;                                               //Processor
    dump[4] = type;                                                                     //Exception type
    dump[6] = REG_DUMP_SIZE;                                                            //Register dump size (r0-r12, sp, lr, pc, cpsr, fpexc)
    dump[7] = CODE_DUMP_SIZE;                                                           //Code dump size (10 ARM instructions, up to 20 Thumb instructions).
    dump[8] = STACK_DUMP_SIZE;                                                          //Stack dump size
    dump[9] = OTHER_DATA_SIZE;                                                          //Other data size
    dump[5] = 40 + REG_DUMP_SIZE + CODE_DUMP_SIZE + STACK_DUMP_SIZE + OTHER_DATA_SIZE;  //Total size

    //Dump registers
    //Current order of saved regs: cpsr, pc, r8-r12, sp, lr, r0-r7
    u32 *regdump = dump + 10;

    u32 cpsr = regs[0];
    u32 pc   = regs[1] - ((type < 3) ? (((cpsr & 0x20) != 0 && type == 1) ? 2 : 4) : 8);

    regdump[15] = pc;
    regdump[16] = cpsr;
    regdump[17] = fpexc;

    for(u32 i = 0; i < 7; i++)
        regdump[8 + i] = regs[2 + i];

    for(u32 i = 0; i < 8; i++)
        regdump[i] = regs[9 + i]; 

    //Dump code
    u16 *codedump = (u16 *)(regdump + dump[6] / 4);
    u16 *instr = (u16 *)pc - dump[7] / 2 + 1;
    for(u32 i = 0; i < dump[7] / 2; i++)
        codedump[i] = instr[i];

    //Dump stack in place
    vu32 *sp = (vu32 *)regdump[13];
    vu32 *stackdump = (vu32 *)((vu8 *)FINAL_BUFFER + 40 + REG_DUMP_SIZE + CODE_DUMP_SIZE);
    
    for(u32 i = 0; i < dump[8] / 4; i++)
        stackdump[i] = sp[i];

    for(u32 i = 0; i < (40 + REG_DUMP_SIZE + CODE_DUMP_SIZE) / 4; i++)
        final[i] = dump[i];
        
    while(final[0] != 0xDEADC0DE);
    mcuReboot();
}
