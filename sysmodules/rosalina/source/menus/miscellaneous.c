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
#include "menus/miscellaneous.h"
#include "input_redirection.h"
#include "memory.h"
#include "draw.h"
#include "hbloader.h"
#include "fmt.h"
#include "utils.h" // for makeARMBranch
#include "minisoc.h"

Menu miscellaneousMenu = {
    "Miscellaneous options menu",
    .nbItems = 5,
    {
        { "Switch the hb. title to the current app.", METHOD, .method = &MiscellaneousMenu_SwitchBoot3dsxTargetTitle },
        { "Change the menu combo", METHOD, .method = MiscellaneousMenu_ChangeMenuCombo },
        { "Start InputRedirection", METHOD, .method = &MiscellaneousMenu_InputRedirection },
        { "Power off", METHOD, .method = &MiscellaneousMenu_PowerOff },
        { "Reboot", METHOD, .method = &MiscellaneousMenu_Reboot },
    }
};

void MiscellaneousMenu_SwitchBoot3dsxTargetTitle(void)
{
    Result res;
    u64 titleId = 0;
    char failureReason[64];

    if(HBLDR_3DSX_TID == HBLDR_DEFAULT_3DSX_TID)
    {
        u32 pidList[0x40];
        s32 processAmount;

        res = svcGetProcessList(&processAmount, pidList, 0x40);
        if(R_SUCCEEDED(res))
        {
            for(s32 i = 0; i < processAmount && (u32)(titleId >> 32) != 0x00040010 && (u32)(titleId >> 32) != 0x00040000; i++)
            {
                Handle processHandle;
                Result res = svcOpenProcess(&processHandle, pidList[i]);
                if(R_FAILED(res))
                    continue;

                svcGetProcessInfo((s64 *)&titleId, processHandle, 0x10001);
                svcCloseHandle(processHandle);
            }
        }

        if(R_SUCCEEDED(res) && ((u32)(titleId >> 32) == 0x00040010 || (u32)(titleId >> 32) == 0x00040000))
        {
            HBLDR_3DSX_TID = titleId;
            miscellaneousMenu.items[0].title = "Switch the hb. title to hblauncher_loader";
        }
        else if(R_FAILED(res))
            sprintf(failureReason, "%08x", (u32)res);
        else
        {
            res = -1;
            strcpy(failureReason, "no suitable process found");
        }
    }
    else
    {
        res = 0;
        HBLDR_3DSX_TID = HBLDR_DEFAULT_3DSX_TID;
        miscellaneousMenu.items[0].title = "Switch the hb. title to the current app.";
    }

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();
    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (%s).", failureReason);

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

static void MiscellaneousMenu_ConvertComboToString(char *out, u32 combo)
{
    static const char *keys[] = { "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y" };
    for(s32 i = 11; i >= 0; i--)
    {
        if(combo & (1 << i))
        {
            strcpy(out, keys[i]);
            out += strlen(keys[i]);
            *out++ = '+';
        }
    }

    out[-1] = 0;
}
void MiscellaneousMenu_ChangeMenuCombo(void)
{
    char comboStrOrig[64], comboStr[64];
    u32 posY;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    MiscellaneousMenu_ConvertComboToString(comboStrOrig, menuCombo);

    Draw_Lock();
    Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

    posY = Draw_DrawFormattedString(10, 30, COLOR_WHITE, "The current menu combo is:  %s", comboStrOrig);
    posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Please enter the new combo:");

    Draw_FlushFramebuffer();
    Draw_Unlock();

    menuCombo = waitCombo();
    MiscellaneousMenu_ConvertComboToString(comboStr, menuCombo);

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        posY = Draw_DrawFormattedString(10, 30, COLOR_WHITE, "The current menu combo is:  %s", comboStrOrig);
        posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Please enter the new combo: %s", comboStr) + SPACING_Y;
        
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Successfully changed the menu combo.");

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void MiscellaneousMenu_InputRedirection(void)
{
    static MyThread *inputRedirectionThread = NULL;
    bool done = false;

    Result res;
    char buf[65];
    bool wasEnabled = inputRedirectionEnabled;
    bool cantStart = false;

    if(wasEnabled)
    {
        res = InputRedirection_DoOrUndoPatches();
        inputRedirectionEnabled = false;
        res = MyThread_Join(inputRedirectionThread, 5 * 1000 * 1000 * 1000LL);
        svcCloseHandle(inputRedirectionThreadStartedEvent);

        if(res != 0)
            sprintf(buf, "Failed to stop InputRedirection (0x%08x).", (u32)res);
        else
            miscellaneousMenu.items[2].title = "Start InputRedirection";
    }
    else
    {
        Handle dummy;
        s64 dummyInfo;
        bool isN3DS = svcGetSystemInfo(&dummyInfo, 0x10001, 0) == 0;

        res = OpenProcessByName("socket", &dummy);
        cantStart = R_FAILED(res);
        svcCloseHandle(dummy);

        if(!cantStart && isN3DS)
        {
            res = OpenProcessByName("ir", &dummy);
            cantStart = R_FAILED(res);
            svcCloseHandle(dummy);
        }
    }

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        if(!wasEnabled && cantStart)
            Draw_DrawString(10, 30, COLOR_WHITE, "Can't start the debugger before the system has fi-\nnished loading.");
        else if(!wasEnabled)
        {
            Draw_DrawString(10, 30, COLOR_WHITE, "Starting InputRedirection...");
            if(!done)
            {
                res = InputRedirection_DoOrUndoPatches();
                if(R_SUCCEEDED(res))
                {
                    res = svcCreateEvent(&inputRedirectionThreadStartedEvent, RESET_STICKY);
                    if(R_SUCCEEDED(res))
                    {
                        inputRedirectionThread = inputRedirectionCreateThread();
                        res = svcWaitSynchronization(inputRedirectionThreadStartedEvent, 10 * 1000 * 1000 * 1000LL);
                        if(res == 0)
                            res = (Result)inputRedirectionStartResult;

                        if(res != 0)
                            InputRedirection_DoOrUndoPatches();
                    }
                }

                if(res != 0)
                    sprintf(buf, "Starting InputRedirection... failed (0x%08x).", (u32)res);
                else
                    miscellaneousMenu.items[2].title = "Stop InputRedirection";

                done = true;
            }

            if(res == 0)
                Draw_DrawString(10, 30, COLOR_WHITE, "Starting InputRedirection... OK.");
            else
                Draw_DrawString(10, 30, COLOR_WHITE, buf);
        }
        else
        {
            if(res == 0)
                Draw_DrawString(10, 30, COLOR_WHITE, "InputRedirection stopped successfully.");
            else
                Draw_DrawString(10, 30, COLOR_WHITE, buf);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void MiscellaneousMenu_Reboot()
{
    Draw_Lock();
    Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");
    Draw_DrawString(10, 30, COLOR_WHITE, "Press A to reboot, press B to go back.");
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            svcKernelSetState(7);
        }
        else if(pressed & BUTTON_B)
        {
            return;
        }
    }
    while(!terminationRequest);
}

void MiscellaneousMenu_PowerOff() // Soft shutdown.
{
    Draw_Lock();
    Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");
    Draw_DrawString(10, 30, COLOR_WHITE, "Press A to power off, press B to go back.");
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            menuLeave();
            srvPublishToSubscriber(0x203, 0);
        }
        else if(pressed & BUTTON_B)
        {
            return;
        }
    }
    while(!terminationRequest);
}