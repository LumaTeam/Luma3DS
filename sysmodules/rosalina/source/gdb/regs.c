/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
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

#include "gdb/regs.h"
#include "gdb/net.h"

GDB_DECLARE_HANDLER(ReadRegisters)
{
    if(ctx->selectedThreadId == 0)
        ctx->selectedThreadId = ctx->currentThreadId;

    ThreadContext regs;
    Result r = svcGetDebugThreadContext(&regs, ctx->debug, ctx->selectedThreadId, THREADCONTEXT_CONTROL_ALL);

    if(R_FAILED(r))
        return GDB_ReplyErrno(ctx, EPERM);

    return GDB_SendHexPacket(ctx, &regs, sizeof(ThreadContext));
}

GDB_DECLARE_HANDLER(WriteRegisters)
{
    if(ctx->selectedThreadId == 0)
        ctx->selectedThreadId = ctx->currentThreadId;

    ThreadContext regs;

    if(GDB_DecodeHex(&regs, ctx->commandData, sizeof(ThreadContext)) != sizeof(ThreadContext))
        return GDB_ReplyErrno(ctx, EPERM);

    Result r = svcSetDebugThreadContext(ctx->debug, ctx->selectedThreadId, &regs, THREADCONTEXT_CONTROL_ALL);
    if(R_FAILED(r))
        return GDB_ReplyErrno(ctx, EPERM);
    else
        return GDB_ReplyOk(ctx);
}

static u32 GDB_ConvertRegisterNumber(ThreadContextControlFlags *flags, u32 gdbNum)
{
    if(gdbNum <= 15)
    {
        *flags = (gdbNum >= 13) ? THREADCONTEXT_CONTROL_CPU_SPRS : THREADCONTEXT_CONTROL_CPU_GPRS;
        return gdbNum;
    }
    else if(gdbNum == 25)
    {
        *flags = THREADCONTEXT_CONTROL_CPU_SPRS;
        return 16;
    }
    else if(gdbNum >= 26 && gdbNum <= 41)
    {
        *flags = THREADCONTEXT_CONTROL_FPU_GPRS;
        return gdbNum - 26;
    }
    else if(gdbNum == 42 || gdbNum == 43)
    {
        *flags = THREADCONTEXT_CONTROL_FPU_SPRS;
        return gdbNum - 42;
    }
    else
    {
        *flags = (ThreadContextControlFlags)0;
        return 0;
    }
}

GDB_DECLARE_HANDLER(ReadRegister)
{
    if(ctx->selectedThreadId == 0)
        ctx->selectedThreadId = ctx->currentThreadId;

    ThreadContext regs;
    ThreadContextControlFlags flags;
    u32 gdbRegNum;

    if(GDB_ParseHexIntegerList(&gdbRegNum, ctx->commandData, 1, 0) == NULL)
        return GDB_ReplyErrno(ctx, EILSEQ);

    u32 n = GDB_ConvertRegisterNumber(&flags, gdbRegNum);
    if(!flags)
        return GDB_ReplyErrno(ctx, EINVAL);

    Result r = svcGetDebugThreadContext(&regs, ctx->debug, ctx->selectedThreadId, flags);

    if(R_FAILED(r))
        return GDB_ReplyErrno(ctx, EPERM);

    if(flags & THREADCONTEXT_CONTROL_CPU_GPRS)
        return GDB_SendHexPacket(ctx, &regs.cpu_registers.r[n], 4);
    else if(flags & THREADCONTEXT_CONTROL_CPU_SPRS)
        return GDB_SendHexPacket(ctx, &regs.cpu_registers.sp + (n - 13), 4); // hacky
    else if(flags & THREADCONTEXT_CONTROL_FPU_GPRS)
        return GDB_SendHexPacket(ctx, &regs.fpu_registers.d[n], 8);
    else
        return GDB_SendHexPacket(ctx, &regs.fpu_registers.fpscr + n, 4); // hacky
}

GDB_DECLARE_HANDLER(WriteRegister)
{
    if(ctx->selectedThreadId == 0)
        ctx->selectedThreadId = ctx->currentThreadId;

    ThreadContext regs;
    ThreadContextControlFlags flags;
    u32 gdbRegNum;

    const char *valueStart = GDB_ParseHexIntegerList(&gdbRegNum, ctx->commandData, 1, '=');
    if(valueStart == NULL || *valueStart != '=')
        return GDB_ReplyErrno(ctx, EILSEQ);

    valueStart++;

    u32 n = GDB_ConvertRegisterNumber(&flags, gdbRegNum);
    u32 value;
    u64 value64;

    if(flags & THREADCONTEXT_CONTROL_FPU_GPRS)
    {
        if(GDB_DecodeHex(&value64, valueStart, 8) != 8 || valueStart[16] != 0)
            return GDB_ReplyErrno(ctx, EINVAL);
    }
    else if(flags)
    {
       if(GDB_DecodeHex(&value, valueStart, 4) != 4 || valueStart[8] != 0)
            return GDB_ReplyErrno(ctx, EINVAL);
    }
    else
        return GDB_ReplyErrno(ctx, EINVAL);

    Result r = svcGetDebugThreadContext(&regs, ctx->debug, ctx->selectedThreadId, flags);

    if(R_FAILED(r))
        return GDB_ReplyErrno(ctx, EPERM);

    if(flags & THREADCONTEXT_CONTROL_CPU_GPRS)
        regs.cpu_registers.r[n] = value;
    else if(flags & THREADCONTEXT_CONTROL_CPU_SPRS)
        *(&regs.cpu_registers.sp + (n - 13)) = value; // hacky
    else if(flags & THREADCONTEXT_CONTROL_FPU_GPRS)
        memcpy(&regs.fpu_registers.d[n], &value64, 8);
    else
        *(&regs.fpu_registers.fpscr + n) = value; // hacky

    r = svcSetDebugThreadContext(ctx->debug, ctx->selectedThreadId, &regs, flags);
    if(R_FAILED(r))
        return GDB_ReplyErrno(ctx, EPERM);
    else
        return GDB_ReplyOk(ctx);
}
