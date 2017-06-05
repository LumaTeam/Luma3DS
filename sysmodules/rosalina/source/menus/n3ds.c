/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
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

#include <3ds.h>
#include "fmt.h"
#include "menus/n3ds.h"
#include "memory.h"
#include "menu.h"

static char clkRateBuf[128 + 1];

Menu N3DSMenu = {
    "New 3DS menu",
    .nbItems = 2,
    {
        { "Enable L2 cache", METHOD, .method = &N3DSMenu_EnableDisableL2Cache },
        { clkRateBuf, METHOD, .method = &N3DSMenu_ChangeClockRate }
    }
};

static s64 clkRate = 0, higherClkRate = 0, L2CacheEnabled = 0;

void N3DSMenu_UpdateStatus(void)
{
    svcGetSystemInfo(&clkRate, 0x10001, 0);
    svcGetSystemInfo(&higherClkRate, 0x10001, 1);
    svcGetSystemInfo(&L2CacheEnabled, 0x10001, 2);

    N3DSMenu.items[0].title = L2CacheEnabled ? "Disable L2 cache" : "Enable L2 cache";
    sprintf(clkRateBuf, "Set clock rate to %uMHz", clkRate != 268 ? 268 : (u32)higherClkRate);
}

void N3DSMenu_ChangeClockRate(void)
{
    N3DSMenu_UpdateStatus();

    s64 newBitMask = (L2CacheEnabled << 1) | ((clkRate != 268 ? 1 : 0) ^ 1);
    svcKernelSetState(10, (u32)newBitMask);

    N3DSMenu_UpdateStatus();
}

void N3DSMenu_EnableDisableL2Cache(void)
{
    N3DSMenu_UpdateStatus();

    s64 newBitMask = ((L2CacheEnabled ^ 1) << 1) | (clkRate != 268 ? 1 : 0);
    svcKernelSetState(10, (u32)newBitMask);

    N3DSMenu_UpdateStatus();
}
