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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <3ds.h>
#include "draw.h"
#include "ifile.h"
#include "timelock.h"


static MyThread timelockThread;
static u8 ALIGN(8) timelockThreadStack[0x1000];

timelockConfig timelockData;

bool isTimeLocked = false;
int pinIndex = 0;
char enteredPIN[PIN_LENGTH] = {0};


MyThread *timelockCreateThread(void)
{
    if (R_FAILED(MyThread_Create(&timelockThread, timelockThreadMain, timelockThreadStack, 0x1000, 52, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    
    return &timelockThread;
}

void timelockThreadMain(void)
{
    while (!isServiceUsable("hid:USER") || !isServiceUsable("fs:USER") || !isServiceUsable("APT:U"))
        svcSleepThread(1000 * 1000 * 500LL);


    hidInit();
    fsInit();
    aptInit();


    // Wait for the home menu to be loaded
    bool homeMenuLoaded;

    do
    {
        APT_GetAppletInfo(APPID_HOMEMENU, NULL, NULL, NULL, &homeMenuLoaded, NULL);
        svcSleepThread(1000 * 1000 * 1000 * 5LL);  // 5s
    } while (!homeMenuLoaded);


    timelockLoadData();


    while (!preTerminationRequested && timelockData.isEnabled)
    {
        if (isRosalinaMenuOpened)
        {
            svcSleepThread(1000 * 1000 * 1000 * 60LL);  // Check in 1min if the Rosalina menu is still opened
            continue;
        }


        if (timelockData.elapsedMinutes <= timelockData.minutes)
        {
            svcSleepThread(1000 * 1000 * 1000 * 60LL * MINUTES_TO_CHECK);

            timelockData.elapsedMinutes += MINUTES_TO_CHECK;
            saveElapsedTime();

            continue;
        }


        if (menuShouldExit)
        {
            // Check if the console is still sleeping in 10s
            svcSleepThread(1000 * 1000 * 1000 * 10LL);
            continue;
        }
        

        resetEnteredPIN();

        timelockEnter();
        timelockShow();
        timelockLeave();


        // Since we force exited the timelock screen to go to sleep mode, we should not treat it as 'unlocked'
        if (menuShouldExit)
        {
            continue;
        }


        timelockData.elapsedMinutes = 0;
        saveElapsedTime();
    }


    fsExit();
    hidExit();
}

void timelockLoadData(void)
{
    IFile file;
    Result res;
    u64 bytesRead;
    s64 out;
    bool isSdMode;

    if (R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203)))
        svcBreak(USERBREAK_ASSERT);
    
    isSdMode = (bool)out;

    FS_ArchiveID archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    res = IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, CONFIG_FILE_PATH), FS_OPEN_READ);

    if (R_SUCCEEDED(res))
        res = IFile_Read(&file, &bytesRead, &timelockData, sizeof(timelockData));
    
    IFile_Close(&file);
}

bool checkPIN(void)
{
    return (memcmp(timelockData.pin, enteredPIN, PIN_LENGTH) == 0);
}

void resetEnteredPIN(void)
{
    for (int i = 0; i < PIN_LENGTH; i++)
        enteredPIN[i] = '0';
    
    pinIndex = 0;
}

void timelockInput(void)
{
    hidScanInput();
    const u32 kDown = hidKeysDown();


    // Shift pin input
    if (kDown & KEY_DLEFT)
    {
        pinIndex = (pinIndex == 0 ? (PIN_LENGTH - 1) : (pinIndex - 1));
    }
    if (kDown & KEY_DRIGHT)
    {
        pinIndex = (pinIndex == (PIN_LENGTH - 1) ? 0 : (pinIndex + 1));
    }

    // Increase/Decrease current pin input
    if (kDown & KEY_DDOWN)
    {
        enteredPIN[pinIndex] = (enteredPIN[pinIndex] == '0' ? '9' : (enteredPIN[pinIndex] - 1));
    }
    if (kDown & KEY_DUP)
    {
        enteredPIN[pinIndex] = (enteredPIN[pinIndex] == '9' ? '0' : (enteredPIN[pinIndex] + 1));
    }

    // Check pin
    if (kDown & KEY_START)
    {
        isTimeLocked = !checkPIN();
    }
}

void timelockEnter(void)
{
    Draw_Lock();

    isTimeLocked = true;
    svcKernelSetState(0x10000, 2 | 1);
    svcSleepThread(5 * 1000 * 100LL);

    if (R_FAILED(Draw_AllocateFramebufferCache(FB_BOTTOM_SIZE)))
    {
        // Oops
        svcKernelSetState(0x10000, 2 | 1);
        svcSleepThread(5 * 1000 * 100LL);
    }
    else
        Draw_SetupFramebuffer();

    Draw_Unlock();
}

void timelockLeave(void)
{
    Draw_Lock();
    Draw_RestoreFramebuffer();
    Draw_FreeFramebufferCache();
    svcKernelSetState(0x10000, 2 | 1);
    Draw_Unlock();
}

static void timelockDraw(void)
{
    Draw_DrawString(10, 10, COLOR_TITLE, "Timelock");
    Draw_DrawString(10, 30, COLOR_WHITE, "Enter the PIN and press START.");

    for (int i = 0; i < PIN_LENGTH; i++)
    {
        Draw_DrawCharacter((i+1)*10, 70, COLOR_WHITE, enteredPIN[i]);
        Draw_DrawCharacter((i+1)*10, 90, COLOR_WHITE, (i == pinIndex ? '^' : ' '));
    }

    Draw_FlushFramebuffer();
}

void timelockShow(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    timelockDraw();
    Draw_Unlock();

    do
    {
        timelockInput();

        Draw_Lock();
        timelockDraw();
        Draw_Unlock();
    }
    while (isTimeLocked && !menuShouldExit);
}

void saveElapsedTime(void)
{
    Result res;
    IFile file;
    u64 total;
    s64 out;
    bool isSdMode;

    if (R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203)))
        svcBreak(USERBREAK_ASSERT);
    
    isSdMode = (bool)out;

    FS_ArchiveID archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    res = IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, CONFIG_FILE_PATH), FS_OPEN_WRITE);

    if (R_SUCCEEDED(res))
        res = IFile_SetSize(&file, sizeof(timelockData));
    if (R_SUCCEEDED(res))
        res = IFile_Write(&file, &total, &timelockData, sizeof(timelockData), 0);
    
    IFile_Close(&file);
}
