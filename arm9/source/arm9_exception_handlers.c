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

#include "arm9_exception_handlers.h"
#include "i2c.h"
#include "screen.h"

#define FINAL_BUFFER    0x25000000

#define REG_DUMP_SIZE   4 * 17
#define CODE_DUMP_SIZE  48

static inline void dumpArm9Memory(ExceptionDumpHeader *dumpHeader, u8 *buf)
{
    // Check if n3ds extra arm9 mem is enabled (if it's possible to read CFG9_EXTMEMCNT9)
    u8 extmemcnt9 = 0;
    safecpy(&extmemcnt9, (const void *)0x10000200, 1);

    u32 size = (extmemcnt9 & 1) ? 0x180000 : 0x100000;
    dumpHeader->additionalDataSize += safecpy(buf, (const void *)0x08000000, size);
}

void __attribute__((noreturn)) arm9ExceptionHandlerMain(u32 *registerDump, u32 type)
{
    ExceptionDumpHeader dumpHeader;

    u8 codeDump[CODE_DUMP_SIZE];

    dumpHeader.magic[0] = 0xDEADC0DE;
    dumpHeader.magic[1] = 0xDEADCAFE;
    dumpHeader.versionMajor = 1;
    dumpHeader.versionMinor = 3;

    dumpHeader.processor = 9;
    dumpHeader.core = 0;
    dumpHeader.type = type;

    dumpHeader.registerDumpSize = REG_DUMP_SIZE;
    dumpHeader.codeDumpSize = CODE_DUMP_SIZE;
    dumpHeader.additionalDataSize = 0;

    u32 cpsr = registerDump[16];
    u32 pc   = registerDump[15] - (type < 3 ? (((cpsr & 0x20) != 0 && type == 1) ? 2 : 4) : 8);

    registerDump[15] = pc;

    //Dump code
    u8 *instr = (u8 *)pc + ((cpsr & 0x20) ? 2 : 4) - dumpHeader.codeDumpSize; //wouldn't work well on 32-bit Thumb instructions, but it isn't much of a problem
    dumpHeader.codeDumpSize = ((u32)instr & (((cpsr & 0x20) != 0) ? 1 : 3)) != 0 ? 0 : safecpy(codeDump, instr, dumpHeader.codeDumpSize);

    //Copy register dump and code dump
    u8 *final = (u8 *)(FINAL_BUFFER + sizeof(ExceptionDumpHeader));
    final += safecpy(final, registerDump, dumpHeader.registerDumpSize);
    final += safecpy(final, codeDump, dumpHeader.codeDumpSize);

    //Dump stack in place
    dumpHeader.stackDumpSize = safecpy(final, (const void *)registerDump[13], 0x1000 - (registerDump[13] & 0xFFF));
    final += dumpHeader.stackDumpSize;

    // See if we need to copy Arm9 memory (check for bkpt 0xFFFD / bkpt 0xFD)
    if(dumpHeader.codeDumpSize > 0)
    {
        if(cpsr & 0x20)
        {
            // Thumb
            u16 instr;
            safecpy(&instr, codeDump + dumpHeader.codeDumpSize - 2, 2);
            if(instr == 0xBEFD) dumpArm9Memory(&dumpHeader, final);
        }
        else
        {
            u32 instr;
            safecpy(&instr, codeDump + dumpHeader.codeDumpSize - 4, 4);
            if(instr == 0xE12FFF7D) dumpArm9Memory(&dumpHeader, final);
        }
    }

    dumpHeader.totalSize = sizeof(ExceptionDumpHeader) + dumpHeader.registerDumpSize + dumpHeader.codeDumpSize + dumpHeader.stackDumpSize + dumpHeader.additionalDataSize;

    //Copy header (actually optimized by the compiler)
    *(ExceptionDumpHeader *)FINAL_BUFFER = dumpHeader;

    if(ARESCREENSINITIALIZED) I2C_writeReg(I2C_DEV_MCU, 0x22, 1 << 0); //Shutdown LCD

    ((void (*)())0xFFFF0830)(); //Ensure that all memory transfers have completed and that the data cache has been flushed

    I2C_writeReg(I2C_DEV_MCU, 0x20, 1 << 2); //Reboot
    while(true);
}
