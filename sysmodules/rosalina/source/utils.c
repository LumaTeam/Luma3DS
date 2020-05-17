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

#include "utils.h"
#include "csvc.h"
#include <string.h>
#include <stdio.h>

void formatMemoryPermission(char *outbuf, MemPerm perm)
{
    if (perm == MEMPERM_DONTCARE)
    {
        strcpy(outbuf, "???");
        return;
    }

    outbuf[0] = perm & MEMPERM_READ ? 'r' : '-';
    outbuf[1] = perm & MEMPERM_WRITE ? 'w' : '-';
    outbuf[2] = perm & MEMPERM_EXECUTE ? 'x' : '-';
    outbuf[3] = '\0';
}

void formatUserMemoryState(char *outbuf, MemState state)
{
    static const char *states[12] =
    {
        "Free",
        "Reserved",
        "IO",
        "Static",
        "Code",
        "Private",
        "Shared",
        "Continuous",
        "Aliased",
        "Alias",
        "AliasCode",
        "Locked"
    };

    strcpy(outbuf, state > 11 ? "Unknown" : states[state]);
}

u32 formatMemoryMapOfProcess(char *outbuf, u32 bufLen, Handle handle)
{
    u32         maxLineSize = 35 + (handle == CUR_PROCESS_HANDLE ? 15 : 0);
    u32         address = 0;
    u32         posInBuffer = 0;
    u32         maxPosInBuffer = bufLen - maxLineSize; // 35 is the maximum length of a formatted region
    MemInfo     memi;
    PageInfo    pagei;
    char        pabuf[32];
    char        permbuf[8];
    char        statebuf[16];

    s64 TTBCR;
    svcGetSystemInfo(&TTBCR, 0x10002, 0);

    while (address < (1u << (32 - (u32)TTBCR)) // Limit to check for regions
        && posInBuffer < maxPosInBuffer
        && R_SUCCEEDED(svcQueryProcessMemory(&memi, &pagei, handle, address)))
    {
        // Update the address for next region
        address = memi.base_addr + memi.size;

        // If region isn't FREE then add it to the list
        if (memi.state != MEMSTATE_FREE)
        {
            if (handle == CUR_PROCESS_HANDLE)
            {
                u32 pa = svcConvertVAToPA((void *)memi.base_addr, false);
                sprintf(pabuf, " (PA %08lx)", pa);
            }
            else
                pabuf[0] = '\0';

            formatMemoryPermission(permbuf, memi.perm);
            formatUserMemoryState(statebuf, memi.state);

            posInBuffer += sprintf(outbuf + posInBuffer, "%08lx - %08lx%s %s %s\n",
                memi.base_addr, address, pabuf, permbuf, statebuf);
        }
    }

    svcCloseHandle(handle);
    return posInBuffer;
}
