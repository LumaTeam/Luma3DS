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
#define LCD_I2C_READ_ADDR       0x40
#define LCD_I2C_REG_REVISION    0xFF
#define LCD_I2C_REG_BL_STATUS   0x62
#define LCD_I2C_BL_READY        0x01
#define LCD_READY_POLL_COUNT    10

static u8 readLcdControllerReg(I2cDevice dev, u8 reg)
{
    u8 data[2] = {0};

    if(!I2C_writeReg(dev, LCD_I2C_READ_ADDR, reg) ||
       !I2C_readRegBuf(dev, LCD_I2C_READ_ADDR, data, sizeof(data)))
        return 0xFF;

    return data[1];
}

static u16 initLcdControllers(void)
{
    const u8 topRevision = readLcdControllerReg(I2C_DEV_LCD_TOP, LCD_I2C_REG_REVISION);
    const u8 bottomRevision = readLcdControllerReg(I2C_DEV_LCD_BOT, LCD_I2C_REG_REVISION);

    // Revision 0 controllers require the legacy initialization sequence.
    if(topRevision == 0)
    {
        I2C_writeReg(I2C_DEV_LCD_TOP, 0x11, 0x10);
        I2C_writeReg(I2C_DEV_LCD_TOP, 0x50, 0x01);
    }
    else I2C_writeReg(I2C_DEV_LCD_TOP, 0xFE, 0xAA);

    if(bottomRevision == 0) I2C_writeReg(I2C_DEV_LCD_BOT, 0x11, 0x10);
    else I2C_writeReg(I2C_DEV_LCD_BOT, 0xFE, 0xAA);
    wait(5);

    I2C_writeReg(I2C_DEV_LCD_TOP, 0x60, 0x00);
    I2C_writeReg(I2C_DEV_LCD_BOT, 0x60, 0x00);
    wait(5);

    I2C_writeReg(I2C_DEV_LCD_TOP, 0x01, 0x10);
    I2C_writeReg(I2C_DEV_LCD_BOT, 0x01, 0x10);
    wait(5);

    return (u16)topRevision | (u16)bottomRevision << 8;
}

static void waitLcdControllersReady(u16 revisions)
{
    const u8 topRevision = revisions & 0xFF;
    const u8 bottomRevision = revisions >> 8;

    // Revision 0 controllers need the legacy delay. A failed revision read
    // uses the same fallback instead of repeatedly polling an unresponsive LCD.
    if(topRevision == 0 || bottomRevision == 0 ||
       topRevision == 0xFF || bottomRevision == 0xFF)
    {
        wait(150);
        return;
    }

    for(u32 i = 0; i < LCD_READY_POLL_COUNT; i++)
    {
        const u8 topStatus = readLcdControllerReg(I2C_DEV_LCD_TOP, LCD_I2C_REG_BL_STATUS);
        const u8 bottomStatus = readLcdControllerReg(I2C_DEV_LCD_BOT, LCD_I2C_REG_BL_STATUS);

        if(topStatus == LCD_I2C_BL_READY && bottomStatus == LCD_I2C_BL_READY) return;
        wait(33);
    }
}

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

            const u16 lcdRevisions = initLcdControllers();

            // LCD panel (bias ?) voltage on
            I2C_writeReg(I2C_DEV_MCU, 0x22, 0x02);
            wait(50);

            // Wait until both LCD controllers report that their backlights are
            // ready before enabling backlight voltage. This mirrors the GSP/GM9
            // sequencing and avoids racing slow panels after a splash.
            waitLcdControllersReady(lcdRevisions);

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
