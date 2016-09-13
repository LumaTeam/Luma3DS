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
*   Screen init code by dark_samus, bil1s, Normmatt, delebile and others
*   Screen deinit code by tiniVi
*/

#pragma once

#include "types.h"

#define PDN_GPU_CNT (*(vu8  *)0x10141200)

#define ARM11_STUB_ADDRESS (0x25000000 - 0x30) //It's currently only 0x28 bytes large. We're putting 0x30 just to be sure here
#define WAIT_FOR_ARM9()    *arm11Entry = 0; while(!*arm11Entry); ((void (*)())*arm11Entry)();

static volatile struct fb {
     u8 *top_left;
     u8 *top_right;
     u8 *bottom;
} *const fb = (volatile struct fb *)0x23FFFE00;

void deinitScreens(void);
void updateBrightness(u32 brightnessIndex);
void clearScreens(bool clearTop, bool clearBottom);
void initScreens(void);