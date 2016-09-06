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

#include "i2c.h"
#include "handlers.h"

#define FINAL_BUFFER    0x25000000

#define REG_DUMP_SIZE   4 * 17
#define CODE_DUMP_SIZE  48

bool cannotAccessAddress(const void *address)
{
    u32 regionSettings[8];
    u32 addr = (u32)address;

    u32 dataAccessPermissions = readMPUConfig(regionSettings);
    for(u32 i = 0; i < 8; i++)
    {
        if((dataAccessPermissions & 0xF) == 0 || (regionSettings[i] & 1) == 0)
            continue; //No access / region not enabled

        u32 regionAddrBase = regionSettings[i] & ~0xFFF;
        u32 regionSize = 1 << (((regionSettings[i] >> 1) & 0x1F) + 1);

        if(addr >= regionAddrBase && addr < regionAddrBase + regionSize)
            return false;

        dataAccessPermissions >>= 4;
    }

    return true;
}

static u32 __attribute__((noinline)) copyMemory(void *dst, const void *src, u32 size, u32 alignment)
{
    u8 *out = (u8 *)dst;
    const u8 *in = (const u8 *)src;

    if(((u32)src & (alignment - 1)) != 0 || cannotAccessAddress(src) || (size != 0 && cannotAccessAddress((u8 *)src + size - 1)))
        return 0;

    for(u32 i = 0; i < size; i++)
        *out++ = *in++;

    return size;
}

void __attribute__((noreturn)) mainHandler(u32 *regs, u32 type)
{
    ExceptionDumpHeader dumpHeader;

    u32 registerDump[REG_DUMP_SIZE / 4];
    u8 codeDump[CODE_DUMP_SIZE];

    dumpHeader.magic[0] = 0xDEADC0DE;
    dumpHeader.magic[1] = 0xDEADCAFE;
    dumpHeader.versionMajor = 1;
    dumpHeader.versionMinor = 2;

    dumpHeader.processor = 9;
    dumpHeader.core = 0;
    dumpHeader.type = type;

    dumpHeader.registerDumpSize = REG_DUMP_SIZE;
    dumpHeader.codeDumpSize = CODE_DUMP_SIZE;
    dumpHeader.additionalDataSize = 0;

    //Dump registers
    //Current order of saved regs: cpsr, pc, r8-r14, r0-r7
    u32 cpsr = regs[0];
    u32 pc   = regs[1] - (type < 3 ? (((cpsr & 0x20) != 0 && type == 1) ? 2 : 4) : 8);

    registerDump[15] = pc;
    registerDump[16] = cpsr;
    for(u32 i = 0; i < 7; i++) registerDump[8 + i]  = regs[2 + i];
    for(u32 i = 0; i < 8; i++) registerDump[i] = regs[9 + i]; 

    //Dump code
    u8 *instr = (u8 *)pc + ((cpsr & 0x20) ? 2 : 4) - dumpHeader.codeDumpSize; //Doesn't work well on 32-bit Thumb instructions, but it isn't much of a problem
    dumpHeader.codeDumpSize = copyMemory(codeDump, instr, dumpHeader.codeDumpSize, ((cpsr & 0x20) != 0) ? 2 : 4);

    //Copy register dump and code dump 
    u8 *final = (u8 *)(FINAL_BUFFER + sizeof(ExceptionDumpHeader));
    final += copyMemory(final, registerDump, dumpHeader.registerDumpSize, 1);
    final += copyMemory(final, codeDump, dumpHeader.codeDumpSize, 1);

    //Dump stack in place
    dumpHeader.stackDumpSize = copyMemory(final, (const void *)registerDump[13], 0x1000 - (registerDump[13] & 0xFFF), 1);

    dumpHeader.totalSize = sizeof(ExceptionDumpHeader) + dumpHeader.registerDumpSize + dumpHeader.codeDumpSize + dumpHeader.stackDumpSize + dumpHeader.additionalDataSize;

    //Copy header (actually optimized by the compiler)
    *(ExceptionDumpHeader *)FINAL_BUFFER = dumpHeader;

    ((void (*)())0xFFFF0830)(); //Ensure that all memory transfers have completed and that the data cache has been flushed
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2); //Reboot
    while(true);
}