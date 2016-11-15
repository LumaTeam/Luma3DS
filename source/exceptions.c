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
#include "utils.h"
#include "../build/bundled.h"

void installArm9Handlers(void)
{
    memcpy((void *)0x01FF8000, arm9_exceptions_bin + 32, arm9_exceptions_bin_size - 32);

    /* IRQHandler is at 0x08000000, but we won't handle it for some reasons
       svcHandler is at 0x08000010, but we won't handle svc either */

    const u32 offsets[] = {0x08, 0x18, 0x20, 0x28};

    for(u32 i = 0; i < 4; i++)
    {
        *(vu32 *)(0x08000000 + offsets[i]) = 0xE51FF004;
        *(vu32 *)(0x08000000 + offsets[i] + 4) = *((u32 *)arm9_exceptions_bin + 1 + i);
    }
}

u32 installArm11Handlers(u32 *exceptionsPage, u32 stackAddress, u32 codeSetOffset)
{
    u32 *endPos = exceptionsPage + 0x400;

    u32 *initFPU;
    for(initFPU = exceptionsPage; initFPU < endPos && *initFPU != 0xE1A0D002; initFPU++);

    u32 *freeSpace;
    for(freeSpace = initFPU; freeSpace < endPos && *freeSpace != 0xFFFFFFFF; freeSpace++);

    u32 *mcuReboot;
    for(mcuReboot = exceptionsPage; mcuReboot < endPos && *mcuReboot != 0xE3A0A0C2; mcuReboot++);

    if(initFPU == endPos || freeSpace == endPos || mcuReboot == endPos || *(u32 *)((u8 *)freeSpace + arm11_exceptions_bin_size - 36) != 0xFFFFFFFF) return 1;

    initFPU += 3;
    mcuReboot -= 2;

    memcpy(freeSpace, arm11_exceptions_bin + 32, arm11_exceptions_bin_size - 32);

    exceptionsPage[1] = MAKE_BRANCH(exceptionsPage + 1, (u8 *)freeSpace + *(u32 *)(arm11_exceptions_bin + 8)  - 32); //Undefined Instruction
    exceptionsPage[3] = MAKE_BRANCH(exceptionsPage + 3, (u8 *)freeSpace + *(u32 *)(arm11_exceptions_bin + 12) - 32); //Prefetch Abort
    exceptionsPage[4] = MAKE_BRANCH(exceptionsPage + 4, (u8 *)freeSpace + *(u32 *)(arm11_exceptions_bin + 16) - 32); //Data Abort
    exceptionsPage[7] = MAKE_BRANCH(exceptionsPage + 7, (u8 *)freeSpace + *(u32 *)(arm11_exceptions_bin + 4)  - 32); //FIQ

    for(u32 *pos = freeSpace; pos < (u32 *)((u8 *)freeSpace + arm11_exceptions_bin_size - 32); pos++)
    {
        switch(*pos) //Perform relocations
        {
            case 0xFFFF3000: *pos = stackAddress; break;
            case 0xEBFFFFFE: *pos = MAKE_BRANCH_LINK(pos, initFPU); break;
            case 0xEAFFFFFE: *pos = MAKE_BRANCH(pos, mcuReboot); break;
            case 0xE12FFF1C: pos[1] = 0xFFFF0000 + 4 * (u32)(freeSpace - exceptionsPage) + pos[1] - 32; break; //bx r12 (mainHandler)
            case 0xBEEFBEEF: *pos = codeSetOffset; break;
        }
    }

    return 0;
}

