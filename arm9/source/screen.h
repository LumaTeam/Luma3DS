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

/*
*   Screen init code by dark_samus, bil1s, Normmatt, delebile and others
*   Screen deinit code by tiniVi
*/

#pragma once

#include "types.h"

#define PDN_GPU_CNT (*(vu32 *)0x10141200)

#define ARESCREENSINITIALIZED ((PDN_GPU_CNT & 0xFF) != 1)

#define ARM11_PARAMETERS_ADDRESS 0x1FFFF000

#define SCREEN_TOP_WIDTH     400
#define SCREEN_BOTTOM_WIDTH  320
#define SCREEN_HEIGHT        240
#define SCREEN_TOP_FBSIZE    (3 * SCREEN_TOP_WIDTH * SCREEN_HEIGHT)
#define SCREEN_BOTTOM_FBSIZE (3 * SCREEN_BOTTOM_WIDTH * SCREEN_HEIGHT)

struct fb {
     u8 *top_left;
     u8 *top_right;
     u8 *bottom;
};

typedef enum
{
    INIT_SCREENS = 0,
    SETUP_FRAMEBUFFERS,
    CLEAR_SCREENS,
    SWAP_FRAMEBUFFERS,
    UPDATE_BRIGHTNESS,
    DEINIT_SCREENS,
    ZEROFILL_N3DS_ABL_REGISTERS,
    PREPARE_ARM11_FOR_FIRMLAUNCH,
    ARM11_READY,
} Arm11Operation;

extern struct fb fbs[2];

extern bool needToSetupScreens;

void prepareArm11ForFirmlaunch(void);
void deinitScreens(void);
void swapFramebuffers(bool isAlternate);
void updateBrightness(u32 brightnessIndex);
void clearScreens(bool isAlternate);
void initScreens(void);
void zerofillN3dsAblRegisters(void);
