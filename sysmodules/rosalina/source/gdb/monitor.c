/*
*   This file is part of Luma3DS.
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   SPDX-License-Identifier: (MIT OR GPL-2.0-or-later)
*/

#include "gdb/monitor.h"
#include "gdb/net.h"
#include "gdb/debug.h"

extern Handle preTerminationEvent;
extern bool preTerminationRequested;

void GDB_RunMonitor(GDBServer *server)
{
    Handle handles[3 + MAX_DEBUG];
    Result r = 0;

    handles[0] = preTerminationEvent;
    handles[1] = server->super.shall_terminate_event;
    handles[2] = server->statusUpdated;

    do
    {
        GDB_LockAllContexts(server);
        for(int i = 0; i < MAX_DEBUG; i++)
        {
            GDBContext *ctx = &server->ctxs[i];
            handles[3 + i] = ctx->eventToWaitFor;
        }
        GDB_UnlockAllContexts(server);

        s32 idx = -1;
        r = svcWaitSynchronizationN(&idx, handles, 3 + MAX_DEBUG, false, -1LL);

        if(R_FAILED(r) || idx < 2)
            break;
        else if(idx == 2) {
            svcSignalEvent(server->statusUpdateReceived);
            continue;
        }
        else
        {
            GDBContext *ctx = &server->ctxs[idx - 3];

            RecursiveLock_Lock(&ctx->lock);
            if(ctx->state == GDB_STATE_DISCONNECTED || ctx->state == GDB_STATE_DETACHING)
            {
                svcClearEvent(ctx->processAttachedEvent);
                ctx->eventToWaitFor = ctx->processAttachedEvent;
                RecursiveLock_Unlock(&ctx->lock);
                continue;
            }

            if(ctx->eventToWaitFor == ctx->processAttachedEvent)
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

                    // GDB doens't close the socket in extended-remote
                    if (ctx->flags & GDB_FLAG_EXTENDED_REMOTE) {
                        ctx->state = GDB_STATE_DETACHING;
                        GDB_DetachFromProcess(ctx);
                        ctx->flags &= GDB_FLAG_PROC_RESTART_MASK;
                    }
                    svcClearEvent(ctx->processAttachedEvent);
                    ctx->eventToWaitFor = ctx->processAttachedEvent;
                }
            }

            RecursiveLock_Unlock(&ctx->lock);
        }
    }
    while(!preTerminationRequested && server->super.running);
}
