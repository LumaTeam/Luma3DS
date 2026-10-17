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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <3ds.h>
#include "draw.h"
#include "ifile.h"
#include "timelock.h"
#include "menus/timelock_config.h"


timelockConfig timelockConfigData;

bool isMenuLocked = true;
char timelockMenuToggle[]     = "[_] Toggle timelock";
char timelockMenuDelay[]      = "[__0m] Delay";
char timelockMenuDailyToggle[]= "[_] Daily reset";
char timelockMenuResetHour[]  = "[__h] Reset hour";

Menu timelockMenu = {
    "Timelock menu",
    {
        { "Unlock menu",            METHOD, .method = &TimelockMenu_UnlockMenu,        .visibility = &TimelockMenu_IsMenuLocked },
        { timelockMenuToggle,       METHOD, .method = &TimelockMenu_ToggleTimelock,    .visibility = &TimelockMenu_IsMenuUnlocked },
        { timelockMenuDelay,        METHOD, .method = &TimelockMenu_Delay,             .visibility = &TimelockMenu_IsMenuUnlocked },
        { timelockMenuDailyToggle,  METHOD, .method = &TimelockMenu_ToggleDailyReset,  .visibility = &TimelockMenu_IsMenuUnlocked },
        { timelockMenuResetHour,    METHOD, .method = &TimelockMenu_ResetHour,         .visibility = &TimelockMenu_IsMenuUnlocked },
        { "Change PIN",             METHOD, .method = &TimelockMenu_PIN,               .visibility = &TimelockMenu_IsMenuUnlocked },
        { "Save settings",          METHOD, .method = &TimelockMenu_SaveSettings,      .visibility = &TimelockMenu_IsMenuUnlocked },
        { "Lock menu",              METHOD, .method = &TimelockMenu_LockMenu,          .visibility = &TimelockMenu_IsMenuUnlocked },
        {},
    }
};

bool TimelockMenu_IsMenuLocked(void)   { return  isMenuLocked; }
bool TimelockMenu_IsMenuUnlocked(void) { return !isMenuLocked; }

void TimelockMenu_LockMenu(void)
{
    isMenuLocked = true;
    menuResetSelectedItem();
}

void TimelockMenu_UnlockMenu(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u8 pinIndex = 0;
    char currentPin[PIN_LENGTH];
    memcpy(currentPin, PIN_ZERO, PIN_LENGTH);

    do {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Timelock menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Enter the PIN and press A.");
        for (int i = 0; i < PIN_LENGTH; i++) {
            Draw_DrawCharacter(10 + (i * 10), 50, COLOR_WHITE, currentPin[i]);
            Draw_DrawCharacter(10 + (i * 10), 60, COLOR_WHITE, (i == pinIndex ? '^' : ' '));
        }
        Draw_DrawString(10, 80, COLOR_WHITE, "Press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        const u32 kDown = waitInputWithTimeout(1000);

        if (kDown & KEY_DLEFT)
            pinIndex = (pinIndex == 0 ? (PIN_LENGTH - 1) : (pinIndex - 1));
        if (kDown & KEY_DRIGHT)
            pinIndex = (pinIndex == (PIN_LENGTH - 1) ? 0 : (pinIndex + 1));
        if (kDown & KEY_DUP)
            currentPin[pinIndex] = (currentPin[pinIndex] == '9' ? '0' : (currentPin[pinIndex] + 1));
        if (kDown & KEY_DDOWN)
            currentPin[pinIndex] = (currentPin[pinIndex] == '0' ? '9' : (currentPin[pinIndex] - 1));

        if (kDown & KEY_A) {
            if (TimelockMenu_CheckPIN(currentPin)) {
                isMenuLocked = false;
                break;
            }
        }
        if (kDown & KEY_B)
            break;
    } while (!menuShouldExit);

    menuResetSelectedItem();
}

bool TimelockMenu_CheckPIN(char *enteredPIN)
{
    return timelockVerifyPin(enteredPIN, timelockConfigData.pinHash, timelockConfigData.pinSalt);
}

void TimelockMenu_LoadData(void)
{
    IFile file;
    Result res;
    u64 bytesRead = 0;
    timelockConfig tmp;
    bool ok = false;

    s64 out = 0;
    if (R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    FS_ArchiveID archiveId = ((bool)out) ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;

    res = IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""),
                     fsMakePath(PATH_ASCII, CONFIG_FILE_PATH), FS_OPEN_READ);
    if (R_SUCCEEDED(res))
        res = IFile_Read(&file, &bytesRead, &tmp, sizeof(tmp));
    IFile_Close(&file);

    if (R_SUCCEEDED(res) && bytesRead == sizeof(tmp)
        && tmp.magic == TIMELOCK_MAGIC && tmp.version == TIMELOCK_VERSION) {
        timelockConfigData = tmp;
        ok = true;
    }

    if (!ok)
        timelockInitDefaults(&timelockConfigData);

    TimelockMenu_UpdateAll();
}

