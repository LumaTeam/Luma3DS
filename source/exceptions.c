/*
*   exceptions.c
*       by TuxSH
*/

#include "exceptions.h"
#include "fs.h"
#include "memory.h"
#include "screeninit.h"
#include "draw.h"
#include "i2c.h"
#include "utils.h"
#include "../build/arm9_exceptions.h"
#include "../build/arm11_exceptions.h"


#define _U __attribute__((unused)) //Silence "unused parameter" warnings
static void __attribute__((naked)) setupStack(_U u32 mode, _U void* SP)
{
    __asm__ volatile(
            "cmp r0, #0                                         \n"
            "moveq r0, #0xf          @ usr => sys               \n"
            "mrs r2, cpsr                                       \n"
            "bic r3, r2, #0xf                                   \n"
            "orr r3, r0              @ processor mode           \n"
            "msr cpsr_c, r3          @ change processor mode    \n"
            "mov sp, r1                                         \n"
            "msr cpsr_c, r2          @ restore processor mode   \n"
            "bx lr                                              \n"
    );
}
#undef _U

void installArm9Handlers(void)
{
    void *payloadAddress = (void *)0x01FF8000;
    u32 *handlers = (u32 *)payloadAddress + 1;
    
    void* SP = (void *)0x02000000; //We make the (full descending) stack point to the end of ITCM for our exception handlers. 
                                   //It doesn't matter if we're overwriting stuff, since we're going to reboot.
    
    memcpy(payloadAddress, arm9_exceptions, arm9_exceptions_size);
    
    setupStack(1, SP);  //FIQ
    setupStack(7, SP);  //Abort
    setupStack(11, SP); //Undefined

    const u32 offsets[] = {0x08, 0x18, 0x20, 0x28};
    
    //IRQHandler is at 0x08000000, but we won't handle it for some reasons
    //svcHandler is at 0x08000010, but we won't handle svc either

    for(u32 i = 0; i < 4; i++)
    {
        *(vu32 *)(0x08000000 + offsets[i]) = 0xE51FF004;
        *(vu32 *)(0x08000000 + offsets[i] + 4) = handlers[i];
    }
}

#define MAKE_BRANCH(src,dst) (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))
#define MAKE_BRANCH_LINK(src,dst) (0xEB000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))

void installArm11Handlers(u32 *exceptionsPage, u32 stackAddr)
{
    u32 *initFPU;
    for(initFPU = exceptionsPage; initFPU < (exceptionsPage + 0x400) && (initFPU[0] != 0xE59F0008 || initFPU[1] != 0xE5900000); initFPU += 1);
    
    u32 *mcuReboot;
    for(mcuReboot = exceptionsPage; mcuReboot < (exceptionsPage + 0x400) && (mcuReboot[0] != 0xE59F4104 || mcuReboot[1] != 0xE3A0A0C2); mcuReboot += 1);

    u32 *freeSpace;
    for(freeSpace = initFPU;  freeSpace < (exceptionsPage + 0x400) && (freeSpace[0] != 0xFFFFFFFF || freeSpace[1] != 0xFFFFFFFF); freeSpace += 1);
    //freeSpace += 4 - ((u32)(freeSpace - exceptionsPage) & 3);
    memcpy(freeSpace, arm11_exceptions + 20, arm11_exceptions_size - 20);
    
    exceptionsPage[1] = MAKE_BRANCH(exceptionsPage + 1, (u8 *)freeSpace + *(u32 *)(arm11_exceptions + 8)  - 20);    //Undefined Instruction
    exceptionsPage[3] = MAKE_BRANCH(exceptionsPage + 3, (u8 *)freeSpace + *(u32 *)(arm11_exceptions + 12) - 20);    //Prefetch Abort
    exceptionsPage[4] = MAKE_BRANCH(exceptionsPage + 4, (u8 *)freeSpace + *(u32 *)(arm11_exceptions + 16) - 20);    //Data Abort
    exceptionsPage[7] = MAKE_BRANCH(exceptionsPage + 7, (u8 *)freeSpace + *(u32 *)(arm11_exceptions + 4)  - 20);    //FIQ
    
    for(u32 *pos = freeSpace; pos < (u32 *)((u8 *)freeSpace + arm11_exceptions_size - 20); pos++)
    {
        switch(*pos)
        {
            case 0xFFFF3000: *pos = stackAddr; break;
            case 0xEBFFFFFE: *pos = MAKE_BRANCH_LINK(pos, initFPU); break;
            case 0xEAFFFFFE: *pos = MAKE_BRANCH(pos, mcuReboot); break;
            case 0xE12FFF1C: pos[1] = 0xFFFF0000 + 4 * (u32)(freeSpace - exceptionsPage) + pos[1] - 20; break; // bx r12 (mainHandler)
            default: break;
        }
    }
}

