/*
*   This file is part of Luma3DS
*   Copyright (C) 2021 Aurora Wright, TuxSH
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

#include "mcu.h"
#include "i2c.h"
#include "gpio.h"

static u32 g_pendingMcuInterrupts = 0;

u32 mcuGetInterruptMask(void)
{
    u32 mask;
    I2C_readRegBuf(I2C_DEV_MCU, 0x18, &mask, 4);
    return mask;
}

void mcuSetInterruptMask(u32 mask)
{
    I2C_writeRegBuf(I2C_DEV_MCU, 0x18, &mask, 4);
}

u32 mcuGetPendingInterrupts(u32 mask)
{
    u32 curMcuInts = 0;
    if (gpioRead(GPIO_MCU_INT))
    {
        // MCU IRQ pin raised
        I2C_readRegBuf(I2C_DEV_MCU, 0x10, &curMcuInts, 4); // this clears the interrupts on the MCU side

        // Add all new MCU interrupts to the pending list
        g_pendingMcuInterrupts |= curMcuInts;
    }

    // Remove the interrupts we'll return from the pending list
    u32 ret = g_pendingMcuInterrupts & mask;
    g_pendingMcuInterrupts &= ~mask;
    return ret;
}

void mcuPowerBacklightsOn(void)
{
    // Doesn't matter if they're already on, it should be idempotent on the MCU side

    u32 prevMask = mcuGetInterruptMask();
    u32 pend = 0;

    mcuSetInterruptMask(~MCU_INT_LCD_BL_ON);
    mcuGetPendingInterrupts(MCU_INT_LCD_BL_ON) ; // Clear any pending interrupts
    I2C_writeReg(I2C_DEV_MCU, 0x22, (u8)(MCU_INT_LCD_BL_ON >> 24));

    // Wait for LCD and backlights to be on
    do
    {
        pend |= mcuGetPendingInterrupts(MCU_INT_LCD_BL_ON);
    } while (pend != MCU_INT_LCD_BL_ON);

    mcuSetInterruptMask(prevMask);
}

void mcuPowerBacklightsOff(void)
{
    // Doesn't matter if they're already off, it should be idempotent on the MCU side

    u32 prevMask = mcuGetInterruptMask();
    u32 pend = 0;

    mcuSetInterruptMask(~MCU_INT_LCD_BL_OFF);
    mcuGetPendingInterrupts(MCU_INT_LCD_BL_OFF) ; // Clear any pending interrupts
    I2C_writeReg(I2C_DEV_MCU, 0x22, (u8)(MCU_INT_LCD_BL_OFF >> 24));

    // Wait for LCD and backlights to be off
    do
    {
        pend |= mcuGetPendingInterrupts(MCU_INT_LCD_BL_OFF);
    } while (pend != MCU_INT_LCD_BL_OFF);

    mcuSetInterruptMask(prevMask);
}

void mcuPowerOff(void)
{
    mcuFinalize();
    I2C_writeReg(I2C_DEV_MCU, 0x20, BIT(0));
    while(true);
}

void mcuReboot(void)
{
    mcuFinalize();
    I2C_writeReg(I2C_DEV_MCU, 0x20, BIT(2));
    while(true);
}

void mcuInit(void)
{
    I2C_init();
    mcuSetInterruptMask(~MCU_INT_MASK_FOR_INPUT);
}

void mcuFinalize(void)
{
    mcuGetPendingInterrupts(0xFFFFFFFF); // purge pending MCU interrupts & internal pending list
    mcuSetInterruptMask(~MCU_INT_DEFAULT_MASK); // Reset MCU mask
}