void TimelockMenu_UpdateAll(void)
{
    TimelockMenu_UpdateStatus(TL_MENU_TOGGLE);
    TimelockMenu_UpdateStatus(TL_MENU_DELAY);
    TimelockMenu_UpdateStatus(TL_MENU_DAILY_TOGGLE);
    TimelockMenu_UpdateStatus(TL_MENU_RESET_HOUR);
}

void TimelockMenu_UpdateStatus(TIMELOCK_CONFIG_MENU menuItem)
{
    switch (menuItem) {
        case TL_MENU_TOGGLE:
            timelockMenuToggle[1] = (timelockConfigData.isEnabled ? 'x' : ' ');
            break;
        case TL_MENU_DELAY: {
            const u16 minutesHundreds = timelockConfigData.minutes / 100;
            const u16 minutesTens     = (timelockConfigData.minutes - (minutesHundreds * 100)) / 10;
            timelockMenuDelay[1] = '0' + minutesHundreds;
            timelockMenuDelay[2] = '0' + minutesTens;
            break;
        }
        case TL_MENU_DAILY_TOGGLE:
            timelockMenuDailyToggle[1] = (timelockConfigData.dailyResetEnabled ? 'x' : ' ');
            break;
        case TL_MENU_RESET_HOUR: {
            const u8 h = timelockConfigData.resetHourLocal;
            timelockMenuResetHour[1] = '0' + (h / 10);
            timelockMenuResetHour[2] = '0' + (h % 10);
            break;
        }
    }
}

void TimelockMenu_ToggleTimelock(void)
{
    timelockConfigData.isEnabled = !timelockConfigData.isEnabled;
    TimelockMenu_UpdateStatus(TL_MENU_TOGGLE);
}

void TimelockMenu_ToggleDailyReset(void)
{
    timelockConfigData.dailyResetEnabled = !timelockConfigData.dailyResetEnabled;
    // Force re-arming on next runtime tick.
    timelockConfigData.nextResetEligibleAtMs = 0;
    TimelockMenu_UpdateStatus(TL_MENU_DAILY_TOGGLE);
}

