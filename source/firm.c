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
#include "fmt.h"
#include "../build/bundled.h"

static inline bool loadFirmFromStorage(FirmwareType firmType)
{
    const char *firmwareFiles[] = {
        "native.firm",
        "twl.firm",
        "agb.firm",
        "safe.firm",
        "sysupdater.firm"
    },
               *cetkFiles[] = {
        "cetk",
        "cetk_twl",
        "cetk_agb",
        "cetk_safe",
        "cetk_sysupdater"
    };

    u32 firmSize = fileRead(firm, firmType == NATIVE_FIRM1X2X ? firmwareFiles[0] : firmwareFiles[(u32)firmType], 0x400000 + sizeof(Cxi) + 0x200);

    if(!firmSize) return false;

    if(firmSize <= sizeof(Cxi) + 0x200) error("The FIRM in /luma is not valid.");

    if(memcmp(firm, "FIRM", 4) != 0)
    {
        u8 cetk[0xA50];

        if(fileRead(cetk, firmType == NATIVE_FIRM1X2X ? cetkFiles[0] : cetkFiles[(u32)firmType], sizeof(cetk)) != sizeof(cetk) ||
           !decryptNusFirm((Ticket *)(cetk + 0x140), (Cxi *)firm, firmSize))
            error("The FIRM in /luma is encrypted or corrupted.");
    }

    //Check that the FIRM is right for the console from the ARM9 section address
    if((firm->section[3].offset != 0 ? firm->section[3].address : firm->section[2].address) != (ISN3DS ? (u8 *)0x8006000 : (u8 *)0x8006800))
        error("The FIRM in /luma is not for this console.");

    return true;
}

static inline void mergeSection0(FirmwareType firmType, bool loadFromStorage)
{
    u32 srcModuleSize;
    const char *extModuleSizeError = "The external FIRM modules are too large.";

    u32 nbModules = 0,
        isCustomModule = false;
    struct
    {
        char name[8];
        u8 *src;
        u32 size;
    } moduleList[6];

    //1) Parse info concerning Nintendo's modules
    for(u8 *src = (u8 *)firm + firm->section[0].offset, *srcEnd = src + firm->section[0].size; src < srcEnd; src += srcModuleSize, nbModules++)
    {
        memcpy(moduleList[nbModules].name, ((Cxi *)src)->exHeader.systemControlInfo.appTitle, 8);
        moduleList[nbModules].src = src;
        srcModuleSize = moduleList[nbModules].size = ((Cxi *)src)->ncch.contentSize * 0x200; 
    }

    if(firmType == NATIVE_FIRM)
    {
        //2) Merge that info with our own modules' 
        for(u8 *src = (u8 *)0x1FF60000; src < (u8 *)(0x1FF60000 + LUMA_SECTION0_SIZE); src += srcModuleSize)
        {
            const char *name = ((Cxi *)src)->exHeader.systemControlInfo.appTitle;

            u32 i;
            for(i = 0; i < nbModules && memcmp(name, moduleList[i].name, 8) != 0; i++);

            if(i == nbModules) isCustomModule = true;

            memcpy(moduleList[i].name, ((Cxi *)src)->exHeader.systemControlInfo.appTitle, 8);
            moduleList[i].src = src;
            srcModuleSize = moduleList[i].size = ((Cxi *)src)->ncch.contentSize * 0x200;
        }

        if(isCustomModule) nbModules++;
    }

    //3) Read or copy the modules
    u8 *dst = firm->section[0].address;
    for(u32 i = 0, dstModuleSize; i < nbModules; i++) 
    {
        dstModuleSize = 0;

        if(loadFromStorage)
        {
            char fileName[24];

            //Read modules from files if they exist
            sprintf(fileName, "sysmodules/%.8s.cxi", moduleList[i].name);

            dstModuleSize = getFileSize(fileName);

            if(dstModuleSize != 0)
            {
                if(dstModuleSize > 0x60000) error(extModuleSizeError);

                if(dstModuleSize <= sizeof(Cxi) + 0x200 ||
                   fileRead(dst, fileName, dstModuleSize) != dstModuleSize ||
                   memcmp(((Cxi *)dst)->ncch.magic, "NCCH", 4) != 0 ||
                   memcmp(moduleList[i].name, ((Cxi *)dst)->exHeader.systemControlInfo.appTitle, sizeof(((Cxi *)dst)->exHeader.systemControlInfo.appTitle)) != 0)
                    error("An external FIRM module is invalid or corrupted.");

                dst += dstModuleSize;
            }
        }

        if(!dstModuleSize)
        {
            memcpy(dst, moduleList[i].src, moduleList[i].size);
            dst += moduleList[i].size;
        }
    }

    //4) Patch NATIVE_FIRM if necessary
    if(isCustomModule)
    {
        if(patchK11ModuleLoading(firm->section[0].size, dst - firm->section[0].address, (u8 *)firm + firm->section[1].offset, firm->section[1].size) != 0)
            error("Failed to inject custom sysmodule");
    }
}

