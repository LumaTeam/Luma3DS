/*
*   This file is part of Luma3DS
*   Copyright (C) 2017-2018 Sono (https://github.com/MarcuzD), panicbit
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
#include "memory.h"
#include "menu.h"
#include "menus/screen_filters.h"
#include "menus/screen_filters_srgb_tables.h"
#include "draw.h"
#include "redshift/colorramp.h"

typedef union {
    struct {
        u8 r;
        u8 g;
        u8 b;
        u8 z;
    };
    u32 raw;
} Pixel;

ScreenFilter topScreenFilter;
ScreenFilter bottomScreenFilter;

static inline bool ScreenFiltersMenu_IsDefaultSettingsFilter(const ScreenFilter *filter)
{
    bool ok = true;
    ok = ok && filter->cct == 6500;
    ok = ok && !filter->invert;
    ok = ok && filter->gamma == 1.0f;
    ok = ok && filter->contrast == 1.0f;
    ok = ok && filter->brightness == 0.0f;
    ok = ok && filter->colorCurveCorrection == 0;
    return ok;
}


static inline bool ScreenFiltersMenu_IsDefaultSettings(void)
{
    return ScreenFiltersMenu_IsDefaultSettingsFilter(&topScreenFilter) && ScreenFiltersMenu_IsDefaultSettingsFilter(&bottomScreenFilter);
}

static inline u8 ScreenFiltersMenu_GetColorLevel(int inLevel, float wp, float a, float b, float g)
{
    float level = inLevel / 255.0f;
    level = a * wp * level + b;
    level = powf(CLAMP(level, 0.0f, 1.0f), g);
    s32 levelInt = (s32)(255.0f * level + 0.5f); // round to nearest integer
    return levelInt <= 0 ? 0 : levelInt >= 255 ? 255 : (u8)levelInt; // clamp
}

static u8 ScreenFilterMenu_CalculatePolynomialColorLutComponent(const float coeffs[][3], u32 component, float gamma, u32 dim, int inLevel)
{
    float x = inLevel / 255.0f;
    float level = 0.0f;
    float xN = 1.0f;

    // Compute a_n * x^n + ... a_0, then clamp, then exponentiate by "gamma" and clamp again
    for (u32 i = 0; i < dim + 1; i++)
    {
        level += coeffs[i][component] * xN;
        xN *= x;
    }

    level = powf(CLAMP(level, 0.0f, 1.0f), gamma);
    s32 levelInt = (s32)(255.0f * level + 0.5f); // round up
    return (u8)CLAMP(levelInt, 0, 255); // clamp again just to be sure
}

static void ScreenFilterMenu_WritePolynomialColorLut(bool top, u8 curveCorrection, const float coeffs[][3], bool invert, float gamma, u32 dim)
{
    if (top)
        GPU_FB_TOP_COL_LUT_INDEX = 0;
    else
        GPU_FB_BOTTOM_COL_LUT_INDEX = 0;

    for (int i = 0; i <= 255; i++) {
        Pixel px;
        int inLevel = invert ? 255 - i : i;
        const u8 (*tbl)[3] = curveCorrection == 2 ? ctrToSrgbTableBottom : ctrToSrgbTableTop;

        u8 inLevelR = curveCorrection > 0 ? tbl[inLevel][0] : inLevel;
        u8 inLevelG = curveCorrection > 0 ? tbl[inLevel][1] : inLevel;
        u8 inLevelB = curveCorrection > 0 ? tbl[inLevel][2] : inLevel;

        px.r = ScreenFilterMenu_CalculatePolynomialColorLutComponent(coeffs, 0, gamma, dim, inLevelR);
        px.g = ScreenFilterMenu_CalculatePolynomialColorLutComponent(coeffs, 1, gamma, dim, inLevelG);
        px.b = ScreenFilterMenu_CalculatePolynomialColorLutComponent(coeffs, 2, gamma, dim, inLevelB);
        px.z = 0;

        if (top)
            GPU_FB_TOP_COL_LUT_ELEM = px.raw;
        else
            GPU_FB_BOTTOM_COL_LUT_ELEM = px.raw;
    }
}

static void ScreenFiltersMenu_ApplyColorSettings(bool top)
{
    const ScreenFilter *filter = top ? &topScreenFilter : &bottomScreenFilter;

    float wp[3];
    colorramp_get_white_point(wp, filter->cct);
    float a = filter->contrast;
    float b = filter->brightness;
    float g = filter->gamma;
    bool inv = filter->invert;

    float poly[][3] = {
        { b, b, b },                            // x^0
        { a * wp[0], a * wp[1], a * wp[2] },    // x^1
    };

    ScreenFilterMenu_WritePolynomialColorLut(top, filter->colorCurveCorrection, poly, inv, g, 1);
}

static void ScreenFiltersMenu_SetCct(u16 cct)
{
    topScreenFilter.cct = cct;
    bottomScreenFilter.cct = cct;
    ScreenFiltersMenu_ApplyColorSettings(true);
    ScreenFiltersMenu_ApplyColorSettings(false);
}

static void ScreenFiltersMenu_UpdateEntries(void)
{
    if (topScreenFilter.colorCurveCorrection == 0)
    {
        screenFiltersMenu.items[10].title = "[IPS recommended] Enhance top screen colors";
        screenFiltersMenu.items[10].method = &ScreenFiltersMenu_SetTopScreenSrgbColorCurve;
    }
    else
    {
        screenFiltersMenu.items[10].title = "Restore top screen color curve";
        screenFiltersMenu.items[10].method = &ScreenFiltersMenu_RestoreTopScreenColorCurve;
    }

    if (bottomScreenFilter.colorCurveCorrection == 0)
    {
        screenFiltersMenu.items[11].title = "[IPS recommended] Enhance bottom screen colors";
        screenFiltersMenu.items[11].method = &ScreenFiltersMenu_SetBottomScreenSrgbColorCurve;
    }
    else
    {
        screenFiltersMenu.items[11].title = "Restore bottom screen color curve";
        screenFiltersMenu.items[11].method = &ScreenFiltersMenu_RestoreBottomScreenColorCurve;
    }
}
static void ScreenFiltersMenu_SetColorCurveCorrection(bool top, u8 colorCurveCorrection)
{
    if (top)
        topScreenFilter.colorCurveCorrection = colorCurveCorrection;
    else
        bottomScreenFilter.colorCurveCorrection = colorCurveCorrection;

    ScreenFiltersMenu_ApplyColorSettings(top);
    ScreenFiltersMenu_UpdateEntries();
}

Menu screenFiltersMenu = {
    "Screen filters menu",
    {
        { "[6500K] Default temperature", METHOD, .method = &ScreenFiltersMenu_SetDefault },
        { "[10000K] Aquarium", METHOD, .method = &ScreenFiltersMenu_SetAquarium },
        { "[7500K] Overcast Sky", METHOD, .method = &ScreenFiltersMenu_SetOvercastSky },
        { "[5500K] Daylight", METHOD, .method = &ScreenFiltersMenu_SetDaylight },
        { "[4200K] Fluorescent", METHOD, .method = &ScreenFiltersMenu_SetFluorescent },
        { "[3400K] Halogen", METHOD, .method = &ScreenFiltersMenu_SetHalogen },
        { "[2700K] Incandescent", METHOD, .method = &ScreenFiltersMenu_SetIncandescent },
        { "[2300K] Warm Incandescent", METHOD, .method = &ScreenFiltersMenu_SetWarmIncandescent },
        { "[1900K] Candle", METHOD, .method = &ScreenFiltersMenu_SetCandle },
        { "[1200K] Ember", METHOD, .method = &ScreenFiltersMenu_SetEmber },
        { "[IPS recommended] Enhance top screen colors", METHOD, .method = &ScreenFiltersMenu_SetTopScreenSrgbColorCurve },
        { "[IPS recommended] Enhance bottom screen colors", METHOD, .method = &ScreenFiltersMenu_SetTopScreenSrgbColorCurve },
        { "Advanced configuration...", METHOD, .method = &ScreenFiltersMenu_AdvancedConfiguration },
        {},
    }
};

#define DEF_CCT_SETTER(temp, name)\
void ScreenFiltersMenu_Set##name(void)\
{\
    ScreenFiltersMenu_SetCct(temp);\
}

#define DEF_SRGB_SETTER(top, profile, name)\
void ScreenFiltersMenu_##name(void)\
{\
    ScreenFiltersMenu_SetColorCurveCorrection(top, profile);\
}

void ScreenFiltersMenu_RestoreSettings(void)
{
    // Precondition: menu has not been entered

    // Not initialized/default: return
    if (ScreenFiltersMenu_IsDefaultSettings())
        return;

    // Wait for GSP to restore the CCT table
    svcSleepThread(20 * 1000 * 1000LL);

    // Pause GSP, then wait a bit to ensure GPU commands complete
    // We need to ensure no GPU stuff is running when changing
    // the gamma table (otherwise colors become and stay glitched).
    svcKernelSetState(0x10000, 2);
    svcSleepThread(5 * 1000 * 100LL);

    ScreenFiltersMenu_ApplyColorSettings(true);
    ScreenFiltersMenu_ApplyColorSettings(false);

    // Unpause GSP
    svcKernelSetState(0x10000, 2);
    svcSleepThread(5 * 1000 * 100LL);
}

void ScreenFiltersMenu_LoadConfig(void)
{
    s64 out = 0;

    svcGetSystemInfo(&out, 0x10000, 0x102);
    topScreenFilter.cct = (u16)out;
    if (topScreenFilter.cct < 1000 || topScreenFilter.cct > 25100)
        topScreenFilter.cct = 6500;

    svcGetSystemInfo(&out, 0x10000, 0x104);
    topScreenFilter.gamma = (float)(out / FLOAT_CONV_MULT);
    if (topScreenFilter.gamma < 0.0f || topScreenFilter.gamma > 8.0f)
        topScreenFilter.gamma = 1.0f;

    svcGetSystemInfo(&out, 0x10000, 0x105);
    topScreenFilter.contrast = (float)(out / FLOAT_CONV_MULT);
    if (topScreenFilter.contrast < 0.0f || topScreenFilter.contrast > 255.0f)
        topScreenFilter.contrast = 1.0f;

    svcGetSystemInfo(&out, 0x10000, 0x106);
    topScreenFilter.brightness = (float)(out / FLOAT_CONV_MULT);
    if (topScreenFilter.brightness < -1.0f || topScreenFilter.brightness > 1.0f)
        topScreenFilter.brightness = 0.0f;

    svcGetSystemInfo(&out, 0x10000, 0x107);
    topScreenFilter.invert = (bool)out;

    svcGetSystemInfo(&out, 0x10000, 0x10D);
    topScreenFilter.colorCurveCorrection = (u8)out;

    svcGetSystemInfo(&out, 0x10000, 0x108);
    bottomScreenFilter.cct = (u16)out;
    if (bottomScreenFilter.cct < 1000 || bottomScreenFilter.cct > 25100)
        bottomScreenFilter.cct = 6500;

    svcGetSystemInfo(&out, 0x10000, 0x109);
    bottomScreenFilter.gamma = (float)(out / FLOAT_CONV_MULT);
    if (bottomScreenFilter.gamma < 0.0f || bottomScreenFilter.gamma > 8.0f)
        bottomScreenFilter.gamma = 1.0f;

    svcGetSystemInfo(&out, 0x10000, 0x10A);
    bottomScreenFilter.contrast = (float)(out / FLOAT_CONV_MULT);
    if (bottomScreenFilter.contrast < 0.0f || bottomScreenFilter.contrast > 255.0f)
        bottomScreenFilter.contrast = 1.0f;

    svcGetSystemInfo(&out, 0x10000, 0x10B);
    bottomScreenFilter.brightness = (float)(out / FLOAT_CONV_MULT);
    if (bottomScreenFilter.brightness < -1.0f || bottomScreenFilter.brightness > 1.0f)
        bottomScreenFilter.brightness = 0.0f;

    svcGetSystemInfo(&out, 0x10000, 0x10C);
    bottomScreenFilter.invert = (bool)out;

    svcGetSystemInfo(&out, 0x10000, 0x10E);
    bottomScreenFilter.colorCurveCorrection = (u8)out;

    ScreenFiltersMenu_UpdateEntries();
}

DEF_CCT_SETTER(6500, Default)

DEF_CCT_SETTER(10000, Aquarium)
DEF_CCT_SETTER(7500, OvercastSky)
DEF_CCT_SETTER(5500, Daylight)
DEF_CCT_SETTER(4200, Fluorescent)
DEF_CCT_SETTER(3400, Halogen)
DEF_CCT_SETTER(2700, Incandescent)
DEF_CCT_SETTER(2300, WarmIncandescent)
DEF_CCT_SETTER(1900, Candle)
DEF_CCT_SETTER(1200, Ember)

void ScreenFiltersMenu_SetTopScreenSrgbColorCurve(void)
{
    ScreenFiltersMenu_SetColorCurveCorrection(true, 1);
}

void ScreenFiltersMenu_RestoreTopScreenColorCurve(void)
{
    ScreenFiltersMenu_SetColorCurveCorrection(true, 0);
}

void ScreenFiltersMenu_SetBottomScreenSrgbColorCurve(void)
{
    ScreenFiltersMenu_SetColorCurveCorrection(false, 2);
}

void ScreenFiltersMenu_RestoreBottomScreenColorCurve(void)
{
    ScreenFiltersMenu_SetColorCurveCorrection(false, 0);
}

static void ScreenFiltersMenu_ClampFilter(ScreenFilter *filter)
{
    filter->cct = CLAMP(filter->cct, 1000, 25100);
    filter->gamma = CLAMP(filter->gamma, 0.0f, 8.0f);
    filter->contrast = CLAMP(filter->contrast, 0.0f, 255.0f);
    filter->brightness = CLAMP(filter->brightness, -1.0f, 1.0f);
}

static void ScreenFiltersMenu_AdvancedConfigurationChangeValue(int pos, int mult, bool sync)
{
    ScreenFilter *filter = pos >= 5 ? &bottomScreenFilter : &topScreenFilter;
    ScreenFilter *otherFilter = pos >= 5 ? &topScreenFilter : &bottomScreenFilter;

    int otherMult = sync ? mult : 0;

    switch (pos % 5)
    {
        case 0:
            filter->cct += 100 * mult;
            otherFilter->cct += 100 * otherMult;
            break;
        case 1:
            filter->gamma += 0.01f * mult;
            otherFilter->gamma += 0.01f * otherMult;
            break;
        case 2:
            filter->contrast += 0.01f * mult;
            otherFilter->contrast += 0.01f * otherMult;
            break;
        case 3:
            filter->brightness += 0.01f * mult;
            otherFilter->brightness += 0.01f * otherMult;
            break;
        case 4:
            filter->invert = !filter->invert;
            otherFilter->invert = sync ? !otherFilter->invert : otherFilter->invert;
            break;
    }

    // Clamp
    ScreenFiltersMenu_ClampFilter(&topScreenFilter);
    ScreenFiltersMenu_ClampFilter(&bottomScreenFilter);

    // Update the LUT
    ScreenFiltersMenu_ApplyColorSettings(true);
    ScreenFiltersMenu_ApplyColorSettings(false);
}

static u32 ScreenFiltersMenu_AdvancedConfigurationHelper(const ScreenFilter *filter, int offset, int pos, u32 posY)
{
    char buf[64];

    Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == offset++ ? '>' : ' ');
    posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Temperature: %12dK    \n", filter->cct);

    floatToString(buf, filter->gamma, 2, true);
    Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == offset++ ? '>' : ' ');
    posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Gamma:       %13s    \n", buf);

    floatToString(buf, filter->contrast, 2, true);
    Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == offset++ ? '>' : ' ');
    posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Contrast:    %13s    \n", buf);

    floatToString(buf, filter->brightness, 2, true);
    Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == offset++ ? '>' : ' ');
    posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Brightness:  %13s    \n", buf);

    Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == offset++ ? '>' : ' ');
    posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Invert:      %13s    \n", filter->invert ? "true" : "false");

    return posY;
}

void ScreenFiltersMenu_AdvancedConfiguration(void)
{
    u32 posY;
    u32 input = 0;
    u32 held = 0;

    int pos = 0;
    int mult = 1;

    bool sync = true;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Screen filters menu");

        posY = 30;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Use left/right to increase/decrease the sel. value.\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Hold R to change the value faster.\n");
        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Update both screens: %s (L to toggle)   \n", sync ? "yes" : "no") + SPACING_Y;

        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Top screen:\n");
        posY = ScreenFiltersMenu_AdvancedConfigurationHelper(&topScreenFilter, 0, pos, posY) + SPACING_Y;

        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Bottom screen:\n");
        posY = ScreenFiltersMenu_AdvancedConfigurationHelper(&bottomScreenFilter, 5, pos, posY) + SPACING_Y;

        input = waitInputWithTimeoutEx(&held, -1);
        mult = (held & KEY_R) ? 10 : 1;

        if (input & KEY_L)
            sync = !sync;
        if (input & KEY_LEFT)
            ScreenFiltersMenu_AdvancedConfigurationChangeValue(pos, -mult, sync);
        if (input & KEY_RIGHT)
            ScreenFiltersMenu_AdvancedConfigurationChangeValue(pos, mult, sync);
        if (input & KEY_UP)
            pos = (10 + pos - 1) % 10;
        if (input & KEY_DOWN)
            pos = (pos + 1) % 10;

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(input & (KEY_A | KEY_B)) && !menuShouldExit);
}
