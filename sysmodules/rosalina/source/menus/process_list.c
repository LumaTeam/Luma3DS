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

#include <3ds.h>
#include "menus/process_list.h"
#include "memory.h"
#include "draw.h"
#include "menu.h"
#include "utils.h"
#include "fmt.h"
#include "gdb/server.h"
#include "minisoc.h"
#include <arpa/inet.h>
#include <sys/socket.h>

typedef struct ProcessInfo
{
    u32 pid;
    u64 titleId;
    char name[8];
    bool isZombie;
} ProcessInfo;

static ProcessInfo infos[0x40] = {0}, infosPrev[0x40] = {0};
extern GDBServer gdbServer;

static inline int ProcessListMenu_FormatInfoLine(char *out, const ProcessInfo *info)
{
    const char *checkbox;
    u32 id;
    for(id = 0; id < MAX_DEBUG && (!(gdbServer.ctxs[id].flags & GDB_FLAG_SELECTED) || gdbServer.ctxs[id].pid != info->pid); id++);
    checkbox = !gdbServer.super.running ? "" : (id < MAX_DEBUG ? "(x) " : "( ) ");

    char commentBuf[23 + 1] = { 0 }; // exactly the size of "Remote: 255.255.255.255"
    memset(commentBuf, ' ', 23);

    if(info->isZombie)
        memcpy(commentBuf, "Zombie", 7);

    else if(gdbServer.super.running && id < MAX_DEBUG)
    {
        if(gdbServer.ctxs[id].state >= GDB_STATE_CONNECTED && gdbServer.ctxs[id].state < GDB_STATE_CLOSING)
        {
            u8 *addr = (u8 *)&gdbServer.ctxs[id].super.addr_in.sin_addr;
            checkbox = "(A) ";
            sprintf(commentBuf, "Remote: %hhu.%hhu.%hhu.%hhu", addr[0], addr[1], addr[2], addr[3]);
        }
        else
        {
            checkbox = "(W) ";
            sprintf(commentBuf, "Port: %d", GDB_PORT_BASE + id);
        }
    }

    return sprintf(out, "%s%-4u    %-8.8s    %s", checkbox, info->pid, info->name, commentBuf); // Theoritically PIDs are 32-bit ints, but we'll only justify 4 digits
}

static inline void ProcessListMenu_HandleSelected(const ProcessInfo *info)
{
    if(!gdbServer.super.running || info->isZombie)
        return;

    u32 id;
    for(id = 0; id < MAX_DEBUG && (!(gdbServer.ctxs[id].flags & GDB_FLAG_SELECTED) || gdbServer.ctxs[id].pid != info->pid); id++);

    GDBContext *ctx = &gdbServer.ctxs[id];

    if(id < MAX_DEBUG)
    {
        if(ctx->flags & GDB_FLAG_USED)
        {
            RecursiveLock_Lock(&ctx->lock);
            ctx->super.should_close = true;
            RecursiveLock_Unlock(&ctx->lock);

            while(ctx->super.should_close)
                svcSleepThread(12 * 1000 * 1000LL);
        }
        else
        {
            RecursiveLock_Lock(&ctx->lock);
            ctx->flags &= ~GDB_FLAG_SELECTED;
            RecursiveLock_Unlock(&ctx->lock);
        }
    }
    else
    {
        for(id = 0; id < MAX_DEBUG && gdbServer.ctxs[id].flags & GDB_FLAG_SELECTED; id++);
        if(id < MAX_DEBUG)
        {
            ctx = &gdbServer.ctxs[id];
            RecursiveLock_Lock(&ctx->lock);
            ctx->pid = info->pid;
            ctx->flags |= GDB_FLAG_SELECTED;
            RecursiveLock_Unlock(&ctx->lock);
        }
    }
}

s32 ProcessListMenu_FetchInfo(void)
{
    u32 pidList[0x40];
    s32 processAmount;

    svcGetProcessList(&processAmount, pidList, 0x40);

    for(s32 i = 0; i < processAmount; i++)
    {
        Handle processHandle;
        Result res = svcOpenProcess(&processHandle, pidList[i]);
        if(R_FAILED(res))
            continue;

        infos[i].pid = pidList[i];
        svcGetProcessInfo((s64 *)&infos[i].name, processHandle, 0x10000);
        svcGetProcessInfo((s64 *)&infos[i].titleId, processHandle, 0x10001);
        infos[i].isZombie = svcWaitSynchronization(processHandle, 0) == 0;
        svcCloseHandle(processHandle);
    }

    return processAmount;
}

void RosalinaMenu_ProcessList(void)
{
    s32 processAmount = ProcessListMenu_FetchInfo();
    s32 selected = 0, page = 0, pagePrev = 0;
    nfds_t nfdsPrev;

    do
    {
        nfdsPrev = gdbServer.super.nfds;
        memcpy(infosPrev, infos, sizeof(infos));

        Draw_Lock();
        if(page != pagePrev)
            Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_TITLE, "Process list");

        if(gdbServer.super.running)
        {
            char ipBuffer[17];
            u32 ip = gethostid();
            u8 *addr = (u8 *)&ip;
            int n = sprintf(ipBuffer, "%hhu.%hhu.%hhu.%hhu", addr[0], addr[1], addr[2], addr[3]);
            Draw_DrawString(SCREEN_BOT_WIDTH - 10 - SPACING_X * n, 10, COLOR_WHITE, ipBuffer);
        }


        for(s32 i = 0; i < PROCESSES_PER_MENU_PAGE && page * PROCESSES_PER_MENU_PAGE + i < processAmount; i++)
        {
            char buf[65] = {0};
            ProcessListMenu_FormatInfoLine(buf, &infos[page * PROCESSES_PER_MENU_PAGE + i]);

            Draw_DrawString(30, 30 + i * SPACING_Y, COLOR_WHITE, buf);
            Draw_DrawCharacter(10, 30 + i * SPACING_Y, COLOR_TITLE, page * PROCESSES_PER_MENU_PAGE + i == selected ? '>' : ' ');
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        if(terminationRequest)
            break;

        u32 pressed;
        do
        {
            pressed = waitInputWithTimeout(50);
            if(pressed != 0 || nfdsPrev != gdbServer.super.nfds)
                break;
            processAmount = ProcessListMenu_FetchInfo();
            if(memcmp(infos, infosPrev, sizeof(infos)) != 0)
                break;
        }
        while(pressed == 0 && !terminationRequest);

        if(pressed & BUTTON_B)
            break;
        else if(pressed & BUTTON_A)
            ProcessListMenu_HandleSelected(&infos[selected]);
        else if(pressed & BUTTON_DOWN)
            selected++;
        else if(pressed & BUTTON_UP)
            selected--;
        else if(pressed & BUTTON_LEFT)
            selected -= PROCESSES_PER_MENU_PAGE;
        else if(pressed & BUTTON_RIGHT)
        {
            if(selected + PROCESSES_PER_MENU_PAGE < processAmount)
                selected += PROCESSES_PER_MENU_PAGE;
            else if((processAmount - 1) / PROCESSES_PER_MENU_PAGE == page)
                selected %= PROCESSES_PER_MENU_PAGE;
            else
                selected = processAmount - 1;
        }

        if(selected < 0)
            selected = processAmount - 1;
        else if(selected >= processAmount)
            selected = 0;

        pagePrev = page;
        page = selected / PROCESSES_PER_MENU_PAGE;
    }
    while(!terminationRequest);
}
