/*
*   mainHandler.c
*       by TuxSH
*
*   This is part of Luma3DS, see LICENSE.txt for details
*/

#include "types.h"
#include "i2c.h"
#include "handlers.h"

#define FINAL_BUFFER    0x25000000

#define REG_DUMP_SIZE   (4*17)
#define CODE_DUMP_SIZE  48
#define OTHER_DATA_SIZE 0

void __attribute__((noreturn)) mainHandler(u32 regs[17], u32 type)
{
    //vu32 *dump = (u32 *)TEMP_BUFFER;
    u32 dump[(40 + REG_DUMP_SIZE + CODE_DUMP_SIZE) / 4];
    
    dump[0] = 0xDEADC0DE;               //Magic
    dump[1] = 0xDEADCAFE;               //Magic
    dump[2] = (1 << 16) | 0;            //Dump format version number
    dump[3] = 9;                        //Processor
    dump[4] = type;                     //Exception type
    dump[6] = REG_DUMP_SIZE;            //Register dump size (r0-r12, sp, lr, pc, cpsr)
    dump[7] = CODE_DUMP_SIZE;           //Code dump size (10 ARM instructions, up to 20 Thumb instructions).
    dump[9] = OTHER_DATA_SIZE;          //Other data size

    //Dump registers
    //Current order of saved regs: cpsr, pc, r8-r12, sp, lr, r0-r7
    vu32 *regdump = dump + 10;

    u32 cpsr = regs[0];
    u32 pc   = regs[1] - ((type < 3) ? (((cpsr & 0x20) != 0 && type == 1) ? 2 : 4) : 8);

    regdump[15] = pc;
    regdump[16] = cpsr;

    for(u32 i = 0; i < 7; i++)
        regdump[8 + i] = regs[2 + i];

    for(u32 i = 0; i < 8; i++)
        regdump[i] = regs[9 + i]; 

    dump[8] = 0x1000 - (regdump[13] & 0xfff);                                           //Stack dump size (max. 0x1000 bytes)
    dump[5] = 40 + REG_DUMP_SIZE + CODE_DUMP_SIZE + dump[8] + OTHER_DATA_SIZE;          //Total size
    
    //Dump code
    u16 *codedump = (u16 *)(regdump + dump[6] / 4);
    vu16 *instr = (vu16 *)pc - dump[7] / 2 + 1;
    for(u32 i = 0; i < dump[7] / 2; i++)
        codedump[i] = instr[i];

    //Dump stack
    vu32 *sp = (vu32 *)regdump[13];
    vu32 *stackdump = (vu32 *)((vu8 *)FINAL_BUFFER + 40 + REG_DUMP_SIZE + CODE_DUMP_SIZE);
    for(u32 i = 0; i < dump[8] / 4; i++)
        stackdump[i] = sp[i];

    vu32 *final = (vu32 *)FINAL_BUFFER;
    for(u32 i = 0; i < (40 + REG_DUMP_SIZE + CODE_DUMP_SIZE) / 4; i++)
        final[i] = dump[i];

    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2); //Reboot
    while(1);
}