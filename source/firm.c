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

#include "firm.h"
#include "config.h"
#include "utils.h"
#include "fs.h"
#include "patches.h"
#include "memory.h"
#include "cache.h"
#include "emunand.h"
#include "crypto.h"
#include "draw.h"
#include "screen.h"
#include "buttons.h"
#include "pin.h"
#include "../build/injector.h"

extern u16 launchedFirmTIDLow[8]; //defined in start.s

static firmHeader *const firm = (firmHeader *)0x24000000;
static const firmSectionHeader *section;

u32 config,
    emuOffset;

bool isN3DS, isDevUnit, isFirmlaunch;

FirmwareSource firmSource;

void main(void)
{
    bool isA9lh;

    u32 newConfig,
        emuHeader;

    FirmwareType firmType;
    FirmwareSource nandType;
    ConfigurationStatus needConfig;

    //Detect the console being used
    isN3DS = PDN_MPCORE_CFG == 7;

    //Detect dev units
    isDevUnit = CFG_UNITINFO != 0;

    //Mount filesystems. CTRNAND will be mounted only if/when needed
    mountFs();

    const char configPath[] = "/luma/config.bin";

    //Attempt to read the configuration file
    needConfig = fileRead(&config, configPath) ? MODIFY_CONFIGURATION : CREATE_CONFIGURATION;

    //Determine if this is a firmlaunch boot
    if(launchedFirmTIDLow[5] != 0)
    {
        if(needConfig == CREATE_CONFIGURATION) mcuReboot();

        isFirmlaunch = true;

        //'0' = NATIVE_FIRM, '1' = TWL_FIRM, '2' = AGB_FIRM
        firmType = launchedFirmTIDLow[7] == u'3' ? SAFE_FIRM : (FirmwareType)(launchedFirmTIDLow[5] - u'0');

        nandType = (FirmwareSource)BOOTCONFIG(0, 3);
        firmSource = (FirmwareSource)BOOTCONFIG(2, 1);
        isA9lh = BOOTCONFIG(3, 1) != 0;
    }
    else
    {
        //Get pressed buttons
        u32 pressed = HID_PAD;

        isFirmlaunch = false;
        firmType = NATIVE_FIRM;

        //Determine if booting with A9LH
        isA9lh = !PDN_SPI_CNT;

        //Determine if the user chose to use the SysNAND FIRM as default for a R boot
        bool useSysAsDefault = isA9lh ? CONFIG(1) : false;

        newConfig = (u32)isA9lh << 3;

        //If it's a MCU reboot, try to force boot options
        if(isA9lh && CFG_BOOTENV)
        {
            //Always force a sysNAND boot when quitting AGB_FIRM
            if(CFG_BOOTENV == 7)
            {
                nandType = FIRMWARE_SYSNAND;
                firmSource = useSysAsDefault ? FIRMWARE_SYSNAND : (FirmwareSource)BOOTCONFIG(2, 1);
                needConfig = DONT_CONFIGURE;

                //Flag to prevent multiple boot options-forcing
                newConfig |= 1 << 4;
            }

            /* Else, force the last used boot options unless a button is pressed
               or the no-forcing flag is set */
            else if(!pressed && !BOOTCONFIG(4, 1))
            {
                nandType = (FirmwareSource)BOOTCONFIG(0, 3);
                firmSource = (FirmwareSource)BOOTCONFIG(2, 1);
                needConfig = DONT_CONFIGURE;
            }
        }

        //Boot options aren't being forced
        if(needConfig != DONT_CONFIGURE)
        {
            PINData pin;

            bool pinExists = CONFIG(7) && readPin(&pin);

            //If we get here we should check the PIN (if it exists) in all cases
            if(pinExists) verifyPin(&pin);

            //If no configuration file exists or SELECT is held, load configuration menu
            bool shouldLoadConfigurationMenu = needConfig == CREATE_CONFIGURATION || ((pressed & BUTTON_SELECT) && !(pressed & BUTTON_L1));

            if(shouldLoadConfigurationMenu)
            {
                configureCFW(configPath);

                if(!pinExists && CONFIG(7)) newPin();

                chrono(2);

                //Update pressed buttons
                pressed = HID_PAD;
            }

            if(isA9lh && !CFG_BOOTENV && pressed == SAFE_MODE)
            {
                nandType = FIRMWARE_SYSNAND;
                firmSource = FIRMWARE_SYSNAND;
            }
            else
            {
                if(CONFIG(6) && loadSplash()) pressed = HID_PAD;

                /* If L and R/A/Select or one of the single payload buttons are pressed,
                   chainload an external payload (the PIN, if any, has been verified)*/
                bool shouldLoadPayload = (pressed & SINGLE_PAYLOAD_BUTTONS) || ((pressed & BUTTON_L1) && (pressed & L_PAYLOAD_BUTTONS));

                if(shouldLoadPayload) loadPayload(pressed);

                if(!CONFIG(6)) loadSplash();

                //If R is pressed, boot the non-updated NAND with the FIRM of the opposite one
                if(pressed & BUTTON_R1)
                {
                    nandType = (useSysAsDefault) ? FIRMWARE_EMUNAND : FIRMWARE_SYSNAND;
                    firmSource = (useSysAsDefault) ? FIRMWARE_SYSNAND : FIRMWARE_EMUNAND;
                }

                /* Else, boot the NAND the user set to autoboot or the opposite one, depending on L,
                   with their own FIRM */
                else
                {
                    nandType = (CONFIG(0) != !(pressed & BUTTON_L1)) ? FIRMWARE_EMUNAND : FIRMWARE_SYSNAND;
                    firmSource = nandType;
                }

                /* If we're booting emuNAND the second emuNAND is set as default and B isn't pressed,
                   or vice-versa, boot the second emuNAND */
                if(nandType != FIRMWARE_SYSNAND && (CONFIG(2) == !(pressed & BUTTON_B))) nandType = FIRMWARE_EMUNAND2;
            }
        }
    }

    //If we need to boot emuNAND, make sure it exists
    if(nandType != FIRMWARE_SYSNAND)
    {
        locateEmuNAND(&emuOffset, &emuHeader, &nandType);
        if(nandType == FIRMWARE_SYSNAND) firmSource = FIRMWARE_SYSNAND;
    }

    //Same if we're using emuNAND as the FIRM source
    else if(firmSource != FIRMWARE_SYSNAND)
        locateEmuNAND(&emuOffset, &emuHeader, &firmSource);

    if(!isFirmlaunch)
    {
        newConfig |= (u32)nandType | ((u32)firmSource << 2);

        /* If the boot configuration is different from previously, overwrite it.
           Just the no-forcing flag being set is not enough */
        if((newConfig & 0x2F) != (config & 0x3F))
        {
            //Preserve user settings (last 26 bits)
            newConfig |= config & 0xFFFFFFC0;

            if(!fileWrite(&newConfig, configPath, 4))
                error("Error writing the configuration file");
        }
    }

    u32 firmVersion = loadFirm(firmType);

    switch(firmType)
    {
        case NATIVE_FIRM:
            patchNativeFirm(firmVersion, nandType, emuHeader, isA9lh);
            break;
        case SAFE_FIRM:
            patchSafeFirm();
            break;
        default:
            //Skip patching on unsupported O3DS AGB/TWL FIRMs
            if(isN3DS || firmVersion >= (firmType == TWL_FIRM ? 0x16 : 0xB)) patchLegacyFirm(firmType);
            break;
    }

    launchFirm(firmType);
}

