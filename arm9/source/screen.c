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

#include "screen.h"
#include "config.h"
#include "memory.h"
#include "i2c.h"
#include "utils.h"

bool needToSetupScreens = true;

struct fb fbs[2] =
{
    {
        .top_left  = (u8 *)0x18300000,
        .top_right = (u8 *)0x18300000,
        .bottom    = (u8 *)0x18346500,
    },
    {
        .top_left  = (u8 *)0x18400000,
        .top_right = (u8 *)0x18400000,
        .bottom    = (u8 *)0x18446500,
    },
};

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

    // Backlight voltage off
    I2C_writeReg(I2C_DEV_MCU, 0x22, 0x14);
    wait(50);

    // LCD panel voltage off
    I2C_writeReg(I2C_DEV_MCU, 0x22, 0x01);
    wait(50);
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

void initScreens(void)
{
    if(needToSetupScreens)
    {
        if(!ARESCREENSINITIALIZED || bootType == FIRMLAUNCH)
        {
            *(vu32 *)ARM11_PARAMETERS_ADDRESS = brightness[MULTICONFIG(BRIGHTNESS)];
            memcpy((void *)(ARM11_PARAMETERS_ADDRESS + 4), fbs, sizeof(fbs));
            invokeArm11Function(INIT_SCREENS);

            // Fragile code, needs proper fix/total rewrite of the baremetal components anyway
            // Assume controller revision is not 0x00 for either screen (this revision is extremely
            // old and shouldn't be seen in retail units nor normal devunits)

            // Controller reset off
            I2C_writeReg(I2C_DEV_LCD_TOP, 0xFE, 0xAA);
            I2C_writeReg(I2C_DEV_LCD_BOT, 0xFE, 0xAA);
            wait(5);

            // Controller power on
            I2C_writeReg(I2C_DEV_LCD_TOP, 0x01, 0x10);
            I2C_writeReg(I2C_DEV_LCD_BOT, 0x01, 0x10);
            wait(5);

            // Clear error flag
            I2C_writeReg(I2C_DEV_LCD_TOP, 0x60, 0x00);
            I2C_writeReg(I2C_DEV_LCD_BOT, 0x60, 0x00);
            wait(5);

            // LCD panel (bias ?) voltage on
            I2C_writeReg(I2C_DEV_MCU, 0x22, 0x02);
            wait(50);

            // Backlight voltage on
            I2C_writeReg(I2C_DEV_MCU, 0x22, 0x28);
            wait(5);
        }
        else updateBrightness(MULTICONFIG(BRIGHTNESS));

        memcpy((void *)ARM11_PARAMETERS_ADDRESS, fbs, sizeof(fbs));
        invokeArm11Function(SETUP_FRAMEBUFFERS);

        clearScreens(true);
        needToSetupScreens = false;
    }

    clearScreens(false);
    swapFramebuffers(false);
}

void zerofillN3dsAblRegisters(void)
{
    invokeArm11Function(ZEROFILL_N3DS_ABL_REGISTERS);
}
