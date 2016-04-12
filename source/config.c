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

void configureCFW(const char *configPath)
{
    initScreens();

    drawString(CONFIG_TITLE, 10, 10, COLOR_TITLE);
    drawString("Press A to select, START to save and reboot", 10, 30, COLOR_WHITE);

    const char *multiOptionsText[]  = { "Screen-init brightness: 4( ) 3( ) 2( ) 1( )",
                                        "New 3DS CPU: Off( ) L2( ) Clock( ) L2+Clock( )" };

    const char *singleOptionsText[] = { "( ) Autoboot SysNAND",
                                        "( ) Updated SysNAND mode (A9LH-only)",
                                        "( ) Force A9LH detection",
                                        "( ) Use second EmuNAND as default",
                                        "( ) Use developer UNITINFO",
                                        "( ) Show current NAND in System Settings",
                                        "( ) Show GBA boot screen in patched AGB_FIRM",
                                        "( ) Enable splash screen with no screen-init" };

    struct multiOption {
        int posY;
        u32 enabled;
        int posXs[4];
    } multiOptions[] = {
        { .posXs = {26, 31, 36, 41} },
        { .posXs = {17, 23, 32, 44} }
    };

    //Calculate the amount of the various kinds of options and pre-select the first single one
    u32 multiOptionsAmount = sizeof(multiOptions) / sizeof(struct multiOption),
        singleOptionsAmount = sizeof(singleOptionsText) / sizeof(char *),
        totalOptions = multiOptionsAmount + singleOptionsAmount,
        selectedOption = multiOptionsAmount;

    struct singleOption {
        int posY;
        u32 enabled;
    } singleOptions[singleOptionsAmount];

    //Parse the existing options
    for(u32 i = 0; i < multiOptionsAmount; i++)
        multiOptions[i].enabled = MULTICONFIG(i);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        singleOptions[i].enabled = CONFIG(i);

    //Character to display a selected option
    char selected = 'x';

    int endPos = 42;

    //Display all the normal options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        multiOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(multiOptionsText[i], 10, multiOptions[i].posY, COLOR_WHITE);
        drawCharacter(selected, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE);
    }

    //Display the first normal option in red
    singleOptions[0].posY = endPos + SPACING_Y + SPACING_Y / 2;
    endPos = drawString(singleOptionsText[0], 10, singleOptions[0].posY, COLOR_RED);

    //Display all the multiple choice options in white except for the first
    for(u32 i = 1; i < singleOptionsAmount; i++)
    {
        singleOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(singleOptionsText[i], 10, singleOptions[i].posY, COLOR_WHITE);
        if(singleOptions[i].enabled) drawCharacter(selected, 10 + SPACING_X, singleOptions[i].posY, COLOR_WHITE);
    }

    u32 pressed = 0;

    //Boring configuration menu
    while(pressed != BUTTON_START)
    {
        pressed = 0;

        do {
            //In any case, if the current option is enabled (or a multiple choice option is selected) we must display a red 'x'
            if(selectedOption < multiOptionsAmount)
                drawCharacter(selected, 10 + multiOptions[selectedOption].posXs[multiOptions[selectedOption].enabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_RED);
            else if(singleOptions[selectedOption - multiOptionsAmount].enabled)
                drawCharacter(selected, 10 + SPACING_X, singleOptions[selectedOption - multiOptionsAmount].posY, COLOR_RED);

            pressed = waitInput();
        }
        while(!(pressed & MENU_BUTTONS));

        //Remember the previously selected option
        static u32 oldSelectedOption;
        oldSelectedOption = selectedOption;

        if(pressed != BUTTON_A)
        {
            switch(pressed)
            {
                case BUTTON_UP:
                    selectedOption = !selectedOption ? totalOptions - 1 : selectedOption - 1;
                    break;
                case BUTTON_DOWN:
                    selectedOption = selectedOption == (totalOptions - 1) ? 0 : selectedOption + 1;
                    break;
                case BUTTON_LEFT:
                    if(!selectedOption) continue;
                    selectedOption = 0;
                    break;
                case BUTTON_RIGHT:
                    if(selectedOption == totalOptions - 1) continue;
                    selectedOption = totalOptions - 1;
                    break;
                default:
                    continue;
            }

            //The user moved to a different option, print the old option in white and the new one in white. Only print 'x's if necessary
            if(oldSelectedOption < multiOptionsAmount)
            {
                drawString(multiOptionsText[oldSelectedOption], 10, multiOptions[oldSelectedOption].posY, COLOR_WHITE);
                drawCharacter(selected, 10 + multiOptions[oldSelectedOption].posXs[multiOptions[oldSelectedOption].enabled] * SPACING_X, multiOptions[oldSelectedOption].posY, COLOR_WHITE);
            }
            else
            {
                u32 singleOldSelected = oldSelectedOption - multiOptionsAmount;
                drawString(singleOptionsText[singleOldSelected], 10, singleOptions[singleOldSelected].posY, COLOR_WHITE);
                if(singleOptions[singleOldSelected].enabled) drawCharacter(selected, 10 + SPACING_X, singleOptions[singleOldSelected].posY, COLOR_WHITE);
            }

            if(selectedOption < multiOptionsAmount)
                drawString(multiOptionsText[selectedOption], 10, multiOptions[selectedOption].posY, COLOR_RED);
            else
            {
                u32 singleSelected = selectedOption - multiOptionsAmount;
                drawString(singleOptionsText[singleSelected], 10, singleOptions[singleSelected].posY, COLOR_RED);
            }
        }
        else
        {
            //The selected option's status changed, print the 'x's accordingly.
            if(selectedOption < multiOptionsAmount)
            {
                u32 oldEnabled = multiOptions[selectedOption].enabled;
                drawCharacter(selected, 10 + multiOptions[selectedOption].posXs[oldEnabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_BLACK);
                multiOptions[selectedOption].enabled = oldEnabled == 3 ? 0 : oldEnabled + 1;
            }
            else
            {
                u32 oldEnabled = singleOptions[selectedOption - multiOptionsAmount].enabled;
                singleOptions[selectedOption - multiOptionsAmount].enabled = !oldEnabled;
                if(oldEnabled) drawCharacter(selected, 10 + SPACING_X, singleOptions[selectedOption - multiOptionsAmount].posY, COLOR_BLACK);
            }
        }
    }

    //Preserve the last-used boot options (last 12 bits)
    config &= 0x3F;

    //Parse and write the new configuration
    for(u32 i = 0; i < multiOptionsAmount; i++)
        config |= multiOptions[i].enabled << (i * 2 + 6);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        config |= singleOptions[i].enabled << (i + 16);

    fileWrite(&config, configPath, 4);

    //Zero the last booted FIRM flag
    CFG_BOOTENV = 0;

    //Reboot
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(1);
}
