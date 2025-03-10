/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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
#include "sysfunctions.h"
#include <3ds.h>

void Sys_SetWireless(bool set)
{
    (set);
    nwmExtInit();
    NWMEXT_ControlWirelessEnabled(set);
    nwmExtExit();
}

void Sys_SetPowerButton(bool set)
{
    u32 mcuIRQMask;
    mcuHwcInit();
    MCUHWC_ReadRegister(0x18, (u8*)&mcuIRQMask, 4);
    if (set)
        mcuIRQMask = 0xC0FF1001;   
    else
        mcuIRQMask = 0xC0FF1000;
    MCUHWC_WriteRegister(0x18, (u8*)&mcuIRQMask, 4);
    mcuHwcExit();
}