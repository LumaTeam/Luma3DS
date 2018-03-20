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

#include "gdb/monitor.h"
#include "gdb/net.h"
#include "gdb/debug.h"

extern Handle terminationRequestEvent;
extern bool terminationRequest;

void GDB_RunMonitor(GDBServer *server)
{
    Handle handles[3 + MAX_DEBUG];
    Result r = 0;

    handles[0] = terminationRequestEvent;
    handles[1] = server->super.shall_terminate_event;
    handles[2] = server->statusUpdated;

    do
    {
        for(int i = 0; i < MAX_DEBUG; i++)
        {
            GDBContext *ctx = &server->ctxs[i];
            handles[3 + i] = ctx->eventToWaitFor;
        }

        s32 idx = -1;
        r = svcWaitSynchronizationN(&idx, handles, 3 + MAX_DEBUG, false, -1LL);

        if(R_FAILED(r) || idx < 2)
            break;
        else if(idx == 2)
            continue;
        else
        {
            GDBContext *ctx = &server->ctxs[idx - 3];

            RecursiveLock_Lock(&ctx->lock);
            if(ctx->state == GDB_STATE_DISCONNECTED || ctx->state == GDB_STATE_CLOSING)
            {
                svcClearEvent(ctx->clientAcceptedEvent);
                ctx->eventToWaitFor = ctx->clientAcceptedEvent;
                RecursiveLock_Unlock(&ctx->lock);
                continue;
            }

            if(ctx->eventToWaitFor == ctx->clientAcceptedEvent)
                ctx->eventToWaitFor = ctx->continuedEvent;
            else if(ctx->eventToWaitFor == ctx->continuedEvent)
                ctx->eventToWaitFor = ctx->debug;
            else
            {
                int res = GDB_HandleDebugEvents(ctx);
                if(res >= 0)
                    ctx->eventToWaitFor = ctx->continuedEvent;
                else if(res == -2)
                {
                    while(GDB_HandleDebugEvents(ctx) != -1) // until we've got all the remaining debug events
                        svcSleepThread(1 * 1000 * 1000LL); // sleep just in case

                    svcClearEvent(ctx->clientAcceptedEvent);
                    ctx->eventToWaitFor = ctx->clientAcceptedEvent;
                }
            }

            RecursiveLock_Unlock(&ctx->lock);
        }
    }
    while(!terminationRequest && server->super.running);
}