static void hexItoa(u32 n, char *out)
{
    const char hexDigits[] = "0123456789ABCDEF";
    u32 i = 0;

    while(n > 0)
    {
        out[7 - i++] = hexDigits[n & 0xF];
        n >>= 4;
    }

    for(; i < 8; i++) out[7 - i] = '0';
}

void detectAndProcessExceptionDumps(void)
{
    const char *handledExceptionNames[] = { 
        "FIQ", "undefined instruction", "prefetch abort", "data abort"
    };
    
    const char *registerNames[] = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12",
        "SP", "LR", "PC", "CPSR", "FPEXC"
    };
    
    char hexstring[] = "00000000";
    
    vu32 *dump = (vu32 *)0x25000000;

    if(dump[0] == 0xDEADC0DE && dump[1] == 0xDEADCAFE && (dump[3] == 9 || (dump[3] & 0xFFFF) == 11))
    {
        char path9[41] = "/luma/dumps/arm9";
        char path11[42] = "/luma/dumps/arm11";
        char fileName[] = "crash_dump_00000000.dmp";

        if(dump[3] == 9)
        {
            findDumpFile(path9, fileName);
            path9[16] = '/';
            memcpy(&path9[17], fileName, sizeof(fileName));
            fileWrite((void *)dump, path9, dump[5]);
        }
        
        else
        {
            findDumpFile(path11, fileName);
            path11[17] = '/';
            memcpy(&path11[18], fileName, sizeof(fileName));
            fileWrite((void *)dump, path11, dump[5]);
        }
        

        char arm11Str[] = "Processor:      ARM11 (core X)";
        if((dump[3] & 0xFFFF) == 11) arm11Str[28] = '0' + (char)(dump[3] >> 16);
        
        initScreens();

        drawString("An exception occurred", 10, 10, COLOR_RED);
        int posY = drawString(((dump[3] & 0xFFFF) == 11) ? arm11Str : "Processor:      ARM9", 10, 30, COLOR_WHITE) + SPACING_Y;
        posY = drawString("Exception type: ", 10, posY, COLOR_WHITE);
        posY = drawString(handledExceptionNames[dump[4]], 10 + 16 * SPACING_X, posY, COLOR_WHITE);

        posY += 3 * SPACING_Y;
        for(u32 i = 0; i < 17; i += 2)
        {
            posY = drawString(registerNames[i], 10, posY, COLOR_WHITE);
            hexItoa(dump[10 + i], hexstring);
            posY = drawString(hexstring, 10 + 7 * SPACING_X, posY, COLOR_WHITE);

            if(dump[3] != 9 || i != 16)
            {
                posY = drawString(registerNames[i + 1], 10 + 22 * SPACING_X, posY, COLOR_WHITE);
                hexItoa(dump[10 + i + 1], hexstring);
                posY = drawString(hexstring, 10 + 29 * SPACING_X, posY, COLOR_WHITE);
            }

            posY += SPACING_Y;
        }

        posY += 2 * SPACING_Y;
        posY = drawString("You can find a dump in the following file:", 10, posY, COLOR_WHITE) + SPACING_Y;
        posY = drawString((dump[3] == 9) ? path9 : path11, 10, posY, COLOR_WHITE) + 2 * SPACING_Y;
        drawString("Press any button to shutdown", 10, posY, COLOR_WHITE);

        waitInput();

        i2cWriteRegister(I2C_DEV_MCU, 0x20, 1);
        while(1);
    }
}