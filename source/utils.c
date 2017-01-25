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
*   waitInput function based on code by d0k3 https://github.com/d0k3/Decrypt9WIP/blob/master/source/hid.c
*/

#include "utils.h"
#include "i2c.h"
#include "buttons.h"
#include "screen.h"
#include "draw.h"
#include "cache.h"

static void startChrono(void)
{
    REG_TIMER_CNT(0) = 0; //67MHz
    for(u32 i = 1; i < 4; i++) REG_TIMER_CNT(i) = 4; //Count-up

    for(u32 i = 0; i < 4; i++) REG_TIMER_VAL(i) = 0;

    REG_TIMER_CNT(0) = 0x80; //67MHz; enabled
    for(u32 i = 1; i < 4; i++) REG_TIMER_CNT(i) = 0x84; //Count-up; enabled
}

static u64 chrono(void)
{
    u64 res = 0;
    for(u32 i = 0; i < 4; i++) res |= REG_TIMER_VAL(i) << (16 * i);

    res /= (TICKS_PER_SEC / 1000);

    return res;
}

u32 waitInput(bool isMenu)
{
    static u64 dPadDelay = 0ULL;
    u32 key,
        oldKey = HID_PAD;

    if(isMenu)
    {
        dPadDelay = dPadDelay > 0ULL ? 87ULL : 143ULL;
        startChrono();
    }

    while(true)
    {
        key = HID_PAD;

        if(!key)
        {
            if(i2cReadRegister(I2C_DEV_MCU, 0x10) == 1) mcuPowerOff();
            oldKey = 0;
            dPadDelay = 0;
            continue;
        }

        if(key == oldKey && (!isMenu || (!(key & DPAD_BUTTONS) || chrono() < dPadDelay))) continue;

        //Make sure the key is pressed
        u32 i;
        for(i = 0; i < 0x13000 && key == HID_PAD; i++);
        if(i == 0x13000) break;
    }

    return key;
}

void mcuPowerOff(void)
{
    if(!ISFIRMLAUNCH && ARESCREENSINITIALIZED) clearScreens(false);

    //Ensure that all memory transfers have completed and that the data cache has been flushed
    flushEntireDCache();

    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
    while(true);
}

void wait(u64 amount)
{
    startChrono();
    while(chrono() < amount);
}

void error(const char *message)
{
    if(!ISFIRMLAUNCH)
    {
        initScreens();

        drawString("An error has occurred:", true, 10, 10, COLOR_RED);
        u32 posY = drawString(message, true, 10, 30, COLOR_WHITE);
        drawString("Press any button to shutdown", true, 10, posY + 2 * SPACING_Y, COLOR_WHITE);

        waitInput(false);
    }

    mcuPowerOff();
}
