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
#include "memory.h"
#include "menu.h"
#include "menus/screen_filters.h"
#include "draw.h"
#include "redshift/redshift.h"
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

static u16 g_c[0x600];
static Pixel g_px[0x400];

int screenFiltersCurrentTemperature = -1;

static void ScreenFiltersMenu_WriteLut(const Pixel* lut)
{
    GPU_FB_TOP_COL_LUT_INDEX = 0;
    GPU_FB_BOTTOM_COL_LUT_INDEX = 0;

    for (int i = 0; i <= 255; i++) {
        GPU_FB_TOP_COL_LUT_ELEM = lut->raw;
        GPU_FB_BOTTOM_COL_LUT_ELEM = lut->raw;
        lut++;
    }
}

static void ScreenFiltersMenu_ApplyColorSettings(color_setting_t* cs)
{
    u8 i = 0;

    memset(g_c, 0, sizeof(g_c));
    memset(g_px, 0, sizeof(g_px));

    do {
        g_px[i].r = i;
        g_px[i].g = i;
        g_px[i].b = i;
        g_px[i].z = 0;
    } while(++i);

    do {
        *(g_c + i + 0x000) = g_px[i].r | (g_px[i].r << 8);
        *(g_c + i + 0x100) = g_px[i].g | (g_px[i].g << 8);
        *(g_c + i + 0x200) = g_px[i].b | (g_px[i].b << 8);
    } while(++i);

    colorramp_fill(g_c + 0x000, g_c + 0x100, g_c + 0x200, 0x100, cs);

    do {
        g_px[i].r = *(g_c + i + 0x000) >> 8;
        g_px[i].g = *(g_c + i + 0x100) >> 8;
        g_px[i].b = *(g_c + i + 0x200) >> 8;
    } while(++i);

    ScreenFiltersMenu_WriteLut(g_px);
}

static void ScreenFiltersMenu_SetCct(int cct)
{
    color_setting_t cs;
    memset(&cs, 0, sizeof(cs));

    cs.temperature = cct;
    /*cs.gamma[0] = 1.0F;
    cs.gamma[1] = 1.0F;
    cs.gamma[2] = 1.0F;
    cs.brightness = 1.0F;*/

    ScreenFiltersMenu_ApplyColorSettings(&cs);
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
    screenFiltersCurrentTemperature = temp;\
    ScreenFiltersMenu_SetCct(temp);\
}

void ScreenFiltersMenu_RestoreCct(void)
{
    // Not initialized: return
    if (screenFiltersCurrentTemperature == -1)
        return;

    // Wait for GSP to restore the CCT table
    while (GPU_FB_TOP_COL_LUT_ELEM != g_px[0].raw)
        svcSleepThread(10 * 1000 * 1000LL);

    svcSleepThread(10 * 1000 * 1000LL);
    ScreenFiltersMenu_WriteLut(g_px);
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
