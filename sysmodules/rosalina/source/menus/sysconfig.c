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
#include "gsp.h"

Menu sysconfigMenu = {
    "System configuration menu",
    .nbItems = 3,
    {
        { "Change Brightness", METHOD, .method = &SysConfigMenu_ChangeBrightness },
        { "Toggle LEDs", METHOD, .method = &SysConfigMenu_ToggleLEDs },
        { "Toggle Wireless", METHOD, .method = &SysConfigMenu_ToggleWireless },
    }
};

void SysConfigMenu_ChangeBrightness(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    s64 out = 0;
    svcGetSystemInfo(&out, 0x10000, 0x201);
    bool isN3DS = (bool)out;

    u8 model = 0;
    cfguInit();
    CFGU_GetSystemModel(&model);
    cfguExit();

    enum consoleModel {
        MODEL_OLD3DS = 0,
        MODEL_OLD3DSXL,
        MODEL_NEW3DS,
        MODEL_2DS,
        MODEL_NEW3DSXL,
        MODEL_NEW2DSXL
    };

    float bottomScreenMultipliers[] = {
        2.15,
        1.5,
        1.35,
        1.0,
        1.5,
        1.06
    };

    float multiplier = bottomScreenMultipliers[model];

    u32 maximumBrightness[2];
    //normal level 5 on 2ds is 0x154
    maximumBrightness[GFX_TOP] = (model == MODEL_2DS ? 0x1FF : 0xFF);
    maximumBrightness[GFX_BOTTOM] = (u32)(maximumBrightness[GFX_TOP]*multiplier);

    u32 brightnessOffset = 0x40;
    u32 topScreenPA = 0x10202200;
    u32 bottomScreenPA = 0x10202A00;

    u32 brightness[2] = {0};
    brightness[GFX_TOP] = REG32(topScreenPA+brightnessOffset);
    brightness[GFX_BOTTOM] = REG32(bottomScreenPA+brightnessOffset);

    //while unrestricted, you can change each screen separately
    bool unrestricted = false;
    gfxScreen_t selectedScreen = GFX_TOP;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "System configuration menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press LEFT/RIGHT to change by 10s,");
        Draw_DrawString(10, 40, COLOR_WHITE, "     or UP/DOWN to change by 1s.");
        Draw_DrawString(10, 50, COLOR_WHITE, "Press A to apply, press B to go back.");
        if(unrestricted)
            Draw_DrawString(10, 60, COLOR_WHITE, "Press L/R to select which screen to modify.");
        else
            Draw_DrawString(10, 60, COLOR_WHITE, "                                           ");

        Draw_DrawString(10, 80, COLOR_RED, "WARNING:");
        Draw_DrawString(10, 90, COLOR_RED, "We're not responsible for any damage caused");
        Draw_DrawString(10, 100, COLOR_RED, "to your device if you set it too high!");

        Draw_DrawFormattedString(10, 120,
            ((unrestricted && (selectedScreen == GFX_TOP)) ? COLOR_GREEN : COLOR_WHITE),
            "Top screen brightness: 0x%.3lX", brightness[GFX_TOP]);
        Draw_DrawFormattedString(10, 130,
            (selectedScreen == GFX_BOTTOM ? COLOR_GREEN : COLOR_WHITE),
            "Bottom screen brightness: 0x%.3lX", brightness[GFX_BOTTOM]);

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            if(isN3DS)
                res = patchGSP();
            return;
        }
        if(pressed & BUTTON_X)
        {
            if(isN3DS)
                res = unpatchGSP();
            return;
        }
        else if(pressed & BUTTON_Y)
        {
            if (model != MODEL_2DS)
            {
                unrestricted = !unrestricted;
                selectedScreen = GFX_TOP;
            }
        }
        else if(pressed & BUTTON_L1)
        {
            if(unrestricted)
                selectedScreen = GFX_TOP;
        }
        else if(pressed & BUTTON_R1)
        {
            if(unrestricted)
                selectedScreen = GFX_BOTTOM;
        }
        else if(pressed & BUTTON_RIGHT)
        {
            brightness[selectedScreen] += 0x10;
            if(brightness[selectedScreen] > maximumBrightness[selectedScreen])
                brightness[selectedScreen] = maximumBrightness[selectedScreen];
        }
        else if(pressed & BUTTON_LEFT)
        {
            brightness[selectedScreen] -= 0x10;
            if(brightness[selectedScreen] > maximumBrightness[selectedScreen])
                brightness[selectedScreen] = 0;
        }
        else if(pressed & BUTTON_UP)
        {
            brightness[selectedScreen] += 1;
            if(brightness[selectedScreen] > maximumBrightness[selectedScreen])
                brightness[selectedScreen] = maximumBrightness[selectedScreen];
        }
        else if(pressed & BUTTON_DOWN)
        {
            brightness[selectedScreen] -= 1;
            if(brightness[selectedScreen] > maximumBrightness[selectedScreen])
                brightness[selectedScreen] = 0;
        }
        else if(pressed & BUTTON_B)
            return;

        //show the changes real-time
        REG32(topScreenPA+brightnessOffset) = brightness[GFX_TOP];
        //in restricted mode, the bottom screen brightness changes depending on the top screen brightness
        if(!unrestricted)
            brightness[GFX_BOTTOM] = (u32)(brightness[GFX_TOP]*multiplier);
        REG32(bottomScreenPA+brightnessOffset) = brightness[GFX_BOTTOM];
    }
    while(!terminationRequest);
}

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
            mcuHwcReadRegister(0x28, &result, 1);
            result = ~result;
            mcuHwcWriteRegister(0x28, &result, 1);
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
