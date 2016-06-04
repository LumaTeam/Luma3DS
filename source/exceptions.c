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

void installArm9Handlers(void)
{
    void *payloadAddress = (void *)0x01FF8000;    
    const u32 offsets[] = {0x08, 0x18, 0x20, 0x28};
    
    memcpy(payloadAddress, arm9_exceptions + 32, arm9_exceptions_size - 32);

    //IRQHandler is at 0x08000000, but we won't handle it for some reasons
    //svcHandler is at 0x08000010, but we won't handle svc either

    for(u32 i = 0; i < 4; i++)
    {
        *(vu32 *)(0x08000000 + offsets[i]) = 0xE51FF004;
        *(vu32 *)(0x08000000 + offsets[i] + 4) = *((const u32 *)arm9_exceptions + 1 + i);
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
    mcuReboot--;

    u32 *freeSpace;
    for(freeSpace = initFPU;  freeSpace < (exceptionsPage + 0x400) && (freeSpace[0] != 0xFFFFFFFF || freeSpace[1] != 0xFFFFFFFF); freeSpace += 1);
    memcpy(freeSpace, arm11_exceptions + 32, arm11_exceptions_size - 32);
    
    exceptionsPage[1] = MAKE_BRANCH(exceptionsPage + 1, (u8 *)freeSpace + *(u32 *)(arm11_exceptions + 8)  - 32);    //Undefined Instruction
    exceptionsPage[3] = MAKE_BRANCH(exceptionsPage + 3, (u8 *)freeSpace + *(u32 *)(arm11_exceptions + 12) - 32);    //Prefetch Abort
    exceptionsPage[4] = MAKE_BRANCH(exceptionsPage + 4, (u8 *)freeSpace + *(u32 *)(arm11_exceptions + 16) - 32);    //Data Abort
    exceptionsPage[7] = MAKE_BRANCH(exceptionsPage + 7, (u8 *)freeSpace + *(u32 *)(arm11_exceptions + 4)  - 32);    //FIQ
    
    for(u32 *pos = freeSpace; pos < (u32 *)((u8 *)freeSpace + arm11_exceptions_size - 32); pos++)
    {
        switch(*pos) //Perform relocations
        {
            case 0xFFFF3000: *pos = stackAddr; break;
            case 0xEBFFFFFE: *pos = MAKE_BRANCH_LINK(pos, initFPU); break;
            case 0xEAFFFFFE: *pos = MAKE_BRANCH(pos, mcuReboot); break;
            case 0xE12FFF1C: pos[1] = 0xFFFF0000 + 4 * (u32)(freeSpace - exceptionsPage) + pos[1] - 32; break; // bx r12 (mainHandler)
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
        u32 size = dump[5];

        if(dump[3] == 9)
        {
            findDumpFile(path9, fileName);
            path9[16] = '/';
            memcpy(&path9[17], fileName, sizeof(fileName));
            fileWrite((void *)dump, path9, size);
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
        if(dump[4] == 2 && dump[7] >= 4 && (dump[10 + 16] & 0x20) == 0 && *(vu32 *)((vu8 *)dump + 40 + dump[6] + dump[7] - 4) == 0xE12FFF7F)
            posY = drawString("(svcBreak)", 10 + 31 * SPACING_X, posY, COLOR_WHITE);
        
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
        
        u32 mode = dump[10 + 16] & 0xF;
        if(dump[4] == 3 && (mode == 7 || mode == 11))
            posY = drawString("Incorrect dump: failed to dump code and/or stack", 10, posY, 0x00FFFF) + 2 * SPACING_Y; //in yellow
            
        posY = drawString("You can find a dump in the following file:", 10, posY, COLOR_WHITE) + SPACING_Y;
        posY = drawString((dump[3] == 9) ? path9 : path11, 10, posY, COLOR_WHITE) + 2 * SPACING_Y;
        drawString("Press any button to shutdown", 10, posY, COLOR_WHITE);

        waitInput();
        
        for(u32 i = 0; i < size / 4; i++) dump[i] = 0; 
        
        clearScreens();
        
        i2cWriteRegister(I2C_DEV_MCU, 0x20, 1); //Shutdown
        while(1);
    }
}