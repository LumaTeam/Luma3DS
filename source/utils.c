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

#include "utils.h"
#include "i2c.h"
#include "buttons.h"
#include "screen.h"
#include "draw.h"
#include "cache.h"

u32 waitInput(void)
{
    u32 pressedKey = 0,
        key;

    //Wait for no keys to be pressed
    while(HID_PAD);

    do
    {
        //Wait for a key to be pressed
        while(!HID_PAD);

        key = HID_PAD;

        //Make sure it's pressed
        for(u32 i = 0x13000; i; i--)
        {
            if(key != HID_PAD) break;
            if(i == 1) pressedKey = 1;
        }
    }
    while(!pressedKey);

    return key;
}

void mcuReboot(void)
{
    if(!isFirmlaunch && PDN_GPU_CNT != 1) clearScreens();

    flushEntireDCache(); //Ensure that all memory transfers have completed and that the data cache has been flushed

    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(true);
}

void mcuPowerOff(void)
{
    if(!isFirmlaunch && PDN_GPU_CNT != 1) clearScreens();

    flushEntireDCache(); //Ensure that all memory transfers have completed and that the data cache has been flushed
    
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
    while(true);
}

//TODO: add support for TIMER IRQ
static inline void startChrono(u64 initialTicks)
{
    //Based on a NATIVE_FIRM disassembly

    REG_TIMER_CNT(0) = 0; //67MHz
    for(u32 i = 1; i < 4; i++) REG_TIMER_CNT(i) = 4; //Count-up

    for(u32 i = 0; i < 4; i++) REG_TIMER_VAL(i) = (u16)(initialTicks >> (16 * i));

    REG_TIMER_CNT(0) = 0x80; //67MHz; enabled
    for(u32 i = 1; i < 4; i++) REG_TIMER_CNT(i) = 0x84; //Count-up; enabled
}

static inline void stopChrono(void)
{
    for(u32 i = 0; i < 4; i++) REG_TIMER_CNT(i) &= ~0x80;
}

void chrono(u32 seconds)
{
    startChrono(0);

    u64 startingTicks = 0;
    for(u32 i = 0; i < 4; i++) startingTicks |= REG_TIMER_VAL(i) << (16 * i);

    u64 res;
    do
    {
        res = 0;
        for(u32 i = 0; i < 4; i++) res |= REG_TIMER_VAL(i) << (16 * i);
    }
    while(res - startingTicks < seconds * TICKS_PER_SEC);

    stopChrono();
}

void error(const char *message)
{
    initScreens();

    drawString("An error has occurred:", 10, 10, COLOR_RED);
    int posY = drawString(message, 10, 30, COLOR_WHITE);
    drawString("Press any button to shutdown", 10, posY + 2 * SPACING_Y, COLOR_WHITE);

    waitInput();
    mcuPowerOff();
}