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

#include <3ds.h>
#include "menus/miscellaneous.h"
#include "input_redirection.h"
#include "ntp.h"
#include "memory.h"
#include "draw.h"
#include "hbloader.h"
#include "fmt.h"
#include "utils.h" // for makeArmBranch
#include "minisoc.h"
#include "ifile.h"
#include "pmdbgext.h"

Menu miscellaneousMenu = {
    "Miscellaneous options menu",
    {
        { "Switch the hb. title to the current app.", METHOD, .method = &MiscellaneousMenu_SwitchBoot3dsxTargetTitle },
        { "Change the menu combo", METHOD, .method = &MiscellaneousMenu_ChangeMenuCombo },
        { "Start InputRedirection", METHOD, .method = &MiscellaneousMenu_InputRedirection },
        { "Sync time and date via NTP", METHOD, .method = &MiscellaneousMenu_SyncTimeDate },
        { "Save settings", METHOD, .method = &MiscellaneousMenu_SaveSettings },
        {},
    }
};

void MiscellaneousMenu_SwitchBoot3dsxTargetTitle(void)
{
    Result res;
    char failureReason[64];

    if(Luma_SharedConfig->hbldr_3dsx_tid == HBLDR_DEFAULT_3DSX_TID)
    {
        FS_ProgramInfo progInfo;
        u32 pid;
        u32 launchFlags;
        res = PMDBG_GetCurrentAppInfo(&progInfo, &pid, &launchFlags);
        if(R_SUCCEEDED(res))
        {
            Luma_SharedConfig->hbldr_3dsx_tid = progInfo.programId;
            miscellaneousMenu.items[0].title = "Switch the hb. title to hblauncher_loader";
        }
        else
        {
            res = -1;
            strcpy(failureReason, "no suitable process found");
        }
    }
    else
    {
        res = 0;
        Luma_SharedConfig->hbldr_3dsx_tid = HBLDR_DEFAULT_3DSX_TID;
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
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

static void MiscellaneousMenu_ConvertComboToString(char *out, u32 combo)
{
    static const char *keys[] = {
        "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y",
        "?", "?",
        "ZL", "ZR",
        "?", "?", "?", "?",
        "Touch",
        "?", "?", "?",
        "CStick Right", "CStick Left", "CStick Up", "CStick Down",
        "CPad Right", "CPad Left", "CPad Up", "CPad Down",
    };

    char *outOrig = out;
    out[0] = 0;
    for(s32 i = 31; i >= 0; i--)
    {
        if(combo & (1 << i))
        {
            strcpy(out, keys[i]);
            out += strlen(keys[i]);
            *out++ = '+';
        }
    }

    if (out != outOrig)
        out[-1] = 0;
}

void MiscellaneousMenu_ChangeMenuCombo(void)
{
    char comboStrOrig[128], comboStr[128];
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
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void MiscellaneousMenu_SaveSettings(void)
{
    Result res;

    IFile file;
    u64 total;

    struct PACKED ALIGN(4)
    {
        char magic[4];
        u16 formatVersionMajor, formatVersionMinor;

        u32 config, multiConfig, bootConfig;
        u64 hbldr3dsxTitleId;
        u32 rosalinaMenuCombo;
    } configData;

    u32 formatVersion;
    u32 config, multiConfig, bootConfig;
    s64 out;
    bool isSdMode;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 2))) svcBreak(USERBREAK_ASSERT);
    formatVersion = (u32)out;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 3))) svcBreak(USERBREAK_ASSERT);
    config = (u32)out;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 4))) svcBreak(USERBREAK_ASSERT);
    multiConfig = (u32)out;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 5))) svcBreak(USERBREAK_ASSERT);
    bootConfig = (u32)out;
    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    memcpy(configData.magic, "CONF", 4);
    configData.formatVersionMajor = (u16)(formatVersion >> 16);
    configData.formatVersionMinor = (u16)formatVersion;
    configData.config = config;
    configData.multiConfig = multiConfig;
    configData.bootConfig = bootConfig;
    configData.hbldr3dsxTitleId = Luma_SharedConfig->hbldr_3dsx_tid;
    configData.rosalinaMenuCombo = menuCombo;

    FS_ArchiveID archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    res = IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/luma/config.bin"), FS_OPEN_CREATE | FS_OPEN_WRITE);

    if(R_SUCCEEDED(res))
        res = IFile_Write(&file, &total, &configData, sizeof(configData), 0);

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
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08lx).", res);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void MiscellaneousMenu_InputRedirection(void)
{
    bool done = false;

    Result res;
    char buf[65];
    bool wasEnabled = inputRedirectionEnabled;
    bool cantStart = false;

    if(wasEnabled)
    {
        res = InputRedirection_Disable(5 * 1000 * 1000 * 1000LL);
        if(res != 0)
            sprintf(buf, "Failed to stop InputRedirection (0x%08lx).", (u32)res);
        else
            miscellaneousMenu.items[2].title = "Start InputRedirection";
    }
    else
    {
        s64     dummyInfo;
        bool    isN3DS = svcGetSystemInfo(&dummyInfo, 0x10001, 0) == 0;
        bool    isSocURegistered;

        res = srvIsServiceRegistered(&isSocURegistered, "soc:U");
        cantStart = R_FAILED(res) || !isSocURegistered;

        if(!cantStart && isN3DS)
        {
            bool    isIrRstRegistered;

            res = srvIsServiceRegistered(&isIrRstRegistered, "ir:rst");
            cantStart = R_FAILED(res) || !isIrRstRegistered;
        }
    }

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        if(!wasEnabled && cantStart)
            Draw_DrawString(10, 30, COLOR_WHITE, "Can't start the input redirection before the system\nhas finished loading.");
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
                        inputRedirectionCreateThread();
                        res = svcWaitSynchronization(inputRedirectionThreadStartedEvent, 10 * 1000 * 1000 * 1000LL);
                        if(res == 0)
                            res = (Result)inputRedirectionStartResult;

                        if(res != 0)
                        {
                            svcCloseHandle(inputRedirectionThreadStartedEvent);
                            InputRedirection_DoOrUndoPatches();
                            inputRedirectionEnabled = false;
                        }
                        inputRedirectionStartResult = 0;
                    }
                }

                if(res != 0)
                    sprintf(buf, "Starting InputRedirection... failed (0x%08lx).", (u32)res);
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
            {
                u32 posY = 30;
                posY = Draw_DrawString(10, posY, COLOR_WHITE, "InputRedirection stopped successfully.\n\n");
                if (isN3DS)
                {
                    posY = Draw_DrawString(
                        10,
                        posY,
                        COLOR_WHITE,
                        "This might cause a key press to be repeated in\n"
                        "Home Menu for no reason.\n\n"
                        "Just pressing ZL/ZR on the console is enough to fix\nthis.\n"
                    );
                }
            }
            else
                Draw_DrawString(10, 30, COLOR_WHITE, buf);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void MiscellaneousMenu_SyncTimeDate(void)
{
    u32 posY;
    u32 input = 0;

    Result res;
    bool cantStart = false;

    bool isSocURegistered;

    time_t t;

    res = srvIsServiceRegistered(&isSocURegistered, "soc:U");
    cantStart = R_FAILED(res) || !isSocURegistered;

    int utcOffset = 12;
	int utcOffsetMinute = 0;
    int absOffset;
    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        absOffset = utcOffset - 12;
        absOffset = absOffset < 0 ? -absOffset : absOffset;
        posY = Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Current UTC offset:  %c%02d%02d", utcOffset < 12 ? '-' : '+', absOffset, utcOffsetMinute);
        posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Use DPAD Left/Right to change hour offset.\nUse DPAD Up/Down to change minute offset.\nPress A when done.") + SPACING_Y;

        input = waitInput();

        if(input & KEY_LEFT) utcOffset = (24 + utcOffset - 1) % 24; // ensure utcOffset >= 0
        if(input & KEY_RIGHT) utcOffset = (utcOffset + 1) % 24;
        if(input & KEY_UP) utcOffsetMinute = (utcOffsetMinute + 1) % 60;
        if(input & KEY_DOWN) utcOffsetMinute = (60 + utcOffsetMinute - 1) % 60;
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(input & (KEY_A | KEY_B)) && !menuShouldExit);

    if (input & KEY_B)
        return;

    utcOffset -= 12;

    res = srvIsServiceRegistered(&isSocURegistered, "soc:U");
    cantStart = R_FAILED(res) || !isSocURegistered;
    res = 0;
    if(!cantStart)
    {
        res = ntpGetTimeStamp(&t);
        if(R_SUCCEEDED(res))
        {
            t += 3600 * utcOffset;
            t += 60 * utcOffsetMinute;
            res = ntpSetTimeDate(t);
        }
    }

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        absOffset = utcOffset;
        absOffset = absOffset < 0 ? -absOffset : absOffset;
        Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Current UTC offset:  %c%02d", utcOffset < 0 ? '-' : '+', absOffset);
        if (cantStart)
            Draw_DrawFormattedString(10, posY + 2 * SPACING_Y, COLOR_WHITE, "Can't sync time/date before the system\nhas finished loading.") + SPACING_Y;
        else if (R_FAILED(res))
            Draw_DrawFormattedString(10, posY + 2 * SPACING_Y, COLOR_WHITE, "Operation failed (%08lx).", (u32)res) + SPACING_Y;
        else
            Draw_DrawFormattedString(10, posY + 2 * SPACING_Y, COLOR_WHITE, "Timedate & RTC updated successfully.\nYou may need to reboot to see the changes.") + SPACING_Y;

        input = waitInput();

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(input & KEY_B) && !menuShouldExit);

}
