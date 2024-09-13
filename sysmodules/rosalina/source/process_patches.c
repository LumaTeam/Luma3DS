/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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
#include <string.h>
#include "csvc.h"
#include "process_patches.h"
#include "utils.h"

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

Result OperateOnProcessByName(const char *name, OperateOnProcessCb func)
{
    Result res;
    Handle processHandle;
    res = OpenProcessByName(name, &processHandle);

    if (R_FAILED(res))
        return res;

    s64 startAddress, textSize, roSize, rwSize;

    svcGetProcessInfo(&textSize, processHandle, 0x10002); // rounded sizes
    svcGetProcessInfo(&roSize, processHandle, 0x10003);
    svcGetProcessInfo(&rwSize, processHandle, 0x10004); // data and bss
    svcGetProcessInfo(&startAddress, processHandle, 0x10005);

    // NOTE: we suppose .text, .rodata, .data+.bss are contiguous & in that order
    u32 totalSize = (u32)(textSize + roSize + rwSize);
    if (R_FAILED(res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, 0x00100000, processHandle, (u32) startAddress, totalSize)))
    {
        svcCloseHandle(processHandle);
        return res;
    }

    res = func(processHandle, (u32)textSize, (u32)roSize, (u32)rwSize);

    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, 0x00100000, totalSize);
    return res;
}
