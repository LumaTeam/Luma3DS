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
#include "exceptions.h"
#include "patches.h"
#include "memory.h"
#include "strings.h"
#include "cache.h"
#include "emunand.h"
#include "crypto.h"
#include "screen.h"
#include "../build/bundled.h"

static Firm *firm = (Firm *)0x24000000;

u32 loadFirm(FirmwareType *firmType, FirmwareSource nandType, bool loadFromStorage)
{
    const char *firmwareFiles[] = {
        "firmware.bin",
        "firmware_twl.bin",
        "firmware_agb.bin",
        "firmware_safe.bin"
    },
               *cetkFiles[] = {
        "cetk",
        "cetk_twl",
        "cetk_agb",
        "cetk_safe"
    };

    //Load FIRM from CTRNAND
    u32 firmVersion = firmRead(firm, (u32)*firmType);

    if(firmVersion == 0xFFFFFFFF) error("Failed to get the CTRNAND FIRM.");

    bool mustLoadFromStorage = false;

    if(!ISN3DS && *firmType == NATIVE_FIRM)
    {
        if(firmVersion < 0x18)
        {
            //We can't boot < 3.x EmuNANDs
            if(nandType != FIRMWARE_SYSNAND) 
                error("An old unsupported EmuNAND has been detected.\nLuma3DS is unable to boot it.");

            if(BOOTCFG_SAFEMODE != 0) error("SAFE_MODE is not supported on 1.x/2.x FIRM.");

            *firmType = NATIVE_FIRM1X2X;
        }

        //We can't boot a 3.x/4.x NATIVE_FIRM, load one from SD/CTRNAND
        else if(firmVersion < 0x25) mustLoadFromStorage = true;
    }

    if(loadFromStorage || mustLoadFromStorage)
    {
        u32 firmSize = fileRead(firm, *firmType == NATIVE_FIRM1X2X ? firmwareFiles[0] : firmwareFiles[(u32)*firmType], 0x400000 + sizeof(Cxi) + 0x200);

        if(firmSize > 0)
        {
            if(firmSize <= sizeof(Cxi) + 0x200) error("The FIRM in /luma is not valid.");

            if(memcmp(firm, "FIRM", 4) != 0)
            {
                u8 cetk[0xA50];

                if(fileRead(cetk, *firmType == NATIVE_FIRM1X2X ? cetkFiles[0] : cetkFiles[(u32)*firmType], sizeof(cetk)) != sizeof(cetk) ||
                   !decryptNusFirm((Ticket *)(cetk + 0x140), (Cxi *)firm, firmSize))
                    error("The FIRM in /luma is encrypted or corrupted.");
            }

            //Check that the FIRM is right for the console from the ARM9 section address
            if((firm->section[3].offset != 0 ? firm->section[3].address : firm->section[2].address) != (ISN3DS ? (u8 *)0x8006000 : (u8 *)0x8006800))
                error("The FIRM in /luma is not for this console.");

            firmVersion = 0xFFFFFFFF;
        }
    }

    if(firmVersion != 0xFFFFFFFF)
    {
        if(mustLoadFromStorage) error("An old unsupported FIRM has been detected.\nCopy a firmware.bin in /luma to boot.");
        if(!decryptExeFs((Cxi *)firm)) error("The CTRNAND FIRM is corrupted.");
    }

    return firmVersion;
}

