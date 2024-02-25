#include <stdint.h>
#include <math.h>
#include <3ds.h>
#include "menus.h"
#include "redshift/redshift.h"
#include "redshift/colorramp.h"
#include "menu.h"
#include "menus/screen_filters.h"
#include "draw.h"
#include "ifile.h"
#include "memory.h"
#include "fmt.h"

/*
  CTR_Redshift - redshift for Nintendo 3DS
  Copyright (C) 2017-2018 Sono
   
   This program is free software: you can redistribute it and/or modify  
   it under the terms of the GNU General Public License as   
   published by the Free Software Foundation, either version 3, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful, but 
   WITHOUT ANY WARRANTY; without even the implied warranty of 
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
   General Lesser Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/*
  Redshift - https://github.com/jonls/redshift
  
   Redshift is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Redshift is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Redshift.  If not, see <http://www.gnu.org/licenses/>.

   Copyright (c) 2009-2017  Jon Lund Steffensen <jonlst@gmail.com>
*/

#define COLORREG_IDX_T REG32(0x10400480)
#define COLORREG_DAT_T REG32(0x10400484)
#define COLORREG_IDX_B REG32(0x10400580)
#define COLORREG_DAT_B REG32(0x10400584)

bool redshiftFilterSelected = false;
bool nightLightSettingsRead = false;
bool nightLightOverride = false;
bool lightModeActive = false;
bool nightModeActive = false;

static char nightLightMainMenuBuf[128 + 1];
static char nightLightSubMenuBuf[128 + 1];

night_light_settings s_nightLight = {
    .light_brightnessLevel = 3,
    .light_ledSuppression = false,
    .light_startHour = 8,
    .light_startMinute = 10,
    .night_brightnessLevel = 1,
    .night_ledSuppression = true,
    .night_startHour = 19,
    .night_startMinute = 30,
    .use_nightMode = true,
    .light_changeBrightness = true,
    .night_changeBrightness = true,
};

void Redshift_SuppressLeds(void)
{
    mcuHwcInit();
    u8 off = 0;
    MCUHWC_WriteRegister(0x28, &off, 1);
    mcuHwcExit();
}

void Redshift_ApplyNightLightSettings(void)
{
    if(!nightLightOverride)
    {
        u64 timeInSeconds = osGetTime() / 1000;
        u64 dayTime = timeInSeconds % 86400;
        u8 hour = dayTime / 3600;
        u8 minute = (dayTime % 3600) / 60;

        // if night mode is enabled and time is between light and night start times
        if(s_nightLight.use_nightMode 
            && ((hour > s_nightLight.night_startHour || (hour == s_nightLight.night_startHour && minute >= s_nightLight.night_startMinute)) 
            || (hour < s_nightLight.light_startHour || (hour == s_nightLight.light_startHour && minute < s_nightLight.light_startMinute))))
        {
            ApplyNightMode();
        }
        else
        {
            ApplyLightMode();
        }
    }
    else
    {
        if (lightModeActive)
        {
            ApplyLightMode();
        }
        else if(nightModeActive)
        {
            ApplyNightMode();
        }
    }
}

bool Redshift_ReadNightLightSettings(void)
{
    IFile file;
    Result res = 0;
    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
            fsMakePath(PATH_ASCII, "/luma/configBootshift.bin"), FS_OPEN_READ);
        
    if(R_SUCCEEDED(res))
    {
        u64 total;
        if(R_SUCCEEDED(IFile_Read(&file, &total, &s_nightLight, sizeof(s_nightLight))))
        {
            IFile_Close(&file);
            return true;
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }
}

static void WriteNightLightSettings(void)
{
    IFile file;
    Result res = 0;
    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
            fsMakePath(PATH_ASCII, "/luma/configBootshift.bin"), FS_OPEN_CREATE | FS_OPEN_WRITE);
        
    if(R_SUCCEEDED(res))
    {
        u64 total;
        IFile_Write(&file, &total, &s_nightLight, sizeof(s_nightLight), 0);
        IFile_Close(&file);
    }
}

void ApplyLightMode(void)
{
    if(s_nightLight.light_ledSuppression) Redshift_SuppressLeds();

    if(s_nightLight.light_changeBrightness)
    {
        backlight_controls s_backlight = {
            .abl_enabled = 0,
            .brightness_level = 0
        };

        cfguInit();
        CFG_GetConfigInfoBlk8(sizeof(backlight_controls), 0x50001, &s_backlight);            
        if(s_backlight.brightness_level != s_nightLight.light_brightnessLevel)
        {
            s_backlight.brightness_level = s_nightLight.light_brightnessLevel;
            CFG_SetConfigInfoBlk8(sizeof(backlight_controls), 0x50001, &s_backlight);
        }
        cfguExit();
    }

    lightModeActive = true;
    nightModeActive = false;
}