void TimelockMenu_Delay(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    char currentMinutesString[12] = "000 minutes";

    u16 currentMinutes = timelockConfigData.minutes;
    if (currentMinutes < 10) currentMinutes = 10;
    {
        const u16 h = currentMinutes / 100;
        const u16 t = (currentMinutes - (h * 100)) / 10;
        currentMinutesString[0] = '0' + h;
        currentMinutesString[1] = '0' + t;
    }

    do {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Timelock delay menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to save the delay.");
        Draw_DrawString(10, 50, COLOR_WHITE, currentMinutesString);
        Draw_DrawString(10, 70, COLOR_WHITE, "Press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        const u32 kDown = waitInputWithTimeout(1000);

        if (kDown & KEY_DUP) {
            currentMinutes = (currentMinutes == 990 ? 10 : (currentMinutes + 10));
        }
        if (kDown & KEY_DDOWN) {
            currentMinutes = (currentMinutes == 10 ? 990 : (currentMinutes - 10));
        }
        if (kDown & (KEY_DUP | KEY_DDOWN)) {
            const u16 h = currentMinutes / 100;
            const u16 t = (currentMinutes - (h * 100)) / 10;
            currentMinutesString[0] = '0' + h;
            currentMinutesString[1] = '0' + t;
        }

        if (kDown & KEY_A) {
            timelockConfigData.minutes = currentMinutes;
            TimelockMenu_UpdateStatus(TL_MENU_DELAY);
            break;
        }
        if (kDown & KEY_B)
            break;
    } while (!menuShouldExit);
}

void TimelockMenu_ResetHour(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u8 hour = timelockConfigData.resetHourLocal;
    if (hour > 23) hour = 0;
    char hourStr[16] = "00:00 (RTC)";

    do {
        hourStr[0] = '0' + (hour / 10);
        hourStr[1] = '0' + (hour % 10);

        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Daily reset hour");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to save.");
        Draw_DrawString(10, 50, COLOR_WHITE, hourStr);
        Draw_DrawString(10, 70, COLOR_WHITE, "Press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        const u32 kDown = waitInputWithTimeout(1000);

        if (kDown & KEY_DUP)
            hour = (hour == 23) ? 0 : (hour + 1);
        if (kDown & KEY_DDOWN)
            hour = (hour == 0) ? 23 : (hour - 1);

        if (kDown & KEY_A) {
            timelockConfigData.resetHourLocal = hour;
            timelockConfigData.nextResetEligibleAtMs = 0;  // re-arm on next tick
            TimelockMenu_UpdateStatus(TL_MENU_RESET_HOUR);
            break;
        }
        if (kDown & KEY_B)
            break;
    } while (!menuShouldExit);
}

void TimelockMenu_PIN(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u8 pinIndex = 0;
    char currentPin[PIN_LENGTH];
    memcpy(currentPin, PIN_ZERO, PIN_LENGTH);   // start fresh (no plaintext to display)

    do {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Timelock PIN menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to save the new PIN.");
        for (int i = 0; i < PIN_LENGTH; i++) {
            Draw_DrawCharacter(10 + (i * 10), 50, COLOR_WHITE, currentPin[i]);
            Draw_DrawCharacter(10 + (i * 10), 60, COLOR_WHITE, (i == pinIndex ? '^' : ' '));
        }
        Draw_DrawString(10, 80, COLOR_WHITE, "Press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        const u32 kDown = waitInputWithTimeout(1000);

        if (kDown & KEY_DLEFT)
            pinIndex = (pinIndex == 0 ? (PIN_LENGTH - 1) : (pinIndex - 1));
        if (kDown & KEY_DRIGHT)
            pinIndex = (pinIndex == (PIN_LENGTH - 1) ? 0 : (pinIndex + 1));
        if (kDown & KEY_DUP)
            currentPin[pinIndex] = (currentPin[pinIndex] == '9' ? '0' : (currentPin[pinIndex] + 1));
        if (kDown & KEY_DDOWN)
            currentPin[pinIndex] = (currentPin[pinIndex] == '0' ? '9' : (currentPin[pinIndex] - 1));

        if (kDown & KEY_A) {
            // Hash against the existing salt (salt persists across PIN changes).
            timelockHashPin(currentPin, timelockConfigData.pinSalt, timelockConfigData.pinHash);
            break;
        }
        if (kDown & KEY_B)
            break;
    } while (!menuShouldExit);
}

void TimelockMenu_SaveSettings(void)
{
    Result res;
    IFile file;
    u64 written = 0;
    s64 out = 0;

    timelockConfigData.elapsedMinutes = 0;  // user-initiated save resets elapsed

    if (R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    FS_ArchiveID archiveId = ((bool)out) ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;

    res = IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""),
                     fsMakePath(PATH_ASCII, CONFIG_FILE_PATH), FS_OPEN_CREATE | FS_OPEN_WRITE);
    if (R_SUCCEEDED(res))
        res = IFile_SetSize(&file, sizeof(timelockConfigData));
    if (R_SUCCEEDED(res))
        res = IFile_Write(&file, &written, &timelockConfigData, sizeof(timelockConfigData), 0);
    IFile_Close(&file);

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Timelock options menu");
        if (R_SUCCEEDED(res)) {
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
            Draw_DrawString(10, 50, COLOR_RED,   "The console needs to be rebooted");
            Draw_DrawString(10, 60, COLOR_RED,   "in order to apply the changes.");
        } else {
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08lx).", res);
        }
        Draw_DrawString(10, 80, COLOR_WHITE, "Press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();
    } while (!(waitInput() & KEY_B) && !menuShouldExit);
}
