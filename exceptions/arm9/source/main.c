/*
*   main.c
*       by TuxSH
*
*   This is part of Luma3DS, see LICENSE.txt for details
*/

#include "types.h"
#include "i2c.h"
#include "handlers.h"

#define TEMP_BUFFER     0x1FF80000 //We choose AXIWRAM as tmp buffer since it's usually ARM11 payloads running there
#define FINAL_BUFFER    0x25000000
#define STACK_DUMP_SIZE 0x2000
#define SP              ((void *)(TEMP_BUFFER + 4 * STACK_DUMP_SIZE))

void __attribute__((noreturn)) mainHandler(u32 regs[17], u32 type)
{
    vu32 *dump = (u32 *)TEMP_BUFFER;

    dump[0] = 0xDEADC0DE;               //Magic
    dump[1] = 0xDEADCAFE;               //Magic
    dump[2] = (1 << 16) | 0;            //Dump format version number
    dump[3] = 9;                        //Processor
    dump[4] = type;                     //Exception type
    dump[6] = 4 * 17;                   //Register dump size (r0-r12, sp, lr, pc, cpsr)
    dump[7] = 40;                       //Code dump size (10 ARM instructions, up to 20 Thumb instructions).
    dump[8] = STACK_DUMP_SIZE;          //Stack dump size
    dump[9] = 0;                        //Other data size
    dump[5] = 40 + dump[6] + dump[7] + dump[8] + dump[9]; //Total size

    //Dump registers
    //Current order of regs: pc, spsr, r8-r12, sp, lr, r0-r7
    vu32 *regdump = dump + 10;

    u32 cpsr = regs[1];
    u32 pc   = regs[0] + (((cpsr & 0x20) != 0 && type == 1) ? 2 : 0);

    regdump[15] = pc;
    regdump[16] = cpsr;

    for(u32 i = 0; i < 7; i++)
        regdump[8 + i] = regs[2 + i];

    for(u32 i = 0; i < 8; i++)
        regdump[i] = regs[9 + i]; 

    //Dump code
    vu16 *codedump = (vu16 *)(regdump + dump[6] / 4);
    vu16 *instr = (vu16 *)pc - dump[7] / 2 + 1;
    for(u32 i = 0; i < dump[7] / 2; i++)
        codedump[i] = instr[i];

    //Dump stack
    vu32 *sp = (vu32 *)regdump[13];
    vu32 *stackdump = (vu32 *)(codedump + dump[7] / 2);
    /* Homebrew/CFW set their stack at 0x27000000, but we'd better not make any assumption here
       as it breaks things it seems */
    for(u32 i = 0; i < dump[8] / 4; i++)
        stackdump[i] = sp[i];

    vu32 *final = (u32 *)FINAL_BUFFER;
    for(u32 i = 0; i < dump[5] / 4; i++)
        final[i] = dump[i];

    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2); //Reboot
    while(1);
}

void main(void)
{
    setupStack(1, SP);  //FIQ
    setupStack(7, SP);  //Abort
    setupStack(11, SP); //Undefined

    *(vu32 *)0x08000004 = 0xE51FF004;   
    *(vu32 *)0x08000008 = (u32)FIQHandler;
    *(vu32 *)0x08000014 = 0xE51FF004;   
    *(vu32 *)0x08000018 = (u32)undefinedInstructionHandler;
    *(vu32 *)0x0800001C = 0xE51FF004;   
    *(vu32 *)0x08000020 = (u32)prefetchAbortHandler;
    *(vu32 *)0x08000028 = 0xE51FF004;
    *(vu32 *)0x0800002C = (u32)dataAbortHandler;
}