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

#include "gsp.h"
#include "utils.h"
#include "memory.h"

static bool gspMapped = false;
static Handle gspHandle;
static u32 totalSize = 0;

static bool offsetFound = false;
static uintptr_t patchOffset = 0;

static bool patchApplied = false;
static u32 originalValues[2][4] = {0};

static Result openAndMapGSP(void)
{
    s64 startAddress, textTotalRoundedSize = 0, rodataTotalRoundedSize = 0, dataTotalRoundedSize = 0;

    Result res = OpenProcessByName("gsp", &gspHandle);

    if(R_SUCCEEDED(res))
    {
        svcGetProcessInfo(&textTotalRoundedSize, gspHandle, 0x10002);
        svcGetProcessInfo(&rodataTotalRoundedSize, gspHandle, 0x10003);
        svcGetProcessInfo(&dataTotalRoundedSize, gspHandle, 0x10004);

        totalSize = (u32)(textTotalRoundedSize + rodataTotalRoundedSize + dataTotalRoundedSize);

        svcGetProcessInfo(&startAddress, gspHandle, 0x10005);
        res = svcMapProcessMemoryEx(gspHandle, 0x00100000, (u32) startAddress, totalSize); //map GSP's memory into our process

        gspMapped = R_SUCCEEDED(res);
    }

    return res;
}

static Result getPatchOffset(void)
{
    static const u32 gspCode[] =
    {
        0xE92D4030,
        0xE1A04000,
        0xE2805C01,
        0xE5D0018C,
    };

    patchOffset = (uintptr_t)memsearch((u8 *)0x00100000, &gspCode, totalSize, sizeof(gspCode));

    offsetFound = (patchOffset != 0);

    return !offsetFound;
}

Result patchGSP(void)
{
    Result res = openAndMapGSP();
    if(R_SUCCEEDED(res))
    {
        if(!offsetFound)
            res = getPatchOffset();

        if(R_SUCCEEDED(res))
        {
            if(!patchApplied)
            {
                memcpy(originalValues[0], (u32 *)(patchOffset + 0x68), 4*sizeof(u32));
                memcpy(originalValues[1], (u32 *)(patchOffset + 0xC8), 4*sizeof(u32));

                memset((u32 *)(patchOffset + 0x68), 0, 4*sizeof(u32));
                memset((u32 *)(patchOffset + 0xC8), 0, 4*sizeof(u32));

                patchApplied = true;
            }
        }

        svcCloseHandle(gspHandle);
        gspMapped = false;
    }

    return res;
}

Result unpatchGSP(void)
{
    Result res = openAndMapGSP();
    if(R_SUCCEEDED(res))
    {
        if(!offsetFound)
            res = getPatchOffset();

        if(R_SUCCEEDED(res))
        {
            if(patchApplied)
            {
                memcpy((u32 *)(patchOffset + 0x68), originalValues[0], 4*sizeof(u32));
                memcpy((u32 *)(patchOffset + 0xC8), originalValues[1], 4*sizeof(u32));

                patchApplied = false;
            }
        }

        svcCloseHandle(gspHandle);
        gspMapped = false;
    }

    return res;
}