u32 patchNativeFirm(u32 firmVersion, FirmwareSource nandType, u32 emuHeader, bool isA9lhInstalled, u32 devMode)
{
    u8 *arm9Section = (u8 *)firm + firm->section[2].offset,
       *arm11Section1 = (u8 *)firm + firm->section[1].offset;

    if(ISN3DS)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip kernel9loader
        kernel9Loader((Arm9Bin *)arm9Section);
        firm->arm9Entry = (u8 *)0x801B01C;
    }

    //Sets the 7.x NCCH KeyX and the 6.x gamecard save data KeyY on >= 6.0 O3DS FIRMs, if not using A9LH or a dev unit
    else if(!ISA9LH && !ISFIRMLAUNCH && firmVersion >= 0x29 && !ISDEVUNIT) set6x7xKeys();

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9Info(arm9Section, firm->section[2].size, &process9Size, &process9MemAddr);

    //Find the Kernel11 SVC table and handler, exceptions page and free space locations
    u32 baseK11VA;
    u8 *freeK11Space;
    u32 *arm11SvcHandler,
        *arm11ExceptionsPage,
        *arm11SvcTable = getKernel11Info(arm11Section1, firm->section[1].size, &baseK11VA, &freeK11Space, &arm11SvcHandler, &arm11ExceptionsPage);

    u32 kernel9Size = (u32)(process9Offset - arm9Section) - sizeof(Cxi) - 0x200,
        ret = 0;

    //Apply signature patches
    ret += patchSignatureChecks(process9Offset, process9Size);

    //Apply EmuNAND patches
    if(nandType != FIRMWARE_SYSNAND) ret += patchEmuNand(arm9Section, kernel9Size, process9Offset, process9Size, emuHeader, firm->section[2].address);

    //Apply FIRM0/1 writes patches on sysNAND to protect A9LH
    else if(isA9lhInstalled) ret += patchFirmWrites(process9Offset, process9Size);

    //Apply firmlaunch patches
    ret += patchFirmlaunches(process9Offset, process9Size, process9MemAddr);

    //11.0 FIRM patches
    if(firmVersion >= (ISN3DS ? 0x21 : 0x52))
    {
        //Apply anti-anti-DG patches
        ret += patchTitleInstallMinVersionChecks(process9Offset, process9Size, firmVersion);

        //Restore svcBackdoor
        ret += reimplementSvcBackdoor(arm11Section1, arm11SvcTable, baseK11VA, &freeK11Space);
    }

    ret += implementSvcGetCFWInfo(arm11Section1, arm11SvcTable, baseK11VA, &freeK11Space);

    //Apply UNITINFO patch
    if(devMode == 2) ret += patchUnitInfoValueSet(arm9Section, kernel9Size);

    if(devMode != 0 && isA9lhInstalled)
    {
        //ARM11 exception handlers
        u32 codeSetOffset,
            stackAddress = getInfoForArm11ExceptionHandlers(arm11Section1, firm->section[1].size, &codeSetOffset);
        ret += installArm11Handlers(arm11ExceptionsPage, stackAddress, codeSetOffset);
        patchSvcBreak11(arm11Section1, arm11SvcTable);
        ret += patchKernel11Panic(arm11Section1, firm->section[1].size);

        //ARM9 exception handlers
        ret += patchArm9ExceptionHandlersInstall(arm9Section, kernel9Size);
        ret += patchSvcBreak9(arm9Section, kernel9Size, (u32)firm->section[2].address);
        ret += patchKernel9Panic(arm9Section, kernel9Size);
    }

    if(CONFIG(PATCHACCESS))
    {
        ret += patchArm11SvcAccessChecks(arm11SvcHandler, (u32 *)(arm11Section1 + firm->section[1].size));
        ret += patchK11ModuleChecks(arm11Section1, firm->section[1].size, &freeK11Space);
        ret += patchP9AccessChecks(process9Offset, process9Size);
    }

    return ret;
}

u32 patchTwlFirm(u32 firmVersion, u32 devMode)
{
    u8 *arm9Section = (u8 *)firm + firm->section[3].offset;
    
    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip kernel9loader
    if(ISN3DS)
    {
        kernel9Loader((Arm9Bin *)arm9Section);
        firm->arm9Entry = (u8 *)0x801301C;
    }

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9Info(arm9Section, firm->section[3].size, &process9Size, &process9MemAddr);

    u32 kernel9Size = (u32)(process9Offset - arm9Section) - sizeof(Cxi) - 0x200,
        ret = 0;

    ret += patchLgySignatureChecks(process9Offset, process9Size);
    ret += patchTwlInvalidSignatureChecks(process9Offset, process9Size);
    ret += patchTwlNintendoLogoChecks(process9Offset, process9Size);
    ret += patchTwlWhitelistChecks(process9Offset, process9Size);
    if(ISN3DS || firmVersion > 0x11) ret += patchTwlFlashcartChecks(process9Offset, process9Size, firmVersion);
    else if(!ISN3DS && firmVersion == 0x11) ret += patchOldTwlFlashcartChecks(process9Offset, process9Size);
    ret += patchTwlShaHashChecks(process9Offset, process9Size);

    //Apply UNITINFO patch
    if(devMode == 2) ret += patchUnitInfoValueSet(arm9Section, kernel9Size);

    return ret;
}

u32 patchAgbFirm(u32 devMode)
{
    u8 *arm9Section = (u8 *)firm + firm->section[3].offset;
    
    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip kernel9loader
    if(ISN3DS)
    {
        kernel9Loader((Arm9Bin *)arm9Section);
        firm->arm9Entry = (u8 *)0x801301C;
    }

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9Info(arm9Section, firm->section[3].size, &process9Size, &process9MemAddr);

    u32 kernel9Size = (u32)(process9Offset - arm9Section) - sizeof(Cxi) - 0x200,
        ret = 0;

    ret += patchLgySignatureChecks(process9Offset, process9Size);
    if(CONFIG(SHOWGBABOOT)) ret += patchAgbBootSplash(process9Offset, process9Size);

    //Apply UNITINFO patch
    if(devMode == 2) ret += patchUnitInfoValueSet(arm9Section, kernel9Size);

    return ret;
}