static inline u32 loadFirm(FirmwareType firmType)
{
    section = firm->section;

    //Load FIRM from CTRNAND, unless it's an O3DS and we're loading a pre-5.0 NATIVE FIRM
    u32 firmVersion = firmRead(firm, (u32)firmType);

    if(!isN3DS && firmType == NATIVE_FIRM && firmVersion < 0x25)
    {
        if(!fileRead(firm, "/luma/firmware.bin") || (((u32)section[2].address >> 8) & 0xFF) != 0x68)
            error("An old unsupported FIRM has been detected.\nCopy firmware.bin in /luma to boot");

        //No assumption regarding FIRM version
        firmVersion = 0xffffffff;
    }
    else decryptExeFs((u8 *)firm);

    return firmVersion;
}

static inline void patchNativeFirm(u32 firmVersion, FirmwareSource nandType, u32 emuHeader, bool isA9lh)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset;
    u8 *arm11Section1 = (u8 *)firm + section[1].offset;

    if(isN3DS)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section);
        firm->arm9Entry = (u8 *)0x801B01C;
    }

    //Sets the 7.x NCCH KeyX and the 6.x gamecard save data KeyY on >= 6.0 O3DS FIRMs, if not using A9LH
    else if(!isA9lh && firmVersion >= 0x29) setRSAMod0DerivedKeys();

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9(arm9Section + 0x15000, section[2].size - 0x15000, &process9Size, &process9MemAddr);

    //Apply signature patches
    patchSignatureChecks(process9Offset, process9Size);

    //Apply emuNAND patches
    if(nandType != FIRMWARE_SYSNAND)
    {
        u32 branchAdditive = (u32)firm + section[2].offset - (u32)section[2].address;
        patchEmuNAND(arm9Section, section[2].size, process9Offset, process9Size, emuOffset, emuHeader, branchAdditive);
    }

    //Apply FIRM0/1 writes patches on sysNAND to protect A9LH
    else if(isA9lh) patchFirmWrites(process9Offset, process9Size);

    //Apply firmlaunch patches
    patchFirmlaunches(process9Offset, process9Size, process9MemAddr);

    //11.0 FIRM patches
    if(firmVersion >= (isN3DS ? 0x21 : 0x52))
    {
        //Apply anti-anti-DG patches
        patchTitleInstallMinVersionCheck(process9Offset, process9Size);

        //Restore svcBackdoor
        reimplementSvcBackdoor(arm11Section1, section[1].size);
    }

    implementSvcGetCFWInfo(arm11Section1, section[1].size);
}

