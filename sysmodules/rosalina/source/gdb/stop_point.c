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

#include "gdb.h"
#include "gdb/net.h"
#include "gdb/watchpoints.h"
#include "gdb/breakpoints.h"

GDB_DECLARE_HANDLER(ToggleStopPoint)
{
    bool add = ctx->commandData[-1] == 'Z';
    u32 lst[3];

    const char *pos = GDB_ParseHexIntegerList(lst, ctx->commandData, 3, ';');
    if(pos == NULL)
        return GDB_ReplyErrno(ctx, EILSEQ);
    bool persist = *pos != 0 && strncmp(pos, ";cmds:1", 7) == 0;

    u32 kind = lst[0];
    u32 addr = lst[1];
    u32 size = lst[2];

    int res;
    static const WatchpointKind kinds[3] = { WATCHPOINT_WRITE, WATCHPOINT_READ, WATCHPOINT_READWRITE };
    switch(kind)
    {
        case 0: // software breakpoint
            if(size != 2 && size != 4)
                return GDB_ReplyEmpty(ctx);
            else
            {
                res = add ? GDB_AddBreakpoint(ctx, addr, size == 2, persist) :
                            GDB_RemoveBreakpoint(ctx, addr);
                return res == 0 ? GDB_ReplyOk(ctx) : GDB_ReplyErrno(ctx, -res);
            }

        // Watchpoints
        case 2:
        case 3:
        case 4:
            res = add ? GDB_AddWatchpoint(ctx, addr, size, kinds[kind - 2]) :
                        GDB_RemoveWatchpoint(ctx, addr, kinds[kind - 2]);
            return res == 0 ? GDB_ReplyOk(ctx) : GDB_ReplyErrno(ctx, -res);

        default:
            return GDB_ReplyEmpty(ctx);
    }
}
