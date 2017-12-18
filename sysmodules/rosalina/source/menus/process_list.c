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
#include "csvc.h"
#include "draw.h"
#include "menu.h"
#include "utils.h"
#include "fmt.h"
#include "ifile.h"
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

static void ProcessListMenu_DumpMemory(const char *name, void *start, u32 size)
{
#define TRY(expr) if(R_FAILED(res = (expr))) goto end;

    Draw_Lock();
    Draw_DrawString(10, 10, COLOR_TITLE, "Memory dump");
    const char * wait_message = "Please wait, this may take a while...";
    Draw_DrawString(10, 30, COLOR_WHITE, wait_message);
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u64 total;
    IFile file;
    Result res;

    char filename[64] = {0};

    FS_Archive archive;
    FS_ArchiveID archiveId;
    s64 out;
    bool isSdMode;

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if(R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/dumps"), 0);
        if((u32)res == 0xC82044BE) // directory already exists
            res = 0;
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/dumps/memory"), 0);
        if((u32)res == 0xC82044BE) // directory already exists
            res = 0;
        FSUSER_CloseArchive(archive);
    }

    unsigned int seconds, minutes, hours, days, year, month;
    u64 milliseconds = osGetTime();
    seconds = milliseconds/1000;
    milliseconds %= 1000;
    minutes = seconds / 60;
    seconds %= 60;
    hours = minutes / 60;
    minutes %= 60;
    days = hours / 24;
    hours %= 24;

    year = 1900;

    while(true)
    {
        bool leapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        unsigned int daysInYear = leapYear ? 366 : 365;
        if (days >= daysInYear)
        {
            days -= daysInYear;
            ++year;
        }
        else
        {
            const unsigned int daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            for(month = 0; month < 12; ++month)
            {
                unsigned int dim = daysInMonth[month];

                if (month == 1 && leapYear)
                    ++dim;

                if (days >= dim)
                    days -= dim;
                else
                    break;
            }
            break;
        }
    }
    days++;
    month++;

    sprintf(filename, "/luma/dumps/memory/%.8s_0x%.8lx_%.4u-%.2u-%.2uT%.2u-%.2u-%.2u.bin", name, (u32)start, year, month, days, hours, minutes, seconds);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    TRY(IFile_Write(&file, &total, start, size, 0));
    TRY(IFile_Close(&file));

end:
    IFile_Close(&file);

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Memory dump");
        Draw_DrawFormattedString(10, 30, COLOR_WHITE, "%*s", strlen(wait_message), " ");
        if(R_FAILED(res))
        {
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%.8lx).", res);
        }
        else
        {
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
        }
        Draw_DrawString(10, 30+SPACING_Y, COLOR_WHITE, "Press B to go back.");

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);

#undef TRY
}

