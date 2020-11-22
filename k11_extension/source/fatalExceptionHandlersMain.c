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
#include <string.h>

#include "fatalExceptionHandlers.h"
#include "utils.h"
#include "kernel.h"
#include "memory.h"
#include "mmu.h"
#include "globals.h"

#define REG_DUMP_SIZE   4 * 23
#define CODE_DUMP_SIZE  96

// Return true if parameters are invalid
static bool checkExceptionHandlerValidity(KProcess *process, vu32 *threadLocalStorage)
{
    if (process == NULL)
        return true;

    u32             stackBottom = threadLocalStorage[0x11];
    u32             exceptionBuf = threadLocalStorage[0x12];
    MemoryInfo      memInfo;
    PageInfo        pageInfo;
    KProcessHwInfo *hwInfo = hwInfoOfProcess(process);

    u32 perm = KProcessHwInfo__GetAddressUserPerm(hwInfo, threadLocalStorage[0x10]);

    if (stackBottom != 1)
    {
        if (KProcessHwInfo__QueryMemory(hwInfo, &memInfo, &pageInfo, (void *)stackBottom)
            || (memInfo.permissions & MEMPERM_RW) != MEMPERM_RW)
            return true;
    }

    if (exceptionBuf > 1)
    {
        if (KProcessHwInfo__QueryMemory(hwInfo, &memInfo, &pageInfo, (void *)exceptionBuf)
            || (memInfo.permissions & MEMPERM_RW) != MEMPERM_RW)
            return true;
    }

    return (perm & MEMPERM_RX) != MEMPERM_RX;
}

bool isExceptionFatal(u32 spsr, u32 *regs, u32 index)
{
    if(CONFIG(DISABLEARM11EXCHANDLERS)) return false;

    if((spsr & 0x1F) != 0x10) return true;

    KThread *thread = currentCoreContext->objectContext.currentThread;
    KProcess *currentProcess = currentCoreContext->objectContext.currentProcess;

    if(thread != NULL && thread->threadLocalStorage != NULL && *((vu32 *)thread->threadLocalStorage + 0x10) != 0)
       return checkExceptionHandlerValidity(currentProcess, (vu32 *)thread->threadLocalStorage);

    if(currentProcess != NULL)
    {
        if(debugOfProcess(currentProcess) != NULL)
            return false;

        thread = KPROCESS_GET_RVALUE(currentProcess, mainThread);
        if(thread != NULL && thread->threadLocalStorage != NULL && *((vu32 *)thread->threadLocalStorage + 0x10) != 0)
           return checkExceptionHandlerValidity(currentProcess, thread->threadLocalStorage);

        if(index == 3 && strcmp(codeSetOfProcess(currentProcess)->processName, "menu") == 0 && // workaround a Home Menu bug leading to a dabort
           regs[0] == 0x3FFF && regs[2] == 0 && regs[5] == 2 && regs[7] == 1)
            return false;
    }

    return true;
}

extern u32 safecpy_sz;
bool isDataAbortExceptionRangeControlled(u32 spsr, u32 addr)
{
    return (!(spsr & 0x20) && (spsr & 0x1F) != 0x10) && (
                ((u32)kernelUsrCopyFuncsStart <= addr && addr < (u32)kernelUsrCopyFuncsEnd) ||
                ((u32)safecpy <= addr && addr < (u32)safecpy + safecpy_sz)
            );
}

void fatalExceptionHandlersMain(u32 *registerDump, u32 type, u32 cpuId)
{
    ExceptionDumpHeader dumpHeader;

    u8 codeDump[CODE_DUMP_SIZE];
    u8 *finalBuffer = (u8 *)PA_PTR(0x25000000);
    u8 *final = finalBuffer;

    dumpHeader.magic[0] = 0xDEADC0DE;
    dumpHeader.magic[1] = 0xDEADCAFE;
    dumpHeader.versionMajor = 1;
    dumpHeader.versionMinor = 3;

    dumpHeader.processor = 11;
    dumpHeader.core = cpuId & 0xF;
    dumpHeader.type = type;

    dumpHeader.registerDumpSize = REG_DUMP_SIZE;
    dumpHeader.codeDumpSize = CODE_DUMP_SIZE;

    u32 cpsr = registerDump[16];
    u32 pc   = registerDump[15] - (type < 3 ? (((cpsr & 0x20) != 0 && type == 1) ? 2 : 4) : 8);

    registerDump[15] = pc;

    //Dump code
    u8 *instr = (u8 *)pc + ((cpsr & 0x20) ? 2 : 4) - (dumpHeader.codeDumpSize >> 1) ; //wouldn't work well on 32-bit Thumb instructions, but it isn't much of a problem
    dumpHeader.codeDumpSize = ((u32)instr & (((cpsr & 0x20) != 0) ? 1 : 3)) != 0 ? 0 : safecpy(codeDump, instr, dumpHeader.codeDumpSize);

    //Copy register dump and code dump
    final = (u8 *)(finalBuffer + sizeof(ExceptionDumpHeader));
    memcpy(final, registerDump, dumpHeader.registerDumpSize);
    final += dumpHeader.registerDumpSize;
    memcpy(final, codeDump, dumpHeader.codeDumpSize);
    final += dumpHeader.codeDumpSize;

    //Dump stack in place
    dumpHeader.stackDumpSize = safecpy(final, (const void *)registerDump[13], 0x1000 - (registerDump[13] & 0xFFF));
    final += dumpHeader.stackDumpSize;

    if(currentCoreContext->objectContext.currentProcess)
    {
        vu64 *additionalData = (vu64 *)final;
        KCodeSet *currentCodeSet = codeSetOfProcess(currentCoreContext->objectContext.currentProcess);
        if(currentCodeSet != NULL)
        {
            dumpHeader.additionalDataSize = 16;
            memcpy((void *)additionalData, currentCodeSet->processName, 8);
            additionalData[1] = currentCodeSet->titleId;
        }
        else dumpHeader.additionalDataSize = 0;
    }
    else dumpHeader.additionalDataSize = 0;

    dumpHeader.totalSize = sizeof(ExceptionDumpHeader) + dumpHeader.registerDumpSize + dumpHeader.codeDumpSize + dumpHeader.stackDumpSize + dumpHeader.additionalDataSize;

    //Copy header (actually optimized by the compiler)
    *(ExceptionDumpHeader *)finalBuffer = dumpHeader;
}
