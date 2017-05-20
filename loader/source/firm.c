/*
*   This file is part of Luma3DS
*   Copyright (C) 2017 Aurora Wright, TuxSH
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
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

#include "firm.h"
#include "memory.h"
#include "cache.h"

void launchFirm(Firm *firm, int argc, char **argv)
{
    //Copy FIRM sections to respective memory locations
    for(u32 sectionNum = 0; sectionNum < 4 && firm->section[sectionNum].size != 0; sectionNum++)
        memcpy(firm->section[sectionNum].address, (u8 *)firm + firm->section[sectionNum].offset, firm->section[sectionNum].size);

    //Set ARM11 entrypoint
    *(vu32 *)0x1FFFFFFC = (u32)firm->arm11Entry;

    //Ensure that all memory transfers have completed and that the caches have been flushed
    flushCaches();

    //Jump to ARM9 entrypoint. Also give it additional arguments it can dismiss
    ((void (*)(int, char**, u32))firm->arm9Entry)(argc, argv, 0x0000BEEF);

    __builtin_unreachable();
}