static void ProcessListMenu_MemoryViewer(const ProcessInfo *info)
{
    Handle processHandle;
    Result res = OpenProcessByName(info->name, &processHandle);

    if(R_SUCCEEDED(res))
    {
        u32 codeStartAddress, heapStartAddress;
        u32 codeDestAddress, heapDestAddress;
        u32 codeTotalSize, heapTotalSize;

        s64 textStartAddress, textTotalRoundedSize, rodataTotalRoundedSize, dataTotalRoundedSize;

        svcGetProcessInfo(&textTotalRoundedSize, processHandle, 0x10002);
        svcGetProcessInfo(&rodataTotalRoundedSize, processHandle, 0x10003);
        svcGetProcessInfo(&dataTotalRoundedSize, processHandle, 0x10004);

        svcGetProcessInfo(&textStartAddress, processHandle, 0x10005);

        codeTotalSize = (u32)(textTotalRoundedSize + rodataTotalRoundedSize + dataTotalRoundedSize);
        codeStartAddress = (u32)textStartAddress; //should be 0x00100000, rarely 0x14000000
        codeDestAddress = 0x00100000;

        MemInfo mem;
        PageInfo out;

        heapDestAddress = heapStartAddress = 0x08000000;
        svcQueryProcessMemory(&mem, &out, processHandle, heapStartAddress);
        heapTotalSize = mem.size;

        Result codeRes = svcMapProcessMemoryEx(processHandle, codeDestAddress, codeStartAddress, codeTotalSize);
        Result heapRes = svcMapProcessMemoryEx(processHandle, heapDestAddress, heapStartAddress, heapTotalSize);

        bool codeAvailable = R_SUCCEEDED(codeRes);
        bool heapAvailable = R_SUCCEEDED(heapRes);

        if(codeAvailable || heapAvailable)
        {
            #define ROWS_PER_SCREEN 0x10
            #define BYTES_PER_ROW 0x10
            #define VIEWER_PAGE_SIZE (ROWS_PER_SCREEN*BYTES_PER_ROW)

            #define totalRows ((menus[MENU_MODE_NORMAL].max - (menus[MENU_MODE_NORMAL].max % BYTES_PER_ROW))/ROWS_PER_SCREEN)

            enum MenuModes {
                MENU_MODE_NORMAL = 0,
                MENU_MODE_GOTO,
                MENU_MODE_SEARCH,

                MENU_MODE_MAX,
            };

            typedef struct {
                u32 selected;
                u8 * buf;

                u32 starti;
                u32 max;
            } MenuData;

            bool editing;

            MenuData menus[MENU_MODE_MAX] = {0};
            int menuMode = MENU_MODE_NORMAL;

            bool checkMode(int newMode)
            {
                if(menuMode == newMode)
                {
                    menuMode = MENU_MODE_NORMAL;
                    return true;
                }
                else
                {
                    menuMode = newMode;
                    return false;
                }
            }

            // Editing
            void selectedByteIncrement(void) { menus[menuMode].buf[menus[menuMode].selected]++; }
            void selectedByteDecrement(void) { menus[menuMode].buf[menus[menuMode].selected]--; }

            void selectedByteAdd0x10(void) { menus[menuMode].buf[menus[menuMode].selected] += 0x10; }
            void selectedByteSub0x10(void) { menus[menuMode].buf[menus[menuMode].selected] -= 0x10; }
            // ------------------------------------------

            // Movement
            #define SELECTED_DEC(decval) do { \
                if(menus[menuMode].selected >= decval) \
                    menus[menuMode].selected -= decval; \
            } while(0)

            #define SELECTED_INC(incval) do { \
                if(menus[menuMode].selected < (menus[menuMode].max - incval)) \
                    menus[menuMode].selected += incval; \
            } while(0)

            void selectedMoveLeft(void) { SELECTED_DEC(1); }
            void selectedMoveRight(void) { SELECTED_INC(1); }

            void selectedMoveUp(void) { SELECTED_DEC(BYTES_PER_ROW); }
            void selectedMoveDown(void) { SELECTED_INC(BYTES_PER_ROW); }
            // ------------------------------------------

            // Viewing
            void viewHeap(void)
            {
                if(!heapAvailable) return;
                menus[MENU_MODE_NORMAL].selected = 0;
                menus[MENU_MODE_NORMAL].buf = (u8*)heapDestAddress;
                menus[MENU_MODE_NORMAL].max = heapTotalSize;
            }
            void viewCode(void)
            {
                if(!codeAvailable) return;
                menus[MENU_MODE_NORMAL].selected = 0;
                menus[MENU_MODE_NORMAL].buf = (u8*)codeDestAddress;
                menus[MENU_MODE_NORMAL].max = codeTotalSize;
            }

            if(heapAvailable)
                viewHeap();
            else
                viewCode();
            // ------------------------------------------

            // Jumping
            u32 gotoAddress = 0;

            void finishJumping(void)
            {
                gotoAddress = __builtin_bswap32(gotoAddress); // The data is edited in reverse, so it needs to be swapped before usage

                u32 codeEndAddress = codeStartAddress + codeTotalSize;
                u32 heapEndAddress = heapStartAddress + heapTotalSize;
                if(gotoAddress >= codeStartAddress && gotoAddress < codeEndAddress)
                    viewCode();
                else if(gotoAddress >= heapStartAddress && gotoAddress < heapEndAddress)
                    viewHeap();

                gotoAddress -= (u32)menus[MENU_MODE_NORMAL].buf;
                menus[MENU_MODE_NORMAL].selected = gotoAddress;
                menus[MENU_MODE_NORMAL].starti = totalRows;
            }

            menus[MENU_MODE_GOTO].buf = (u8*)&gotoAddress;
            menus[MENU_MODE_GOTO].max = sizeof(gotoAddress);
            // ------------------------------------------

            // Searching
            #define searchPatternSize menus[MENU_MODE_SEARCH].max
            #define searchPatternMaxSize (u32)VIEWER_PAGE_SIZE
            u8 searchPattern[searchPatternMaxSize] = {0};

            void searchPatternEnlarge(void)
            {
                searchPatternSize++;
                if(searchPatternSize > searchPatternMaxSize)
                    searchPatternSize = 1;
            }
            void searchPatternReduce(void)
            {
                searchPatternSize--;
                if(searchPatternSize < 1)
                    searchPatternSize = searchPatternMaxSize;
            }

            void finishSearching(void)
            {
                u8 * startpos = (u8*)((u32)menus[MENU_MODE_NORMAL].buf + menus[MENU_MODE_NORMAL].selected);
                u32 size = menus[MENU_MODE_NORMAL].max - menus[MENU_MODE_NORMAL].selected;
                if (size >= searchPatternSize)
                    menus[MENU_MODE_NORMAL].selected = (u32)memsearch(startpos, searchPattern, size, searchPatternSize) - (u32)menus[MENU_MODE_NORMAL].buf;
            }

            menus[MENU_MODE_SEARCH].buf = searchPattern;
            menus[MENU_MODE_SEARCH].max = 1;
            // ------------------------------------------

            void drawMenu(void)
            {
                Draw_Lock();
                Draw_DrawString(10, 10, COLOR_TITLE, "Memory viewer");

                // Instructions
                const u32 instructionsY = 30;
                u32 viewerY = instructionsY + SPACING_Y + 6;
                Draw_DrawString(10, instructionsY, COLOR_WHITE, "D-PAD to move, X to jump, Y to search, A to edit.");

                switch(menuMode)
                {
                    case MENU_MODE_NORMAL:
                        Draw_DrawString(10 + SPACING_X * 9, instructionsY, COLOR_GREEN, "move");
                        break;
                    case MENU_MODE_GOTO:
                        Draw_DrawString(10 + SPACING_X * 20, instructionsY, COLOR_GREEN, "jump");
                        break;
                    case MENU_MODE_SEARCH:
                        Draw_DrawString(10 + SPACING_X * 31, instructionsY, COLOR_GREEN, "search");
                        break;
                    default: break;
                }

                if(editing)
                    Draw_DrawString(10 + SPACING_X * 44, instructionsY, COLOR_RED, "edit");
                // ------------------------------------------

                // Location
                if(codeAvailable && heapAvailable)
                {
                    const u32 infoY = instructionsY + SPACING_Y;
                    viewerY += SPACING_Y;
                    Draw_DrawString(10, infoY, COLOR_WHITE, "Press L or R to switch between heap and code.");
                    if((u32)menus[MENU_MODE_NORMAL].buf == heapDestAddress)
                        Draw_DrawString(10 + SPACING_X * 31, infoY, COLOR_GREEN, "heap");
                    if((u32)menus[MENU_MODE_NORMAL].buf == codeDestAddress)
                        Draw_DrawString(10 + SPACING_X * 40, infoY, COLOR_GREEN, "code");
                }
                // ------------------------------------------

                for(u32 row = menus[menuMode].starti; row < (menus[menuMode].starti + ROWS_PER_SCREEN); row++)
                {
                    u32 offset = row - menus[menuMode].starti;
                    u32 y = viewerY + offset*SPACING_Y;

                    u32 address = row*BYTES_PER_ROW;
                    Draw_DrawFormattedString(10, y, COLOR_TITLE, "%.8lx | ", address + ((menuMode == MENU_MODE_NORMAL) ? (u32)menus[MENU_MODE_NORMAL].buf : 0));

                    for(int cursor = 0; cursor < BYTES_PER_ROW; cursor++, address++)
                    {
                        u32 x = 10+66 + cursor*14 + (cursor >= BYTES_PER_ROW/2)*10;

                        if(address < menus[menuMode].max)
                        {
                            Draw_DrawFormattedString(x, y,
                            address == menus[menuMode].selected ? (editing ? COLOR_RED : COLOR_GREEN) : COLOR_WHITE,
                            "%.2x",
                            menus[menuMode].buf[address]);
                        }
                        else
                            Draw_DrawString(x, y, COLOR_WHITE, "  ");
                    }
                }

                Draw_FlushFramebuffer();
                Draw_Unlock();
            }

            void clearMenu(void)
            {
                Draw_Lock();
                Draw_ClearFramebuffer();
                Draw_FlushFramebuffer();
                Draw_Unlock();
            }

            void handleScrolling(void)
            {
                for(u32 i = totalRows; i > 0 ; i--)
                {

                    u32 scroll = menus[MENU_MODE_NORMAL].starti;
                    u32 selectedRow = (menus[MENU_MODE_NORMAL].selected - (menus[MENU_MODE_NORMAL].selected % BYTES_PER_ROW))/BYTES_PER_ROW;

                    if(scroll > selectedRow)
                        scroll--;

                    if((i <= selectedRow) && \
                       ((selectedRow - scroll) >= ROWS_PER_SCREEN) && \
                       (scroll != (totalRows - ROWS_PER_SCREEN)))
                        scroll++;

                    menus[MENU_MODE_NORMAL].starti = scroll;
                }

                if (menus[MENU_MODE_NORMAL].starti > (totalRows - ROWS_PER_SCREEN))
                    menus[MENU_MODE_NORMAL].starti = totalRows - ROWS_PER_SCREEN;
            }

            clearMenu();

            do
            {
                if(menuMode == MENU_MODE_NORMAL)
                    handleScrolling();

                drawMenu();

                u32 pressed = waitInputWithTimeout(1000);

                if(pressed & BUTTON_A)
                    editing = !editing;
                else if(pressed & BUTTON_X)
                {
                    if(checkMode(MENU_MODE_GOTO))
                        finishJumping();
                    else
                        gotoAddress = __builtin_bswap32(((u32)menus[MENU_MODE_NORMAL].buf) + menus[MENU_MODE_NORMAL].selected);
                }
                else if(pressed & BUTTON_Y)
                {
                    if(checkMode(MENU_MODE_SEARCH))
                        finishSearching();
                }
                else if(pressed & BUTTON_SELECT)
                {
                    clearMenu();
                    ProcessListMenu_DumpMemory(info->name, menus[MENU_MODE_NORMAL].buf, menus[MENU_MODE_NORMAL].max);
                    clearMenu();
                }

                if(editing)
                {
                    // Edit the highlighted byte
                    if(pressed & BUTTON_LEFT)
                        selectedByteAdd0x10();
                    else if(pressed & BUTTON_RIGHT)
                        selectedByteSub0x10();
                    else if(pressed & BUTTON_UP)
                        selectedByteIncrement();
                    else if(pressed & BUTTON_DOWN)
                        selectedByteDecrement();
                }
                else
                {
                    // Move the cursor
                    if(pressed & BUTTON_LEFT)
                        selectedMoveLeft();
                    else if(pressed & BUTTON_RIGHT)
                        selectedMoveRight();
                    else if(pressed & BUTTON_UP)
                        selectedMoveUp();
                    else if(pressed & BUTTON_DOWN)
                        selectedMoveDown();

                    else if(pressed & BUTTON_L1)
                    {
                        if(menuMode == MENU_MODE_NORMAL)
                            viewHeap();
                        else if(menuMode == MENU_MODE_SEARCH)
                            searchPatternReduce();
                    }
                    else if(pressed & BUTTON_R1)
                    {
                        if(menuMode == MENU_MODE_NORMAL)
                            viewCode();
                        else if(menuMode == MENU_MODE_SEARCH)
                            searchPatternEnlarge();
                    }
                }

                if(pressed & BUTTON_B) // go back to the list, or the simple viewer
                {
                    if(menuMode != MENU_MODE_NORMAL)
                    {
                        menuMode = MENU_MODE_NORMAL;
                        editing = false;
                    }
                    else
                        break;
                }

                if(menus[menuMode].selected >= menus[menuMode].max)
                    menus[menuMode].selected = menus[menuMode].max - 1;
            }
            while(!terminationRequest);

            clearMenu();
        }

        if(codeAvailable)
            svcUnmapProcessMemoryEx(processHandle, codeDestAddress, codeTotalSize);
        if(heapAvailable)
            svcUnmapProcessMemoryEx(processHandle, heapDestAddress, heapTotalSize);

        svcCloseHandle(processHandle);
    }
}

static inline void ProcessListMenu_HandleSelected(const ProcessInfo *info)
{
    if(!gdbServer.super.running || info->isZombie)
    {
        ProcessListMenu_MemoryViewer(info);
        return;
    }

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
