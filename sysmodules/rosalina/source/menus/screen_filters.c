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

#define TEMP_DEFAULT NEUTRAL_TEMP

int screenFiltersCurrentTemperature = TEMP_DEFAULT;

void writeLut(u32* lut)
{
    GPU_FB_TOP_COL_LUT_INDEX = 0;
    GPU_FB_BOTTOM_COL_LUT_INDEX = 0;

    for (int i = 0; i <= 255; i++) {
        GPU_FB_TOP_COL_LUT_ELEM = *lut;
        GPU_FB_BOTTOM_COL_LUT_ELEM = *lut;
        lut++;
    }
}

typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 z;
} Pixel;

static u16 g_c[0x600];
static Pixel g_px[0x400];

void applyColorSettings(color_setting_t* cs)
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

    writeLut((u32*)g_px);
}

Menu screenFiltersMenu = {
    "Screen filters menu",
    {
        { "Disable", METHOD, .method = &screenFiltersSetDisabled },
        { "Reduce blue light (level 1)", METHOD, .method = &screenFiltersReduceBlueLevel1 },
        { "Reduce blue light (level 2)", METHOD, .method = &screenFiltersReduceBlueLevel2 },
        { "Reduce blue light (level 3)", METHOD, .method = &screenFiltersReduceBlueLevel3 },
        { "Reduce blue light (level 4)", METHOD, .method = &screenFiltersReduceBlueLevel4 },
        { "Reduce blue light (level 5)", METHOD, .method = &screenFiltersReduceBlueLevel5 },
        {},
    }
};

void screenFiltersSetDisabled(void)
{
    screenFiltersCurrentTemperature = TEMP_DEFAULT;
    screenFiltersSetTemperature(screenFiltersCurrentTemperature);
}

void screenFiltersReduceBlueLevel1(void)
{
    screenFiltersCurrentTemperature = 4300;
    screenFiltersSetTemperature(screenFiltersCurrentTemperature);
}

void screenFiltersReduceBlueLevel2(void)
{
    screenFiltersCurrentTemperature = 3200;
    screenFiltersSetTemperature(screenFiltersCurrentTemperature);
}

void screenFiltersReduceBlueLevel3(void)
{
    screenFiltersCurrentTemperature = 2100;
    screenFiltersSetTemperature(screenFiltersCurrentTemperature);
}

void screenFiltersReduceBlueLevel4(void)
{
    screenFiltersCurrentTemperature = 1550;
    screenFiltersSetTemperature(screenFiltersCurrentTemperature);
}

void screenFiltersReduceBlueLevel5(void)
{
    screenFiltersCurrentTemperature = 1000;
    screenFiltersSetTemperature(screenFiltersCurrentTemperature);
}

void screenFiltersSetTemperature(int temperature)
{
    color_setting_t cs;
    memset(&cs, 0, sizeof(cs));

    cs.temperature = temperature;
    /*cs.gamma[0] = 1.0F;
    cs.gamma[1] = 1.0F;
    cs.gamma[2] = 1.0F;
    cs.brightness = 1.0F;*/

    applyColorSettings(&cs);
}
