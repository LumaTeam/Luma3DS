/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
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
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
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
ConfigurationStatus needConfig;
static CfgData oldConfig;

bool readConfig(void)
{
    bool ret;

    if(fileRead(&configData, CONFIG_FILE, sizeof(CfgData)) != sizeof(CfgData) ||
       memcmp(configData.magic, "CONF", 4) != 0 ||
       configData.formatVersionMajor != CONFIG_VERSIONMAJOR ||
       configData.formatVersionMinor != CONFIG_VERSIONMINOR)
    {
        memset(&configData, 0, sizeof(CfgData));

        ret = false;
    }
    else ret = true;

    oldConfig = configData;

    return ret;
}

void writeConfig(bool isConfigOptions)
{
    //If the configuration is different from previously, overwrite it.
    if(needConfig != CREATE_CONFIGURATION && ((isConfigOptions && configData.config == oldConfig.config && configData.multiConfig == oldConfig.multiConfig) ||
                                              (!isConfigOptions && configData.bootConfig == oldConfig.bootConfig))) return;

    if(needConfig == CREATE_CONFIGURATION)
    {
        memcpy(configData.magic, "CONF", 4);
        configData.formatVersionMajor = CONFIG_VERSIONMAJOR;
        configData.formatVersionMinor = CONFIG_VERSIONMINOR;

        needConfig = MODIFY_CONFIGURATION;
    }

    if(!fileWrite(&configData, CONFIG_FILE, sizeof(CfgData)))
        error("Error writing the configuration file");
}

void configMenu(bool oldPinStatus, u32 oldPinMode)
{


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
        singleOptionsAmount = sizeof(singleOptions) / sizeof(struct singleOption);

    //Parse the existing options
    for(u32 i = 0; i < multiOptionsAmount; i++)
        multiOptions[i].enabled = MULTICONFIG(i);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        singleOptions[i].enabled = CONFIG(i);

    //Character to display a selected option
    char selected = 'x';

    u32 endPos = 10 + 2 * SPACING_Y;

    //Display all the multiple choice options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        if(!multiOptions[i].visible) continue;

        multiOptions[i].posY = endPos + SPACING_Y;
        endPos = 0;
        drawCharacter(true, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE, selected);
    }

    endPos += SPACING_Y / 2;

    //Display all the normal options in white except for the first one


    //Parse and write the new configuration
    configData.multiConfig = 0;
    for(u32 i = 0; i < multiOptionsAmount; i++)
        configData.multiConfig |= multiOptions[i].enabled << (i * 2);

    configData.config = 0;
    for(u32 i = 0; i < singleOptionsAmount; i++)
        configData.config |= (singleOptions[i].enabled ? 1 : 0) << i;

    u32 newPinMode = MULTICONFIG(PIN);

    if(newPinMode != 0) newPin(oldPinStatus && newPinMode == oldPinMode, newPinMode);
    else if(oldPinStatus) fileDelete(PIN_FILE);

    writeConfig(true);

}