void detectAndProcessExceptionDumps(void)
{
    volatile ExceptionDumpHeader *dumpHeader = (volatile ExceptionDumpHeader *)0x25000000;

    if(dumpHeader->magic[0] != 0xDEADC0DE || dumpHeader->magic[1] == 0xDEADCAFE || (dumpHeader->processor != 9 && dumpHeader->processor != 11)) return;

    const vu32 *regs = (vu32 *)((vu8 *)dumpHeader + sizeof(ExceptionDumpHeader));
    const vu8 *stackDump = (vu8 *)regs + dumpHeader->registerDumpSize + dumpHeader->codeDumpSize;
    const vu8 *additionalData = stackDump + dumpHeader->stackDumpSize;

    const char *handledExceptionNames[] = { 
        "FIQ", "undefined instruction", "prefetch abort", "data abort"
    };

    const char *specialExceptions[] = {
        "(kernel panic)", "(svcBreak)"
    };

    const char *registerNames[] = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12",
        "SP", "LR", "PC", "CPSR", "FPEXC"
    };

    char hexString[] = "00000000";

    initScreens();

    drawString("An exception occurred", true, 10, 10, COLOR_RED);
    u32 posY = drawString(dumpHeader->processor == 11 ? "Processor:       ARM11 (core  )" : "Processor:       ARM9", true, 10, 30, COLOR_WHITE);
    if(dumpHeader->processor == 11) drawCharacter('0' + dumpHeader->core, true, 10 + 29 * SPACING_X, 30, COLOR_WHITE);

    posY = drawString("Exception type:  ", true, 10, posY + SPACING_Y, COLOR_WHITE);
    drawString(handledExceptionNames[dumpHeader->type], true, 10 + 17 * SPACING_X, posY, COLOR_WHITE);

    if(dumpHeader->type == 2)
    {
        if((regs[16] & 0x20) == 0 && dumpHeader->codeDumpSize >= 4)
        {
        u32 instr = *(vu32 *)(stackDump - 4);
        if(instr == 0xE12FFF7E) drawString(specialExceptions[0], true, 10 + 32 * SPACING_X, posY, COLOR_WHITE);
        else if(instr == 0xEF00003C) drawString(specialExceptions[1], true, 10 + 32 * SPACING_X, posY, COLOR_WHITE);
        }
        else if((regs[16] & 0x20) == 0 && dumpHeader->codeDumpSize >= 2)
        {
        u16 instr = *(vu16 *)(stackDump - 2);
        if(instr == 0xDF3C) drawString(specialExceptions[1], true, 10 + 32 * SPACING_X, posY, COLOR_WHITE);
        }
    }

    if(dumpHeader->processor == 11 && dumpHeader->additionalDataSize != 0)
    {
        char processName[] = "Current process:     ";
        memcpy(processName + sizeof(processName) - 9, (void *)additionalData, 8);
        posY = drawString(processName, true, 10, posY + SPACING_Y, COLOR_WHITE);
    }

    posY += SPACING_Y;

    for(u32 i = 0; i < 17; i += 2)
    {
        posY = drawString(registerNames[i], true, 10, posY + SPACING_Y, COLOR_WHITE);
        hexItoa(regs[i], hexString, 8, true);
        drawString(hexString, true, 10 + 7 * SPACING_X, posY, COLOR_WHITE);

        if(i != 16 || dumpHeader->processor != 9)
        {
        drawString(registerNames[i + 1], true, 10 + 22 * SPACING_X, posY, COLOR_WHITE);
        hexItoa(i == 16 ? regs[20] : regs[i + 1], hexString, 8, true);
        drawString(hexString, true, 10 + 29 * SPACING_X, posY, COLOR_WHITE);
        }
    }

    posY += SPACING_Y;

    u32 mode = regs[16] & 0xF;
    if(dumpHeader->type == 3 && (mode == 7 || mode == 11))
        posY = drawString("Incorrect dump: failed to dump code and/or stack", true, 10, posY + SPACING_Y, COLOR_YELLOW) + SPACING_Y;

    u32 posYBottom = drawString("Stack dump:", false, 10, 10, COLOR_WHITE) + SPACING_Y;

    for(u32 line = 0; line < 19 && stackDump < additionalData; line++)
    {
        hexItoa(regs[13] + 8 * line, hexString, 8, true);
        posYBottom = drawString(hexString, false, 10, posYBottom + SPACING_Y, COLOR_WHITE);
        drawCharacter(':', false, 10 + 8 * SPACING_X, posYBottom, COLOR_WHITE);

        for(u32 i = 0; i < 8 && stackDump < additionalData; i++, stackDump++)
        {
        char byteString[] = "00";
        hexItoa(*stackDump, byteString, 2, false);
        drawString(byteString, false, 10 + 10 * SPACING_X + 3 * i * SPACING_X, posYBottom, COLOR_WHITE);
        }
    }

    char path[36];
    char fileName[] = "crash_dump_00000000.dmp";
    const char *pathFolder = dumpHeader->processor == 9 ? "dumps/arm9" : "dumps/arm11";

    findDumpFile(pathFolder, fileName);
    memcpy(path, pathFolder, strlen(pathFolder) + 1);
    concatenateStrings(path, "/");
    concatenateStrings(path, fileName);

    if(fileWrite((void *)dumpHeader, path, dumpHeader->totalSize))
    {
        posY = drawString("You can find a dump in the following file:", true, 10, posY + SPACING_Y, COLOR_WHITE);
        posY = drawString(path, true, 10, posY + SPACING_Y, COLOR_WHITE) + SPACING_Y;
    }
    else posY = drawString("Error writing the dump file", true, 10, posY + SPACING_Y, COLOR_RED);

    drawString("Press any button to shutdown", true, 10, posY + SPACING_Y, COLOR_WHITE);

    memset32((void *)dumpHeader, 0, dumpHeader->totalSize);

    waitInput(false);
    mcuPowerOff();
}