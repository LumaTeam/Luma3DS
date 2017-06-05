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

#include "gdb/verbose.h"
#include "gdb/net.h"
#include "gdb/debug.h"

static const struct
{
    const char *name;
    GDBCommandHandler handler;
} gdbVerboseCommandHandlers[] =
{
    { "Cont?", GDB_VERBOSE_HANDLER(ContinueSupported) },
    { "Cont",  GDB_VERBOSE_HANDLER(Continue) },
    { "MustReplyEmpty", GDB_HANDLER(Unsupported) },
};

GDB_DECLARE_HANDLER(VerboseCommand)
{
    char *nameBegin = ctx->commandData; // w/o leading 'v'
    if(*nameBegin == 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    char *nameEnd;
    char *vData = NULL;

    for(nameEnd = nameBegin; *nameEnd != 0 && *nameEnd != ';' && *nameEnd != ':'; nameEnd++);
    if(*nameEnd != 0)
    {
        *nameEnd = 0;
        vData = nameEnd + 1;
    }

    for(u32 i = 0; i < sizeof(gdbVerboseCommandHandlers) / sizeof(gdbVerboseCommandHandlers[0]); i++)
    {
        if(strcmp(gdbVerboseCommandHandlers[i].name, nameBegin) == 0)
        {
            ctx->commandData = vData;
            return gdbVerboseCommandHandlers[i].handler(ctx);
        }
    }

    return GDB_HandleUnsupported(ctx); // No handler found!
}

GDB_DECLARE_VERBOSE_HANDLER(ContinueSupported)
{
    return GDB_SendPacket(ctx, "vCont;c;C", 9);
}
