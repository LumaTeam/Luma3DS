#include <3ds.h>
#include "draw.h"
#include "menus.h"

float Volume_ExtractVolume(int nul, int one, int slider)
{
    if(slider <= nul || one < nul)
        return 0;
    
    if(one == nul) //hardware returns 0 on divbyzero
        return (slider > one) ? 1.0F : 0;
    
    float ret = (float)(slider - nul) / (float)(one - nul);
    if((ret + (1 / 256.0F)) < 1.0F)
        return ret;
    else
        return 1.0F;
}

static void Volume_AdjustVolume(u8* out, int slider, float value)
{
    int smin = 0xFF;
    int smax = 0x00;
    
    if(slider)
    {
        float diff = 1.0F;
        
        int min;
        int max;
        for(min = 0; min != slider; min++)
        for(max = slider; max != 0x100; max++)
        {
            float rdiff = value - Volume_ExtractVolume(min, max, slider);
            
            if(rdiff < 0 || rdiff > diff)
                continue;
            
            diff = rdiff;
            smin = min;
            smax = max;
            
            if(rdiff < (1 / 256.0F))
                break;
        }
    }
    
    out[0] = smin & 0xFF;
    out[1] = smax & 0xFF;
}

void Volume_ControlVolume(void)
{
    u8 volumeSlider[2], dspVolumeSlider[2], disableVolumeSlider[2];
    int target = 0, targetMin = 0, targetMax = 64;
    disableVolumeSlider[0] = 0xFF;
    disableVolumeSlider[1] = 0x00;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    mcuHwcInit();
    // mute and disable volume slider
    MCUHWC_WriteRegister(0x58, disableVolumeSlider, 2);

    do
    {
        // https://www.3dbrew.org/wiki/I2C_Registers#Device_3
        MCUHWC_ReadRegister(0x58, dspVolumeSlider, 2); // Register-mapped ADC register
        MCUHWC_ReadRegister(0x27, volumeSlider + 0, 1); // Raw volume slider state
        MCUHWC_ReadRegister(0x09, volumeSlider + 1, 1); // Volume slider state

        float coe = Volume_ExtractVolume(dspVolumeSlider[0], dspVolumeSlider[1], volumeSlider[0]);

        u32 out = (u32)((coe * 100.0F) + (1 / 256.0F));
        u32 in = (u32)(target * 1.5625F);

        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_TITLE, "Volume Control by Sono, integration Nutez");
        u32 posY = 30;
        posY = Draw_DrawFormattedString(
            10,
            posY,
            COLOR_WHITE,
            "To set: %02X (%lu%%)\n\nVOL: %.2X - %.2X\nRNG: %.2X / %.2X\n\n",
            target, in, volumeSlider[1], volumeSlider[0], dspVolumeSlider[0], dspVolumeSlider[1]
        );
        posY = Draw_DrawFormattedString(
            10,
            posY,
            COLOR_WHITE,
            "Current volume: %lu%%\n\n",
            out);
        posY = Draw_DrawString(10, posY, COLOR_GREEN, "Controls:\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Up/Down for +-1, Right/Left for +-10 [0; 64].\n\n");

        if(!volumeSlider[0] || volumeSlider[0] == 0xFF)
        {
            posY = Draw_DrawString(10, posY, COLOR_RED, "Note:\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Your volume slider is right at the edge,\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "so it can't be adjusted by software.\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "If it is physically stuck there then you \n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "may need to solder to fix the volume :(\n");
        }
        else if(volumeSlider[0] < 0x10 || volumeSlider[0] >= 0xF0)
        {
            posY = Draw_DrawString(10, posY, COLOR_RED, "Note:\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Your volume slider is too close to the\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "edge, so output range is limited.\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "If you can, do move the slider to the\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "center to get better software range.\n");
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if (pressed & DIRECTIONAL_KEYS)
        {
            if (pressed & KEY_UP)
            {
                target += 1;
            }
            else if (pressed & KEY_DOWN)
            {
                target -= 1;
            }
            else if (pressed & KEY_RIGHT)
            {
                target += 10;
            }
            else if (pressed & KEY_LEFT)
            {
                target -= 10;
            }

            target = target > targetMax ? targetMax : target;
            target = target < targetMin ? targetMin : target;

            Volume_AdjustVolume(dspVolumeSlider, volumeSlider[0], target / 64.0F);
            MCUHWC_WriteRegister(0x58, dspVolumeSlider, 2);
        }

        if (pressed & KEY_B)
            return;
    }
    while (!menuShouldExit);

    mcuHwcExit();  
}
