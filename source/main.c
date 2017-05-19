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
#include "emunand.h"
#include "fs.h"
#include "firm.h"
#include "utils.h"
#include "exceptions.h"
#include "draw.h"
#include "strings.h"
#include "buttons.h"
#include "pin.h"
#include "crypto.h"
#include "fmt.h"
#include "memory.h"
#include "i2c.h"

extern CfgData configData;
extern ConfigurationStatus needConfig;
extern FirmwareSource firmSource;

u16 launchedFirmTidLow[8];
u16 launchedPath[41];

void main(int argc, char **argv)
{
    bool isSafeMode = false,
         isNoForceFlagSet = false;
    u32 emuHeader;
    FirmwareType firmType;
    FirmwareSource nandType;

    switch(argc)
    {
        case 0:
            error("Unsupported launcher.");
            break;

        case 1: //Normal boot
        {
            u32 i;
            for(i = 0; i < 40 && launchedPath[i] != 0; i++) //Copy and convert the path to utf16
                launchedPath[2 * i] = argv[0][i];
            for(; i < 41; i++)
                launchedPath[i] = 0;

            for(u32 i = 0; i < 8; i++)
                launchedFirmTidLow[i] = 0;
            break;
        }

        case 2: //Firmlaunch
        {
            
            i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
            while(true);
            u32 i;
            u16 *p = (u16 *)argv[0];
            for(i = 0; i < 40 && launchedPath[i] != 0; i++)
                launchedPath[i] = p[i];
            for(; i < 41; i++)
                launchedPath[i] = 0;

            memcpy(launchedFirmTidLow, (u16 *)argv[1], 16);
            break;
        }
    }

    char mountPoint[5] = {0};
    for(u32 i = 0; i < 4 && argv[0][i] != ':'; i++)
        mountPoint[i] = argv[0][i];

    //Mount SD or CTRNAND
    bool isSdMode;

    if(memcmp(mountPoint, "sdmc", 4) == 0)
    {
        if(!mountFs(true, false)) error("Failed to mount SD.");
        isSdMode = true;
    }
    else if(memcmp(mountPoint, "nand", 4) == 0)
    {
        firmSource = FIRMWARE_SYSNAND;
        if(!mountFs(false, true)) error("Failed to mount SD and CTRNAND.");
        isSdMode = false;
    }
    else
        error("Launched from an unsupported location");

    //Attempt to read the configuration file
    needConfig = readConfig() ? MODIFY_CONFIGURATION : CREATE_CONFIGURATION;

    //Determine if this is a firmlaunch boot
    if(ISFIRMLAUNCH)
    {
        if(needConfig == CREATE_CONFIGURATION) mcuPowerOff();

        switch(launchedFirmTidLow[7])
        {
            case u'2':
                firmType = (FirmwareType)(launchedFirmTidLow[5] - u'0');
                break;
            case u'3':
                firmType = SAFE_FIRM;
                break;
            case u'1':
                firmType = SYSUPDATER_FIRM;
                break;
        }

        nandType = (FirmwareSource)BOOTCFG_NAND;
        firmSource = (FirmwareSource)BOOTCFG_FIRM;

        goto boot;
    }

    detectAndProcessExceptionDumps();
    installArm9Handlers();

    firmType = NATIVE_FIRM;

    //Get pressed buttons
    u32 pressed = HID_PAD;

    //If it's a MCU reboot, try to force boot options
    if(CFG_BOOTENV && needConfig != CREATE_CONFIGURATION)
    {

        //Always force a SysNAND boot when quitting AGB_FIRM
        if(CFG_BOOTENV == 7)
        {
            nandType = FIRMWARE_SYSNAND;
            firmSource = (BOOTCFG_NAND != 0) == (BOOTCFG_FIRM != 0) ? FIRMWARE_SYSNAND : (FirmwareSource)BOOTCFG_FIRM;

            //Prevent multiple boot options-forcing
            isNoForceFlagSet = true;

            goto boot;
        }

        /* Else, force the last used boot options unless a button is pressed
           or the no-forcing flag is set */
        if(!pressed && !BOOTCFG_NOFORCEFLAG)
        {
            nandType = (FirmwareSource)BOOTCFG_NAND;
            firmSource = (FirmwareSource)BOOTCFG_FIRM;

            goto boot;
        }
    }

    u32 pinMode = MULTICONFIG(PIN);
    bool pinExists = pinMode != 0 && verifyPin(pinMode);

    //If no configuration file exists or SELECT is held, load configuration menu
    bool shouldLoadConfigMenu = needConfig == CREATE_CONFIGURATION || ((pressed & (BUTTON_SELECT | BUTTON_L1)) == BUTTON_SELECT);

    if(shouldLoadConfigMenu)
    {
        configMenu(isSdMode, pinExists, pinMode);

        //Update pressed buttons
        pressed = HID_PAD;
    }

    if(!CFG_BOOTENV && pressed == SAFE_MODE)
    {
        nandType = FIRMWARE_SYSNAND;
        firmSource = FIRMWARE_SYSNAND;

        isSafeMode = true;

        //If the PIN has been verified, wait to make it easier to press the SAFE_MODE combo
        if(pinExists && !shouldLoadConfigMenu)
        {
            while(HID_PAD & PIN_BUTTONS);
            wait(2000ULL);
        }

        goto boot;
    }

    u32 splashMode = MULTICONFIG(SPLASH);

    if(splashMode == 1 && loadSplash()) pressed = HID_PAD;

    if((pressed & (BUTTON_START | BUTTON_L1)) == BUTTON_START)
    {
        payloadMenu();
        pressed = HID_PAD;
    }
    else if(((pressed & SINGLE_PAYLOAD_BUTTONS) && !(pressed & (BUTTON_L1 | BUTTON_R1 | BUTTON_A))) ||
            ((pressed & L_PAYLOAD_BUTTONS) && (pressed & BUTTON_L1))) loadPayload(pressed, NULL);

    if(splashMode == 2) loadSplash();

    //If booting from CTRNAND, always use SysNAND
    if(!isSdMode) nandType = FIRMWARE_SYSNAND;

    //If R is pressed, boot the non-updated NAND with the FIRM of the opposite one
    else if(pressed & BUTTON_R1)
    {
        if(CONFIG(USEEMUFIRM))
        {
            nandType = FIRMWARE_SYSNAND;
            firmSource = FIRMWARE_EMUNAND;
        }
        else
        {
            nandType = FIRMWARE_EMUNAND;
            firmSource = FIRMWARE_SYSNAND;
        }
    }

    /* Else, boot the NAND the user set to autoboot or the opposite one, depending on L,
       with their own FIRM */
    else firmSource = nandType = (CONFIG(AUTOBOOTEMU) == ((pressed & BUTTON_L1) == BUTTON_L1)) ? FIRMWARE_SYSNAND : FIRMWARE_EMUNAND;

    //If we're booting EmuNAND or using EmuNAND FIRM, determine which one from the directional pad buttons, or otherwise from the config
    if(nandType == FIRMWARE_EMUNAND || firmSource == FIRMWARE_EMUNAND)
    {
        FirmwareSource tempNand;
        switch(pressed & DPAD_BUTTONS)
        {
            case BUTTON_UP:
                tempNand = FIRMWARE_EMUNAND;
                break;
            case BUTTON_RIGHT:
                tempNand = FIRMWARE_EMUNAND2;
                break;
            case BUTTON_DOWN:
                tempNand = FIRMWARE_EMUNAND3;
                break;
            case BUTTON_LEFT:
                tempNand = FIRMWARE_EMUNAND4;
                break;
            default:
                tempNand = (FirmwareSource)(1 + MULTICONFIG(DEFAULTEMU));
                break;
        }

        if(nandType == FIRMWARE_EMUNAND) nandType = tempNand;
        else firmSource = tempNand;
    }

boot:

    //If we need to boot EmuNAND, make sure it exists
    if(nandType != FIRMWARE_SYSNAND)
    {
        locateEmuNand(&emuHeader, &nandType);
        if(nandType == FIRMWARE_SYSNAND) firmSource = FIRMWARE_SYSNAND;
    }

    //Same if we're using EmuNAND as the FIRM source
    else if(firmSource != FIRMWARE_SYSNAND)
        locateEmuNand(&emuHeader, &firmSource);

    if(!ISFIRMLAUNCH)
    {
        configData.config = (configData.config & 0xFFFFFF80) | ((u32)isNoForceFlagSet << 6) | ((u32)firmSource << 3) | (u32)nandType;
        writeConfig(false);
    }

    if(isSdMode && !mountFs(false, false)) error("Failed to mount CTRNAND.");

    bool loadFromStorage = CONFIG(LOADEXTFIRMSANDMODULES);
    u32 firmVersion = loadFirm(&firmType, firmSource, loadFromStorage, isSafeMode);

    bool doUnitinfoPatch = CONFIG(PATCHUNITINFO),
         enableExceptionHandlers = CONFIG(ENABLEEXCEPTIONHANDLERS);
    u32 res;
    switch(firmType)
    {
        case NATIVE_FIRM:
            res = patchNativeFirm(firmVersion, nandType, emuHeader, isSafeMode, doUnitinfoPatch, enableExceptionHandlers);
            break;
        case TWL_FIRM:
            res = patchTwlFirm(firmVersion, doUnitinfoPatch);
            break;
        case AGB_FIRM:
            res = patchAgbFirm(doUnitinfoPatch);
            break;
        case SAFE_FIRM:
        case SYSUPDATER_FIRM:
        case NATIVE_FIRM1X2X:
            res = patch1x2xNativeAndSafeFirm(enableExceptionHandlers);
            break;
    }

    if(res != 0)
    {
        char patchesError[43];
        sprintf(patchesError, "Failed to apply %u FIRM patch(es).", res);
        error(patchesError);
    }

    launchFirm(firmType, loadFromStorage);
}
