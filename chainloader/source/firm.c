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

#include "firm.h"
#include "memory.h"
#include "cache.h"

void disableMpuAndJumpToEntrypoints(int argc, char **argv, void *arm11Entry, void *arm9Entry);

void launchFirm(Firm *firm, int argc, char **argv)
{
    //Copy FIRM sections to respective memory locations
    for(u32 sectionNum = 0; sectionNum < 4; sectionNum++)
        memcpy(firm->section[sectionNum].address, (u8 *)firm + firm->section[sectionNum].offset, firm->section[sectionNum].size);

    disableMpuAndJumpToEntrypoints(argc, argv, firm->arm9Entry, firm->arm11Entry);

    __builtin_unreachable();
}
