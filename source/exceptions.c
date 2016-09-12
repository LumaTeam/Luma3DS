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

#ifdef DEV
#include "exceptions.h"
#include "fs.h"
#include "strings.h"
#include "memory.h"
#include "screen.h"
#include "draw.h"
#include "utils.h"
#include "../build/arm9_exceptions.h"
#include "../build/arm11_exceptions.h"

void installArm9Handlers(void)
{
    const u32 offsets[] = {0x08, 0x18, 0x20, 0x28};

    memcpy((void *)0x01FF8000, arm9_exceptions + 32, arm9_exceptions_size - 32);

    /* IRQHandler is at 0x08000000, but we won't handle it for some reasons
       svcHandler is at 0x08000010, but we won't handle svc either */

    for(u32 i = 0; i < 4; i++)
    {
        *(vu32 *)(0x08000000 + offsets[i]) = 0xE51FF004;
        *(vu32 *)(0x08000000 + offsets[i] + 4) = *((u32 *)arm9_exceptions + 1 + i);
    }
}

void installArm11Handlers(u32 *exceptionsPage, u32 stackAddress, u32 codeSetOffset)
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
            case 0xFFFF3000: *pos = stackAddress; break;
            case 0xEBFFFFFE: *pos = MAKE_BRANCH_LINK(pos, initFPU); break;
            case 0xEAFFFFFE: *pos = MAKE_BRANCH(pos, mcuReboot); break;
            case 0xE12FFF1C: pos[1] = 0xFFFF0000 + 4 * (u32)(freeSpace - exceptionsPage) + pos[1] - 32; break; //bx r12 (mainHandler)
            case 0xBEEFBEEF: *pos = codeSetOffset; break;
            default: break;
        }
    }
}

void detectAndProcessExceptionDumps(void)
{
    volatile ExceptionDumpHeader *dumpHeader = (volatile ExceptionDumpHeader *)0x25000000;

    if(dumpHeader->magic[0] == 0xDEADC0DE && dumpHeader->magic[1] == 0xDEADCAFE && (dumpHeader->processor == 9 || dumpHeader->processor == 11))
    {
        const vu32 *regs = (vu32 *)((vu8 *)dumpHeader + sizeof(ExceptionDumpHeader));
        const vu8 *additionalData = (vu8 *)dumpHeader + dumpHeader->totalSize - dumpHeader->additionalDataSize;

        const char *handledExceptionNames[] = { 
            "FIQ", "undefined instruction", "prefetch abort", "data abort"
        };

        const char *registerNames[] = {
            "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12",
            "SP", "LR", "PC", "CPSR", "FPEXC"
        };

        char hexString[] = "00000000";

        initScreens();

        drawString("An exception occurred", 10, 10, COLOR_RED);
        int posY = drawString(dumpHeader->processor == 11 ? "Processor:       ARM11 (core  )" : "Processor:       ARM9", 10, 30, COLOR_WHITE) + SPACING_Y;
        if(dumpHeader->processor == 11) drawCharacter('0' + dumpHeader->core, 10 + 29 * SPACING_X, 30, COLOR_WHITE);

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
            char processName[] = "Current process:         ";
            memcpy(processName + sizeof(processName) - 9, (void *)additionalData, 8);
            posY = drawString(processName, 10, posY, COLOR_WHITE);
        }

        posY += 3 * SPACING_Y;

        for(u32 i = 0; i < 17; i += 2)
        {
            posY = drawString(registerNames[i], 10, posY, COLOR_WHITE);
            hexItoa(regs[i], hexString, 8);
            posY = drawString(hexString, 10 + 7 * SPACING_X, posY, COLOR_WHITE);

            if(dumpHeader->processor != 9 || i != 16)
            {
                posY = drawString(registerNames[i + 1], 10 + 22 * SPACING_X, posY, COLOR_WHITE);
                hexItoa(i == 16 ? regs[20] : regs[i + 1], hexString, 8);
                posY = drawString(hexString, 10 + 29 * SPACING_X, posY, COLOR_WHITE);
            }

            posY += SPACING_Y;
        }

        posY += 2 * SPACING_Y;

        u32 mode = regs[16] & 0xF;
        if(dumpHeader->type == 3 && (mode == 7 || mode == 11))
        {
            posY = drawString("Incorrect dump: failed to dump code and/or stack", 10, posY, COLOR_YELLOW) + 2 * SPACING_Y;
            if(dumpHeader->processor != 9) posY -= SPACING_Y;
        }

        selectScreen(true);
        
        int posYBottom = drawString("Stack dump:", 10, 10, COLOR_WHITE) + 2 * SPACING_Y;
        vu8 *stackDump = (vu8 *)dumpHeader + sizeof(ExceptionDumpHeader) + dumpHeader->registerDumpSize + dumpHeader->codeDumpSize + dumpHeader->stackDumpSize + dumpHeader->additionalDataSize;
        u32 stackBytePos = 0;
        for(u32 line = 0; line < 19 && stackBytePos < dumpHeader->stackDumpSize; line++)
        {
            hexItoa(regs[13] + 8 * line, hexString, 8);
            drawString(hexString, 10, posYBottom, COLOR_WHITE);
            drawCharacter(':', 10 + 8 * SPACING_X, posYBottom, COLOR_WHITE);
            for(u32 i = 0; i < 8 && stackBytePos < dumpHeader->stackDumpSize; i++)
            {
                char byteString[] = "00";
                hexItoa(*stackDump++, byteString, 2);
                drawString(byteString, 10 + 10 * SPACING_X + 3 * i * SPACING_X, posYBottom, COLOR_WHITE);
            }

            posYBottom += SPACING_Y;
        }

        selectScreen(false);

        u32 size = dumpHeader->totalSize;
        char path[42];
        char fileName[] = "crash_dump_00000000.dmp";
        const char *pathFolder = dumpHeader->processor == 9 ? "/luma/dumps/arm9" : "/luma/dumps/arm11";

        findDumpFile(pathFolder, fileName);
        memcpy(path, pathFolder, strlen(pathFolder) + 1);
        concatenateStrings(path, "/");
        concatenateStrings(path, fileName);

        if(fileWrite((void *)dumpHeader, path, size))
        {
            posY = drawString("You can find a dump in the following file:", 10, posY, COLOR_WHITE) + SPACING_Y;
            posY = drawString(path, 10, posY, COLOR_WHITE) + 2 * SPACING_Y;
        }
        else posY = drawString("Error writing the dump file", 10, posY, COLOR_RED) + SPACING_Y;

        drawString("Press any button to shutdown", 10, posY, COLOR_WHITE);

        memset32((void *)dumpHeader, 0, size);

        waitInput();
        mcuPowerOff();
    }
}
#endif