u32 patch1x2xNativeAndSafeFirm(u32 devMode)
{
    u8 *arm9Section = (u8 *)firm + firm->section[2].offset;

    if(ISN3DS)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip kernel9loader
        kernel9Loader((Arm9Bin *)arm9Section);
        firm->arm9Entry = (u8 *)0x801B01C;
    }

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9Info(arm9Section, firm->section[2].size, &process9Size, &process9MemAddr);

    u32 kernel9Size = (u32)(process9Offset - arm9Section) - sizeof(Cxi) - 0x200,
        ret = 0;

    ret += ISN3DS ? patchFirmWrites(process9Offset, process9Size) : patchOldFirmWrites(process9Offset, process9Size);

    if(devMode != 0)
    {
        //ARM9 exception handlers
        ret += patchArm9ExceptionHandlersInstall(arm9Section, kernel9Size);
        ret += patchSvcBreak9(arm9Section, kernel9Size, (u32)firm->section[2].address);
    }

    return ret;
}

static inline void copySection0AndInjectSystemModules(FirmwareType firmType, bool loadFromStorage)
{
    u32 maxModuleSize = firmType == NATIVE_FIRM ? 0x80000 : 0x600000,
        srcModuleSize,
        dstModuleSize;
    const char *extModuleSizeError = "The external FIRM modules are too large.";

    for(u8 *src = (u8 *)firm + firm->section[0].offset, *srcEnd = src + firm->section[0].size, *dst = firm->section[0].address;
        src < srcEnd; src += srcModuleSize, dst += dstModuleSize, maxModuleSize -= dstModuleSize)
    {
        srcModuleSize = ((Cxi *)src)->ncch.contentSize * 0x200;
        const char *moduleName = ((Cxi *)src)->exHeader.systemControlInfo.appTitle;

        bool loadedModule;

        if(!loadFromStorage) loadedModule = false;
        else
        {
            char fileName[24] = "sysmodules/";

            //Read modules from files if they exist
            concatenateStrings(fileName, moduleName);
            concatenateStrings(fileName, ".cxi");

            dstModuleSize = getFileSize(fileName);

            if(dstModuleSize == 0) loadedModule = false;
            else
            {
                if(dstModuleSize > maxModuleSize) error(extModuleSizeError);

                if(dstModuleSize <= sizeof(Cxi) + 0x200 ||
                   fileRead(dst, fileName, dstModuleSize) != dstModuleSize ||
                   memcmp(((Cxi *)dst)->ncch.magic, "NCCH", 4) != 0 ||
                   memcmp(moduleName, ((Cxi *)dst)->exHeader.systemControlInfo.appTitle, sizeof(((Cxi *)dst)->exHeader.systemControlInfo.appTitle)) != 0)
                    error("An external FIRM module is invalid or corrupted.");

                loadedModule = true;
            }
        }

        if(!loadedModule)
        {
            const u8 *module;

            if(firmType == NATIVE_FIRM && memcmp(moduleName, "loader", 6) == 0)
            {
                module = injector_bin;
                dstModuleSize = injector_bin_size;
            }
            else
            {
                module = src;
                dstModuleSize = srcModuleSize;
            }

            if(dstModuleSize > maxModuleSize) error(extModuleSizeError);

            memcpy(dst, module, dstModuleSize);
        }
    }
}

void launchFirm(FirmwareType firmType, bool loadFromStorage)
{
    //Allow module injection and/or inject 3ds_injector on new NATIVE_FIRMs and LGY FIRMs
    u32 sectionNum;
    if(firmType == NATIVE_FIRM || (loadFromStorage && firmType != SAFE_FIRM && firmType != NATIVE_FIRM1X2X))
    {
        copySection0AndInjectSystemModules(firmType, loadFromStorage);
        sectionNum = 1;
    }
    else sectionNum = 0;

    //Copy FIRM sections to respective memory locations
    for(; sectionNum < 4 && firm->section[sectionNum].size != 0; sectionNum++)
        memcpy(firm->section[sectionNum].address, (u8 *)firm + firm->section[sectionNum].offset, firm->section[sectionNum].size);

    //Determine the ARM11 entry to use
    vu32 *arm11;
    if(ISFIRMLAUNCH) arm11 = (vu32 *)0x1FFFFFFC;
    else
    {
        deinitScreens();
        arm11 = (vu32 *)BRAHMA_ARM11_ENTRY;
    }

    //Set ARM11 kernel entrypoint
    *arm11 = (u32)firm->arm11Entry;

    //Ensure that all memory transfers have completed and that the caches have been flushed
    flushEntireDCache();
    flushEntireICache();

    //Final jump to ARM9 kernel
    ((void (*)())firm->arm9Entry)();
}