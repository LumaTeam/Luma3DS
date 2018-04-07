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

#include "gdb/breakpoints.h"

#define _REENT_ONLY
#include <errno.h>

u32 GDB_FindClosestBreakpointSlot(GDBContext *ctx, u32 address)
{
    if(ctx->nbBreakpoints == 0 || address <= ctx->breakpoints[0].address)
        return 0;
    else if(address > ctx->breakpoints[ctx->nbBreakpoints - 1].address)
        return ctx->nbBreakpoints;

    u32 a = 0, b = ctx->nbBreakpoints - 1, m;

    do
    {
        m = (a + b) / 2;
        if(ctx->breakpoints[m].address < address)
            a = m;
        else if(ctx->breakpoints[m].address > address)
            b = m;
        else
            return m;
    }
    while(b - a > 1);

    return b;
}

int GDB_GetBreakpointInstruction(u32 *instruction, GDBContext *ctx, u32 address)
{
    u32 id = GDB_FindClosestBreakpointSlot(ctx, address);

    if(id == ctx->nbBreakpoints || ctx->breakpoints[id].address != address)
        return -EINVAL;

    if(instruction != NULL)
        *instruction = ctx->breakpoints[id].savedInstruction;

    return 0;
}

int GDB_AddBreakpoint(GDBContext *ctx, u32 address, bool thumb, bool persist)
{
    if(!thumb && (address & 3) != 0)
        return -EINVAL;

    address &= ~1;

    u32 id = GDB_FindClosestBreakpointSlot(ctx, address);

    if(id != ctx->nbBreakpoints && ctx->breakpoints[id].instructionSize != 0 && ctx->breakpoints[id].address == address)
        return 0;
    else if(ctx->nbBreakpoints == MAX_BREAKPOINT)
        return -EBUSY;

    for(u32 i = ctx->nbBreakpoints; i > id && i != 0; i--)
        ctx->breakpoints[i] = ctx->breakpoints[i - 1];

    ctx->nbBreakpoints++;

    Breakpoint *bkpt = &ctx->breakpoints[id];
    u32 instr = thumb ? BREAKPOINT_INSTRUCTION_THUMB : BREAKPOINT_INSTRUCTION_ARM;
    if(R_FAILED(svcReadProcessMemory(&bkpt->savedInstruction, ctx->debug, address, thumb ? 2 : 4)) ||
       R_FAILED(svcWriteProcessMemory(ctx->debug, &instr, address, thumb ? 2 : 4)))
    {
        for(u32 i = id; i < ctx->nbBreakpoints - 1; i++)
            ctx->breakpoints[i] = ctx->breakpoints[i + 1];

        memset(&ctx->breakpoints[--ctx->nbBreakpoints], 0, sizeof(Breakpoint));
        return -EFAULT;
    }

    bkpt->instructionSize = thumb ? 2 : 4;
    bkpt->address = address;
    bkpt->persistent = persist;

    return 0;
}

int GDB_DisableBreakpointById(GDBContext *ctx, u32 id)
{
    Breakpoint *bkpt = &ctx->breakpoints[id];
    if(R_FAILED(svcWriteProcessMemory(ctx->debug, &bkpt->savedInstruction, bkpt->address, bkpt->instructionSize)))
        return -EFAULT;
    else return 0;
}

int GDB_RemoveBreakpoint(GDBContext *ctx, u32 address)
{
    address &= ~1;

    u32 id = GDB_FindClosestBreakpointSlot(ctx, address);
    if(id == ctx->nbBreakpoints || ctx->breakpoints[id].address != address)
        return -EINVAL;

    int r = GDB_DisableBreakpointById(ctx, id);
    if(r != 0)
        return r;
    else
    {
        for(u32 i = id; i < ctx->nbBreakpoints - 1; i++)
            ctx->breakpoints[i] = ctx->breakpoints[i + 1];

        memset(&ctx->breakpoints[--ctx->nbBreakpoints], 0, sizeof(Breakpoint));

        return 0;
    }
}
