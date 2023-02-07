#include "debugger.h"
#include "memory.h"
#include "minisoc.h"
#include "fmt.h"
#include "pmdbgext.h"
#include "pmdbgext.h"
#include "MyThread.h"
#include "task_runner.h"
#include <3ds.h>
#include <3ds/ipc.h>
#include <3ds/services/cfgu.h>
#include <3ds/applets/error.h>
#include "menu.h"

GDBServer gdbServer = {0};
GDBContext *nextApplicationGdbCtx = NULL;

static MyThread debuggerSocketThread;
static MyThread debuggerDebugThread;
static u8 ALIGN(8) debuggerSocketThreadStack[0x5000];
static u8 ALIGN(8) debuggerDebugThreadStack[0x3000];

void debuggerSocketThreadMain(void)
{
    GDB_IncrementServerReferenceCount(&gdbServer);
    GDB_RunServer(&gdbServer);
    GDB_DecrementServerReferenceCount(&gdbServer);
}

MyThread *debuggerCreateSocketThread(void)
{
    MyThread_Create(&debuggerSocketThread, debuggerSocketThreadMain, debuggerSocketThreadStack, 0x5000, 0x20, CORE_SYSTEM);
    return &debuggerSocketThread;
}

void debuggerDebugThreadMain(void)
{
    GDB_IncrementServerReferenceCount(&gdbServer);
    GDB_RunMonitor(&gdbServer);
    GDB_DecrementServerReferenceCount(&gdbServer);
}

MyThread *debuggerCreateDebugThread(void)
{
    MyThread_Create(&debuggerDebugThread, debuggerDebugThreadMain, debuggerDebugThreadStack, 0x3000, 0x20, CORE_SYSTEM);
    return &debuggerDebugThread;
}

void debuggerFetchAndSetNextApplicationDebugHandleTask(void *argdata)
{
    (void)argdata;
    if (!nextApplicationGdbCtx)
        return;
    Handle debug = 0;
    PMDBG_RunQueuedProcess(&debug);
    GDB_LockAllContexts(&gdbServer);
    nextApplicationGdbCtx->debug = debug;
    if (debug == 0)
        nextApplicationGdbCtx->flags = 0;
    else
        nextApplicationGdbCtx->flags |= GDB_FLAG_ATTACHED_AT_START;
    nextApplicationGdbCtx = NULL;
    GDB_UnlockAllContexts(&gdbServer);
}

Result debuggerDisable(s64 timeout)
{
    Result res = 0;
    bool initialized = gdbServer.referenceCount != 0;
    if (initialized)
    {
        svcSignalEvent(gdbServer.super.shall_terminate_event);
        server_kill_connections(&gdbServer.super);

        res = MyThread_Join(&debuggerDebugThread, timeout);
        if (res == 0)
            res = MyThread_Join(&debuggerSocketThread, timeout);

        Handle dummy = 0;
        PMDBG_RunQueuedProcess(&dummy);
        svcCloseHandle(dummy);
        PMDBG_DebugNextApplicationByForce(false);
        nextApplicationGdbCtx = NULL;
    }

    return res;
}

Result debuggerEnable(s64 timeout)
{
    Result res = 0;
    bool initialized = gdbServer.super.running;
    if (!initialized)
    {
        res = GDB_InitializeServer(&gdbServer);
        Handle handles[3] = {gdbServer.super.started_event, gdbServer.super.shall_terminate_event, preTerminationEvent};
        s32 idx;
        if (res == 0)
        {
            debuggerCreateSocketThread();
            debuggerCreateDebugThread();
            res = svcWaitSynchronizationN(&idx, handles, 3, false, timeout);
            if (res == 0)
                res = gdbServer.super.init_result;
        }
    }
    return res;
}

Result debugNextApplicationByForce()
{
    Result res = 0;
    if (nextApplicationGdbCtx != NULL)
        return 0;
    else
    {
        nextApplicationGdbCtx = GDB_SelectAvailableContext(&gdbServer, GDB_PORT_BASE + 3, GDB_PORT_BASE + 4);
        if (nextApplicationGdbCtx != NULL)
        {
            nextApplicationGdbCtx->debug = 0;
            nextApplicationGdbCtx->pid = 0xFFFFFFFF;
            res = PMDBG_DebugNextApplicationByForce(true);
            if (R_SUCCEEDED(res))
                return 1;
            else
            {
                nextApplicationGdbCtx->flags = 0;
                nextApplicationGdbCtx->localPort = 0;
                nextApplicationGdbCtx = NULL;
                return res;
            }
        }
        else
            return 2;
    }
}

void handleRosalinaDebugger(void *ctx)
{
    (void)ctx;
    u32 *cmdbuf = getThreadCommandBuffer();
    Result res = 0;
    switch (cmdbuf[0] >> 16)
    {
    case 1: // Enable debugger
        if (cmdbuf[0] != IPC_MakeHeader(1, 1, 0))
        {
            cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
            cmdbuf[1] = 0xD9001830;
            break;
        }
        if (cmdbuf[1])
        {
            res = debuggerEnable(5 * 1000 * 1000 * 1000LL);
        }
        else
        {
            res = debuggerDisable(2 * 1000 * 1000 * 1000LL);
        }

        cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
        cmdbuf[1] = res;
        break;
    case 2: // Debug next process
        if (cmdbuf[0] != IPC_MakeHeader(2, 0, 0))
        {
            cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
            cmdbuf[1] = 0xD9001830;
            break;
        }
        GDB_LockAllContexts(&gdbServer);
        res = debugNextApplicationByForce();
        GDB_UnlockAllContexts(&gdbServer);
        cmdbuf[1] = res;
        break;
    }
}

void handleNextApplicationDebuggedByForce(u32 notificationId)
{
    (void)notificationId;
    // Following call needs to be async because pm -> Loader depends on rosalina hb:ldr, handled in this very thread.
    TaskRunner_RunTask(debuggerFetchAndSetNextApplicationDebugHandleTask, NULL, 0);
}