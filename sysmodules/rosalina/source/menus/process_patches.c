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
#include "csvc.h"
#include "menus/process_patches.h"
#include "memory.h"
#include "draw.h"
#include "hbloader.h"
#include "fmt.h"
#include "utils.h"

Menu processPatchesMenu = {
    "Process patches menu",
    .nbItems = 2,
    {
        { "Patch SM for the service checks", METHOD, .method = &ProcessPatchesMenu_PatchUnpatchSM },
        { "Patch FS for the archive checks", METHOD, .method = &ProcessPatchesMenu_PatchUnpatchFS },
    }
};

static Result ProcessPatchesMenu_DoPatchUnpatchSM(u32 textTotalRoundedSize)
{
    static bool patched = false;
    static u32 *off;
    static u32 origData;

    if(patched)
    {
        *off = origData;
        patched = false;
    }
    else
    {
        for(off = (u32 *)0x00100000; off < (u32 *)(0x00100000 + textTotalRoundedSize) - 3 &&
            (off[0] != 0xE1A01006 || (off[1] & 0xFFFF) != 5);
            off++);

        if(off >= (u32 *)(0x00100000 + textTotalRoundedSize) - 3)
            return -1;

        off += 2;
        *off = 0xE3A00001; // mov r0, #1
        patched = true;
    }

    processPatchesMenu.items[0].title = patched ? "Unpatch SM for the service checks" : "Patch SM for the service checks";
    return 0;
}

static Result ProcessPatchesMenu_DoPatchUnpatchFS(u32 textTotalRoundedSize)
{
    static bool patched = false;
    static u16 *off;
    static u16 origData[2];
    static const u16 pattern[2] = {
        0x7401, // strb r1, [r0, #16]
        0x2000, // movs r0, #0
    };

    if(patched)
    {
        memcpy(off, &origData, sizeof(origData));
        patched = false;
    }
    else
    {
        off = (u16 *)memsearch((u8 *)0x00100000, &pattern, textTotalRoundedSize, sizeof(pattern));
        if(off == NULL)
            return -1;

        for(; (*off & 0xFF00) != 0xB500; off++); // Find function start

        memcpy(origData, off, 4);
        off[0] = 0x2001; // mov r0, #1
        off[1] = 0x4770; // bx lr

        patched = true;
    }

    processPatchesMenu.items[1].title = patched ? "Unpatch FS for the archive checks" : "Patch FS for the archive checks";
    return 0;
}

Result OpenProcessByName(const char *name, Handle *h)
{
    u32 pidList[0x40];
    s32 processCount;
    svcGetProcessList(&processCount, pidList, 0x40);
    Handle dstProcessHandle = 0;

    for(s32 i = 0; i < processCount; i++)
    {
        Handle processHandle;
        Result res = svcOpenProcess(&processHandle, pidList[i]);
        if(R_FAILED(res))
            continue;

        char procName[8] = {0};
        svcGetProcessInfo((s64 *)procName, processHandle, 0x10000);
        if(strncmp(procName, name, 8) == 0)
            dstProcessHandle = processHandle;
        else
            svcCloseHandle(processHandle);
    }

    if(dstProcessHandle == 0)
        return -1;

    *h = dstProcessHandle;
    return 0;
}

static u32 ProcessPatchesMenu_PatchUnpatchProcessByName(const char *name, Result (*func)(u32 size))
{
    Result res;
    Handle processHandle;
    OpenProcessByName(name, &processHandle);

    s64 textTotalRoundedSize = 0, startAddress = 0;
    svcGetProcessInfo(&textTotalRoundedSize, processHandle, 0x10002); // only patch .text
    svcGetProcessInfo(&startAddress, processHandle, 0x10005);
    if(R_FAILED(res = svcMapProcessMemoryEx(processHandle, 0x00100000, (u32) startAddress, textTotalRoundedSize)))
        return res;

    res = func(textTotalRoundedSize);

    svcUnmapProcessMemoryEx(processHandle, 0x00100000, textTotalRoundedSize);
    return res;
}

static void ProcessPatchesMenu_PatchUnpatchProcess(const char *processName, Result (*func)(u32 size))
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    Result res = ProcessPatchesMenu_PatchUnpatchProcessByName(processName, func);

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Process patches menu");
        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08x).", res);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void ProcessPatchesMenu_PatchUnpatchSM(void)
{
    ProcessPatchesMenu_PatchUnpatchProcess("sm", &ProcessPatchesMenu_DoPatchUnpatchSM);
}

void ProcessPatchesMenu_PatchUnpatchFS(void)
{
    ProcessPatchesMenu_PatchUnpatchProcess("fs", &ProcessPatchesMenu_DoPatchUnpatchFS);
}