void ApplyNightMode(void)
{
    if(s_nightLight.night_ledSuppression) Redshift_SuppressLeds();

    if(s_nightLight.light_changeBrightness)
    {
        backlight_controls s_backlight = {
            .abl_enabled = 0,
            .brightness_level = 0
        };

        cfguInit();
        CFG_GetConfigInfoBlk8(sizeof(backlight_controls), 0x50001, &s_backlight);        
        if(s_backlight.brightness_level != s_nightLight.night_brightnessLevel)
        {
            s_backlight.brightness_level = s_nightLight.night_brightnessLevel;
            CFG_SetConfigInfoBlk8(sizeof(backlight_controls), 0x50001, &s_backlight);
        }
        cfguExit();
    }

    lightModeActive = false;
    nightModeActive = true;
}

void Redshift_UpdateNightLightStatuses(void)
{
    char statusBuf[64];
    sprintf(statusBuf, ": %s%s", 
    lightModeActive ? "[Light" : "[Night", 
    (!s_nightLight.use_nightMode || nightLightOverride) ? " Single Mode]" : " Switch Mode]");

    sprintf(nightLightMainMenuBuf, "Screen filters%s", (nightLightSettingsRead && !redshiftFilterSelected) ? statusBuf : "...");
    sprintf(nightLightSubMenuBuf, "Night/Light Config%s", (nightLightSettingsRead && !redshiftFilterSelected) ? statusBuf : "");

    rosalinaMenu.items[7].title = nightLightMainMenuBuf;
    screenFiltersMenu.items[11].title = nightLightSubMenuBuf;
}

