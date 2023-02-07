/*
 *   This file is part of Luma3DS
 *   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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

#include <stdio.h>
#include "menus/debugger_menu.h"
#include "draw.h"

Menu debuggerMenu = {
    "Debugger options menu",
    {
        {"Enable debugger", METHOD, .method = &DebuggerMenu_EnableDebugger},
        {"Disable debugger", METHOD, .method = &DebuggerMenu_DisableDebugger},
        {"Force-debug next application at launch", METHOD, .method = &DebuggerMenu_DebugNextApplicationByForce},
        {},
    }};

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

        if (alreadyEnabled)
            Draw_DrawString(10, 30, COLOR_WHITE, "Already enabled!");
        else if (!isSocURegistered)
            Draw_DrawString(10, 30, COLOR_WHITE, "Can't start the debugger before the system has fi-\nnished loading.");
        else
        {
            Draw_DrawString(10, 30, COLOR_WHITE, "Starting debugger...");

            if (!done)
            {
                res = debuggerEnable(5 * 1000 * 1000 * 1000LL);
                if (res != 0)
                    sprintf(buf, "Starting debugger... failed (0x%08lx).", (u32)res);
                done = true;
            }
            if (res == 0)
                Draw_DrawString(10, 30, COLOR_WHITE, "Starting debugger... OK.");
            else
                Draw_DrawString(10, 30, COLOR_WHITE, buf);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    } while (!(waitInput() & KEY_B) && !menuShouldExit);
}

void DebuggerMenu_DisableDebugger(void)
{
    bool initialized = gdbServer.referenceCount != 0;

    Result res = initialized ? debuggerDisable(2 * 1000 * 1000 * 1000LL) : 0;
    char buf[65];

    if (res != 0)
        sprintf(buf, "Failed to disable debugger (0x%08lx).", (u32)res);

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Debugger options menu");
        Draw_DrawString(10, 30, COLOR_WHITE, initialized ? (res == 0 ? "Debugger disabled successfully." : buf) : "Debugger not enabled.");
        Draw_FlushFramebuffer();
        Draw_Unlock();
    } while (!(waitInput() & KEY_B) && !menuShouldExit);
}

void DebuggerMenu_DebugNextApplicationByForce(void)
{
    bool initialized = gdbServer.referenceCount != 0;
    Result res = 0;
    char buf[256];

    if (initialized)
    {
        GDB_LockAllContexts(&gdbServer);
        res = debugNextApplicationByForce();
        GDB_UnlockAllContexts(&gdbServer);
        switch (res)
        {
        case 0:
            strcpy(buf, "Operation already performed.");
            break;
        case 1:
            sprintf(buf, "Operation succeeded.\nUse port %d to connect to the next launched\napplication.", nextApplicationGdbCtx->localPort);
            break;
        case 2:
            strcpy(buf, "Failed to allocate a slot.\nPlease unselect a process in the process list first");
            break;
        default:
            if (!R_SUCCEEDED(res))
            {
                sprintf(buf, "Operation failed (0x%08lx).", (u32)res);
            }
            break;
        }
    }
    else
        strcpy(buf, "Debugger not enabled.");

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Debugger options menu");
        Draw_DrawString(10, 30, COLOR_WHITE, buf);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    } while (!(waitInput() & KEY_B) && !menuShouldExit);
}
