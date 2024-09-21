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
#include <math.h>
#include "fmt.h"
#include "menus/n3ds.h"
#include "memory.h"
#include "menu.h"
#include "n3ds.h"
#include "draw.h"

static char clkRateBuf[128 + 1];

static QtmCalibrationData lastQtmCal = {0};
static bool qtmCalRead = false;

Menu N3DSMenu = {
    "New 3DS menu",
    {
        { "Enable L2 cache", METHOD, .method = &N3DSMenu_EnableDisableL2Cache },
        { clkRateBuf, METHOD, .method = &N3DSMenu_ChangeClockRate },
        { "Temporarily disable Super-Stable 3D", METHOD, .method = &N3DSMenu_ToggleSs3d, .visibility = &N3DSMenu_CheckNotN2dsXl },
        { "Test parallax barrier positions", METHOD, .method = &N3DSMenu_TestBarrierPositions, .visibility = &N3DSMenu_CheckNotN2dsXl },
        { "Super-Stable 3D calibration", METHOD, .method = &N3DSMenu_Ss3dCalibration, .visibility = &N3DSMenu_CheckNotN2dsXl },
        {},
    }
};

static s64 clkRate = 0, higherClkRate = 0, L2CacheEnabled = 0;
static bool qtmUnavailableAndNotBlacklisted = false; // true on N2DSXL, though we check MCU system model data first
static QtmStatus lastUpdatedQtmStatus;

bool N3DSMenu_CheckNotN2dsXl(void)
{
    // Also check if qtm could be initialized
    return isQtmInitialized && mcuInfoTableRead && mcuInfoTable[9] < 5 && !qtmUnavailableAndNotBlacklisted;
}

