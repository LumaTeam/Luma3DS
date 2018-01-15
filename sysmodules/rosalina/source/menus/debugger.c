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

#include "menus/debugger.h"
#include "memory.h"
#include "draw.h"
#include "minisoc.h"
#include "fmt.h"
#include "gdb/server.h"
#include "gdb/debug.h"
#include "gdb/monitor.h"
#include "gdb/net.h"

Menu debuggerMenu = {
    "Debugger options menu",
    .nbItems = 2,
    {
        { "Enable debugger",  METHOD, .method = &DebuggerMenu_EnableDebugger  },
        { "Disable debugger", METHOD, .method = &DebuggerMenu_DisableDebugger }
    }
};

static MyThread debuggerSocketThread;
static MyThread debuggerDebugThread;
static u8 ALIGN(8) debuggerSocketThreadStack[0x4000];
static u8 ALIGN(8) debuggerDebugThreadStack[0x2000];

GDBServer gdbServer = { 0 };

void debuggerSocketThreadMain(void);
MyThread *debuggerCreateSocketThread(void)
{
    MyThread_Create(&debuggerSocketThread, debuggerSocketThreadMain, debuggerSocketThreadStack, 0x4000, 0x20, CORE_SYSTEM);
    return &debuggerSocketThread;
}

void debuggerDebugThreadMain(void);
MyThread *debuggerCreateDebugThread(void)
{
    MyThread_Create(&debuggerDebugThread, debuggerDebugThreadMain, debuggerDebugThreadStack, 0x2000, 0x20, CORE_SYSTEM);
    return &debuggerDebugThread;
}

void DebuggerMenu_EnableDebugger(void)
{
    bool done = false, alreadyEnabled = gdbServer.super.running;
    Result res = 0;
    char buf[65];
    bool isSocURegistered;

    res = srvIsServiceRegistered(&isSocURegistered, "soc:U");
    isSocURegistered = R_SUCCEEDED(res) && isSocURegistered;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Debugger options menu");

        if(alreadyEnabled)
            Draw_DrawString(10, 30, COLOR_WHITE, "Already enabled!");
        else if(!isSocURegistered)
            Draw_DrawString(10, 30, COLOR_WHITE, "Can't start the debugger before the system has fi-\nnished loading.");
        else
        {
            Draw_DrawString(10, 30, COLOR_WHITE, "Starting debugger...");

            if(!done)
            {
                res = GDB_InitializeServer(&gdbServer);
                if(R_SUCCEEDED(res))
                {
                    debuggerCreateSocketThread();
                    debuggerCreateDebugThread();
                    res = svcWaitSynchronization(gdbServer.super.started_event, 10 * 1000 * 1000 * 1000LL);
                }

                if(res != 0)
                    sprintf(buf, "Starting debugger... failed (0x%08x).", (u32)res);

                done = true;
            }
            if(res == 0)
                Draw_DrawString(10, 30, COLOR_WHITE, "Starting debugger... OK.");
            else
                Draw_DrawString(10, 30, COLOR_WHITE, buf);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void DebuggerMenu_DisableDebugger(void)
{
    bool initialized = gdbServer.referenceCount != 0;
    Result res = 0;
    char buf[65];

    if(initialized)
    {
        svcSignalEvent(gdbServer.super.shall_terminate_event);
        res = MyThread_Join(&debuggerDebugThread, 5 * 1000 * 1000 * 1000LL);
        if(res == 0)
            res = MyThread_Join(&debuggerSocketThread, 5 * 1000 * 1000 * 1000LL);
        svcKernelSetState(0x10000, 2);
    }

    if(res != 0)
        sprintf(buf, "Failed to disable debugger (0x%08x).", (u32)res);

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Debugger options menu");
        Draw_DrawString(10, 30, COLOR_WHITE, initialized ? (res == 0 ? "Debugger disabled successfully." : buf) : "Debugger not enabled.");
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void debuggerSocketThreadMain(void)
{
    GDB_IncrementServerReferenceCount(&gdbServer);
    GDB_RunServer(&gdbServer);
    GDB_DecrementServerReferenceCount(&gdbServer);
}

void debuggerDebugThreadMain(void)
{
    GDB_IncrementServerReferenceCount(&gdbServer);
    GDB_RunMonitor(&gdbServer);
    GDB_DecrementServerReferenceCount(&gdbServer);
}
