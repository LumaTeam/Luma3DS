/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
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
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

#include "config.h"
#include "utils.h"
#include "screen.h"
#include "draw.h"
#include "fs.h"
#include "i2c.h"
#include "buttons.h"

char *str_format(char *dst, const char *src1, const char *src2)
{
    while (*src1)
    {
        *dst++ = *src1++;
    }

    *dst++ = ':';

    while (*src2)
    {
        *dst++ = *src2++;
    }
    *dst = 0;

    return dst;
}

//CONFIG
//15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
//                                           x  x : nandType
//                                        x       : firmSource
//                                     x          : isA9lh
//                                  x             : Flag to prevent multiple boot options-forcing
//                               x                : ?
//                         x  x                   : Screen-init brightness
//                   x  x                         : New 3DS CPU

//CONFIG
//31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 
//                                              x : Autoboot SysNAND
//                                           x    : SysNAND is updated
//                                        x       : Use second EmuNAND as default
//                                     x          : Region/language emu. and ext. .code
//                                  x             : Show current NAND in System Settings
//                               x                : Show GBA boot screen(patched AGB_FIRM)
//                            x                   : Splash screen with no screen-init


typedef struct _OPTION_TEXT
{
    int bit_start;
    int bit_mask;
    int value_select;
    int posY;
    const char* option_text;
    int value_count;
    const char* value_text[8];
}OPTION_TEXT, *POPTION_TEXT;

void configureCFW(const char *configPath)
{
    clearScreens();
    char szString[100];

    drawString(CONFIG_TITLE, 10, 10, COLOR_TITLE);
    drawString("Press A to select, START to save", 10, 30, COLOR_WHITE);

    OPTION_TEXT OptionTexts[] =
    {
        {    6, 3, 0, 0, "Screen-init brightness                ", 4, {"4", "3", "2", "1"}},
        {    8, 3, 0, 0, "New 3DS CPU                           ", 4, {"Off", "Clock", "L2", "Clock+L2"}},
        {   -1, 0, 0, 0, "                                      ", 0, {""}},
        {   16, 1, 0, 0, "Autoboot SysNAND                      ", 2, {"OFF","ON"}},
        {   17, 1, 0, 0, "Use SysNAND FIRM if booting w/R (A9LH)", 2, {"OFF","ON"}},
        {   18, 1, 0, 0, "Use second EmuNAND as default         ", 2, {"OFF","ON"}},
        {   19, 1, 0, 0, "Region/language emu. and ext. .code   ", 2, {"OFF","ON"}},
        {   20, 1, 0, 0, "Show current NAND in System Settings  ", 2, {"OFF","ON"}},
        {   21, 1, 0, 0, "Show GBA boot screen(patched AGB_FIRM)", 2, {"OFF","ON"}},
        {   22, 1, 0, 0, "Splash screen with no screen-init     ", 2, {"OFF","ON"}}
        {   23, 1, 0, 0, "Use a PIN                             ", 2, {"OFF","ON"}}
    };


    u32 totalCount = sizeof (OptionTexts) / sizeof (OPTION_TEXT);
    u32 selectedOption = 3;//Autoboot SysNAND
    int endPos = 42;

    //Parse the existing options
    for(u32 i = 0; i < totalCount; i++)
    {
        OptionTexts[i].value_select = (config >> OptionTexts[i].bit_start) & OptionTexts[i].bit_mask;
    }

    //Display all options
    for (u32 i=0;i<totalCount;i++)
    {
        OptionTexts[i].posY = endPos + SPACING_Y;

        if (OptionTexts[i].bit_start == -1)
        {
            endPos += SPACING_Y;
            continue;
        }

        str_format (szString, OptionTexts[i].option_text, OptionTexts[i].value_text[OptionTexts[i].value_select]);
        endPos = drawString (szString, 10, OptionTexts[i].posY, selectedOption == i ? COLOR_RED : COLOR_WHITE);
    }


    u32 pressed = 0;

    //Boring configuration menu
    while(pressed != BUTTON_START)
    {
        do
        {
            pressed = waitInput();
        }
        while(!(pressed & MENU_BUTTONS));

        if(pressed != BUTTON_A)
        {
            //Remember the previously selected option
            u32 oldSelectedOption = selectedOption;

            switch(pressed)
            {
                case BUTTON_UP:
                    selectedOption = !selectedOption ? (totalCount - 1) : selectedOption - 1;
                    while (OptionTexts[selectedOption].bit_start == -1)
                    {
                        selectedOption = !selectedOption ? (totalCount - 1) : selectedOption - 1;
                    }
                    break;
                case BUTTON_DOWN:
                    selectedOption = selectedOption == (totalCount - 1) ? 0 : selectedOption + 1;
                    while (OptionTexts[selectedOption].bit_start == -1)
                    {
                       selectedOption = selectedOption == (totalCount -1 ) ? 0 : selectedOption + 1;
                    }
                    break;
                case BUTTON_LEFT:
                    selectedOption = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedOption = (totalCount - 1);
                    break;
                default:
                    continue;
            }

            if(selectedOption == oldSelectedOption) continue;

            //The user moved to a different option, print the old option in white and the new one in red.
            str_format (szString, OptionTexts[oldSelectedOption].option_text, OptionTexts[oldSelectedOption].value_text[OptionTexts[oldSelectedOption].value_select]);
            drawString (szString, 10, OptionTexts[oldSelectedOption].posY, COLOR_WHITE);

            str_format (szString, OptionTexts[selectedOption].option_text, OptionTexts[selectedOption].value_text[OptionTexts[selectedOption].value_select]);
            drawString (szString, 10, OptionTexts[selectedOption].posY, COLOR_RED);
        }
        else
        {
            //The selected option's status changed.
            str_format (szString, OptionTexts[selectedOption].option_text, OptionTexts[selectedOption].value_text[OptionTexts[selectedOption].value_select]);
            drawString (szString, 10, OptionTexts[selectedOption].posY, COLOR_BLACK);

            OptionTexts[selectedOption].value_select++;
            if (OptionTexts[selectedOption].value_select >= OptionTexts[selectedOption].value_count)
            {
                OptionTexts[selectedOption].value_select = 0;
            }
                
            str_format (szString, OptionTexts[selectedOption].option_text, OptionTexts[selectedOption].value_text[OptionTexts[selectedOption].value_select]);
            drawString (szString, 10, OptionTexts[selectedOption].posY, COLOR_RED);

            if(OptionTexts[selectedOption].bit_start == 6)
                updateBrightness(OptionTexts[selectedOption].value_select);
        }
   }

    //Preserve the last-used boot options (last 12 bits)
    config &= 0x3F;

    //Parse and write the new configuration
    for(u32 i = 0; i < totalCount; i++)
    {
        config |= OptionTexts[i].value_select << OptionTexts[i].bit_start;
    }

    if(!fileWrite(&config, configPath, 4))
    {
        createDirectory("luma");
        if(!fileWrite(&config, configPath, 4))
            error("Error writing the configuration file");
    }

    //Wait for the pressed buttons to change
    while(HID_PAD == BUTTON_START);
}