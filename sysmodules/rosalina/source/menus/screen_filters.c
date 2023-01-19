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
    level = powf(a * wp * level + b, g);
    s32 levelInt = (s32)(255.0f * level + 0.5f); // round to nearest integer
    return levelInt <= 0 ? 0 : levelInt >= 255 ? 255 : (u8)levelInt; // clamp
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

    for (int i = 0; i <= 255; i++) {
        Pixel px;
        px.r = ScreenFiltersMenu_GetColorLevel(i, wp[0], a, b, g);
        px.g = ScreenFiltersMenu_GetColorLevel(i, wp[1], a, b, g);
        px.b = ScreenFiltersMenu_GetColorLevel(i, wp[2], a, b, g);
        px.z = 0;
        GPU_FB_TOP_COL_LUT_ELEM = px.raw;
        GPU_FB_BOTTOM_COL_LUT_ELEM = px.raw;
    }
}

void ScreenFiltersMenu_SetCct(int cct)
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
        {},
    }
};

#define DEF_CCT_SETTER(temp, name)\
void ScreenFiltersMenu_Set##name(void)\
{\
    ScreenFiltersMenu_SetCct(temp);\
}

void ScreenFiltersMenu_RestoreCct(void)
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
