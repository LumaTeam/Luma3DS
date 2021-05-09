#include <stdint.h>
#include <math.h>
#include <3ds.h>
#include "menus.h"
#include "redshift/redshift.h"
#include "redshift/colorramp.h"
#include "menu.h"
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

bool customFilterSelected = false;

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

void Redshift_EditableFilter(void)
{
    Redshift_SuppressLeds();

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
            fsMakePath(PATH_ASCII, "/luma/redshift.bin"), FS_OPEN_READ);
        
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
            customFilterSelected = true;
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
                fsMakePath(PATH_ASCII, "/luma/redshift.bin"), FS_OPEN_CREATE | FS_OPEN_WRITE);
            
            if(R_SUCCEEDED(res))
            {
                u64 total;
                res = IFile_Write(&file, &total, screens, sizeof(screens), 0);
                IFile_Close(&file);
                customFilterSelected = true;
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
        Draw_DrawString(10, 152, COLOR_WHITE, "Only saved filter config will be reapplied");

        Draw_FlushFramebuffer();
        Draw_Unlock();
        svcSleepThread(1 * 1000 * 1000LL);
    }
    while(!menuShouldExit);
}

void Redshift_ApplySavedFilter(void)
{
    Redshift_SuppressLeds();

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
        fsMakePath(PATH_ASCII, "/luma/redshift.bin"), FS_OPEN_READ);
    
    if(R_SUCCEEDED(res))
    {
        u64 total;
        IFile_Read(&file, &total, screens, sizeof(screens));
        ApplyCS(screens + 0, 0);
        ApplyCS(screens + 1, 1);
        IFile_Close(&file);
    }
}

void Redshift_SuppressLeds(void){
    mcuHwcInit();
    u8 off = 0;
    MCUHWC_WriteRegister(0x28, &off, 1);
    mcuHwcExit();
}
