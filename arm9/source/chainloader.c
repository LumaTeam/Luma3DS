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

#include "chainloader.h"
#include "screen.h"

void disableMpuAndJumpToEntrypoints(int argc, char **argv, void *arm11Entry, void *arm9Entry);

#pragma GCC optimize (3)

static void *xmemcpy(void *dst, const void *src, u32 len)
{
    const u8 *src8 = (const u8 *)src;
    u8 *dst8 = (u8 *)dst;

    for (u32 i = 0; i < len; i++) {
        dst8[i] = src8[i];
    }

    return dst;
}

static void doLaunchFirm(Firm *firm, int argc, char **argv)
{
    //Copy FIRM sections to respective memory locations
    for(u32 sectionNum = 0; sectionNum < 4; sectionNum++)
        xmemcpy(firm->section[sectionNum].address, (u8 *)firm + firm->section[sectionNum].offset, firm->section[sectionNum].size);

    disableMpuAndJumpToEntrypoints(argc, argv, firm->arm9Entry, firm->arm11Entry);

    __builtin_unreachable();
}

void chainloader_main(int argc, char **argv, Firm *firm)
{
    char *argvPassed[2],
         absPath[24 + 255];
    struct fb fbs[2];

    if(argc > 0)
    {
        u32 i;
        for(i = 0; i < sizeof(absPath) - 1 && argv[0][i] != 0; i++)
            absPath[i] = argv[0][i];
        absPath[i] = 0;

        argvPassed[0] = (char *)absPath;
    }

    if(argc == 2)
    {
        struct fb *fbsrc = (struct fb *)argv[1];

        fbs[0] = fbsrc[0];
        fbs[1] = fbsrc[1];

        argvPassed[1] = (char *)&fbs;
    }

    doLaunchFirm(firm, argc, argvPassed);
}