void Redshift_ConfigureNightLightSettings(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    int sel = 0;
    u8 minBri = 1, maxBri = 5, hourMax = 23, minuteMax = 50;
    char fmtbuf[0x40];

    do
    {
        u32 pressed = waitInputWithTimeout(1000);

        if (pressed & DIRECTIONAL_KEYS)
        {
            if(pressed & KEY_DOWN)
            {
                if(++sel > 10) sel = 0;
            }
            else if(pressed & KEY_UP)
            {
                if(--sel < 0) sel = 10;
            }
            else if (pressed & KEY_RIGHT)
            {
                switch(sel){
                    case 0:
                        s_nightLight.light_brightnessLevel += 1;
                        s_nightLight.light_brightnessLevel = s_nightLight.light_brightnessLevel > maxBri ? minBri : s_nightLight.light_brightnessLevel;
                    break;
                    case 1:
                        s_nightLight.light_changeBrightness = !s_nightLight.light_changeBrightness;
                    break;
                    case 2:
                        s_nightLight.light_ledSuppression = !s_nightLight.light_ledSuppression;
                    break;
                    case 3:
                        s_nightLight.light_startHour += 1;
                        s_nightLight.light_startHour = s_nightLight.light_startHour > hourMax ? 0 : s_nightLight.light_startHour;
                    break;
                    case 4:
                        s_nightLight.light_startMinute += 10;
                        s_nightLight.light_startMinute = s_nightLight.light_startMinute > minuteMax ? 0 : s_nightLight.light_startMinute;
                    break;
                    case 5:
                        s_nightLight.use_nightMode = !s_nightLight.use_nightMode;
                    break;
                    case 6:
                        s_nightLight.night_brightnessLevel += 1;
                        s_nightLight.night_brightnessLevel = s_nightLight.night_brightnessLevel > maxBri ? minBri : s_nightLight.night_brightnessLevel;
                    break;
                    case 7:
                        s_nightLight.night_changeBrightness = !s_nightLight.night_changeBrightness;
                    break;
                    case 8:
                        s_nightLight.night_ledSuppression = !s_nightLight.night_ledSuppression;
                    break;
                    case 9:
                        s_nightLight.night_startHour += 1;
                        s_nightLight.night_startHour = s_nightLight.night_startHour > hourMax ? 0 : s_nightLight.night_startHour;
                    break;
                    case 10:
                        s_nightLight.night_startMinute += 10;
                        s_nightLight.night_startMinute = s_nightLight.night_startMinute > minuteMax ? 0 : s_nightLight.night_startMinute;
                    break;
                }
            }
            else if (pressed & KEY_LEFT)
            {
                switch(sel){
                    case 0:
                        s_nightLight.light_brightnessLevel -= 1;
                        s_nightLight.light_brightnessLevel = s_nightLight.light_brightnessLevel > maxBri ? maxBri : s_nightLight.light_brightnessLevel;
                    break;
                    case 1:
                        s_nightLight.light_changeBrightness = !s_nightLight.light_changeBrightness;
                    break;
                    case 2:
                        s_nightLight.light_ledSuppression = !s_nightLight.light_ledSuppression;
                    break;
                    case 3:
                        s_nightLight.light_startHour -= 1;
                        s_nightLight.light_startHour = s_nightLight.light_startHour > hourMax ? hourMax : s_nightLight.light_startHour;
                    break;
                    case 4:
                        s_nightLight.light_startMinute -= 10;
                        s_nightLight.light_startMinute = s_nightLight.light_startMinute > minuteMax ? minuteMax : s_nightLight.light_startMinute;
                    break;
                    case 5:
                        s_nightLight.use_nightMode = !s_nightLight.use_nightMode;
                    break;
                    case 6:
                        s_nightLight.night_brightnessLevel -= 1;
                        s_nightLight.night_brightnessLevel = s_nightLight.night_brightnessLevel > maxBri ? maxBri : s_nightLight.night_brightnessLevel;
                    break;
                    case 7:
                        s_nightLight.night_changeBrightness = !s_nightLight.night_changeBrightness;
                    break;
                    case 8:
                        s_nightLight.night_ledSuppression = !s_nightLight.night_ledSuppression;
                    break;
                    case 9:
                        s_nightLight.night_startHour -= 1;
                        s_nightLight.night_startHour = s_nightLight.night_startHour > hourMax ? hourMax : s_nightLight.night_startHour;
                    break;
                    case 10:
                        s_nightLight.night_startMinute -= 10;
                        s_nightLight.night_startMinute = s_nightLight.night_startMinute > minuteMax ? minuteMax : s_nightLight.night_startMinute;
                    break;
                }
            }
        }
        else if (pressed & KEY_START)
        {
            WriteNightLightSettings();
            nightLightSettingsRead = true;
        }
        else if (pressed & KEY_X) {
            nightLightOverride = !nightLightOverride;
            Redshift_UpdateNightLightStatuses();
        }
        else if (pressed & KEY_B) {
            break;
        }

        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_TITLE, "Night/Light - by Nutez, discovery Sono");
        u32 posY = 30;

        sprintf(fmtbuf, "%c Light Mode brightness: %i", (sel == 0 ? '>' : ' '), s_nightLight.light_brightnessLevel);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Light Mode set brightness: %s", (sel == 1 ? '>' : ' '), s_nightLight.light_changeBrightness ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Light Mode suppress LEDs: %s", (sel == 2 ? '>' : ' '), s_nightLight.light_ledSuppression ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Light Mode start hour: %i", (sel == 3 ? '>' : ' '), s_nightLight.light_startHour);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Light Mode start minute: %i", (sel == 4 ? '>' : ' '), s_nightLight.light_startMinute);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + (SPACING_Y);

        sprintf(fmtbuf, "%c Enable Night Mode: %s", (sel == 5 ? '>' : ' '), s_nightLight.use_nightMode ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_RED, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode brightness: %i", (sel == 6 ? '>' : ' '), s_nightLight.night_brightnessLevel);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode set brightness: %s", (sel == 7 ? '>' : ' '), s_nightLight.night_changeBrightness ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode suppress LEDs: %s", (sel == 8 ? '>' : ' '), s_nightLight.night_ledSuppression ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode start hour: %i", (sel == 9 ? '>' : ' '), s_nightLight.night_startHour);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode start minute: %i", (sel == 10 ? '>' : ' '), s_nightLight.night_startMinute);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + (SPACING_Y*2);

        posY = Draw_DrawString(10, posY, COLOR_GREEN, "Controls:") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, " D-Pad to edit settings.") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, " Press START to save. Press B to exit.") + SPACING_Y;
        posY = Draw_DrawString(10, posY, COLOR_WHITE, " X to stop time check until reboot.") + SPACING_Y;
        if (nightLightOverride) posY = Draw_DrawString(10, posY, COLOR_RED, " [Time check overridden]") + SPACING_Y;

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!menuShouldExit);
}
