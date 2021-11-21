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

static const char filterLocs[3][32] = {
    "/luma/redshift.bin",
    "/luma/lightshift.bin",
    "/luma/nightshift.bin"
};

night_light_settings s_nightLight = {
    .light_brightnessLevel = 3,
    .light_filterEnabled = true,
    .light_ledSuppression = false,
    .light_startHour = 8,
    .light_startMinute = 10,
    .night_brightnessLevel = 2,
    .night_filterEnabled = true,
    .night_ledSuppression = true,
    .night_startHour = 19,
    .night_startMinute = 30,
    .use_nightMode = true,
    .light_changeBrightness = true,
    .night_changeBrightness = true,
};

static void WriteAt(u32 v, u8 idx, int screen)
{
    if(screen & 1)
    {
        COLORREG_IDX_B = (u32)idx;
        COLORREG_DAT_B = v;
    }
    else
    {
        COLORREG_IDX_T = (u32)idx;
        COLORREG_DAT_T = v;
    }
}

static void ClampCS(color_setting_t* cs)
{
    if(cs->temperature < MIN_TEMP) cs->temperature = MIN_TEMP;
    if(cs->temperature > MAX_TEMP) cs->temperature = MAX_TEMP;
    
    if(cs->gamma[0] < MIN_GAMMA) cs->gamma[0] = MIN_GAMMA;
    if(cs->gamma[1] < MIN_GAMMA) cs->gamma[1] = MIN_GAMMA;
    if(cs->gamma[2] < MIN_GAMMA) cs->gamma[2] = MIN_GAMMA;
    if(cs->gamma[0] > MAX_GAMMA) cs->gamma[0] = MAX_GAMMA;
    if(cs->gamma[1] > MAX_GAMMA) cs->gamma[1] = MAX_GAMMA;
    if(cs->gamma[2] > MAX_GAMMA) cs->gamma[2] = MAX_GAMMA;
    
    if(cs->brightness < MIN_BRIGHTNESS) cs->brightness = MIN_BRIGHTNESS;
    if(cs->brightness > MAX_BRIGHTNESS) cs->brightness = MAX_BRIGHTNESS;
}

static void ApplyCS(color_setting_t* cs, int screen)
{
    u16 c[0x300];
    u8 i = 0;
    
    do
    {
        *(c + i + 0x000) = i | (i << 8);
        *(c + i + 0x100) = i | (i << 8);
        *(c + i + 0x200) = i | (i << 8);
    }
    while(++i);
    
    colorramp_fill_brightness(c + 0x000, c + 0x100, c + 0x200, 0x100, cs);
    
    do
    {
        WriteAt((c[i] >> 8) | ((c[i + 0x100] >> 8) << 8) | ((c[i + 0x200] >> 8) << 16), i, screen);
    }
    while(++i);
}

float fmodf(float f1, float f2);

