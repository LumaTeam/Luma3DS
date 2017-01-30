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
#include "memory.h"
#include "fs.h"
#include "utils.h"
#include "screen.h"
#include "draw.h"
#include "buttons.h"
#include "pin.h"

CfgData configData;

bool readConfig(void)
{
    if(fileRead(&configData, CONFIG_FILE, sizeof(CfgData)) != sizeof(CfgData) ||
       memcmp(configData.magic, "CONF", 4) != 0 ||
       configData.formatVersionMajor != CONFIG_VERSIONMAJOR ||
       configData.formatVersionMinor != CONFIG_VERSIONMINOR)
    {
        configData.config = 0;

        return false;
    }

    return true;
}

void writeConfig(ConfigurationStatus needConfig, u32 configTemp)
{
    /* If the configuration is different from previously, overwrite it.
       Just the no-forcing flag being set is not enough */
    if(needConfig != CREATE_CONFIGURATION && (configTemp & 0xFFFFFF7F) == configData.config) return;

    if(needConfig == CREATE_CONFIGURATION)
    {
        memcpy(configData.magic, "CONF", 4);
        configData.formatVersionMajor = CONFIG_VERSIONMAJOR;
        configData.formatVersionMinor = CONFIG_VERSIONMINOR;
    }

    //Merge the new options and new boot configuration
    configData.config = (configData.config & 0xFFFFFF00) | (configTemp & 0xFF);

    if(!fileWrite(&configData, CONFIG_FILE, sizeof(CfgData)))
        error("Error writing the configuration file");
}

