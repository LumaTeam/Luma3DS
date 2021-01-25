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

#pragma once

#include "types.h"

/*
 * Bit13:   Battery low (at 10%, 5% and sub-1%)
 * Bit7:    MCU WDT reset
 * Bit6:    Shell opened (GPIO1_0 1->0)
 * Bit5:    Shell closed (GPIO1_0 0->1)
 * Bit1:    Power button held
 * Bit0:    Power button pressed
 */
#define MCU_INT_MASK_FOR_INPUT  (BIT(13)|BIT(7)|BIT(6)|BIT(5)|BIT(1)|BIT(0))
#define MCU_INT_LCD_BL_ON       (BIT(29)|BIT(27)|BIT(25))
#define MCU_INT_LCD_BL_OFF      (BIT(28)|BIT(26)|BIT(24))
#define MCU_INT_DEFAULT_MASK    0x0000E7FFu

u32 mcuGetInterruptMask(void);
void mcuSetInterruptMask(u32 mask); // 1 for each interrupt you want to *disable* (mask)
u32 mcuGetPendingInterrupts(u32 mask);

void mcuPowerBacklightsOn(void);
void mcuPowerBacklightsOff(void);

void NORETURN mcuPowerOff(void);
void NORETURN mcuReboot(void);

void mcuInit(void);
void mcuFinalize(void);
