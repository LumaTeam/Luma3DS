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

/*
*   About cache coherency:
*
*   Flushing the data cache for all memory regions read from/written to by both processors is mandatory on the ARM9 processor.
*   Thus, we make sure there'll be a cache miss on the ARM9 next time it's read.
*   Otherwise the ARM9 won't see the changes made and things will break.
*
*   On the ARM11, in the environment we're in, the MMU isn't enabled and nothing is cached.
*/

#include "screen.h"
#include "config.h"
#include "memory.h"
#include "i2c.h"
#include "utils.h"

struct fb fbs[2];

static const u32 brightness[4] = {0x5F, 0x4C, 0x39, 0x26};

static volatile Arm11Operation *operation = (volatile Arm11Operation *)0x1FF80004;

static void invokeArm11Function(Arm11Operation op)
{
    while(*operation != ARM11_READY);
    *operation = op;
    while(*operation != ARM11_READY); 
}

void prepareArm11ForFirmlaunch(void)
{
    invokeArm11Function(PREPARE_ARM11_FOR_FIRMLAUNCH);
}

void deinitScreens(void)
{
    if(ARESCREENSINITIALIZED) invokeArm11Function(DEINIT_SCREENS);
}

void updateBrightness(u32 brightnessIndex)
{
    *(vu32 *)ARM11_PARAMETERS_ADDRESS = brightness[brightnessIndex];
    invokeArm11Function(UPDATE_BRIGHTNESS);
}

void swapFramebuffers(bool isAlternate)
{
    *(volatile bool *)ARM11_PARAMETERS_ADDRESS = isAlternate;
    invokeArm11Function(SWAP_FRAMEBUFFERS);
}

void clearScreens(bool isAlternate)
{
    struct fb *fbTemp = isAlternate ? &fbs[1] : &fbs[0];

    *(volatile struct fb *)ARM11_PARAMETERS_ADDRESS = *fbTemp;
    invokeArm11Function(CLEAR_SCREENS);
}

static void initScreensSequence(void)
{
    *(vu32 *)ARM11_PARAMETERS_ADDRESS = brightness[MULTICONFIG(BRIGHTNESS)];
    invokeArm11Function(INIT_SCREENS_SEQUENCE);
}

static void setupFramebuffers(void)
{
    fbs[0].top_left = (u8 *)0x18300000;
    fbs[1].top_left = (u8 *)0x18400000;
    fbs[0].top_right = (u8 *)0x18300000;
    fbs[1].top_right = (u8 *)0x18400000;
    fbs[0].bottom = (u8 *)0x18346500;
    fbs[1].bottom = (u8 *)0x18446500;

    memcpy((void *)ARM11_PARAMETERS_ADDRESS, fbs, sizeof(fbs));
    invokeArm11Function(SETUP_FRAMEBUFFERS);
}

void initScreens(void)
{
    static bool needToSetup = true;

    if(needToSetup)
    {
        if(!ARESCREENSINITIALIZED)
        {
            initScreensSequence();

            //Turn on backlight
            i2cWriteRegister(I2C_DEV_MCU, 0x22, 0x2A);
        }
        else updateBrightness(MULTICONFIG(BRIGHTNESS));

        setupFramebuffers();
        needToSetup = false;
    }

    clearScreens(false);
    clearScreens(true);
    swapFramebuffers(false);
}
