/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include "exceptions.h"
#include "fs.h"
#include "memory.h"
#include "screen.h"
#include "draw.h"
#include "utils.h"
#include "fmt.h"
#include "buttons.h"
#include "arm9_exception_handlers.h"

// See https://github.com/LumaTeam/luma3ds_exception_dump_parser

void installArm9Handlers(void)
{
    vu32 *dstVeneers = (vu32 *)0x08000000;

    for(u32 i = 0; i < 6; i++)
    {
        if(arm9ExceptionHandlerAddressTable[i] != 0)
        {
            dstVeneers[2 * i] = 0xE51FF004;
            dstVeneers[2 * i + 1] = arm9ExceptionHandlerAddressTable[i];
        }
    }
}

void detectAndProcessExceptionDumps(void)
{
    volatile ExceptionDumpHeader *dumpHeader = (volatile ExceptionDumpHeader *)0x25000000;

    if(dumpHeader->magic[0] != 0xDEADC0DE || dumpHeader->magic[1] != 0xDEADCAFE || (dumpHeader->processor != 9 && dumpHeader->processor != 11)) return;

    const vu32 *regs = (vu32 *)((vu8 *)dumpHeader + sizeof(ExceptionDumpHeader));
    const vu8 *stackDump = (vu8 *)regs + dumpHeader->registerDumpSize + dumpHeader->codeDumpSize;
    const vu8 *additionalData = stackDump + dumpHeader->stackDumpSize;

    static const char *handledExceptionNames[] = {
        "FIQ", "undefined instruction", "prefetch abort", "data abort"
    },
                      *specialExceptions[] = {
        "kernel panic", "svcBreak"
    },
                      *registerNames[] = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12",
        "SP", "LR", "PC", "CPSR", "FPEXC"
    },
                      *faultStatusNames[] = {
        "Alignment", "Instr.cache maintenance op.",
        "Ext.Abort on translation - Lv1", "Ext.Abort on translation - Lv2",
        "Translation - Section", "Translation - Page", "Access bit - Section", "Access bit - Page",
        "Domain - Section", "Domain - Page", "Permission - Section", "Permission - Page",
        "Precise External Abort", "Imprecise External Abort", "Debug event"
    };

    static const u32 faultStatusValues[] = {
        0b1, 0b100, 0b1100, 0b1110, 0b101, 0b111, 0b11, 0b110, 0b1001, 0b1011, 0b1101,
        0b1111, 0b1000, 0b10110, 0b10
    };

    initScreens();

    drawString(true, 10, 10, COLOR_RED, "An exception occurred");
    u32 posY;
    if(dumpHeader->processor == 11) posY = drawFormattedString(true, 10, 30, COLOR_WHITE, "Processor:       Arm11 (core %u)", dumpHeader->core);
    else posY = drawString(true, 10, 30, COLOR_WHITE, "Processor:       Arm9");

    if(dumpHeader->type == 2)
    {
        if((regs[16] & 0x20) == 0 && dumpHeader->codeDumpSize >= 4)
        {
            u32 instr = *(vu32 *)(stackDump - 4);
            if(instr == 0xE12FFF7E)
                posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "Exception type:  %s (%s)", handledExceptionNames[dumpHeader->type], specialExceptions[0]);
            else if(instr == 0xEF00003C)
                posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "Exception type:  %s (%s)", handledExceptionNames[dumpHeader->type], specialExceptions[1]);
            else
                posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "Exception type:  %s", handledExceptionNames[dumpHeader->type]);
        }
        else if((regs[16] & 0x20) != 0 && dumpHeader->codeDumpSize >= 2)
        {
            u16 instr = *(vu16 *)(stackDump - 2);
            if(instr == 0xDF3C)
                posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "Exception type:  %s (%s)", handledExceptionNames[dumpHeader->type], specialExceptions[0]);
            else
                posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "Exception type:  %s", handledExceptionNames[dumpHeader->type]);
        }
        else
            posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "Exception type:  %s", handledExceptionNames[dumpHeader->type]);
    }
    else
        posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "Exception type:  %s", handledExceptionNames[dumpHeader->type]);

    if(dumpHeader->processor == 11 && dumpHeader->type >= 2)
    {
        u32 xfsr = (dumpHeader->type == 2 ? regs[18] : regs[17]) & 0xF;

        for(u32 i = 0; i < 15; i++)
            if(xfsr == faultStatusValues[i])
            {
                posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "Fault status:    %s", faultStatusNames[i]);
                break;
            }
    }

    if(dumpHeader->additionalDataSize != 0)
    {
        u32 size = dumpHeader->additionalDataSize;
        if(dumpHeader->processor == 11)
            posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE,
                                    "Current process: %.8s (%016llX)", (const char *)additionalData, *(vu64 *)(additionalData + 8));
        else
            posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE,
                                    "Arm9 memory dump at offset %X size %lX", (uintptr_t)additionalData - (uintptr_t)dumpHeader, size);
    }
    posY += SPACING_Y;

    for(u32 i = 0; i < 17; i += 2)
    {
        posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "%-7s%08lX", registerNames[i], regs[i]);

        if(i != 16)
            posY = drawFormattedString(true, 10 + 22 * SPACING_X, posY, COLOR_WHITE, "%-7s%08lX", registerNames[i + 1], regs[i + 1]);
        else if(dumpHeader->processor == 11)
            posY = drawFormattedString(true, 10 + 22 * SPACING_X, posY, COLOR_WHITE, "%-7s%08lX", registerNames[i + 1], regs[20]);
    }

    if(dumpHeader->processor == 11 && dumpHeader->type == 3)
        posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "%-7s%08lX       Access type: %s", "FAR", regs[19], regs[17] & (1u << 11) ? "Write" : "Read");

    posY += SPACING_Y;

    u32 mode = regs[16] & 0xF;
    if(dumpHeader->type == 3 && (mode == 7 || mode == 11))
        posY = drawString(true, 10, posY + SPACING_Y, COLOR_YELLOW, "Incorrect dump: failed to dump code and/or stack") + SPACING_Y;

    u32 posYBottom = drawString(false, 10, 10, COLOR_WHITE, "Stack dump:") + SPACING_Y;

    for(u32 line = 0; line < 19 && stackDump < additionalData; line++)
    {
        posYBottom = drawFormattedString(false, 10, posYBottom + SPACING_Y, COLOR_WHITE, "%08lX:", regs[13] + 8 * line);

        for(u32 i = 0; i < 8 && stackDump < additionalData; i++, stackDump++)
            drawFormattedString(false, 10 + 10 * SPACING_X + 3 * i * SPACING_X, posYBottom, COLOR_WHITE, "%02X", *stackDump);
    }

    static const char *choiceMessage[] = {"Press A to save the crash dump", "Press any other button to shutdown"};

    drawString(true, 10, posY + SPACING_Y, COLOR_WHITE, choiceMessage[0]);
    drawString(true, 10, posY + SPACING_Y + SPACING_Y , COLOR_WHITE, choiceMessage[1]);

    if(waitInput(false) != BUTTON_A) goto exit;

    drawString(true, 10, posY + SPACING_Y, COLOR_BLACK, choiceMessage[0]);
    drawString(true, 10, posY + SPACING_Y + SPACING_Y , COLOR_BLACK, choiceMessage[1]);

    char folderPath[32],
         path[128],
         fileName[32];

    sprintf(folderPath, "dumps/arm%u", dumpHeader->processor);
    findDumpFile(folderPath, fileName);
    sprintf(path, "%s/%s", folderPath, fileName);

    if(fileWrite((void *)dumpHeader, path, dumpHeader->totalSize))
    {
        posY = drawString(true, 10, posY + SPACING_Y, COLOR_WHITE, "You can find the dump in the following file:");
        posY = drawFormattedString(true, 10, posY + SPACING_Y, COLOR_WHITE, "%s:/luma/%s", isSdMode ? "SD" : "CTRNAND", path) + SPACING_Y;
    }
    else posY = drawString(true, 10, posY + SPACING_Y, COLOR_RED, "Error writing the dump file");

    drawString(true, 10, posY + SPACING_Y, COLOR_WHITE, "Press any button to shutdown");

    waitInput(false);

exit:
    memset((void *)dumpHeader, 0, dumpHeader->totalSize);
    mcuPowerOff();
}
