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
#include "luminance.h"
#include "utils.h"

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
    // Unlike SetLuminanceLevel, SetLuminance doesn't
    // check if preset <= 5, and actually allows the lumiance
    // levels provisioned for the "brightness boost mode" (brighter
    // when adapter is plugged in), even when the feature is disabled
    // (it is disabled for anything but the OG model, iirc)
    readCalibration();
    return s_blPwmData.luminanceLevels[6];
}

u32 getCurrentLuminance(bool top)
{
    u32 regbase = top ? 0x10202200 : 0x10202A00;

    readCalibration();

    bool is3d = (REG32(0x10202000 + 0x000) & 1) != 0;
    const float *coeffs = s_blPwmData.coeffs[top ? (is3d ? 2 : 1) : 0];
    u32 brightness = REG32(regbase + 0x40);
    float ratio = getPwmRatio(s_blPwmData.brightnessMax, REG32(regbase + 0x44));

    return brightnessToLuminance(brightness, coeffs, ratio);
}
