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
*   waitInput function based on code by d0k3 https://github.com/d0k3/Decrypt9WIP/blob/master/source/hid.c
*/

#include "utils.h"
#include "i2c.h"
#include "buttons.h"
#include "screen.h"
#include "draw.h"
#include "cache.h"
#include "fmt.h"
#include "memory.h"
#include "fs.h"
#include "mcu.h"

static void startChrono(void)
{
    static bool isChronoStarted = false;

    if(isChronoStarted) return;

    REG_TIMER_CNT(0) = 0; //67MHz
    for(u32 i = 1; i < 4; i++) REG_TIMER_CNT(i) = 4; //Count-up

    for(u32 i = 0; i < 4; i++) REG_TIMER_VAL(i) = 0;

    REG_TIMER_CNT(0) = 0x80; //67MHz; enabled
    for(u32 i = 1; i < 4; i++) REG_TIMER_CNT(i) = 0x84; //Count-up; enabled

    isChronoStarted = true;
}

static u64 chrono(void)
{
    u64 res = 0;
    for(u32 i = 0; i < 4; i++) res |= REG_TIMER_VAL(i) << (16 * i);

    res /= (TICKS_PER_SEC / 1000);

    return res;
}

void powerOff(void)
{
    if(!needToSetupScreens) clearScreens(false);

    mcuPowerBacklightsOff();

    // Ensure that all memory transfers have completed and that the data cache has been flushed
    flushEntireDCache();

    mcuPowerOff();
}

u32 waitInput(bool isMenu)
{
    static u64 dPadDelay = 0ULL;
    u64 initialValue = 0ULL;
    u32 key,
        oldKey = HID_PAD;

    if(isMenu)
    {
        dPadDelay = dPadDelay > 0ULL ? 87ULL : 143ULL;
        startChrono();
        initialValue = chrono();
    }

    while(true)
    {
        // There are two sources of truth for the shell state: the gpio reg and the MCU

        /*
         * Bit13:   Battery low (at 10%, 5% and sub-1%)
         * Bit7:    MCU WDT reset
         * Bit6:    Shell opened (GPIO1_0 1->0)
         * Bit5:    Shell closed (GPIO1_0 0->1)
         * Bit1:    Power button held
         * Bit0:    Power button pressed
         */

        // Ignore "shell opened", just ack
        u32 mcuInts = mcuGetPendingInterrupts(MCU_INT_MASK_FOR_INPUT);

        if (mcuInts & (BIT(7) | BIT(5) | BIT(1) | BIT(0)))
            powerOff();

        if (mcuInts & BIT(13))
        {
            u8 battLevelIntPart = I2C_readReg(I2C_DEV_MCU, 0xB);
            if (battLevelIntPart == 0)
                powerOff();
        }

        key = HID_PAD;

        if(!key)
        {
            oldKey = 0;
            dPadDelay = 0;
            continue;
        }

        if(key == oldKey && (!isMenu || (!(key & DPAD_BUTTONS) || chrono() - initialValue < dPadDelay))) continue;

        //Make sure the key is pressed
        u32 i;
        for(i = 0; i < 0x13000 && key == HID_PAD; i++);
        if(i == 0x13000) break;
    }

    return key;
}

void wait(u64 amount)
{
    startChrono();

    u64 initialValue = chrono();

    while(chrono() - initialValue < amount);
}

void error(const char *fmt, ...)
{
    char buf[DRAW_MAX_FORMATTED_STRING_SIZE + 1];

    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    initScreens();
    drawString(true, 10, 10, COLOR_RED, "An error has occurred:");
    u32 posY = drawString(true, 10, 30, COLOR_WHITE, buf);
    drawString(true, 10, posY + 2 * SPACING_Y, COLOR_WHITE, "Press any button to shutdown");

    waitInput(false);
    powerOff();
}