void N3DSMenu_UpdateStatus(void)
{
    svcGetSystemInfo(&clkRate, 0x10001, 0);
    svcGetSystemInfo(&higherClkRate, 0x10001, 1);
    svcGetSystemInfo(&L2CacheEnabled, 0x10001, 2);

    N3DSMenu.items[0].title = L2CacheEnabled ? "Disable L2 cache" : "Enable L2 cache";
    sprintf(clkRateBuf, "Set clock rate to %luMHz", clkRate != 268 ? 268 : (u32)higherClkRate);

    if (N3DSMenu_CheckNotN2dsXl())
    {
        bool blacklisted = false;

        // Read status
        if (R_FAILED(QTMS_GetQtmStatus(&lastUpdatedQtmStatus)))
            qtmUnavailableAndNotBlacklisted = true; // stop showing QTM options if unavailable but not blacklisted

        else if (lastUpdatedQtmStatus == QTM_STATUS_UNAVAILABLE)
            qtmUnavailableAndNotBlacklisted = R_FAILED(QTMU_IsCurrentAppBlacklisted(&blacklisted)) || !blacklisted;


        MenuItem *item = &N3DSMenu.items[2];

        if (lastUpdatedQtmStatus == QTM_STATUS_ENABLED)
            item->title = "Temporarily disable Super-Stable 3D";
        else
            item->title = "Temporarily enable Super-Stable 3D";
    }
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

void N3DSMenu_ToggleSs3d(void)
{
    if (qtmUnavailableAndNotBlacklisted)
        return;

    if (lastUpdatedQtmStatus == QTM_STATUS_ENABLED)
        QTMS_SetQtmStatus(QTM_STATUS_SS3D_DISABLED);
    else // both SS3D disabled and unavailable/blacklisted states
        QTMS_SetQtmStatus(QTM_STATUS_ENABLED);

    N3DSMenu_UpdateStatus();
}

void N3DSMenu_TestBarrierPositions(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u8 pos = 0;
    QTMS_DisableAutoBarrierControl(); // assume it doesn't fail
    QTMS_GetCurrentBarrierPosition(&pos);

    u32 pressed = 0;

    do
    {
        Draw_Lock();

        Draw_DrawString(10, 10, COLOR_TITLE, "New 3DS menu");
        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Use left/right to adjust the barrier's position.\n\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Each position corresponds to 5.2mm horizontal eye\nmovement (assuming ideal viewing conditions).\n\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Once you figure out the ideal central position, you\ncan then use it in the calibration submenu.\n\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Auto-barrier adjustment behavior is restored on\nexit.\n\n");

        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Barrier position: %2hhu\n", pos);

        Draw_FlushFramebuffer();
        pressed = waitInputWithTimeout(1000);

        if (pressed & KEY_LEFT)
        {
            pos = pos > 11 ? 11 : pos; // pos is 13 when SS3D is disabled/camera is in use
            pos = (12 + pos - 1) % 12;
            QTMS_SetBarrierPosition(pos);
        }
        else if (pressed & KEY_RIGHT)
        {
            pos = pos > 11 ? 11 : pos; // pos is 13 when SS3D is disabled/camera is in use
            pos = (pos + 1) % 12;
            QTMS_SetBarrierPosition(pos);
        }

        Draw_Unlock();
    } while(!menuShouldExit && !(pressed & KEY_B));

    QTMS_EnableAutoBarrierControl();
}

void N3DSMenu_Ss3dCalibration(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u8 currentPos;
    QtmTrackingData trackingData = {0};

    if (R_FAILED(QTMS_GetCurrentBarrierPosition(&currentPos)))
        currentPos = 13;

    bool calReadFailed = false;

    if (!qtmCalRead)
    {
        cfguInit();
        calReadFailed =  R_FAILED(CFG_GetConfigInfoBlk8(sizeof(QtmCalibrationData), QTM_CAL_CFG_BLK_ID, &lastQtmCal));
        cfguExit();
        if (!calReadFailed)
            qtmCalRead = true;
    }

    bool trackingDisabled = currentPos == 13;

    if (currentPos < 12)
        QTMS_EnableAutoBarrierControl(); // assume this doesn't fail

    u32 pressed = 0;
    do
    {
        if (!trackingDisabled)
        {
            QTMS_GetCurrentBarrierPosition(&currentPos);
            QTMU_GetTrackingData(&trackingData);
        }

        Draw_Lock();

        Draw_DrawString(10, 10, COLOR_TITLE, "New 3DS menu");
        u32 posY = 30;

        if (trackingDisabled)
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "SS3D disabled or camera in use.\nPress B to exit this menu.\n");
        else if (calReadFailed)
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Failed to read calibration data.\nPress B to exit this menu.\n");
        else
        {
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Right/Left:  +- 1\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Up/Down:     +- 0.1\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "R/L:         +- 0.01\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "A:           save to system config\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Y:           reload last saved calibration\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "B:           exit this menu\n\n");

            char calStr[16];
            floatToString(calStr, lastQtmCal.centerBarrierPosition, 2, true);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Center position:          %-5s\n\n", calStr);

            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Current barrier position: %-2hhu\n", currentPos);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Current eye distance:     %-2d cm\n", (int)roundf(qtmEstimateEyeToCameraDistance(&trackingData) / 10.0f));
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Optimal eye distance:     %-2d cm\n", (int)roundf(lastQtmCal.viewingDistance / 10.0f));
        }

        Draw_FlushFramebuffer();
        pressed = waitInputWithTimeout(15);

        if (!calReadFailed)
        {
            if (pressed & KEY_LEFT)
            {
                lastQtmCal.centerBarrierPosition = fmodf(12.0f + lastQtmCal.centerBarrierPosition - 1.0f, 12.0f);
                QTMS_SetCalibrationData(&lastQtmCal, false);
            }
            else if (pressed & KEY_RIGHT)
            {
                lastQtmCal.centerBarrierPosition = fmodf(lastQtmCal.centerBarrierPosition + 1.0f, 12.0f);
                QTMS_SetCalibrationData(&lastQtmCal, false);
            }

            if (pressed & KEY_DOWN)
            {
                lastQtmCal.centerBarrierPosition = fmodf(12.0f + lastQtmCal.centerBarrierPosition - 0.1f, 12.0f);
                QTMS_SetCalibrationData(&lastQtmCal, false);
            }
            else if (pressed & KEY_UP)
            {
                lastQtmCal.centerBarrierPosition = fmodf(lastQtmCal.centerBarrierPosition + 0.1f, 12.0f);
                QTMS_SetCalibrationData(&lastQtmCal, false);
            }

            if (pressed & KEY_L)
            {
                lastQtmCal.centerBarrierPosition = fmodf(12.0f + lastQtmCal.centerBarrierPosition - 0.01f, 12.0f);
                QTMS_SetCalibrationData(&lastQtmCal, false);
            }
            else if (pressed & KEY_R)
            {
                lastQtmCal.centerBarrierPosition = fmodf(lastQtmCal.centerBarrierPosition + 0.01f, 12.0f);
                QTMS_SetCalibrationData(&lastQtmCal, false);
            }

            if (pressed & KEY_A)
                QTMS_SetCalibrationData(&lastQtmCal, true);

            if (pressed & KEY_Y)
            {
                cfguInit();
                calReadFailed =  R_FAILED(CFG_GetConfigInfoBlk8(sizeof(QtmCalibrationData), QTM_CAL_CFG_BLK_ID, &lastQtmCal));
                cfguExit();
                qtmCalRead = !calReadFailed;
                if (qtmCalRead)
                    QTMS_SetCalibrationData(&lastQtmCal, false);
            }
        }

        Draw_Unlock();
    } while(!menuShouldExit && !(pressed & KEY_B));
}
