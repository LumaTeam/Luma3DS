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

void GDB_InitializeContext(GDBContext *ctx)
{
    RecursiveLock_Init(&ctx->lock);

    RecursiveLock_Lock(&ctx->lock);

    svcCreateEvent(&ctx->continuedEvent, RESET_ONESHOT);
    svcCreateEvent(&ctx->clientAcceptedEvent, RESET_STICKY);

    ctx->eventToWaitFor = ctx->clientAcceptedEvent;
    ctx->continueFlags = (DebugFlags)(DBG_SIGNAL_FAULT_EXCEPTION_EVENTS | DBG_INHIBIT_USER_CPU_EXCEPTION_HANDLERS);

    RecursiveLock_Unlock(&ctx->lock);
}

void GDB_FinalizeContext(GDBContext *ctx)
{
    RecursiveLock_Lock(&ctx->lock);

    svcClearEvent(ctx->clientAcceptedEvent);

    svcCloseHandle(ctx->clientAcceptedEvent);
    svcCloseHandle(ctx->continuedEvent);

    RecursiveLock_Unlock(&ctx->lock);
}

GDB_DECLARE_HANDLER(Unsupported)
{
    return GDB_ReplyEmpty(ctx);
}
