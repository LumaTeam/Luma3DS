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

#include "gdb/server.h"
#include "gdb/net.h"
#include "gdb/query.h"
#include "gdb/verbose.h"
#include "gdb/thread.h"
#include "gdb/debug.h"
#include "gdb/regs.h"
#include "gdb/mem.h"
#include "gdb/watchpoints.h"
#include "gdb/breakpoints.h"
#include "gdb/stop_point.h"

Result GDB_InitializeServer(GDBServer *server)
{
    Result ret = server_init(&server->super);
    if(ret != 0)
        return ret;

    server->super.host = 0;

    server->super.accept_cb = (sock_accept_cb)GDB_AcceptClient;
    server->super.data_cb   = (sock_data_cb)  GDB_DoPacket;
    server->super.close_cb  = (sock_close_cb) GDB_CloseClient;

    server->super.alloc     = (sock_alloc_func)   GDB_GetClient;
    server->super.free      = (sock_free_func)    GDB_ReleaseClient;

    server->super.clients_per_server = 1;

    server->referenceCount = 0;
    svcCreateEvent(&server->statusUpdated, RESET_ONESHOT);

    for(u32 i = 0; i < sizeof(server->ctxs) / sizeof(GDBContext); i++)
        GDB_InitializeContext(server->ctxs + i);

    GDB_ResetWatchpoints();

    return 0;
}

void GDB_FinalizeServer(GDBServer *server)
{
    server_finalize(&server->super);

    svcCloseHandle(server->statusUpdated);
}

void GDB_IncrementServerReferenceCount(GDBServer *server)
{
    AtomicPostIncrement(&server->referenceCount);
}

void GDB_DecrementServerReferenceCount(GDBServer *server)
{
    if(AtomicDecrement(&server->referenceCount) == 0)
        GDB_FinalizeServer(server);
}

void GDB_RunServer(GDBServer *server)
{
    server_bind(&server->super, GDB_PORT_BASE);
    server_bind(&server->super, GDB_PORT_BASE + 1);
    server_bind(&server->super, GDB_PORT_BASE + 2);
    server_run(&server->super);
}

int GDB_AcceptClient(GDBContext *ctx)
{
    RecursiveLock_Lock(&ctx->lock);
    Result r = svcDebugActiveProcess(&ctx->debug, ctx->pid);
    if(R_SUCCEEDED(r))
    {
        DebugEventInfo *info = &ctx->latestDebugEvent;
        ctx->state = GDB_STATE_CONNECTED;
        ctx->processExited = ctx->processEnded = false;
        ctx->latestSentPacketSize = 0;
        while(R_SUCCEEDED(svcGetProcessDebugEvent(info, ctx->debug)) && info->type != DBGEVENT_EXCEPTION &&
              info->exception.type != EXCEVENT_ATTACH_BREAK)
        {
            GDB_PreprocessDebugEvent(ctx, info);
            svcContinueDebugEvent(ctx->debug, ctx->continueFlags);
        }
    }
    else
    {
        RecursiveLock_Unlock(&ctx->lock);
        return -1;
    }

    svcSignalEvent(ctx->clientAcceptedEvent);
    RecursiveLock_Unlock(&ctx->lock);

    return 0;
}

int GDB_CloseClient(GDBContext *ctx)
{
    RecursiveLock_Lock(&ctx->lock);

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

    svcClearEvent(ctx->clientAcceptedEvent);
    ctx->eventToWaitFor = ctx->clientAcceptedEvent;
    RecursiveLock_Unlock(&ctx->lock);
    return 0;
}

GDBContext *GDB_GetClient(GDBServer *server, u16 port)
{
    if(port < GDB_PORT_BASE || port >= GDB_PORT_BASE + sizeof(server->ctxs) / sizeof(GDBContext))
        return NULL;

    GDBContext *ctx = &server->ctxs[port - GDB_PORT_BASE];
    if(!(ctx->flags & GDB_FLAG_USED) && (ctx->flags & GDB_FLAG_SELECTED))
    {
        RecursiveLock_Lock(&ctx->lock);
        ctx->flags |= GDB_FLAG_USED;
        ctx->state = GDB_STATE_CONNECTED;
        RecursiveLock_Unlock(&ctx->lock);

        return ctx;
    }

    return NULL;
}

