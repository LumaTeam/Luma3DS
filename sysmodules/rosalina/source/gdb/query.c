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

#include "gdb/query.h"
#include "gdb/xfer.h"
#include "gdb/thread.h"
#include "gdb/mem.h"
#include "gdb/net.h"
#include "gdb/remote_command.h"

typedef enum GDBQueryDirection
{
    GDB_QUERY_DIRECTION_READ,
    GDB_QUERY_DIRECTION_WRITE
} GDBQueryDirection;

// https://gcc.gnu.org/onlinedocs/gcc-5.3.0/cpp/Stringification.html
#define xstr(s) str(s)
#define str(s) #s

#define GDB_QUERY_HANDLER_LIST_ITEM_3(name, name2, direction)   { name, GDB_QUERY_HANDLER(name2), GDB_QUERY_DIRECTION_##direction }
#define GDB_QUERY_HANDLER_LIST_ITEM(name, direction)            GDB_QUERY_HANDLER_LIST_ITEM_3(xstr(name), name, direction)

static const struct
{
    const char *name;
    GDBCommandHandler handler;
    GDBQueryDirection direction;
} gdbQueryHandlers[] =
{
    GDB_QUERY_HANDLER_LIST_ITEM(Supported, READ),
    GDB_QUERY_HANDLER_LIST_ITEM(Xfer, READ),
    GDB_QUERY_HANDLER_LIST_ITEM(StartNoAckMode, WRITE),
    GDB_QUERY_HANDLER_LIST_ITEM(Attached, READ),
    GDB_QUERY_HANDLER_LIST_ITEM(fThreadInfo, READ),
    GDB_QUERY_HANDLER_LIST_ITEM(sThreadInfo, READ),
    GDB_QUERY_HANDLER_LIST_ITEM(ThreadEvents, WRITE),
    GDB_QUERY_HANDLER_LIST_ITEM(ThreadExtraInfo, READ),
    GDB_QUERY_HANDLER_LIST_ITEM(GetTLSAddr, READ),
    GDB_QUERY_HANDLER_LIST_ITEM_3("C", CurrentThreadId, READ),
    GDB_QUERY_HANDLER_LIST_ITEM_3("Search", SearchMemory, READ),
    GDB_QUERY_HANDLER_LIST_ITEM(CatchSyscalls, WRITE),
    GDB_QUERY_HANDLER_LIST_ITEM(Rcmd, READ),
};

static int GDB_HandleQuery(GDBContext *ctx, GDBQueryDirection direction)
{
    char *nameBegin = ctx->commandData; // w/o leading 'q'/'Q'
    if(*nameBegin == 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    char *nameEnd;
    char *queryData = NULL;

    for(nameEnd = nameBegin; *nameEnd != 0 && *nameEnd != ':' && *nameEnd != ','; nameEnd++);
    if(*nameEnd != 0)
    {
        *nameEnd = 0;
        queryData = nameEnd + 1;
    }
    else
        queryData = nameEnd;

    for(u32 i = 0; i < sizeof(gdbQueryHandlers) / sizeof(gdbQueryHandlers[0]); i++)
    {
        if(strcmp(gdbQueryHandlers[i].name, nameBegin) == 0 && gdbQueryHandlers[i].direction == direction)
        {
            ctx->commandData = queryData;
            return gdbQueryHandlers[i].handler(ctx);
        }
    }

    return GDB_HandleUnsupported(ctx); // No handler found!
}

int GDB_HandleReadQuery(GDBContext *ctx)
{
    return GDB_HandleQuery(ctx, GDB_QUERY_DIRECTION_READ);
}

int GDB_HandleWriteQuery(GDBContext *ctx)
{
    return GDB_HandleQuery(ctx, GDB_QUERY_DIRECTION_WRITE);
}

GDB_DECLARE_QUERY_HANDLER(Supported)
{
    return GDB_SendFormattedPacket(ctx,
        "PacketSize=%x;"
        "qXfer:features:read+;qXfer:osdata:read+;"
        "QStartNoAckMode+;QThreadEvents+;QCatchSyscalls+;"
        "vContSupported+;swbreak+",

        GDB_BUF_LEN // should have been sizeof(ctx->buffer) but GDB memory functions are bugged
    );
}

GDB_DECLARE_QUERY_HANDLER(StartNoAckMode)
{
    ctx->state = GDB_STATE_NOACK_SENT;
    return GDB_ReplyOk(ctx);
}

GDB_DECLARE_QUERY_HANDLER(Attached)
{
    return GDB_SendPacket(ctx, "1", 1);
}

GDB_DECLARE_QUERY_HANDLER(CatchSyscalls)
{
    if(ctx->commandData[0] == '0')
    {
        memset(ctx->svcMask, 0, 32);
        return R_SUCCEEDED(svcKernelSetState(0x10002, ctx->pid, false)) ? GDB_ReplyOk(ctx) : GDB_ReplyErrno(ctx, EPERM);
    }
    else if(ctx->commandData[0] == '1')
    {
        if(ctx->commandData[1] == ';')
        {
            u32 id;
            const char *pos = ctx->commandData + 1;
            memset(ctx->svcMask, 0, 32);

            do
            {
                pos = GDB_ParseHexIntegerList(&id, pos + 1, 1, ';');
                if(pos == NULL)
                    return GDB_ReplyErrno(ctx, EILSEQ);

                if(id < 0xFE)
                    ctx->svcMask[id / 32] |= 1 << (31 - (id % 32));
            }
            while(*pos != 0);
        }
        else
            memset(ctx->svcMask, 0xFF, 32);

        return R_SUCCEEDED(svcKernelSetState(0x10002, ctx->pid, true, ctx->svcMask)) ? GDB_ReplyOk(ctx) : GDB_ReplyErrno(ctx, EPERM);
    }
    else
        return GDB_ReplyErrno(ctx, EILSEQ);
}
