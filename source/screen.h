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

#define ARESCREENSINITIALIZED (PDN_GPU_CNT != 1)

#define ARM11_STUB_ADDRESS 0x1FFFFD00
#define WAIT_FOR_ARM9()    *arm11Entry = 0; while(!*arm11Entry); ((void (*)())*arm11Entry)();

#define SCREEN_TOP_WIDTH     400
#define SCREEN_BOTTOM_WIDTH  320
#define SCREEN_HEIGHT        240
#define SCREEN_TOP_FBSIZE    (3 * SCREEN_TOP_WIDTH * SCREEN_HEIGHT)
#define SCREEN_BOTTOM_FBSIZE (3 * SCREEN_BOTTOM_WIDTH * SCREEN_HEIGHT)

static volatile struct fb {
     u8 *top_left;
     u8 *top_right;
     u8 *bottom;
}  __attribute__((packed)) *const fbs = (volatile struct fb *)0x23FFFE00;

extern CfgData configData;

void deinitScreens(void);
void swapFramebuffers(bool isAlternate);
void updateBrightness(u32 brightnessIndex);
void clearScreens(bool isAlternate);
void initScreens(void);