void Redshift_EditableFilter(u8 filterNo)
{
    if (filterNo == 0)
    {
        Redshift_SuppressLeds();
    }

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();
    
    color_setting_t cs;
    memset(&cs, 0, sizeof(cs));
    cs.temperature = NEUTRAL_TEMP;
    cs.gamma[0] = 1.0F;
    cs.gamma[1] = 1.0F;
    cs.gamma[2] = 1.0F;
    cs.brightness = 1.0F;
    
    color_setting_t screens[2];
    memcpy(screens + 0, &cs, sizeof(cs));
    memcpy(screens + 1, &cs, sizeof(cs));
    
    IFile file;
    Result res = 0;
    if(!(HID_PAD & ~(KEY_A | KEY_X)))
    {
        res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
            fsMakePath(PATH_ASCII, filterLocs[filterNo]), FS_OPEN_READ);
        
        if(R_SUCCEEDED(res))
        {
            u64 total;
            IFile_Read(&file, &total, screens, sizeof(screens));
            ApplyCS(screens + 0, 0);
            ApplyCS(screens + 1, 1);
            cs.temperature = screens[0].temperature;
            cs.gamma[0] = screens[0].gamma[0];
            cs.gamma[1] = screens[0].gamma[1];
            cs.gamma[2] = screens[0].gamma[2];
            cs.brightness = screens[0].brightness;
            IFile_Close(&file);
            redshiftFilterSelected = filterNo == 0 ? true : false;
            screenFiltersCurrentTemperature = -1;
        }
    }
    
    while(HID_PAD & (KEY_A | KEY_X))
        svcSleepThread(1 * 1000 * 1000LL);
    
    u32 kDown = 0;
    //u32 kUp = 0;
    u32 kHeld = 0;
    u32 kOld = 0;
    int sel = 0;
    
    char fmtbuf[0x40];

    do
    {
        kOld = kHeld;
        kHeld = HID_PAD;
        kDown = (~kOld) & kHeld;
        //kUp = kOld & (~kHeld);
        
        if(kDown & KEY_START)
        {
            res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                fsMakePath(PATH_ASCII, filterLocs[filterNo]), FS_OPEN_CREATE | FS_OPEN_WRITE);
            
            if(R_SUCCEEDED(res))
            {
                u64 total;
                res = IFile_Write(&file, &total, screens, sizeof(screens), 0);
                IFile_Close(&file);
                redshiftFilterSelected = filterNo == 0 ? true : false;
            }
        }

        if(kDown & (KEY_B))
            return;

		if (kDown & (KEY_SELECT))
		{
            menuToggleLeds();
		}
        
        if(kDown & KEY_DOWN)
        {
            if(++sel > 4) sel = 4;
        }
        if(kDown & KEY_UP)
        {
            if(--sel < 0) sel = 0;
        }
        
        if(kDown & (KEY_Y))
        {
            memset(&cs, 0, sizeof(cs));
            cs.temperature = NEUTRAL_TEMP;
            cs.gamma[0] = 1.0F;
            cs.gamma[1] = 1.0F;
            cs.gamma[2] = 1.0F;
            cs.brightness = 1.0F;
        }
        
        if((((kDown & KEY_RIGHT) ? 1 : 0) ^ ((kDown & KEY_LEFT) ? 1 : 0)))
        {
            if(!sel)
            {
                if(kDown & KEY_RIGHT)
                    cs.temperature += (kHeld & (KEY_L | KEY_R)) ? 1 : 100;
                else
                    cs.temperature -= (kHeld & (KEY_L | KEY_R)) ? 1 : 100;
            }
            else
            {
                float* f = 0;
                if(!(sel >> 2))
                    f = &cs.gamma[sel - 1];
                
                if(!(sel ^ 4))
                    f = &cs.brightness;
                
                if(f)
                {
                    if(kDown & KEY_RIGHT)
                        *f = (((float)(((int)((*f * 100.0F) + 0.0001F)) + ((kHeld & (KEY_L | KEY_R)) ? 1 : 10))) / 100.0F) + 0.000001F;
                    else
                        *f = (((float)(((int)((*f * 100.0F) + 0.0001F)) - ((kHeld & (KEY_L | KEY_R)) ? 1 : 10))) / 100.0F) + 0.000001F;
                }
            }
        }
        
        if(kDown)
        {
            ClampCS(&cs);
            
            if(kHeld & KEY_A)
            {
                memcpy(screens + 1, &cs, sizeof(cs));
                ApplyCS(&cs, 1);
            }
            if(kHeld & KEY_X)
            {
                memcpy(screens + 0, &cs, sizeof(cs));
                ApplyCS(&cs, 0);
            }
        }
        
        Draw_Lock();
        Draw_DrawString(10, 8, COLOR_RED, "CTR_Redshift - ported by Sono, minor edit Nutez");
        Draw_DrawString(10, 18, COLOR_WHITE, "START save filter config, SELECT toggle LEDs");
        Draw_DrawString(10, 28, COLOR_WHITE, "X set top screen, A set bottom screen");
        Draw_DrawString(10, 38, COLOR_WHITE, "LEFT/RIGHT change value, +hold L1/R1 to fine tune)");
		Draw_DrawString(10, 48, COLOR_WHITE, "Y reset values to neutral, B back to menu");
        Draw_DrawString(10, 58, COLOR_WHITE, "Initial values are reloaded from top screen save");
        
        sprintf(fmtbuf, "%c Colortemp: %iK", (sel == 0 ? '>' : ' '), cs.temperature);
        Draw_DrawString(10, 76, COLOR_WHITE, fmtbuf);
        
        sprintf(fmtbuf, "%c Gamma[R]: %i.%02i", (sel == 1 ? '>' : ' '),
            (int)cs.gamma[0], (int)fmodf((cs.gamma[0] * 100.0F) + 0.0001F, 100.0F));
        Draw_DrawString(10, 92, COLOR_WHITE, fmtbuf);
        sprintf(fmtbuf, "%c Gamma[G]: %i.%02i", (sel == 2 ? '>' : ' '),
            (int)cs.gamma[1], (int)fmodf((cs.gamma[1] * 100.0F) + 0.0001F, 100.0F));
        Draw_DrawString(10, 100, COLOR_WHITE, fmtbuf);
        sprintf(fmtbuf, "%c Gamma[B]: %i.%02i", (sel == 3 ? '>' : ' '),
            (int)cs.gamma[2], (int)fmodf((cs.gamma[2] * 100.0F) + 0.0001F, 100.0F));
        Draw_DrawString(10, 108, COLOR_WHITE, fmtbuf);
        
        sprintf(fmtbuf, "%c Faux brightness: %i.%02i", (sel == 4 ? '>' : ' '),
            (int)cs.brightness, (int)fmodf((cs.brightness * 100.0F) + 0.0001F, 100.0F));
        Draw_DrawString(10, 124, COLOR_WHITE, fmtbuf);

        Draw_DrawString(10, 144, COLOR_GREEN, "Note:");
        Draw_DrawString(20, 154, COLOR_WHITE, "* Only saved filter config will be reapplied");
        if (filterNo == 0)
        {
            Draw_DrawString(20, 164, COLOR_WHITE, "* Night/Light now overridden until reboot or");
            Draw_DrawString(20, 174, COLOR_WHITE, "reapplication from Screen Filters menu");
        }
        else 
        {
            Draw_DrawString(20, 164, COLOR_WHITE, "* Night/Light brightness settings are applied");
            Draw_DrawString(20, 174, COLOR_WHITE, "upon exiting the Rosalina menu");
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
        svcSleepThread(1 * 1000 * 1000LL);
    }
    while(!menuShouldExit);
}

