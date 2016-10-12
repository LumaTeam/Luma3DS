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

extern CfgData configData;
extern FirmwareSource firmSource;

void main(void)
{
    u32 configTemp,
        emuHeader;
    FirmwareType firmType;
    FirmwareSource nandType;
    ConfigurationStatus needConfig;

    //Mount SD or CTRNAND
    bool isSdMode;
    if(mountFs(true, false)) isSdMode = true;
    else
    {
        firmSource = FIRMWARE_SYSNAND;
        if(!mountFs(false, true)) error("Failed to mount SD and CTRNAND.");
        isSdMode = false;
    }

    //Attempt to read the configuration file
    needConfig = readConfig() ? MODIFY_CONFIGURATION : CREATE_CONFIGURATION;

    //Determine if this is a firmlaunch boot
    if(ISFIRMLAUNCH)
    {
        if(needConfig == CREATE_CONFIGURATION) mcuReboot();

        //'0' = NATIVE_FIRM, '1' = TWL_FIRM, '2' = AGB_FIRM
        firmType = launchedFirmTidLow[7] == u'3' ? SAFE_FIRM : (FirmwareType)(launchedFirmTidLow[5] - u'0');

        nandType = (FirmwareSource)BOOTCFG_NAND;
        firmSource = (FirmwareSource)BOOTCFG_FIRM;
    }
    else
    {
        if(ISA9LH)
        {
            detectAndProcessExceptionDumps();
            installArm9Handlers();
        }

        firmType = NATIVE_FIRM;

        //Get pressed buttons
        u32 pressed = HID_PAD;

        //Save old options and begin saving the new boot configuration
        configTemp = (configData.config & 0xFFFFFE00) | ((u32)ISA9LH << 6);

        //If it's a MCU reboot, try to force boot options
        if(ISA9LH && CFG_BOOTENV)
        {
            //Always force a SysNAND boot when quitting AGB_FIRM
            if(CFG_BOOTENV == 7)
            {
                nandType = FIRMWARE_SYSNAND;
                firmSource = CONFIG(USESYSFIRM) ? FIRMWARE_SYSNAND : (FirmwareSource)BOOTCFG_FIRM;
                needConfig = DONT_CONFIGURE;

                //Flag to prevent multiple boot options-forcing
                configTemp |= 1 << 7;
            }

            /* Else, force the last used boot options unless a button is pressed
               or the no-forcing flag is set */
            else if(needConfig != CREATE_CONFIGURATION && !pressed && !BOOTCFG_NOFORCEFLAG)
            {
                nandType = (FirmwareSource)BOOTCFG_NAND;
                firmSource = (FirmwareSource)BOOTCFG_FIRM;
                needConfig = DONT_CONFIGURE;
            }
        }

        //Boot options aren't being forced
        if(needConfig != DONT_CONFIGURE)
        {
            u32 pinMode = MULTICONFIG(PIN);
            bool pinExists = pinMode != 0 && verifyPin(pinMode);

            //If no configuration file exists or SELECT is held, load configuration menu
            bool shouldLoadConfigMenu = needConfig == CREATE_CONFIGURATION || ((pressed & BUTTON_SELECT) && !(pressed & BUTTON_L1));

            if(shouldLoadConfigMenu)
            {
                configMenu(isSdMode, pinExists, pinMode);

                //Update pressed buttons
                pressed = HID_PAD;
            }

            if(ISA9LH && !CFG_BOOTENV && pressed == SAFE_MODE)
            {
                nandType = FIRMWARE_SYSNAND;
                firmSource = FIRMWARE_SYSNAND;

                //Flag to tell loader to init SD
                configTemp |= 1 << 8;

                //If the PIN has been verified, wait to make it easier to press the SAFE_MODE combo
                if(pinExists && !shouldLoadConfigMenu)
                {
                    while(HID_PAD & PIN_BUTTONS);
                    chrono(2);
                }
            }
            else
            {
                u32 splashMode = MULTICONFIG(SPLASH);

                if(splashMode == 1 && loadSplash()) pressed = HID_PAD;

                /* If L and R/A/Select or one of the single payload buttons are pressed,
                   chainload an external payload */
                bool shouldLoadPayload = ((pressed & SINGLE_PAYLOAD_BUTTONS) && !(pressed & (BUTTON_L1 | BUTTON_R1 | BUTTON_A))) ||
                                         ((pressed & L_PAYLOAD_BUTTONS) && (pressed & BUTTON_L1));

                if(shouldLoadPayload) loadPayload(pressed);

                if(splashMode == 2) loadSplash();

                //If booting from CTRNAND, always use SysNAND
                if(!isSdMode) nandType = FIRMWARE_SYSNAND;

                //If R is pressed, boot the non-updated NAND with the FIRM of the opposite one
                else if(pressed & BUTTON_R1)
                {
                    //Determine if the user chose to use the SysNAND FIRM as default for a R boot
                    bool useSysAsDefault = ISA9LH ? CONFIG(USESYSFIRM) : false;

                    nandType = useSysAsDefault ? FIRMWARE_EMUNAND : FIRMWARE_SYSNAND;
                    firmSource = useSysAsDefault ? FIRMWARE_SYSNAND : FIRMWARE_EMUNAND;
                }

                /* Else, boot the NAND the user set to autoboot or the opposite one, depending on L,
                   with their own FIRM */
                else
                {
                    nandType = (CONFIG(AUTOBOOTSYS) != !(pressed & BUTTON_L1)) ? FIRMWARE_EMUNAND : FIRMWARE_SYSNAND;
                    firmSource = nandType;
                }

                //If we're booting EmuNAND or using EmuNAND FIRM, determine which one from the directional pad buttons, or otherwise from the config
                if(nandType == FIRMWARE_EMUNAND || firmSource == FIRMWARE_EMUNAND)
                {
                    FirmwareSource temp;
                    switch(pressed & EMUNAND_BUTTONS)
                    {
                        case BUTTON_UP:
                            temp = FIRMWARE_EMUNAND;
                            break;
                        case BUTTON_RIGHT:
                            temp = FIRMWARE_EMUNAND2;
                            break;
                        case BUTTON_DOWN:
                            temp = FIRMWARE_EMUNAND3;
                            break;
                        case BUTTON_LEFT:
                            temp = FIRMWARE_EMUNAND4;
                            break;
                        default:
                            temp = (FirmwareSource)(1 + MULTICONFIG(DEFAULTEMU));
                            break;
                    }

                    if(nandType == FIRMWARE_EMUNAND) nandType = temp;
                    else firmSource = temp;
                }
            }
        }
    }

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
        configTemp |= (u32)nandType | ((u32)firmSource << 3);
        writeConfig(needConfig, configTemp);
    }

    bool loadFromStorage = CONFIG(LOADEXTFIRMSANDMODULES);
    u32 firmVersion = loadFirm(&firmType, firmSource, loadFromStorage, isSdMode);

    u32 devMode = MULTICONFIG(DEVOPTIONS);

    u32 res;
    switch(firmType)
    {
        case NATIVE_FIRM:
            res = patchNativeFirm(firmVersion, nandType, emuHeader, isSdMode, devMode);
            break;
        case SAFE_FIRM:
        case NATIVE_FIRM1X2X:
            res = ISA9LH ? patch1x2xNativeAndSafeFirm(devMode) : 0;
            break;
        case TWL_FIRM:
            res = patchTwlFirm(firmVersion, devMode);
            break;
        case AGB_FIRM:
            res = patchAgbFirm(devMode);
            break;
    }

    if(res != 0)
    {
        char patchesError[] = "Failed to apply    FIRM patch(es).";
        hexItoa(res, patchesError + 16, 2, false);
        error(patchesError);
    }

    launchFirm(firmType, loadFromStorage);
}