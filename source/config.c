/*
*   config.c
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "config.h"
#include "utils.h"
#include "screeninit.h"
#include "draw.h"
#include "fs.h"
#include "i2c.h"
#include "buttons.h"

void configureCFW(const char *configPath, const char *patchedFirms[])
{
    initScreens();

    drawString(CONFIG_TITLE, 10, 10, COLOR_TITLE);
    drawString("Press A to select, START to save and reboot", 10, 30, COLOR_WHITE);

    const char *optionsText[] = { "Screen-init brightness: 4( ) 3( ) 2( ) 1( )",
                                  "( ) Updated SysNAND mode (A9LH-only)",
                                  "( ) Use pre-patched FIRMs",
                                  "( ) Force A9LH detection",
                                  "( ) Use 9.0 FIRM as default",
                                  "( ) Use second EmuNAND as default",
                                  "( ) Show current NAND in System Settings",
                                  "( ) Show GBA boot screen in patched AGB_FIRM",
                                  "( ) Enable splash screen with no screen-init" };

    u32 optionsAmount = sizeof(optionsText) / sizeof(char *);

    struct option {
        int posY;
        u32 enabled;
    } options[optionsAmount];

    //Parse the existing configuration
    options[0].enabled = CONFIG(10, 3);
    for(u32 i = optionsAmount; i; i--)
        options[i].enabled = CONFIG((i - 1), 1);

    //Pre-select the first configuration option
    u32 selectedOption = 1,
        oldSelectedOption = 0,
        optionChanged = 0;

    //Character to display a selected option
    char selected = 'x';

    //Starting position
    options[0].posY = 52;

    //Display all the normal options in white, brightness will be displayed later
    for(u32 i = 1; i < optionsAmount; i++)
    {
        options[i].posY = drawString(optionsText[i], 10, options[i - 1].posY + SPACING_Y + (!(1 - i) * 7), COLOR_WHITE);
        if(options[i].enabled) drawCharacter(selected, 10 + SPACING_X, options[i].posY, COLOR_WHITE);
    }

    //Boring configuration menu
    while(1)
    {
        u32 pressed = 0;

        do {
            //The status of the selected option changed, black out the previously visible 'x' if needed
            if(optionChanged)
            {
                if(!selectedOption)
                    drawCharacter(selected, 10 + (26 + 5 * (optionChanged - 1)) * SPACING_X, options[0].posY, COLOR_BLACK);
                else if(!options[selectedOption].enabled)
                    drawCharacter(selected, 10 + SPACING_X, options[selectedOption].posY, COLOR_BLACK);

                optionChanged = 0;
            }

            //The selected option changed, draw the new one in red and the old one (with its 'x') in white
            else if(selectedOption != oldSelectedOption)
            {
                drawString(optionsText[oldSelectedOption], 10, options[oldSelectedOption].posY, COLOR_WHITE);
                drawString(optionsText[selectedOption], 10, options[selectedOption].posY, COLOR_RED);

                if(!oldSelectedOption)
                    drawCharacter(selected, 10 + (26 + 5 * options[0].enabled) * SPACING_X, options[0].posY, COLOR_WHITE);
                else if(options[oldSelectedOption].enabled)
                    drawCharacter(selected, 10 + SPACING_X, options[oldSelectedOption].posY, COLOR_WHITE);
            }

            //In any case, if the current option is enabled (or brightness is selected) we must display a red 'x'
            if(!selectedOption)
                drawCharacter(selected, 10 + (26 + 5 * options[0].enabled) * SPACING_X, options[0].posY, COLOR_RED);
            else if(options[selectedOption].enabled)
                drawCharacter(selected, 10 + SPACING_X, options[selectedOption].posY, COLOR_RED);

            pressed = waitInput();
        }
        while(!(pressed & MENU_BUTTONS));

        //Remember the previously selected option
        oldSelectedOption = selectedOption;

        switch(pressed)
        {
            case BUTTON_UP:
                selectedOption = !selectedOption ? optionsAmount - 1 : selectedOption - 1;
                break;
            case BUTTON_DOWN:
                selectedOption = selectedOption == (optionsAmount - 1) ? 1 : selectedOption + 1;
                break;
            case BUTTON_LEFT:
                selectedOption = 1;
                break;
            case BUTTON_RIGHT:
                selectedOption = optionsAmount - 1;
                break;
            case BUTTON_A:
                optionChanged = 1;
                if(selectedOption) options[selectedOption].enabled = !options[selectedOption].enabled;
                else
                {
                    optionChanged += options[0].enabled;
                    options[0].enabled = options[0].enabled == 3 ? 0 : options[0].enabled + 1;
                }
                break;
        }

        if(pressed == BUTTON_START) break;
    }

    //If the user has been using A9LH and the "Updated SysNAND" setting changed, delete the patched 9.0 FIRM
    if(CONFIG(16, 1) && (CONFIG(0, 1) != options[1].enabled)) fileDelete(patchedFirms[3]);

    //If the "Show GBA boot screen in patched AGB_FIRM" setting changed, delete the patched AGB_FIRM
    if(CONFIG(6, 1) != options[7].enabled) fileDelete(patchedFirms[5]);

    //Preserve the last-used boot options (last 12 bits)
    config &= 0xFFF000;

    //Parse and write the new configuration
    config |= options[0].enabled << 10;
    for(u32 i = optionsAmount; i; i--)
        config |= options[i].enabled << (i - 1);

    fileWrite(&config, configPath, 3);

    //Zero the last booted FIRM flag
    CFG_BOOTENV = 0;

    //Reboot
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(1);
}