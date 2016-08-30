/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

#include "exceptions.h"
#include "fs.h"
#include "strings.h"
#include "memory.h"
#include "screen.h"
#include "draw.h"
#include "i2c.h"
#include "utils.h"
#include "../build/arm9_exceptions.h"
#include "../build/arm11_exceptions.h"

void installArm9Handlers(void)
{
    const u32 offsets[] = {0x08, 0x18, 0x20, 0x28};
    
    memcpy((void *)0x01FF8000, arm9_exceptions + 32, arm9_exceptions_size - 32);

    //IRQHandler is at 0x08000000, but we won't handle it for some reasons
    //svcHandler is at 0x08000010, but we won't handle svc either

    for(u32 i = 0; i < 4; i++)
    {
        *(vu32 *)(0x08000000 + offsets[i]) = 0xE51FF004;
        *(vu32 *)(0x08000000 + offsets[i] + 4) = *((const u32 *)arm9_exceptions + 1 + i);
    }
}

void installArm11Handlers(u32 *exceptionsPage, u32 stackAddr, u32 codeSetOffset)
{
    u32 *initFPU;
    for(initFPU = exceptionsPage; initFPU < (exceptionsPage + 0x400) && (initFPU[0] != 0xE59F0008 || initFPU[1] != 0xE5900000); initFPU++);
    
    u32 *mcuReboot;
    for(mcuReboot = exceptionsPage; mcuReboot < (exceptionsPage + 0x400) && (mcuReboot[0] != 0xE59F4104 || mcuReboot[1] != 0xE3A0A0C2); mcuReboot++);
    mcuReboot--;

    u32 *freeSpace;
    for(freeSpace = initFPU; freeSpace < (exceptionsPage + 0x400) && (freeSpace[0] != 0xFFFFFFFF || freeSpace[1] != 0xFFFFFFFF); freeSpace++);

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
            case 0xE12FFF1C: pos[1] = 0xFFFF0000 + 4 * (u32)(freeSpace - exceptionsPage) + pos[1] - 32; break; //bx r12 (mainHandler)
            case 0xBEEFBEEF: *pos = codeSetOffset; break;
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
    volatile ExceptionDumpHeader *dumpHeader = (volatile ExceptionDumpHeader *)0x25000000;

    if(dumpHeader->magic[0] == 0xDEADC0DE && dumpHeader->magic[1] == 0xDEADCAFE && (dumpHeader->processor == 9 || dumpHeader->processor == 11))
    {
        char path[42];
        char fileName[] = "crash_dump_00000000.dmp";
        u32 size = dumpHeader->totalSize;

        char *pathFolder = dumpHeader->processor == 9 ? "/luma/dumps/arm9" : "/luma/dumps/arm11";

        findDumpFile(pathFolder, fileName);
        memcpy(path, pathFolder, strlen(pathFolder));
        concatenateStrings(path, "/");
        concatenateStrings(path, fileName);

        fileWrite((void *)dumpHeader, path, size);

        vu32 *regs = (vu32 *)((vu8 *)dumpHeader + sizeof(ExceptionDumpHeader));
        vu8 *additionalData = (vu8 *)dumpHeader + dumpHeader->totalSize - dumpHeader->additionalDataSize;

        const char *handledExceptionNames[] = { 
            "FIQ", "undefined instruction", "prefetch abort", "data abort"
        };
    
        const char *registerNames[] = {
            "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12",
            "SP", "LR", "PC", "CPSR", "FPEXC"
        };
    
        char hexstring[] = "00000000";

        char arm11Str[] = "Processor:       ARM11 (core X)";
        if(dumpHeader->processor == 11) arm11Str[29] = '0' + (char)(dumpHeader->core);

        initScreens();

        drawString("An exception occurred", 10, 10, COLOR_RED);
        int posY = drawString(dumpHeader->processor == 11 ? arm11Str : "Processor:       ARM9", 10, 30, COLOR_WHITE) + SPACING_Y;
        
        posY = drawString("Exception type:  ", 10, posY, COLOR_WHITE);
        posY = drawString(handledExceptionNames[dumpHeader->type], 10 + 17 * SPACING_X, posY, COLOR_WHITE);
        
        if(dumpHeader->type == 2)
        {
            if((regs[16] & 0x20) == 0 && dumpHeader->codeDumpSize >= 4)
            {
                u32 instr = *(vu32 *)((vu8 *)dumpHeader + sizeof(ExceptionDumpHeader) + dumpHeader->registerDumpSize + dumpHeader->codeDumpSize - 4);
                if(instr == 0xE12FFF7E)
                    posY = drawString("(kernel panic)", 10 + 32 * SPACING_X, posY, COLOR_WHITE);
                else if(instr == 0xEF00003C)
                    posY = drawString("(svcBreak)", 10 + 32 * SPACING_X, posY, COLOR_WHITE);
            }
            else if((regs[16] & 0x20) == 0 && dumpHeader->codeDumpSize >= 2)
            {
                u16 instr = *(vu16 *)((vu8 *)dumpHeader + sizeof(ExceptionDumpHeader) + dumpHeader->registerDumpSize + dumpHeader->codeDumpSize - 2);
                if(instr == 0xDF3C)
                    posY = drawString("(svcBreak)", 10 + 32 * SPACING_X, posY, COLOR_WHITE);
            }
        }

        if(dumpHeader->processor == 11 && dumpHeader->additionalDataSize != 0)
        {
            posY += SPACING_Y;
            char processNameStr[] = "Current process: --------";
            memcpy(processNameStr + 17, (char *)additionalData, 8);
            posY = drawString(processNameStr, 10, posY, COLOR_WHITE);
        }
        
        posY += 3 * SPACING_Y;

        for(u32 i = 0; i < 17; i += 2)
        {
            posY = drawString(registerNames[i], 10, posY, COLOR_WHITE);
            hexItoa(regs[i], hexstring);
            posY = drawString(hexstring, 10 + 7 * SPACING_X, posY, COLOR_WHITE);

            if(dumpHeader->processor != 9 || i != 16)
            {
                posY = drawString(registerNames[i + 1], 10 + 22 * SPACING_X, posY, COLOR_WHITE);
                hexItoa(i == 16 ? regs[20] : regs[i + 1], hexstring);
                posY = drawString(hexstring, 10 + 29 * SPACING_X, posY, COLOR_WHITE);
            }

            posY += SPACING_Y;
        }

        posY += 2 * SPACING_Y;
        
        u32 mode = regs[16] & 0xF;
        if(dumpHeader->type == 3 && (mode == 7 || mode == 11))
        {
            posY = drawString("Incorrect dump: failed to dump code and/or stack", 10, posY, 0x00FFFF) + 2 * SPACING_Y; //In yellow
            if(dumpHeader->processor != 9) posY -= SPACING_Y;
        }
            
        posY = drawString("You can find a dump in the following file:", 10, posY, COLOR_WHITE) + SPACING_Y;
        posY = drawString(path, 10, posY, COLOR_WHITE) + 2 * SPACING_Y;
        drawString("Press any button to shutdown", 10, posY, COLOR_WHITE);

        waitInput();

        memset32((void *)dumpHeader, 0, size); 

        mcuPowerOff();
    }
}