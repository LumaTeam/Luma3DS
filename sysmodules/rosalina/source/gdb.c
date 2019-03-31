/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2019 Aurora Wright, TuxSH
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

#include "gdb/debug.h"

#include "gdb/watchpoints.h"
#include "gdb/breakpoints.h"
#include "gdb/stop_point.h"

void GDB_InitializeContext(GDBContext *ctx)
{
    memset(ctx, 0, sizeof(GDBContext));
    RecursiveLock_Init(&ctx->lock);

    RecursiveLock_Lock(&ctx->lock);

    svcCreateEvent(&ctx->continuedEvent, RESET_ONESHOT);
    svcCreateEvent(&ctx->processAttachedEvent, RESET_STICKY);

    ctx->eventToWaitFor = ctx->processAttachedEvent;
    ctx->continueFlags = (DebugFlags)(DBG_SIGNAL_FAULT_EXCEPTION_EVENTS | DBG_INHIBIT_USER_CPU_EXCEPTION_HANDLERS);

    RecursiveLock_Unlock(&ctx->lock);
}

void GDB_FinalizeContext(GDBContext *ctx)
{
    RecursiveLock_Lock(&ctx->lock);

    svcClearEvent(ctx->processAttachedEvent);

    svcCloseHandle(ctx->processAttachedEvent);
    svcCloseHandle(ctx->continuedEvent);

    RecursiveLock_Unlock(&ctx->lock);
}

Result GDB_AttachToProcess(GDBContext *ctx)
{
    Result r;

    // Two cases: attached during execution, or started attached
    // The second case will have, after RunQueuedProcess: attach process, debugger break, attach thread (with creator = 0)

    if (!(ctx->flags & GDB_FLAG_ATTACHED_AT_START))
        svcDebugActiveProcess(&ctx->debug, ctx->pid);
    else
    {
        r = 0;
    }
    if(R_SUCCEEDED(r))
    {
        // Note: ctx->pid will be (re)set while processing 'attach process'
        DebugEventInfo *info = &ctx->latestDebugEvent;
        ctx->state = GDB_STATE_CONNECTED;
        ctx->processExited = ctx->processEnded = false;
        ctx->latestSentPacketSize = 0;
        if (!(ctx->flags & GDB_FLAG_ATTACHED_AT_START))
        {
            while(R_SUCCEEDED(svcGetProcessDebugEvent(info, ctx->debug)) &&
                info->type != DBGEVENT_EXCEPTION &&
                info->exception.type != EXCEVENT_ATTACH_BREAK)
            {
                GDB_PreprocessDebugEvent(ctx, info);
                svcContinueDebugEvent(ctx->debug, ctx->continueFlags);
            }
        }
        else
        {
            // Attach process, debugger break
            for(u32 i = 0; i < 2; i++)
            {
                if (R_FAILED(r = svcGetProcessDebugEvent(info, ctx->debug)))
                    return r;
                GDB_PreprocessDebugEvent(ctx, info);
                if (R_FAILED(r = svcContinueDebugEvent(ctx->debug, ctx->continueFlags)))
                    return r;
            }

            if(R_FAILED(r = svcWaitSynchronization(ctx->debug, -1LL)))
                return r;
            if (R_FAILED(r = svcGetProcessDebugEvent(info, ctx->debug)))
                return r;
            // Attach thread
            GDB_PreprocessDebugEvent(ctx, info);
        }
    }
    else
        return r;

    return svcSignalEvent(ctx->processAttachedEvent);
}

void GDB_DetachFromProcess(GDBContext *ctx)
{
    DebugEventInfo dummy;
    for(u32 i = 0; i < ctx->nbBreakpoints; i++)
    {
        if(!ctx->breakpoints[i].persistent)
            GDB_DisableBreakpointById(ctx, i);
    }
    memset(&ctx->breakpoints, 0, sizeof(ctx->breakpoints));
    ctx->nbBreakpoints = 0;

    for(u32 i = 0; i < ctx->nbWatchpoints; i++)
    {
        GDB_RemoveWatchpoint(ctx, ctx->watchpoints[i], WATCHPOINT_DISABLED);
        ctx->watchpoints[i] = 0;
    }
    ctx->nbWatchpoints = 0;

    svcKernelSetState(0x10002, ctx->pid, false);
    memset(ctx->svcMask, 0, 32);

    memset(ctx->memoryOsInfoXmlData, 0, sizeof(ctx->memoryOsInfoXmlData));
    memset(ctx->processesOsInfoXmlData, 0, sizeof(ctx->processesOsInfoXmlData));
    memset(ctx->threadListData, 0, sizeof(ctx->threadListData));
    ctx->threadListDataPos = 0;

    svcClearEvent(ctx->processAttachedEvent);
    ctx->eventToWaitFor = ctx->processAttachedEvent;

    //svcSignalEvent(server->statusUpdated);

    /*
        There's a possibility of a race condition with a possible user exception handler, but you shouldn't
        use 'kill' on APPLICATION titles in the first place (reboot hanging because the debugger is still running, etc).
    */

    ctx->continueFlags = (DebugFlags)0;

    while(R_SUCCEEDED(svcGetProcessDebugEvent(&dummy, ctx->debug)));
    while(R_SUCCEEDED(svcContinueDebugEvent(ctx->debug, ctx->continueFlags)));
    if(ctx->flags & GDB_FLAG_TERMINATE_PROCESS)
    {
        svcTerminateDebugProcess(ctx->debug);
        ctx->processEnded = true;
        ctx->processExited = false;
    }

    while(R_SUCCEEDED(svcGetProcessDebugEvent(&dummy, ctx->debug)));
    while(R_SUCCEEDED(svcContinueDebugEvent(ctx->debug, ctx->continueFlags)));

    svcCloseHandle(ctx->debug);
    ctx->debug = 0;


    ctx->eventToWaitFor = ctx->processAttachedEvent;
    ctx->continueFlags = (DebugFlags)(DBG_SIGNAL_FAULT_EXCEPTION_EVENTS | DBG_INHIBIT_USER_CPU_EXCEPTION_HANDLERS);
    ctx->pid = 0;
    ctx->currentThreadId = ctx->selectedThreadId = ctx->selectedThreadIdForContinuing = 0;
    ctx->nbThreads = 0;
    ctx->totalNbCreatedThreads = 0;
    memset(ctx->threadInfos, 0, sizeof(ctx->threadInfos));
    ctx->catchThreadEvents = false;
}

GDB_DECLARE_HANDLER(Unsupported)
{
    return GDB_ReplyEmpty(ctx);
}