void Redshift_ApplySavedFilter(u8 filterNo)
{
    if (filterNo == 0)
    {
        Redshift_SuppressLeds();
    }

    color_setting_t cs;
    memset(&cs, 0, sizeof(cs));
    cs.temperature = NEUTRAL_TEMP;
    cs.gamma[0] = 1.0F;
    cs.gamma[1] = 1.0F;
    cs.gamma[2] = 1.0F;
    cs.brightness = 1.0F;
    
    color_setting_t screens[2];
    memcpy(screens + 0, &cs, sizeof(cs));
    memcpy(screens + 1, &cs, sizeof(cs));
    
    IFile file;
    Result res = 0;

    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
        fsMakePath(PATH_ASCII, filterLocs[filterNo]), FS_OPEN_READ);
    
    if(R_SUCCEEDED(res))
    {
        u64 total;
        IFile_Read(&file, &total, screens, sizeof(screens));
        ApplyCS(screens + 0, 0);
        ApplyCS(screens + 1, 1);
        IFile_Close(&file);
    }
}

void Redshift_SuppressLeds(void)
{
    mcuHwcInit();
    u8 off = 0;
    MCUHWC_WriteRegister(0x28, &off, 1);
    mcuHwcExit();
}

bool Redshift_LcdsAvailable(void)
{
    Result res = 0;
    Handle gspLcdHandle;
    res = srvGetServiceHandle(&gspLcdHandle, "gsp::Lcd");
    if(R_SUCCEEDED(res)) 
    {
        u8 tries = 0;
        u32 *cmdbuf = getThreadCommandBuffer();

	    cmdbuf[0] = IPC_MakeHeader(0x0E,0,0); // 0x0E0000

        while(tries < 51)
        {
            if(R_SUCCEEDED(res = svcSendSyncRequest(gspLcdHandle))) 
            {
                u8 lcdStatus = cmdbuf[2];

                if((lcdStatus & 8) == 0)
                {
                    if((lcdStatus & 4) && (lcdStatus & 15) == 7) // Sono discovery
                    {
                        svcCloseHandle(gspLcdHandle);
                        return true;
                    }
                }
            }

            svcSleepThread(20 * 1000 * 1000LL);
            tries += 1;
        }   

        svcCloseHandle(gspLcdHandle);
        return false;   
    }
    return false; 
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
            if(s_nightLight.night_filterEnabled && Redshift_LcdsAvailable()) Redshift_ApplySavedFilter(2);
        }
        else
        {
            ApplyLightMode();
            if(s_nightLight.light_filterEnabled && Redshift_LcdsAvailable()) Redshift_ApplySavedFilter(1);
        }
    }
    else
    {
        if (lightModeActive)
        {
            ApplyLightMode();
            if(s_nightLight.light_filterEnabled && Redshift_LcdsAvailable()) Redshift_ApplySavedFilter(1);
        }
        else if(nightModeActive)
        {
            ApplyNightMode();
            if(s_nightLight.night_filterEnabled && Redshift_LcdsAvailable()) Redshift_ApplySavedFilter(2);
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

    sprintf(nightLightMainMenuBuf, "Screen filters%s", (nightLightSettingsRead && !redshiftFilterSelected && screenFiltersCurrentTemperature == -1) ? statusBuf : "...");
    sprintf(nightLightSubMenuBuf, "Night/Light Config%s", (nightLightSettingsRead && !redshiftFilterSelected && screenFiltersCurrentTemperature == -1) ? statusBuf : "");

    rosalinaMenu.items[0].title = nightLightMainMenuBuf;
    screenFiltersMenu.items[13].title = nightLightSubMenuBuf;
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
                if(++sel > 12) sel = 12;
            }
            else if(pressed & KEY_UP)
            {
                if(--sel < 0) sel = 0;
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
                        s_nightLight.light_filterEnabled = !s_nightLight.light_filterEnabled;
                    break;
                    case 3:
                        s_nightLight.light_ledSuppression = !s_nightLight.light_ledSuppression;
                    break;
                    case 4:
                        s_nightLight.light_startHour += 1;
                        s_nightLight.light_startHour = s_nightLight.light_startHour > hourMax ? 0 : s_nightLight.light_startHour;
                    break;
                    case 5:
                        s_nightLight.light_startMinute += 10;
                        s_nightLight.light_startMinute = s_nightLight.light_startMinute > minuteMax ? 0 : s_nightLight.light_startMinute;
                    break;
                    case 6:
                        s_nightLight.use_nightMode = !s_nightLight.use_nightMode;
                    break;
                    case 7:
                        s_nightLight.night_brightnessLevel += 1;
                        s_nightLight.night_brightnessLevel = s_nightLight.night_brightnessLevel > maxBri ? minBri : s_nightLight.night_brightnessLevel;
                    break;
                    case 8:
                        s_nightLight.night_changeBrightness = !s_nightLight.night_changeBrightness;
                    break;
                    case 9:
                        s_nightLight.night_filterEnabled = !s_nightLight.night_filterEnabled;
                    break;
                    case 10:
                        s_nightLight.night_ledSuppression = !s_nightLight.night_ledSuppression;
                    break;
                    case 11:
                        s_nightLight.night_startHour += 1;
                        s_nightLight.night_startHour = s_nightLight.night_startHour > hourMax ? 0 : s_nightLight.night_startHour;
                    break;
                    case 12:
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
                        s_nightLight.light_filterEnabled = !s_nightLight.light_filterEnabled;
                    break;
                    case 3:
                        s_nightLight.light_ledSuppression = !s_nightLight.light_ledSuppression;
                    break;
                    case 4:
                        s_nightLight.light_startHour -= 1;
                        s_nightLight.light_startHour = s_nightLight.light_startHour > hourMax ? hourMax : s_nightLight.light_startHour;
                    break;
                    case 5:
                        s_nightLight.light_startMinute -= 10;
                        s_nightLight.light_startMinute = s_nightLight.light_startMinute > minuteMax ? minuteMax : s_nightLight.light_startMinute;
                    break;
                    case 6:
                        s_nightLight.use_nightMode = !s_nightLight.use_nightMode;
                    break;
                    case 7:
                        s_nightLight.night_brightnessLevel -= 1;
                        s_nightLight.night_brightnessLevel = s_nightLight.night_brightnessLevel > maxBri ? maxBri : s_nightLight.night_brightnessLevel;
                    break;
                    case 8:
                        s_nightLight.night_changeBrightness = !s_nightLight.night_changeBrightness;
                    break;
                    case 9:
                        s_nightLight.night_filterEnabled = !s_nightLight.night_filterEnabled;
                    break;
                    case 10:
                        s_nightLight.night_ledSuppression = !s_nightLight.night_ledSuppression;
                    break;
                    case 11:
                        s_nightLight.night_startHour -= 1;
                        s_nightLight.night_startHour = s_nightLight.night_startHour > hourMax ? hourMax : s_nightLight.night_startHour;
                    break;
                    case 12:
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

        sprintf(fmtbuf, "%c Light Mode apply filter: %s", (sel == 2 ? '>' : ' '), s_nightLight.light_filterEnabled ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Light Mode suppress LEDs: %s", (sel == 3 ? '>' : ' '), s_nightLight.light_ledSuppression ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Light Mode start hour: %i", (sel == 4 ? '>' : ' '), s_nightLight.light_startHour);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Light Mode start minute: %i", (sel == 5 ? '>' : ' '), s_nightLight.light_startMinute);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + (SPACING_Y);

        sprintf(fmtbuf, "%c Enable Night Mode: %s", (sel == 6 ? '>' : ' '), s_nightLight.use_nightMode ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_RED, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode brightness: %i", (sel == 7 ? '>' : ' '), s_nightLight.night_brightnessLevel);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode set brightness: %s", (sel == 8 ? '>' : ' '), s_nightLight.night_changeBrightness ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode apply filter: %s", (sel == 9 ? '>' : ' '), s_nightLight.night_filterEnabled ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode suppress LEDs: %s", (sel == 10 ? '>' : ' '), s_nightLight.night_ledSuppression ? "[true]" : "[false]");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode start hour: %i", (sel == 11 ? '>' : ' '), s_nightLight.night_startHour);
        posY = Draw_DrawString(10, posY, COLOR_WHITE, fmtbuf) + SPACING_Y;

        sprintf(fmtbuf, "%c Night Mode start minute: %i", (sel == 12 ? '>' : ' '), s_nightLight.night_startMinute);
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
