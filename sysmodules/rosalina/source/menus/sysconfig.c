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
#include "menus/sysconfig.h"
#include "memory.h"
#include "draw.h"
#include "fmt.h"
#include "utils.h"
#include "ifile.h"

Menu sysconfigMenu = {
    "System configuration menu",
    .nbItems = 2,
    {
        { "Toggle LEDs", METHOD, .method = &SysConfigMenu_ToggleLEDs },
        { "Toggle Wireless", METHOD, .method = &SysConfigMenu_ToggleWireless },
    }
};

void SysConfigMenu_ToggleLEDs(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "System configuration menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to toggle, press B to go back.");
        Draw_DrawString(10, 50, COLOR_RED, "WARNING:");
        Draw_DrawString(10, 60, COLOR_WHITE, "  * Entering sleep mode will reset the LED state!");
        Draw_DrawString(10, 70, COLOR_WHITE, "  * LEDs cannot be toggled when the battery is low!");

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            mcuHwcInit();
            u8 result;
            MCUHWC_ReadRegister(0x28, &result, 1);
            result = ~result;
            MCUHWC_WriteRegister(0x28, &result, 1);
            mcuHwcExit();
        }
        else if(pressed & BUTTON_B)
            return;
    }
    while(!terminationRequest);
}

void SysConfigMenu_ToggleWireless(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    bool nwmRunning = false;

    u32 pidList[0x40];
    s32 processAmount;

    svcGetProcessList(&processAmount, pidList, 0x40);

    for(s32 i = 0; i < processAmount; i++)
    {
        Handle processHandle;
        Result res = svcOpenProcess(&processHandle, pidList[i]);
        if(R_FAILED(res))
            continue;

        char processName[8] = {0};
        svcGetProcessInfo((s64 *)&processName, processHandle, 0x10000);
        svcCloseHandle(processHandle);

        if(!strncmp(processName, "nwm", 4))
        {
            nwmRunning = true;
            break;
        }
    }

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "System configuration menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to toggle, press B to go back.");

        u8 wireless = (*(vu8 *)((0x10140000 | (1u << 31)) + 0x180));

        if(nwmRunning)
        {
            Draw_DrawString(10, 50, COLOR_WHITE, "Current status:");
            Draw_DrawString(100, 50, (wireless ? COLOR_GREEN : COLOR_RED), (wireless ? " ON " : " OFF"));
        }
        else
        {
            Draw_DrawString(10, 50, COLOR_RED, "NWM isn't running.");
            Draw_DrawString(10, 60, COLOR_RED, "If you're currently on Test Menu,");
            Draw_DrawString(10, 70, COLOR_RED, "exit then press R+RIGHT to toggle the WiFi.");
            Draw_DrawString(10, 80, COLOR_RED, "Otherwise, simply exit and wait a few seconds.");
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A && nwmRunning)
        {
            nwmExtInit();
            NWMEXT_ControlWirelessEnabled(!wireless);
            nwmExtExit();
        }
        else if(pressed & BUTTON_B)
            return;
    }
    while(!terminationRequest);
}