void GDB_ReleaseClient(GDBServer *server, GDBContext *ctx)
{
    DebugEventInfo dummy;

    svcSignalEvent(server->statusUpdated);

    RecursiveLock_Lock(&ctx->lock);

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

    ctx->flags = (GDBFlags)0;
    ctx->state = GDB_STATE_DISCONNECTED;

    ctx->eventToWaitFor = ctx->clientAcceptedEvent;
    ctx->continueFlags = (DebugFlags)(DBG_SIGNAL_FAULT_EXCEPTION_EVENTS | DBG_INHIBIT_USER_CPU_EXCEPTION_HANDLERS);
    ctx->pid = 0;
    ctx->currentThreadId = ctx->selectedThreadId = ctx->selectedThreadIdForContinuing = 0;
    ctx->nbThreads = 0;
    memset(ctx->threadInfos, 0, sizeof(ctx->threadInfos));
    ctx->catchThreadEvents = false;
    ctx->enableExternalMemoryAccess = false;
    RecursiveLock_Unlock(&ctx->lock);
}

static const struct
{
    char command;
    GDBCommandHandler handler;
} gdbCommandHandlers[] =
{
    { '?', GDB_HANDLER(GetStopReason) },
    { 'c', GDB_HANDLER(Continue) },
    { 'C', GDB_HANDLER(Continue) },
    { 'D', GDB_HANDLER(Detach) },
    { 'g', GDB_HANDLER(ReadRegisters) },
    { 'G', GDB_HANDLER(WriteRegisters) },
    { 'H', GDB_HANDLER(SetThreadId) },
    { 'k', GDB_HANDLER(Kill) },
    { 'm', GDB_HANDLER(ReadMemory) },
    { 'M', GDB_HANDLER(WriteMemory) },
    { 'p', GDB_HANDLER(ReadRegister) },
    { 'P', GDB_HANDLER(WriteRegister) },
    { 'q', GDB_HANDLER(ReadQuery) },
    { 'Q', GDB_HANDLER(WriteQuery) },
    { 'T', GDB_HANDLER(IsThreadAlive) },
    { 'v', GDB_HANDLER(VerboseCommand) },
    { 'X', GDB_HANDLER(WriteMemoryRaw) },
    { 'z', GDB_HANDLER(ToggleStopPoint) },
    { 'Z', GDB_HANDLER(ToggleStopPoint) },
};

static inline GDBCommandHandler GDB_GetCommandHandler(char command)
{
    static const u32 nbHandlers = sizeof(gdbCommandHandlers) / sizeof(gdbCommandHandlers[0]);

    u32 i;
    for(i = 0; i < nbHandlers && gdbCommandHandlers[i].command != command; i++);

    return i < nbHandlers ? gdbCommandHandlers[i].handler : GDB_HANDLER(Unsupported);
}

int GDB_DoPacket(GDBContext *ctx)
{
    int ret;

    RecursiveLock_Lock(&ctx->lock);
    GDBFlags oldFlags = ctx->flags;

    if(ctx->state == GDB_STATE_DISCONNECTED)
        return -1;

    int r = GDB_ReceivePacket(ctx);
    if(r == 0)
        ret = 0;
    else if(r == -1)
        ret = -1;
    else if(ctx->buffer[0] == '\x03')
    {
        GDB_HandleBreak(ctx);
        ret = 0;
    }
    else if(ctx->buffer[0] == '$')
    {
        GDBCommandHandler handler = GDB_GetCommandHandler(ctx->buffer[1]);
        ctx->commandData = ctx->buffer + 2;
        ret = handler(ctx);
    }
    else
        ret = 0;

    RecursiveLock_Unlock(&ctx->lock);
    if(ctx->state == GDB_STATE_CLOSING)
        return -1;

    if((oldFlags & GDB_FLAG_PROCESS_CONTINUING) && !(ctx->flags & GDB_FLAG_PROCESS_CONTINUING))
    {
        if(R_FAILED(svcBreakDebugProcess(ctx->debug)))
            ctx->flags |= GDB_FLAG_PROCESS_CONTINUING;
    }
    else if(!(oldFlags & GDB_FLAG_PROCESS_CONTINUING) && (ctx->flags & GDB_FLAG_PROCESS_CONTINUING))
        svcSignalEvent(ctx->continuedEvent);

    return ret;
}
