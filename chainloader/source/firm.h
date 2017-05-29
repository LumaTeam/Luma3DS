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

#pragma once

#include "types.h"

typedef struct __attribute__((packed))
{
    u32 offset;
    u8 *address;
    u32 size;
    u32 procType;
    u8 hash[0x20];
} FirmSection;

typedef struct __attribute__((packed))
{
    char magic[4];
    u32 reserved1;
    u8 *arm11Entry;
    u8 *arm9Entry;
    u8 reserved2[0x30];
    FirmSection section[4];
} Firm;

void launchFirm(Firm *firm, int argc, char **argv);
