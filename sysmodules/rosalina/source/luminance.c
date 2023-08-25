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
#include <stdio.h>
#include "luminance.h"
#include "utils.h"
#include "draw.h"
#include "menu.h"

extern bool isN3DS;

typedef struct BlPwmData
{
    float coeffs[3][3];
    u8 numLevels;
    u8 unk;
    u16  luminanceLevels[7];
    u16  brightnessMax;
    u16  brightnessMin;
} BlPwmData;

// Calibration, with (dubious) default values as fallback
static BlPwmData s_blPwmData = {
    .coeffs = {
        { 0.00111639f, 1.41412f, 0.07178809f },
        { 0.000418169f, 0.66567f, 0.06098654f },
        { 0.00208543f, 1.55639f, 0.0385939f }
    },
    .numLevels = 5,
    .unk = 0,
    .luminanceLevels = { 20, 43, 73, 95, 117, 172, 172 },
    .brightnessMax = 512,
    .brightnessMin = 13,
};

static inline float getPwmRatio(u32 brightnessMax, u32 pwmCnt)
{
    u32 val = (pwmCnt & 0x10000) ? pwmCnt & 0x3FF : 511; // check pwm enabled flag
    return (float)brightnessMax / (val + 1);
}

// nn's asm has rounding errors (originally at 10^-3)
static inline u32 luminanceToBrightness(u32 luminance, const float coeffs[3], u32 minLuminance, float pwmRatio)
{
    float x = (float)luminance;
    float y = coeffs[0]*x*x + coeffs[1]*x + coeffs[2];
    y = (y <= minLuminance ? (float)minLuminance : y) / pwmRatio;

    return (u32)(y + 0.5f);
}

static inline u32 brightnessToLuminance(u32 brightness, const float coeffs[3], float pwmRatio)
{
    // Find polynomial root of ax^2 + bx + c = y

    float y = (float)brightness * pwmRatio;
    float a = coeffs[0];
    float b = coeffs[1];
    float c = coeffs[2] - y;

    float x0 = (-b + sqrtf(b*b - 4.0f*a*c)) / (a + a);

    return (u32)(x0 + 0.5f);
}

static void readCalibration(void)
{
    static bool calibRead = false;

    if (!calibRead) {
        cfguInit();
        calibRead = R_SUCCEEDED(CFG_GetConfigInfoBlk8(sizeof(BlPwmData), 0x50002, &s_blPwmData));
        cfguExit();
    }
}

u32 getMinLuminancePreset(void)
{
    readCalibration();
    return s_blPwmData.luminanceLevels[0];
}

u32 getMaxLuminancePreset(void)
{
    readCalibration();
    return s_blPwmData.luminanceLevels[s_blPwmData.numLevels - 1];
}

u32 getCurrentLuminance(bool top)
{
    u32 regbase = top ? 0x10202200 : 0x10202A00;

    readCalibration();

    const float *coeffs = s_blPwmData.coeffs[top ? (isN3DS ? 2 : 1) : 0];
    u32 brightness = REG32(regbase + 0x40);
    float ratio = getPwmRatio(s_blPwmData.brightnessMax, REG32(regbase + 0x44));

    return brightnessToLuminance(brightness, coeffs, ratio);
}

void setBrightnessAlt(u32 lumTop, u32 lumBot) 
{
    u32 regbaseTop = 0x10202200;
    u32 regbaseBot = 0x10202A00; 
    u32 offset = 0x40; // https://www.3dbrew.org/wiki/LCD_Registers
    const float *coeffsTop = s_blPwmData.coeffs[1];
    const float *coeffsBot = s_blPwmData.coeffs[0];
    float ratioTop = getPwmRatio(s_blPwmData.brightnessMax, REG32(regbaseTop + 0x44));
    float ratioBot = getPwmRatio(s_blPwmData.brightnessMax, REG32(regbaseBot + 0x44));
    u8 *screenTop = (u8 *)PA_PTR(regbaseTop +  offset);
    u8 *screenBot = (u8 *)PA_PTR(regbaseBot +  offset);

    *screenBot = luminanceToBrightness(lumBot, coeffsBot, 0, ratioBot);
    *screenTop = luminanceToBrightness(lumTop, coeffsTop, 0, ratioTop);
}

void Luminance_RecalibrateBrightnessDefaults(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u32 kHeld = 0;
    int sel = 0, minBri = 0, maxBri = 172;
    char fmtbuf[0x40];

    cfguInit();
        CFG_GetConfigInfoBlk8(sizeof(BlPwmData), 0x50002, &s_blPwmData);
    cfguExit();
    
    s_blPwmData.brightnessMin = 1;

    do
    {
        kHeld = HID_PAD;
        u32 pressed = waitInputWithTimeout(1000);

        if (pressed & DIRECTIONAL_KEYS)
        {
            if(pressed & KEY_DOWN)
            {
                if(++sel > 4) sel = 4;
            }
            else if(pressed & KEY_UP)
            {
                if(--sel < 0) sel = 0;
            }
            else if (pressed & KEY_RIGHT)
            {
                s_blPwmData.luminanceLevels[sel] += (kHeld & (KEY_L | KEY_R)) ? 10 : 1;
                s_blPwmData.luminanceLevels[sel] = s_blPwmData.luminanceLevels[sel] > maxBri ? minBri : s_blPwmData.luminanceLevels[sel];
            }
            else if (pressed & KEY_LEFT)
            {
                s_blPwmData.luminanceLevels[sel] -= (kHeld & (KEY_L | KEY_R)) ? 10 : 1;
                s_blPwmData.luminanceLevels[sel] = s_blPwmData.luminanceLevels[sel] > maxBri ? maxBri : s_blPwmData.luminanceLevels[sel];
            }
        }
        
        if (pressed & KEY_B)
            break;

        if(pressed & KEY_START)
        {
            cfguInit();
            if(R_SUCCEEDED(CFG_SetConfigInfoBlk8(sizeof(BlPwmData), 0x50002, &s_blPwmData))) 
            {
                CFG_UpdateConfigSavegame();
            }
            cfguExit();
            break;
        }

        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_TITLE, "Permanent brightness recalibration - by Nutez");
        u32 posY = 30;
        
        posY = Draw_DrawString(10, posY, COLOR_RED, "WARNING: ") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * brightness preview not possible here\n    due to glitch risk.") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * test values via 'Change screen brightness'.") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * avoid frequent use to minimise NAND(!) wear.") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * 172 is only presumed(!) safe for prolonged use.") + (SPACING_Y*2);

        sprintf(fmtbuf, "%c Level 1 value: %i", (sel == 0 ? '>' : ' '), s_blPwmData.luminanceLevels[0]);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Level 2 value: %i", (sel == 1 ? '>' : ' '), s_blPwmData.luminanceLevels[1]);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Level 3 value: %i", (sel == 2 ? '>' : ' '), s_blPwmData.luminanceLevels[2]);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Level 4 value: %i", (sel == 3 ? '>' : ' '), s_blPwmData.luminanceLevels[3]);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Level 5 value: %i", (sel == 4 ? '>' : ' '), s_blPwmData.luminanceLevels[4]);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + (SPACING_Y*2);

        posY = Draw_DrawString(10, posY, COLOR_GREEN, "Controls:") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, " UP/DOWN to choose level to edit.") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, " RIGHT/LEFT for +/-1, +hold L1 or R1 for +/-10.") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, " Press START to save all value changes.") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, " Reboot may be required to see applied changes.") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, " Press B to exit.");

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!menuShouldExit);
}
