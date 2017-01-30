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

/*
*   Code for locating the SDMMC struct by Normmatt
*/

#pragma once

#include "types.h"

#define ROUND_TO_4MB(a) (((a) + 0x2000 - 1) & (~(0x2000 - 1)))

void locateEmuNand(u32 *emuHeader, FirmwareSource *nandType);
u32 patchEmuNand(u8 *arm9Section, u32 kernel9Size, u8 *process9Offset, u32 process9Size, u32 emuHeader, u8 *kernel9Address);