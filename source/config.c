/*
*   config.c
*/

#include "config.h"
#include "utils.h"
#include "screen.h"
#include "draw.h"
#include "fs.h"
#include "i2c.h"
#include "buttons.h"
#include "pin.h"

void configureCFW(const char *configPath)
{
    u32 needToDeinit = 0;

    // Verify we actually need to init screen.
    if(PDN_GPU_CNT == 1)
    {
        needToDeinit = initScreens();
    }

    // We need to clear the screen incase we verify the pin to remove the data related to that.
    clearScreens();

    drawString(CONFIG_TITLE, 10, 10, COLOR_TITLE);
    drawString("Press A to select, START to save", 10, 30, COLOR_WHITE);

    const char *multiOptionsText[]  = { "Screen-init brightness: 4( ) 3( ) 2( ) 1( )",
                                        "New 3DS CPU: Off( ) Clock( ) L2( ) Clock+L2( )" };

    const char *singleOptionsText[] = { "( ) Autoboot SysNAND",
                                        "( ) SysNAND is updated (A9LH-only)",
                                        "( ) Force A9LH detection",
                                        "( ) Use second EmuNAND as default",
                                        "( ) Enable region/language emu. and external       .code loading",
                                        "( ) Show current NAND in System Settings",
                                        "( ) Show GBA boot screen in patched AGB_FIRM",
                                        "( ) Enable splash screen with no screen-init",
                                        "( ) Use Pin",
                                        "( ) Ask to change pin when exiting config menu" };

    struct multiOption {
        int posXs[4];
        int posY;
        u32 enabled;
    } multiOptions[] = {
        { .posXs = {26, 31, 36, 41} },
        { .posXs = {17, 26, 32, 44} }
    };

    //Calculate the amount of the various kinds of options and pre-select the first single one
    u32 multiOptionsAmount = sizeof(multiOptions) / sizeof(struct multiOption),
        singleOptionsAmount = sizeof(singleOptionsText) / sizeof(char *),
        totalIndexes = multiOptionsAmount + singleOptionsAmount - 1,
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

    //Display all the multiple choice options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        multiOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(multiOptionsText[i], 10, multiOptions[i].posY, COLOR_WHITE);
        drawCharacter(selected, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE);
    }

    endPos += SPACING_Y / 2;
    u32 color = COLOR_RED;

    //Display all the normal options in white except for the first one
    for(u32 i = 0; i < singleOptionsAmount; i++)
    {
        singleOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(singleOptionsText[i], 10, singleOptions[i].posY, color);
        if(singleOptions[i].enabled) drawCharacter(selected, 10 + SPACING_X, singleOptions[i].posY, color);
        color = COLOR_WHITE;
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
                    selectedOption = !selectedOption ? totalIndexes : selectedOption - 1;
                    break;
                case BUTTON_DOWN:
                    selectedOption = selectedOption == totalIndexes ? 0 : selectedOption + 1;
                    break;
                case BUTTON_LEFT:
                    selectedOption = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedOption = totalIndexes;
                    break;
                default:
                    continue;
            }

            if(selectedOption == oldSelectedOption) continue;

            //The user moved to a different option, print the old option in white and the new one in red. Only print 'x's if necessary
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
            //The selected option's status changed, print the 'x's accordingly
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

        //In any case, if the current option is enabled (or a multiple choice option is selected) we must display a red 'x'
        if(selectedOption < multiOptionsAmount)
        {
            if(selectedOption == 0)
                updateBrightness(multiOptions[selectedOption].enabled);
            
            drawCharacter(selected, 10 + multiOptions[selectedOption].posXs[multiOptions[selectedOption].enabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_RED);
        }
        else
        {
            u32 singleSelected = selectedOption - multiOptionsAmount;
            if(singleOptions[singleSelected].enabled) drawCharacter(selected, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_RED);
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

    //Wait for the pressed buttons to change
    while(HID_PAD == BUTTON_START);

    // pin not enabled, if pin binary exists, delete it.
    if (!singleOptions[8].enabled)
    {
        if (doesPinExist() == true)
        {
            // If it does exist, this means your turning it off - so we need to verify its you who is turning it off.
            verifyPin(false);
            deletePin();
        }
    }
    else
    {
        if(doesPinExist() == true)
        {
            // only if user specifies it to be changed each time
            if (CONFIG(9))
            {
                clearScreens();
                drawString("Do you want to change your pin?", 10, 10, COLOR_WHITE);
                drawString(" (A) Yes, (B) No.", 10, 20, COLOR_WHITE);
            
                bool running = true;
                while (running)
                {
                    u32 choice = 0;
                    do
                    {
                        choice = waitInput();
                    }
                    while(!(choice & PIN_BUTTONS));

                    if(choice == BUTTON_A)
                    {
                        // Verify old pin to change to new pin.
                        verifyPin(false);
                        newPin();
                        running = false;
                    }
                    else
                    {
                        running = false;
                    }
                }
            }
        }
        else
        {
            // if enabled, but no pin, create new pin.
            newPin();
        }
    }

    if(needToDeinit)
    {
        //Turn off backlight
        i2cWriteRegister(I2C_DEV_MCU, 0x22, 0x16);
        deinitScreens();
        PDN_GPU_CNT = 1;
    }
}