u32 loadFirm(FirmwareType *firmType, FirmwareSource nandType, bool loadFromStorage, bool isSafeMode)
{
    //Load FIRM from CTRNAND
    u32 firmVersion = firmRead(firm, (u32)*firmType);

    if(firmVersion == 0xFFFFFFFF) error("Failed to get the CTRNAND FIRM.");

    bool mustLoadFromStorage = false;

    if(!ISN3DS && *firmType == NATIVE_FIRM && !ISDEVUNIT)
    {
        if(firmVersion < 0x18)
        {
            //We can't boot < 3.x EmuNANDs
            if(nandType != FIRMWARE_SYSNAND)
                error("An old unsupported EmuNAND has been detected.\nLuma3DS is unable to boot it.");

            if(isSafeMode) error("SAFE_MODE is not supported on 1.x/2.x FIRM.");

            *firmType = NATIVE_FIRM1X2X;
        }

        //We can't boot a 3.x/4.x NATIVE_FIRM, load one from SD/CTRNAND
        else if(firmVersion < 0x25) mustLoadFromStorage = true;
    }

    if((loadFromStorage || mustLoadFromStorage) && loadFirmFromStorage(*firmType)) firmVersion = 0xFFFFFFFF;
    else
    {
        if(mustLoadFromStorage) error("An old unsupported FIRM has been detected.\nCopy a firmware.bin in /luma to boot.");
        if(!decryptExeFs((Cxi *)firm)) error("The CTRNAND FIRM is corrupted.");
        if(ISDEVUNIT) firmVersion = 0xFFFFFFFF;
    }

    return firmVersion;
}

u32 patchNativeFirm(u32 firmVersion, FirmwareSource nandType, bool loadFromStorage, bool isSafeMode, bool doUnitinfoPatch, bool enableExceptionHandlers)
{
    u8 *arm9Section = (u8 *)firm + firm->section[2].offset,
       *arm11Section1 = (u8 *)firm + firm->section[1].offset;

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

    //Find the Kernel11 SVC table and handler, exceptions page and free space locations
    u32 baseK11VA;
    u8 *freeK11Space;
    u32 *arm11SvcHandler,
        *arm11DAbtHandler,
        *arm11ExceptionsPage,
        *arm11SvcTable = getKernel11Info(arm11Section1, firm->section[1].size, &baseK11VA, &freeK11Space, &arm11SvcHandler, &arm11DAbtHandler, &arm11ExceptionsPage);

    u32 kernel9Size = (u32)(process9Offset - arm9Section) - sizeof(Cxi) - 0x200,
        ret = 0;

    //Apply signature patches
    ret += patchSignatureChecks(process9Offset, process9Size);

    //Apply EmuNAND patches
    if(nandType != FIRMWARE_SYSNAND) ret += patchEmuNand(arm9Section, kernel9Size, process9Offset, process9Size, firm->section[2].address);

    //Apply FIRM0/1 writes patches on SysNAND to protect A9LH
    else ret += patchFirmWrites(process9Offset, process9Size);

    //Apply firmlaunch patches
    ret += patchFirmlaunches(process9Offset, process9Size, process9MemAddr);

    //Apply dev unit check patches related to NCCH encryption
    if(!ISDEVUNIT)
    {
        ret += patchZeroKeyNcchEncryptionCheck(process9Offset, process9Size);
        ret += patchNandNcchEncryptionCheck(process9Offset, process9Size);
    }

    //11.0 FIRM patches
    if(firmVersion >= (ISN3DS ? 0x21 : 0x52))
    {
        //Apply anti-anti-DG patches
        ret += patchTitleInstallMinVersionChecks(process9Offset, process9Size, firmVersion);

        //Restore svcBackdoor
        ret += reimplementSvcBackdoor(arm11Section1, arm11SvcTable, baseK11VA, &freeK11Space);
    }

    //Stub svc 0x59 on 11.3+ FIRMs
    if(firmVersion >= (ISN3DS ? 0x2D : 0x5C)) ret += stubSvcRestrictGpuDma(arm11Section1, arm11SvcTable, baseK11VA);

    ret += implementSvcGetCFWInfo(arm11Section1, arm11SvcTable, baseK11VA, &freeK11Space, isSafeMode);

    //Apply UNITINFO patches
    if(doUnitinfoPatch)
    {
        ret += patchUnitInfoValueSet(arm9Section, kernel9Size);
        if(!ISDEVUNIT) ret += patchCheckForDevCommonKey(process9Offset, process9Size);
    }

    if(enableExceptionHandlers)
    {
        //ARM11 exception handlers
        u32 codeSetOffset,
            stackAddress = getInfoForArm11ExceptionHandlers(arm11Section1, firm->section[1].size, &codeSetOffset);
        ret += installArm11Handlers(arm11ExceptionsPage, stackAddress, codeSetOffset, arm11DAbtHandler, baseK11VA + ((u8 *)arm11DAbtHandler - arm11Section1));
        patchSvcBreak11(arm11Section1, arm11SvcTable, baseK11VA);
        ret += patchKernel11Panic(arm11Section1, firm->section[1].size);

        //ARM9 exception handlers
        ret += patchArm9ExceptionHandlersInstall(arm9Section, kernel9Size);
        ret += patchSvcBreak9(arm9Section, kernel9Size, (u32)firm->section[2].address);
        ret += patchKernel9Panic(arm9Section, kernel9Size);
    }

    bool patchAccess = CONFIG(PATCHACCESS),
         patchGames = CONFIG(PATCHGAMES);

    if(patchAccess || patchGames)
    {
        ret += patchK11ModuleChecks(arm11Section1, firm->section[1].size, &freeK11Space, patchGames);

        if(patchAccess)
        {
            ret += patchArm11SvcAccessChecks(arm11SvcHandler, (u32 *)(arm11Section1 + firm->section[1].size));
            ret += patchP9AccessChecks(process9Offset, process9Size);
        }
    }

    mergeSection0(NATIVE_FIRM, loadFromStorage);
    firm->section[0].size = 0;

    return ret;
}

