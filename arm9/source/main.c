/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2023 Aurora Wright, TuxSH
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
#include "emunand.h"
#include "fs.h"
#include "firm.h"
#include "utils.h"
#include "exceptions.h"
#include "draw.h"
#include "buttons.h"
#include "pin.h"
#include "crypto.h"
#include "memory.h"
#include "deliver_arg.h"
#include "screen.h"
#include "i2c.h"
#include "fmt.h"
#include "fatfs/sdmmc/sdmmc.h"

extern u8 __itcm_start__[], __itcm_lma__[], __itcm_bss_start__[], __itcm_end__[];

extern CfgData configData;
extern ConfigurationStatus needConfig;

bool isSdMode;
char launchedPathForFatfs[256];
u16 launchedPath[80+1];
BootType bootType;

u16 mcuFwVersion;
u8 mcuConsoleInfo[9];

void main(int argc, char **argv, u32 magicWord)
{
    bool isFirmProtEnabled = true,
         isSafeMode = false,
         needToInitSd = false,
         isNoForceFlagSet = false,
         isInvalidLoader = false,
         isNtrBoot = false;
    FirmwareType firmType = NATIVE_FIRM;
    FirmwareSource nandType = FIRMWARE_SYSNAND;
    u32 emunandIndex = 0;

    const vu8 *bootMediaStatus = (const vu8 *)0x1FFFE00C;
    const vu32 *bootPartitionsStatus = (const vu32 *)0x1FFFE010;
    u32 firmlaunchTidLow = 0;

    //Shell closed, no error booting NTRCARD, NAND paritions not even considered
    isNtrBoot = bootMediaStatus[3] == 2 && !bootMediaStatus[1] && !bootPartitionsStatus[0] && !bootPartitionsStatus[1];

    if((magicWord & 0xFFFF) == 0xBEEF && argc >= 1) //Normal (B9S) boot
    {
        bootType = isNtrBoot ? B9SNTR : B9S;
        strncpy(launchedPathForFatfs, argv[0], sizeof(launchedPathForFatfs) - 1);
        launchedPathForFatfs[sizeof(launchedPathForFatfs) - 1] = 0;

        u32 i;
        for(i = 0; i < sizeof(launchedPath)/2 - 1 && argv[0][i] != 0; i++) //Copy and convert the path to UTF-16
            launchedPath[i] = argv[0][i];
        launchedPath[i] = 0;
    }
    else if(magicWord == 0xBABE && argc == 2) //Firmlaunch
    {
        bootType = FIRMLAUNCH;

        u32 i;
        u16 *p = (u16 *)argv[0];
        for(i = 0; i < sizeof(launchedPath)/2 - 1 && p[i] != 0; i++)
        {
            launchedPath[i] = p[i];
            launchedPathForFatfs[i] = (u8)p[i]; // UCS-2 to ascii. Meh.
        }
        launchedPath[i] = 0;

        for(i = 0; i < 8; i++)
            firmlaunchTidLow = (argv[1][2 * i] > '9' ? argv[1][2 * i] - 'a' + 10 : argv[1][2 * i] - '0') | (firmlaunchTidLow << 4);
    }
    else if(magicWord == 0xB002) //FIRM/NTRCARD boot
    {
        if(isNtrBoot) bootType = NTR;
        else
        {
            const char *path;
            if(!((vu8 *)bootPartitionsStatus)[2])
            {
                bootType = FIRM0;
                path = "firm0:";
            }
            else
            {
                bootType = FIRM1;
                path = "firm1:";
            }

            for(u32 i = 0; i < 7; i++) //Copy and convert the path to UTF-16
                launchedPath[i] = path[i];
            strcpy(launchedPathForFatfs, path);
        }

        setupKeyslots();
    }
    else isInvalidLoader = true;

    // Set up the additional sections, overwrites argc
    memcpy(__itcm_start__, __itcm_lma__, __itcm_bss_start__ - __itcm_start__);
    memset(__itcm_bss_start__, 0, __itcm_end__ - __itcm_bss_start__);
    I2C_init();

    u8 mcuFwVerHi = I2C_readReg(I2C_DEV_MCU, 0) - 0x10;
    u8 mcuFwVerLo = I2C_readReg(I2C_DEV_MCU, 1);
    mcuFwVersion = ((u16)mcuFwVerHi << 16) | mcuFwVerLo;

    // Check if fw is older than factory. See https://www.3dbrew.org/wiki/MCU_Services#MCU_firmware_versions for a table
    if (mcuFwVerHi < 1) error("Unsupported MCU FW version %d.%d.", (int)mcuFwVerHi, (int)mcuFwVerLo);

    I2C_readRegBuf(I2C_DEV_MCU, 0x7F, mcuConsoleInfo, 9);

    if(isInvalidLoader) error("Launched using an unsupported loader.");

    installArm9Handlers();

    if(memcmp(launchedPath, u"sdmc", 8) == 0)
    {
        if(!mountSdCardPartition(true)) error("Failed to mount SD.");
        isSdMode = true;
    }
    else if(memcmp(launchedPath, u"nand", 8) == 0)
    {
        if(!remountCtrNandPartition(true)) error("Failed to mount CTRNAND.");
        isSdMode = false;
    }
    else if(bootType == NTR || memcmp(launchedPath, u"firm", 8) == 0)
    {
        if(mountSdCardPartition(true)) isSdMode = true;
        else if(remountCtrNandPartition(true)) isSdMode = false;
        else error("Failed to mount SD and CTRNAND.");

        if(bootType == NTR)
        {
            while(HID_PAD & NTRBOOT_BUTTONS);
            loadHomebrewFirm(0);
            mcuPowerOff();
        }
    }
    else
    {
        char mountPoint[5];

        u32 i;
        for(i = 0; i < 4 && launchedPath[i] != u':'; i++)
            mountPoint[i] = (char)launchedPath[i];
        mountPoint[i] = 0;

        error("Launched from an unsupported location: %s.", mountPoint);
    }

    detectAndProcessExceptionDumps();

    //Attempt to read the configuration file
    needConfig = readConfig() ? MODIFY_CONFIGURATION : CREATE_CONFIGURATION;

    //Determine if this is a firmlaunch boot
    if(bootType == FIRMLAUNCH)
    {
        if(needConfig == CREATE_CONFIGURATION) mcuPowerOff();

        switch(firmlaunchTidLow & 0xF)
        {
            case 2:
                firmType = (FirmwareType)((firmlaunchTidLow >> 8) & 0xF);
                break;
            case 3:
                firmType = SAFE_FIRM;
                break;
            case 1:
                firmType = SYSUPDATER_FIRM;
                break;
        }

        nandType = (FirmwareSource)BOOTCFG_NAND;
        emunandIndex = BOOTCFG_EMUINDEX;
        isFirmProtEnabled = !BOOTCFG_NTRCARDBOOT;

        goto boot;
    }

    firmType = NATIVE_FIRM;
    isFirmProtEnabled = bootType != NTR;

    //Get pressed buttons
    u32 pressed = HID_PAD;

    //If it's a MCU reboot, try to force boot options
    if(CFG_BOOTENV && needConfig != CREATE_CONFIGURATION)
    {
        u32 bootenv = CFG_BOOTENV;
        bool validTlnc = bootenv == 3 && hasValidTlncAutobootParams();

        if (validTlnc)
            needToInitSd = true;

        //Always force a SysNAND boot when quitting AGB_FIRM
        if(bootenv == 7)
        {
            nandType = FIRMWARE_SYSNAND;

            // Prevent multiple boot options-forcing
            // This bit is a bit weird. Basically, as you return to Home Menu by pressing either
            // the HOME or POWER button, nandType will be overridden to "SysNAND" (needed). But,
            // if you reboot again (e.g. via Rosalina menu), it'll use your default settings.
            if(nandType != BOOTCFG_NAND) isNoForceFlagSet = true;

            goto boot;
        }

        // Configure homebrew autoboot (if deliver arg ends up not containing anything)
        if (bootenv == 1 && MULTICONFIG(AUTOBOOTMODE) != 0)
            configureHomebrewAutoboot();

        /* Force the last used boot options if doing autolaunch from TWL, or unless a button is pressed
           or the no-forcing flag is set */
        if(validTlnc || !(pressed || BOOTCFG_NOFORCEFLAG))
        {
            nandType = (FirmwareSource)BOOTCFG_NAND;
            emunandIndex = BOOTCFG_EMUINDEX;

            goto boot;
        }
    }

    u32 pinMode = MULTICONFIG(PIN);
    bool shouldLoadConfigMenu = needConfig == CREATE_CONFIGURATION || ((pressed & (BUTTON_SELECT | BUTTON_L1)) == BUTTON_SELECT);
    bool pinExists = pinMode != 0 && verifyPin(pinMode);

    /* If the PIN has been verified, wait to make it easier to press the SAFE_MODE combo or the configuration menu button
       (if not already pressed, for the latter) */
    if(pinExists && !shouldLoadConfigMenu)
    {
        while(HID_PAD & PIN_BUTTONS);
        wait(2000ULL);

        //Update pressed buttons
        pressed = HID_PAD;
    }

    shouldLoadConfigMenu = needConfig == CREATE_CONFIGURATION || ((pressed & (BUTTON_SELECT | BUTTON_L1)) == BUTTON_SELECT);
    if(shouldLoadConfigMenu)
    {
        configMenu(pinExists, pinMode);

        //Update pressed buttons
        pressed = HID_PAD;
    }

    if(!CFG_BOOTENV && pressed == SAFE_MODE)
    {
        nandType = FIRMWARE_SYSNAND;

        isSafeMode = true;
        needToInitSd = true;

        goto boot;
    }

    u32 splashMode = MULTICONFIG(SPLASH);

    if(splashMode == 1 && loadSplash()) pressed = HID_PAD;

    bool autoBootEmu = CONFIG(AUTOBOOTEMU);

    if((pressed & (BUTTON_START | BUTTON_L1)) == BUTTON_START)
    {
        loadHomebrewFirm(0);
        pressed = HID_PAD;
    }
    else if((((pressed & SINGLE_PAYLOAD_BUTTONS) || (!autoBootEmu && (pressed & DPAD_BUTTONS))) && !(pressed & (BUTTON_L1 | BUTTON_R1))) ||
            (((pressed & L_PAYLOAD_BUTTONS) || (autoBootEmu && (pressed & DPAD_BUTTONS))) && (pressed & BUTTON_L1))) loadHomebrewFirm(pressed);

    if(splashMode == 2 && loadSplash()) pressed = HID_PAD;

    //Check SAFE_MODE combo again
    if(!CFG_BOOTENV && pressed == SAFE_MODE)
    {
        nandType = FIRMWARE_SYSNAND;

        isSafeMode = true;
        needToInitSd = true;

        goto boot;
    }

    // Set-up autoboot
    if (MULTICONFIG(AUTOBOOTMODE) != 0)
        configureHomebrewAutoboot();

    //If booting from CTRNAND, always use SysNAND
    if(!isSdMode) nandType = FIRMWARE_SYSNAND;
    else nandType = (autoBootEmu == ((pressed & BUTTON_L1) == BUTTON_L1)) ? FIRMWARE_SYSNAND : FIRMWARE_EMUNAND;

    //If we're booting EmuNAND or using EmuNAND FIRM, determine which one from the directional pad buttons, or otherwise from the config
    if(nandType == FIRMWARE_EMUNAND)
    {
        switch(pressed & DPAD_BUTTONS)
        {
            case BUTTON_UP:
                emunandIndex = 0;
                break;
            case BUTTON_RIGHT:
                emunandIndex = 1;
                break;
            case BUTTON_DOWN:
                emunandIndex = 2;
                break;
            case BUTTON_LEFT:
                emunandIndex = 3;
                break;
            default:
                emunandIndex = MULTICONFIG(DEFAULTEMU);
                break;
        }
    }

boot:

    //If we need to boot EmuNAND, make sure it exists
    if(nandType != FIRMWARE_SYSNAND)
    {
        locateEmuNand(&nandType, &emunandIndex, true);
        if(nandType == FIRMWARE_EMUNAND && (*(vu16 *)(SDMMC_BASE + REG_SDSTATUS0) & TMIO_STAT0_WRPROTECT) == 0) //Make sure the SD card isn't write protected
            error("The SD card is locked, EmuNAND can not be used.\nPlease turn the write protection switch off.");
    }

    ctrNandLocation = nandType; // for CTRNAND partition

    if(bootType != FIRMLAUNCH)
    {
        configData.bootConfig = ((bootType == NTR ? 1 : 0) << 4) | ((u32)isNoForceFlagSet << 3) | ((u32)emunandIndex << 1) | (u32)nandType;
        writeConfig(false);
    }

    bool loadFromStorage = CONFIG(LOADEXTFIRMSANDMODULES);
    u32 firmVersion = loadNintendoFirm(&firmType, nandType, loadFromStorage, isSafeMode);

    bool doUnitinfoPatch = CONFIG(PATCHUNITINFO);
    u32 res = 0;
    switch(firmType)
    {
        case NATIVE_FIRM:
        {
            res = patchNativeFirm(firmVersion, nandType, loadFromStorage, isFirmProtEnabled, needToInitSd, doUnitinfoPatch);
            break;
        }
        case TWL_FIRM:
            res = patchTwlFirm(firmVersion, loadFromStorage, doUnitinfoPatch);
            break;
        case AGB_FIRM:
            res = patchAgbFirm(loadFromStorage, doUnitinfoPatch);
            break;
        case SAFE_FIRM:
        case SYSUPDATER_FIRM:
        case NATIVE_FIRM1X2X:
            res = patch1x2xNativeAndSafeFirm();
            break;
        case NATIVE_PROTOTYPE:
            res = patchPrototypeNative(nandType);
            break;
    }

    if(res != 0) error("Failed to apply %u FIRM patch(es).", res);

    unmountPartitions();
    if(bootType != FIRMLAUNCH) deinitScreens();
    launchFirm(0, NULL);
}