void configMenu(bool isSdMode, bool oldPinStatus, u32 oldPinMode)
{
    const char *multiOptionsText[]  = { "Default EmuNAND: 1( ) 2( ) 3( ) 4( )",
                                        "Screen brightness: 4( ) 3( ) 2( ) 1( )",
                                        "Splash: Off( ) Before( ) After( ) payloads",
                                        "PIN lock: Off( ) 4( ) 6( ) 8( ) digits",
                                        "New 3DS CPU: Off( ) Clock( ) L2( ) Clock+L2( )",
                                        "Dev. features: Off( ) ErrDisp( ) UNITINFO( )"
                                      };

    const char *singleOptionsText[] = { "( ) Autoboot SysNAND",
                                        "( ) Use SysNAND FIRM if booting with R",
                                        "( ) Enable loading external FIRMs and modules",
                                        "( ) Use custom path",
                                        "( ) Enable game patching",
                                        "( ) Show NAND or user string in System Settings",
                                        "( ) Show GBA boot screen in patched AGB_FIRM",
                                        "( ) Patch SVC/service/archive/ARM9 access",
                                        "( ) Hide Pin when entering"
                                      };

    const char *optionsDescription[]  = { "Select the default EmuNAND.\n\n"
                                          "It will be booted when no\n"
                                          "directional pad buttons are pressed.",

                                          "Select the screen brightness.",

                                          "Enable splash screen support.\n\n"
                                          "\t* 'Before payloads' displays it\n"
                                          "before booting payloads\n"
                                          "(intended for splashes that display\n"
                                          "button hints).\n\n"
                                          "\t* 'After payloads' displays it\n"
                                          "afterwards.",

                                          "Activate a PIN lock.\n\n"
                                          "The PIN will be asked each time\n"
                                          "Luma3DS boots.\n\n"
                                          "4, 6 or 8 digits can be selected.\n\n"
                                          "The ABXY buttons and the directional\n"
                                          "pad buttons can be used as keys.\n\n"
                                          "A message can also be displayed\n"
                                          "(refer to the wiki for instructions).",

                                          "Select the New 3DS CPU mode.\n\n"
                                          "This won't apply to\n"
                                          "New 3DS exclusive/enhanced games.\n\n"
                                          "'Clock+L2' can cause issues with some\n"
                                          "games.",

                                          "Select the developer features.\n\n"
                                          "\t* If 'Off' is not checked, exception\n"
                                          "handlers will be enabled on A9LH.\n"
                                          "\t* 'ErrDisp' also displays debug info\n"
                                          "on the 'An error has occurred' screen.\n"
                                          "\t* 'UNITINFO' also makes the console\n"
                                          "be always detected as a\n"
                                          "development unit\n"
                                          "(which breaks online features, amiibos\n"
                                          "and retail CIAs, but allows installing\n"
                                          "and booting some developer software).\n\n"
                                          "Only change this if you know what you\n"
                                          "are doing!",

                                          "If enabled, SysNAND will be launched\n"
                                          "on boot.\n\n"
                                          "Otherwise, an EmuNAND will.\n\n"
                                          "Hold L on boot to switch NAND.\n\n"
                                          "To use a different EmuNAND from the\n"
                                          "default, hold a directional pad button\n"
                                          "(Up/Right/Down/Left equal EmuNANDs\n"
                                          "1/2/3/4).",

                                          "If enabled, when holding R on boot\n"
                                          "EmuNAND will be booted with the\n"
                                          "SysNAND FIRM.\n\n"
                                          "Otherwise, SysNAND will be booted\n"
                                          "with an EmuNAND FIRM.\n\n"
                                          "To use a different EmuNAND from the\n"
                                          "default, hold a directional pad button\n"
                                          "(Up/Right/Down/Left equal EmuNANDs\n"
                                          "1/2/3/4), also add A if you have\n"
                                          "a matching payload.",

                                          "Enable loading external FIRMs and\n"
                                          "system modules.\n\n"
                                          "This isn't needed in most cases.\n\n"
                                          "Refer to the wiki for instructions.",

                                          "Use a custom path for the\n"
                                          "Luma3DS payload.\n\n"
                                          "Refer to the wiki for instructions.",

                                          "Enable overriding the region and\n"
                                          "language configuration and the usage\n"
                                          "of patched code binaries and\n"
                                          "custom RomFS for specific games.\n\n"
                                          "Also makes certain DLCs for\n"
                                          "out-of-region games work.\n\n"
                                          "Enabling this requires the\n"
                                          "archive patch to be applied.\n\n"
                                          "Refer to the wiki for instructions.",

                                          "Enable showing the current NAND/FIRM:\n\n"
                                          "\t* Sys  = SysNAND\n"
                                          "\t* Emu  = EmuNAND 1\n"
                                          "\t* EmuX = EmuNAND X\n"
                                          "\t* SysE = SysNAND with EmuNAND 1 FIRM\n"
                                          "\t* SyEX = SysNAND with EmuNAND X FIRM\n"
                                          "\t* EmuS = EmuNAND 1 with SysNAND FIRM\n"
                                          "\t* EmXS = EmuNAND X with SysNAND FIRM\n\n"
                                          "or an user-defined custom string in\n"
                                          "System Settings.\n\n"
                                          "Refer to the wiki for instructions.",

                                          "Enable showing the GBA boot screen\n"
                                          "when booting GBA games.",

                                          "Disable SVC, service, archive and ARM9\n"
                                          "exheader access checks.\n\n"
                                          "The service and archive patches\n"
                                          "don't work on New 3DS FIRMs between\n"
                                          "9.3 and 10.4.\n\n"
                                          "Only change this if you know what you\n"
                                          "are doing!",
                                          
                                          "Hides the input when entering pin\n"
                                          "to unlock the 3DS"
                                       };

    struct multiOption {
        u32 posXs[4];
        u32 posY;
        u32 enabled;
        bool visible;
    } multiOptions[] = {
        { .posXs = {19, 24, 29, 34}, .visible = isSdMode },
        { .posXs = {21, 26, 31, 36}, .visible = true },
        { .posXs = {12, 22, 31, 0}, .visible = true  },
        { .posXs = {14, 19, 24, 29}, .visible = true },
        { .posXs = {17, 26, 32, 44}, .visible = ISN3DS },
        { .posXs = {19, 30, 42, 0}, .visible = true  }
    };

    struct singleOption {
        u32 posY;
        bool enabled;
        bool visible;
    } singleOptions[] = {
        { .visible = isSdMode },
        { .visible = isSdMode },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true }
    };

    //Calculate the amount of the various kinds of options and pre-select the first single one
    u32 multiOptionsAmount = sizeof(multiOptions) / sizeof(struct multiOption),
        singleOptionsAmount = sizeof(singleOptions) / sizeof(struct singleOption),
        totalIndexes = multiOptionsAmount + singleOptionsAmount - 1,
        selectedOption,
        singleSelected;
    bool isMultiOption = false;

    //Parse the existing options
    for(u32 i = 0; i < multiOptionsAmount; i++)
        multiOptions[i].enabled = MULTICONFIG(i);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        singleOptions[i].enabled = CONFIG(i);

    initScreens();

    drawString(CONFIG_TITLE, true, 10, 10, COLOR_TITLE);
    drawString("Press A to select, START to save", true, 10, 10 + SPACING_Y, COLOR_TITLE);

    //Character to display a selected option
    char selected = 'x';

    u32 endPos = 10 + 2 * SPACING_Y;

    //Display all the multiple choice options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        if(!multiOptions[i].visible) continue;

        multiOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(multiOptionsText[i], true, 10, multiOptions[i].posY, COLOR_WHITE);
        drawCharacter(selected, true, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE);
    }

    endPos += SPACING_Y / 2;

    //Display all the normal options in white except for the first one
    for(u32 i = 0, color = COLOR_RED; i < singleOptionsAmount; i++)
    {
        if(!singleOptions[i].visible) continue;

        singleOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(singleOptionsText[i], true, 10, singleOptions[i].posY, color);
        if(singleOptions[i].enabled) drawCharacter(selected, true, 10 + SPACING_X, singleOptions[i].posY, color);

        if(color == COLOR_RED)
        {
            singleSelected = i;
            selectedOption = i + multiOptionsAmount;
            color = COLOR_WHITE;
        }
    }

    drawString(optionsDescription[selectedOption], false, 10, 10, COLOR_WHITE);

    //Boring configuration menu
    while(true)
    {
        u32 pressed;
        do
        {
            pressed = waitInput(true);
        }
        while(!(pressed & MENU_BUTTONS));

        if(pressed == BUTTON_START) break;

        if(pressed != BUTTON_A)
        {
            //Remember the previously selected option
            u32 oldSelectedOption = selectedOption;

            while(true)
            {
                switch(pressed)
                {
                    case BUTTON_UP:
                        selectedOption = !selectedOption ? totalIndexes : selectedOption - 1;
                        break;
                    case BUTTON_DOWN:
                        selectedOption = selectedOption == totalIndexes ? 0 : selectedOption + 1;
                        break;
                    case BUTTON_LEFT:
                        pressed = BUTTON_DOWN;
                        selectedOption = 0;
                        break;
                    case BUTTON_RIGHT:
                        pressed = BUTTON_UP;
                        selectedOption = totalIndexes;
                        break;
                    default:
                        break;
                }

                if(selectedOption < multiOptionsAmount)
                {
                    if(!multiOptions[selectedOption].visible) continue;

                    isMultiOption = true;
                    break;
                }
                else
                {
                    singleSelected = selectedOption - multiOptionsAmount;

                    if(!singleOptions[singleSelected].visible) continue;

                    isMultiOption = false;
                    break;
                }
            }

            if(selectedOption == oldSelectedOption) continue;

            //The user moved to a different option, print the old option in white and the new one in red. Only print 'x's if necessary
            if(oldSelectedOption < multiOptionsAmount)
            {
                drawString(multiOptionsText[oldSelectedOption], true, 10, multiOptions[oldSelectedOption].posY, COLOR_WHITE);
                drawCharacter(selected, true, 10 + multiOptions[oldSelectedOption].posXs[multiOptions[oldSelectedOption].enabled] * SPACING_X, multiOptions[oldSelectedOption].posY, COLOR_WHITE);
            }
            else
            {
                u32 singleOldSelected = oldSelectedOption - multiOptionsAmount;
                drawString(singleOptionsText[singleOldSelected], true, 10, singleOptions[singleOldSelected].posY, COLOR_WHITE);
                if(singleOptions[singleOldSelected].enabled) drawCharacter(selected, true, 10 + SPACING_X, singleOptions[singleOldSelected].posY, COLOR_WHITE);
            }

            if(isMultiOption) drawString(multiOptionsText[selectedOption], true, 10, multiOptions[selectedOption].posY, COLOR_RED);
            else drawString(singleOptionsText[singleSelected], true, 10, singleOptions[singleSelected].posY, COLOR_RED);

            drawString(optionsDescription[oldSelectedOption], false, 10, 10, COLOR_BLACK);
            drawString(optionsDescription[selectedOption], false, 10, 10, COLOR_WHITE);
        }
        else
        {
            //The selected option's status changed, print the 'x's accordingly
            if(isMultiOption)
            {
                u32 oldEnabled = multiOptions[selectedOption].enabled;
                drawCharacter(selected, true, 10 + multiOptions[selectedOption].posXs[oldEnabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_BLACK);
                multiOptions[selectedOption].enabled = (oldEnabled == 3 || !multiOptions[selectedOption].posXs[oldEnabled + 1]) ? 0 : oldEnabled + 1;

                if(selectedOption == BRIGHTNESS) updateBrightness(multiOptions[BRIGHTNESS].enabled);
            }
            else
            {
                bool oldEnabled = singleOptions[singleSelected].enabled;
                singleOptions[singleSelected].enabled = !oldEnabled;
                if(oldEnabled) drawCharacter(selected, true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_BLACK);
            }
        }

        //In any case, if the current option is enabled (or a multiple choice option is selected) we must display a red 'x'
        if(isMultiOption) drawCharacter(selected, true, 10 + multiOptions[selectedOption].posXs[multiOptions[selectedOption].enabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_RED);
        else if(singleOptions[singleSelected].enabled) drawCharacter(selected, true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_RED);
    }

    //Preserve the last-used boot options (first 9 bits)
    configData.config &= 0xFF;

    //Parse and write the new configuration
    for(u32 i = 0; i < multiOptionsAmount; i++)
        configData.config |= multiOptions[i].enabled << (i * 2 + 8);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        configData.config |= (singleOptions[i].enabled ? 1 : 0) << (i + 20);

    u32 newPinMode = MULTICONFIG(PIN);

    if(newPinMode != 0) newPin(oldPinStatus && newPinMode == oldPinMode, newPinMode);
    else if(oldPinStatus) fileDelete(PIN_FILE);

    while(HID_PAD & PIN_BUTTONS);
    wait(2000ULL);
}