static inline void patchLegacyFirm(FirmwareType firmType)
{
    if(isN3DS)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader((u8 *)firm + section[3].offset);
        firm->arm9Entry = (u8 *)0x801301C;
    }

    applyLegacyFirmPatches((u8 *)firm, firmType);

    if(firmType == TWL_FIRM)
        patchTwlBg((u8 *)firm + section[1].offset);
}

static inline void patchSafeFirm(void)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset;

    if(isN3DS)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section);
        firm->arm9Entry = (u8 *)0x801B01C;

        patchFirmWrites(arm9Section, section[2].size);
    }
    else patchFirmWriteSafe(arm9Section, section[2].size);
}

static inline void copySection0AndInjectSystemModules(FirmwareType firmType)
{
    u8 *arm11Section0 = (u8 *)firm + section[0].offset;

    struct
    {
        u32 size;
        char name[8];
        const u8 *addr;
    } modules[5] = {{0}};

    u8 *pos = arm11Section0, *end = pos + section[0].size;
    u32 n = 0;

    u32 loaderIndex = 0;
    
    while(pos < end)
    {
        modules[n].addr = pos;
        modules[n].size = *(u32 *)(pos + 0x104) * 0x200;
        
        memcpy(modules[n].name, pos + 0x200, 8);
        pos += modules[n].size;

        if(firmType == NATIVE_FIRM && memcmp(modules[n].name, "loader", 7) == 0) loaderIndex = n;
        n++;
    }

    if(firmType == NATIVE_FIRM)
    {
        modules[loaderIndex].size = injector_size;
        modules[loaderIndex].addr = injector;
    }

    pos = section[0].address;
    for(u32 i = 0; i < n; i++)
    {
        memcpy(pos, modules[i].addr, modules[i].size);
        pos += modules[i].size;
    }

}

static inline void launchFirm(FirmwareType firmType)
{
    //If we're booting NATIVE_FIRM, section0 needs to be copied separately to inject 3ds_injector
    u32 sectionNum;
    if(firmType != SAFE_FIRM)
    {
        copySection0AndInjectSystemModules(firmType);
        sectionNum = 1;
    }
    else sectionNum = 0;

    //Copy FIRM sections to respective memory locations
    for(; sectionNum < 4 && section[sectionNum].size; sectionNum++)
        memcpy(section[sectionNum].address, (u8 *)firm + section[sectionNum].offset, section[sectionNum].size);

    //Determine the ARM11 entry to use
    vu32 *arm11;
    if(isFirmlaunch) arm11 = (u32 *)0x1FFFFFFC;
    else
    {
        deinitScreens();
        arm11 = (u32 *)0x1FFFFFF8;
    }

    //Set ARM11 kernel entrypoint
    *arm11 = (u32)firm->arm11Entry;

    flushEntireDCache(); //Ensure that all memory transfers have completed and that the data cache has been flushed 
    flushEntireICache();

    //Final jump to ARM9 kernel
    ((void (*)())firm->arm9Entry)();
}