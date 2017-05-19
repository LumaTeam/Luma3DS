/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
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

#include "memory.h"
#include "cache.h"
#include "firm.h"

void main(int argc __attribute__((unused)), char **argv)
{
    Firm *firm = (Firm *)0x24000000;
    char absPath[24 + 255];

    u32 i;
    for(i = 0; i < 23 + 255 && argv[0][i] != 0; i++)
        absPath[i] = argv[0][i];
    for(; i < 24 + 255; i++)
        absPath[i] = 0;

    char *argvPassed[1] = {absPath};

    launchFirm(firm, 1, argvPassed);
}