u32 patchTwlFirm(u32 firmVersion, bool loadFromStorage, bool doUnitinfoPatch)
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
    if(doUnitinfoPatch) ret += patchUnitInfoValueSet(arm9Section, kernel9Size);

    if(loadFromStorage)
    {
        mergeSection0(TWL_FIRM, true);
        firm->section[0].size = 0;
    }

    return ret;
}

u32 patchAgbFirm(bool loadFromStorage, bool doUnitinfoPatch)
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
    if(doUnitinfoPatch) ret += patchUnitInfoValueSet(arm9Section, kernel9Size);

    if(loadFromStorage)
    {
        mergeSection0(AGB_FIRM, true);
        firm->section[0].size = 0;
    }

    return ret;
}

u32 patch1x2xNativeAndSafeFirm(bool enableExceptionHandlers)
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

    ret += ISN3DS ? patchSignatureChecks(process9Offset, process9Size) : patchOldSignatureChecks(process9Offset, process9Size);

    if(enableExceptionHandlers)
    {
        //ARM9 exception handlers
        ret += patchArm9ExceptionHandlersInstall(arm9Section, kernel9Size);
        ret += patchSvcBreak9(arm9Section, kernel9Size, (u32)firm->section[2].address);
    }

    return ret;
}

static __attribute__((noinline)) bool overlaps(u32 as, u32 ae, u32 bs, u32 be)
{
    if (as <= bs && bs <= ae)
        return true;
    else if (bs <= as && as <= be)
        return true;
    return false;
}

bool checkFirmPayload(void)
{
    if(memcmp(firm->magic, "FIRM", 4) != 0 || firm->arm9Entry == NULL) //Allow for the ARM11 entrypoint to be zero in which case nothing is done on the ARM11 side
        return false;

    u32 size = 0x200;
    for(u32 i = 0; i < 4; i++)
        size += firm->section[i].size;

    bool arm9EpFound = false,
         arm11EpFound = false;

    for(u32 i = 0; i < 4; i++)
    {
        __attribute__((aligned(4))) u8 hash[0x20];

        FirmSection *section = &firm->section[i];

        //Allow empty sections
        if(section->size == 0)
            continue;

        if((section->offset < 0x200) ||
           (section->address + section->size < section->address) || //Overflow check
           ((u32)section->address & 3) || (section->offset & 0x1FF) || (section->size & 0x1FF) || //Alignment check
           (overlaps((u32)section->address, (u32)section->address + section->size, 0x01FF8000, 0x01FF8000 + 0x8000)) ||
           (overlaps((u32)section->address, (u32)section->address + section->size, 0x1FFFFC00, 0x20000000)) ||
           (overlaps((u32)section->address, (u32)section->address + section->size, (u32)firm + section->offset, (u32)firm + size)))
            return false;

        sha(hash, (u8 *)firm + section->offset, section->size, SHA_256_MODE);

        if(memcmp(hash, section->hash, 0x20) != 0)
            return false;

        if(firm->arm9Entry >= section->address && firm->arm9Entry < (section->address + section->size))
            arm9EpFound = true;

        if(firm->arm11Entry >= section->address && firm->arm11Entry < (section->address + section->size))
            arm11EpFound = true;
    }

    return arm9EpFound && (firm->arm11Entry == NULL || arm11EpFound);
}

void launchFirm(int argc, char **argv)
{
    u32 *loaderAddress = (u32 *)0x01FF9000;

    prepareArm11ForFirmlaunch();

    memcpy(loaderAddress, loader_bin, loader_bin_size);

    flushDCacheRange(loaderAddress, loader_bin_size);
    flushICacheRange(loaderAddress, loader_bin_size);

    ((void (*)(int, char **, u32))loaderAddress)(argc, argv, 0x0000BEEF);
}
