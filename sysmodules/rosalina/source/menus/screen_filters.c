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

int screenFiltersCurrentTemperature = 6500;
float screenFiltersCurrentGamma = 1.0f;
float screenFiltersCurrentContrast = 1.0f;
float screenFiltersCurrentBrightness = 0.0f;
bool screenFiltersCurrentInvert = false;

static inline bool ScreenFiltersMenu_IsDefaultSettings(void)
{
    bool ret = screenFiltersCurrentTemperature == 6500;
    ret = ret && screenFiltersCurrentGamma == 1.0f;
    ret = ret && screenFiltersCurrentContrast == 1.0f;
    ret = ret && screenFiltersCurrentBrightness == 0.0f;
    return ret;
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

static void ScreenFilterMenu_WritePolynomialColorLut(const float coeffs[][3], bool invert, float gamma, u32 dim)
{
    GPU_FB_TOP_COL_LUT_INDEX = 0;
    GPU_FB_BOTTOM_COL_LUT_INDEX = 0;

    for (int i = 0; i <= 255; i++) {
        Pixel px;
        int inLevel = invert ? 255 - i : i;
        px.r = ScreenFilterMenu_CalculatePolynomialColorLutComponent(coeffs, 0, gamma, dim, inLevel);
        px.g = ScreenFilterMenu_CalculatePolynomialColorLutComponent(coeffs, 1, gamma, dim, inLevel);
        px.b = ScreenFilterMenu_CalculatePolynomialColorLutComponent(coeffs, 2, gamma, dim, inLevel);
        px.z = 0;
        GPU_FB_TOP_COL_LUT_ELEM = px.raw;
        GPU_FB_BOTTOM_COL_LUT_ELEM = px.raw;
    }
}

static void ScreenFiltersMenu_ApplyColorSettings(void)
{
    GPU_FB_TOP_COL_LUT_INDEX = 0;
    GPU_FB_BOTTOM_COL_LUT_INDEX = 0;

    float wp[3];
    colorramp_get_white_point(wp, screenFiltersCurrentTemperature);
    float a = screenFiltersCurrentContrast;
    float g = screenFiltersCurrentGamma;
    float b = screenFiltersCurrentBrightness;
    bool inv = screenFiltersCurrentInvert;

    float poly[][3] = {
        { b, b, b },                            // x^0
        { a * wp[0], a * wp[1], a * wp[2] },    // x^1
    };

    ScreenFilterMenu_WritePolynomialColorLut(poly, inv, g, 1);
}

static void ScreenFiltersMenu_SetCct(int cct)
{
    screenFiltersCurrentTemperature = cct;
    ScreenFiltersMenu_ApplyColorSettings();
}

Menu screenFiltersMenu = {
    "Screen filters menu",
    {
        { "[6500K] Default", METHOD, .method = &ScreenFiltersMenu_SetDefault },
        { "[10000K] Aquarium", METHOD, .method = &ScreenFiltersMenu_SetAquarium },
        { "[7500K] Overcast Sky", METHOD, .method = &ScreenFiltersMenu_SetOvercastSky },
        { "[5500K] Daylight", METHOD, .method = &ScreenFiltersMenu_SetDaylight },
        { "[4200K] Fluorescent", METHOD, .method = &ScreenFiltersMenu_SetFluorescent },
        { "[3400K] Halogen", METHOD, .method = &ScreenFiltersMenu_SetHalogen },
        { "[2700K] Incandescent", METHOD, .method = &ScreenFiltersMenu_SetIncandescent },
        { "[2300K] Warm Incandescent", METHOD, .method = &ScreenFiltersMenu_SetWarmIncandescent },
        { "[1900K] Candle", METHOD, .method = &ScreenFiltersMenu_SetCandle },
        { "[1200K] Ember", METHOD, .method = &ScreenFiltersMenu_SetEmber },
        { "Advanced configuration", METHOD, .method = &ScreenFiltersMenu_AdvancedConfiguration },
        {},
    }
};

#define DEF_CCT_SETTER(temp, name)\
void ScreenFiltersMenu_Set##name(void)\
{\
    ScreenFiltersMenu_SetCct(temp);\
}

void ScreenFiltersMenu_RestoreSettings(void)
{
    // Not initialized/default: return
    if (ScreenFiltersMenu_IsDefaultSettings())
        return;

    // Wait for GSP to restore the CCT table
    svcSleepThread(20 * 1000 * 1000LL);
    ScreenFiltersMenu_ApplyColorSettings();
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

static void ScreenFiltersMenu_AdvancedConfigurationChangeValue(int pos, int mult)
{
    switch (pos)
    {
        case 0:
            screenFiltersCurrentTemperature += 100 * mult;
            break;
        case 1:
            screenFiltersCurrentGamma += 0.01f * mult;
            break;
        case 2:
            screenFiltersCurrentContrast += 0.01f * mult;
            break;
        case 3:
            screenFiltersCurrentBrightness += 0.01f * mult;
            break;
        case 4:
            screenFiltersCurrentInvert = !screenFiltersCurrentInvert;
            break;
    }

    // Clamp
    screenFiltersCurrentTemperature = CLAMP(screenFiltersCurrentTemperature, 1000, 25100);
    screenFiltersCurrentGamma = CLAMP(screenFiltersCurrentGamma, 0.0f, 1411.0f); // ln(255) / ln(254/255): (254/255)^1411 <= 1/255
    screenFiltersCurrentContrast = CLAMP(screenFiltersCurrentContrast, 0.0f, 255.0f);
    screenFiltersCurrentBrightness = CLAMP(screenFiltersCurrentBrightness, -1.0f, 1.0f);

    // Update the LUT
    ScreenFiltersMenu_ApplyColorSettings();
}

void ScreenFiltersMenu_AdvancedConfiguration(void)
{
    u32 posY;
    u32 input = 0;
    u32 held = 0;

    char buf[64];
    int pos = 0;
    int mult = 1;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Screen filters menu");

        posY = 30;
        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Use left/right to increase/decrease the sel. value.\n");
        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Hold R to change the value faster.\n") + SPACING_Y;

        Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == 0 ? '>' : ' ');
        posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Temperature: %12dK    \n", screenFiltersCurrentTemperature);

        floatToString(buf, screenFiltersCurrentGamma, 2, true);
        Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == 1 ? '>' : ' ');
        posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Gamma:       %13s    \n", buf);

        floatToString(buf, screenFiltersCurrentContrast, 2, true);
        Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == 2 ? '>' : ' ');
        posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Contrast:    %13s    \n", buf);

        floatToString(buf, screenFiltersCurrentBrightness, 2, true);
        Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == 3 ? '>' : ' ');
        posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Brightness:  %13s    \n", buf);

        Draw_DrawCharacter(10, posY, COLOR_TITLE, pos == 4 ? '>' : ' ');
        posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "Invert:      %13s    \n", screenFiltersCurrentInvert ? "true" : "false");

        input = waitInputWithTimeoutEx(&held, -1);
        mult = (held & KEY_R) ? 10 : 1;

        if (input & KEY_LEFT)
            ScreenFiltersMenu_AdvancedConfigurationChangeValue(pos, -mult);
        if (input & KEY_RIGHT)
            ScreenFiltersMenu_AdvancedConfigurationChangeValue(pos, mult);
        if (input & KEY_UP)
            pos = (5 + pos - 1) % 5;
        if (input & KEY_DOWN)
            pos = (pos + 1) % 5;

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(input & (KEY_A | KEY_B)) && !menuShouldExit);
}
