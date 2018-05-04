
#include <3ds.h>
#include "draw.h"
#include "menus/gsplcd.h"

#define BRIGTHNESS_MINI 16
#define BRIGTHNESS_MAXI 172


Menu Brightness = {
    "Screens Brightness options menu",
    .nbItems = 1,
    {
        { "Setup Screens Brightness", METHOD, .method = &setup_Brightness },
    }
};

u32 BrightnessTop = 80;
u32 BrightnessBottom = 80;

void setup_Brightness(void)
{
	char screen[5][32] = {"Screen TOP","Screen BOT", "Screen TOP/BOT","Turn ON/OFF Screen TOP","Turn ON/OFF Screen BOT"};
	int index = 0;
	Draw_Lock();
	Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();
	Draw_Unlock();
	
	gspLcdInit();
	
	while(true)
	{
		
		Draw_Lock();
		Draw_DrawString(10, 10, COLOR_TITLE, "Setup Screens Brightness");
		Draw_DrawString(10, 30, COLOR_WHITE, "Press LEFT/RIGHT changed Brightness or ON/OFF");
		Draw_DrawString(10, 40, COLOR_WHITE, "Press UP/DOWN changed options");
		Draw_DrawString(10, 50, COLOR_WHITE, "press B to return");
		Draw_DrawFormattedString(10, 70, COLOR_WHITE, "Brightness TOP screen : %d",BrightnessTop);
		Draw_DrawFormattedString(10, 80, COLOR_WHITE, "Brightness BOT screen : %d",BrightnessBottom);
		for(int i = 0; i < 5; i++)
		{
			Draw_DrawFormattedString(10, 100+(i*11), i == index ? COLOR_TITLE : COLOR_WHITE, "   %s",screen[i]);
			if(i == index)Draw_DrawString(10, 100+(i*11),COLOR_TITLE, "=>");
		}
		Draw_FlushFramebuffer();
		Draw_Unlock();
		
		u32 pressed = waitInput();
		if(pressed & BUTTON_B){
			break;
		} else if(pressed & BUTTON_UP){
			index = (index == 0) ? 4 : index-1;
		} else if(pressed & BUTTON_DOWN){
			index = (index == 4) ? 0 : index+1;
		} else if(pressed & BUTTON_LEFT){
			
			if(index == 0)BrightnessTop = (BrightnessTop == 0) ? BRIGTHNESS_MAXI : BrightnessTop-4;
			if(index == 1)BrightnessBottom = (BrightnessBottom == 0) ? BRIGTHNESS_MAXI : BrightnessBottom-4;
			if(index == 2){
				BrightnessTop = (BrightnessTop == 0) ? BRIGTHNESS_MAXI : BrightnessTop-4;
				BrightnessBottom = BrightnessTop;
			}
			if(index == 3){menuLeave();GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_TOP);menuEnter();}
			if(index == 4){menuLeave();GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);menuEnter();}
			
		} else if(pressed & BUTTON_RIGHT){
			
			if(index == 0)BrightnessTop = (BrightnessTop == BRIGTHNESS_MAXI) ? 0 : BrightnessTop+4;
			if(index == 1)BrightnessBottom = (BrightnessBottom == BRIGTHNESS_MAXI) ? 0 : BrightnessBottom+4;
			if(index == 2){
				BrightnessTop = (BrightnessTop == BRIGTHNESS_MAXI) ? 0 : BrightnessTop+4;
				BrightnessBottom = BrightnessTop;
			}
			if(index == 3){menuLeave();GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_TOP);menuEnter();}
			if(index == 4){menuLeave();GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTTOM);menuEnter();}
		}
		menuLeave();
		GSPLCD_SetBrightnessRaw(GSPLCD_SCREEN_TOP , BrightnessTop);
		GSPLCD_SetBrightnessRaw(GSPLCD_SCREEN_BOTTOM , BrightnessBottom);
		menuEnter();
	}
	
	gspLcdExit();
	
}
