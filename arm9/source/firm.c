/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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

#include "firm.h"
#include "config.h"
#include "utils.h"
#include "fs.h"
#include "exceptions.h"
#include "patches.h"
#include "memory.h"
#include "cache.h"
#include "emunand.h"
#include "crypto.h"
#include "screen.h"
#include "fmt.h"
#include "chainloader.h"

static Firm *firm = (Firm *)0x20001000;

static __attribute__((noinline)) bool overlaps(u32 as, u32 ae, u32 bs, u32 be)
{
    if(as <= bs && bs <= ae)
        return true;
    if(bs <= as && as <= be)
        return true;
    return false;
}

static __attribute__((noinline)) bool inRange(u32 as, u32 ae, u32 bs, u32 be)
{
   if(as >= bs && ae <= be)
        return true;
   return false;
}

static bool checkFirm(u32 firmSize)
{
    if(memcmp(firm->magic, "FIRM", 4) != 0 || firm->arm9Entry == NULL) //Allow for the Arm11 entrypoint to be zero in which case nothing is done on the Arm11 side
        return false;

    bool arm9EpFound = false,
         arm11EpFound = false;

    u32 size = 0x200;
    for(u32 i = 0; i < 4; i++)
        size += firm->section[i].size;

    if(firmSize < size) return false;

    for(u32 i = 0; i < 4; i++)
    {
        FirmSection *section = &firm->section[i];

        //Allow empty sections
        if(section->size == 0)
            continue;

        if((section->offset < 0x200) ||
           (section->address + section->size < section->address) || //Overflow check
           ((u32)section->address & 3) || (section->offset & 0x1FF) || (section->size & 0x1FF) || //Alignment check
           (overlaps((u32)section->address, (u32)section->address + section->size, (u32)firm, (u32)firm + size)) ||
           ((!inRange((u32)section->address, (u32)section->address + section->size, 0x08000000, 0x08000000 + 0x00100000)) &&
            (!inRange((u32)section->address, (u32)section->address + section->size, 0x18000000, 0x18000000 + 0x00600000)) &&
            (!inRange((u32)section->address, (u32)section->address + section->size, 0x1FF00000, 0x1FFFFC00)) &&
            (!inRange((u32)section->address, (u32)section->address + section->size, 0x20000000, 0x20000000 + 0x8000000))))
            return false;

        __attribute__((aligned(4))) u8 hash[0x20];

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

static inline u32 loadFirmFromStorage(FirmwareType firmType)
{
    static const char *firmwareFiles[] = {
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

    u32 firmSize = fileRead(firm, firmwareFiles[(u32)firmType], 0x400000 + sizeof(Cxi) + 0x200);

    if(!firmSize) return 0;

    static const char *extFirmError = "The external FIRM is not valid.";

    if(firmSize <= sizeof(Cxi) + 0x200) error(extFirmError);

    if(memcmp(firm, "FIRM", 4) != 0)
    {
        if(firmSize <= sizeof(Cxi) + 0x400) error(extFirmError);

        u8 cetk[0xA50];

        if(fileRead(cetk, cetkFiles[(u32)firmType], sizeof(cetk)) != sizeof(cetk))
            error("The cetk is missing or corrupted.");

        firmSize = decryptNusFirm((Ticket *)(cetk + 0x140), (Cxi *)firm, firmSize);

        if(!firmSize) error("Unable to decrypt the external FIRM.");
    }

    if(!checkFirm(firmSize)) error("The external FIRM is invalid or corrupted.");

    return firmSize;
}

u32 loadNintendoFirm(FirmwareType *firmType, FirmwareSource nandType, bool loadFromStorage, bool isSafeMode)
{
    u32 firmVersion,
        firmSize;

    bool ctrNandError = isSdMode && !mountFs(false, false);

    if(!ctrNandError)
    {
        //Load FIRM from CTRNAND
        firmVersion = firmRead(firm, (u32)*firmType);

        if(firmVersion == 0xFFFFFFFF) ctrNandError = true;
        else
        {
            firmSize = decryptExeFs((Cxi *)firm);

            if(!firmSize || !checkFirm(firmSize)) ctrNandError = true;
        }
    }

    bool loadedFromStorage = false;

    if(loadFromStorage || ctrNandError)
    {
        u32 result = loadFirmFromStorage(*firmType);

        if(result != 0)
        {
            loadedFromStorage = true;
            firmSize = result;
        }
        else if(ctrNandError) error("Unable to mount CTRNAND or load the CTRNAND FIRM.\nPlease use an external one.");
    }

    //Check that the FIRM is right for the console from the Arm9 section address
    if((firm->section[3].offset != 0 ? firm->section[3].address : firm->section[2].address) != (ISN3DS ? (u8 *)0x8006000 : (u8 *)0x8006800))
        error("The %s FIRM is not for this console.", loadedFromStorage ? "external" : "CTRNAND");

    if(!ISN3DS && *firmType == NATIVE_FIRM && firm->section[0].address == (u8 *)0x1FF80000)
    {
        //We can't boot < 3.x EmuNANDs
        if(nandType != FIRMWARE_SYSNAND) error("An old unsupported EmuNAND has been detected.\nLuma3DS is unable to boot it.");

        //If you want to use SAFE_FIRM on 1.0, use Luma from NAND & comment this line:
        if(isSafeMode) error("SAFE_MODE is not supported on 1.x/2.x FIRM.");

        *firmType = NATIVE_FIRM1X2X;
    }

    if(loadedFromStorage || ISDEVUNIT)
    {
        firmVersion = 0xFFFFFFFF;

        if(!ISN3DS && *firmType == NATIVE_FIRM)
        {
            __attribute__((aligned(4))) static const u8 hashes[3][0x20] = {
                {0x39, 0x75, 0xB5, 0x28, 0x24, 0x5E, 0x8B, 0x56, 0xBC, 0x83, 0x79, 0x41, 0x09, 0x2C, 0x42, 0xE6,
                 0x26, 0xB6, 0x80, 0x59, 0xA5, 0x56, 0xF9, 0xF9, 0x6E, 0xF3, 0x63, 0x05, 0x58, 0xDF, 0x35, 0xEF},
                {0x81, 0x9E, 0x71, 0x58, 0xE5, 0x44, 0x73, 0xF7, 0x48, 0x78, 0x7C, 0xEF, 0x5E, 0x30, 0xE2, 0x28,
                 0x78, 0x0B, 0x21, 0x23, 0x94, 0x63, 0xE8, 0x4E, 0x06, 0xBB, 0xD6, 0x8D, 0xA0, 0x99, 0xAE, 0x98},
                {0x1D, 0xD5, 0xB0, 0xC2, 0xD9, 0x4A, 0x4A, 0xF3, 0x23, 0xDD, 0x2F, 0x65, 0x21, 0x95, 0x9B, 0x7E,
                 0xF2, 0x71, 0x7E, 0xB6, 0x7A, 0x3A, 0x74, 0x78, 0x0D, 0xE3, 0xB5, 0x0C, 0x2B, 0x7F, 0x85, 0x37}
            };

            u32 i;
            for(i = 0; i < 3; i++) if(memcmp(firm->section[1].hash, hashes[i], 0x20) == 0) break;

            switch(i)
            {
                case 0:
                    firmVersion = 0x18;
                    break;
                case 1:
                    firmVersion = 0x1D;
                    break;
                case 2:
                    firmVersion = 0x1F;
                    break;
            }
        }
    }

    return firmVersion;
}

void loadHomebrewFirm(u32 pressed)
{
    char path[10 + 255];
    bool hasDisplayedMenu = false;
    bool found = !pressed ? payloadMenu(path, &hasDisplayedMenu) : findPayload(path, pressed);

    if(!found) return;

    u32 maxPayloadSize = (u32)((u8 *)0x27FFE000 - (u8 *)firm),
        payloadSize = fileRead(firm, path, maxPayloadSize);

    if(payloadSize <= 0x200 || !checkFirm(payloadSize)) error("The payload is invalid or corrupted.");

    char absPath[24 + 255];

    if(isSdMode) sprintf(absPath, "sdmc:/luma/%s", path);
    else sprintf(absPath, "nand:/rw/luma/%s", path);

    char *argv[2] = {absPath, (char *)fbs};
    bool wantsScreenInit = (firm->reserved2[0] & 1) != 0;

    if(!hasDisplayedMenu && wantsScreenInit)
        initScreens(); // Don't init the screens unless we have to, if not already done

    launchFirm(wantsScreenInit ? 2 : 1, argv);
}

static inline void mergeSection0(FirmwareType firmType, u32 firmVersion, bool loadFromStorage)
{
    u32 srcModuleSize,
        nbModules = 0;

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

    // SAFE_FIRM only for N3DS and only if ENABLESAFEFIRMROSALINA is on
    if((firmType == NATIVE_FIRM || firmType == SAFE_FIRM) && (ISN3DS || firmVersion >= 0x1D))
    {
        //2) Merge that info with our own modules'
        for(u8 *src = (u8 *)0x18180000; memcmp(((Cxi *)src)->ncch.magic, "NCCH", 4) == 0; src += srcModuleSize)
        {
            const char *name = ((Cxi *)src)->exHeader.systemControlInfo.appTitle;

            u32 i;

            for(i = 0; i < 5 && memcmp(name, moduleList[i].name, 8) != 0; i++);

            if(i == 5)
            {
                nbModules++;
                memcpy(moduleList[i].name, ((Cxi *)src)->exHeader.systemControlInfo.appTitle, 8);
            }

            moduleList[i].src = src;
            srcModuleSize = moduleList[i].size = ((Cxi *)src)->ncch.contentSize * 0x200;
        }
    }

    //3) Read or copy the modules
    u8 *dst = firm->section[0].address;
    const char *extModuleSizeError = "The external FIRM modules are too large.";
    // SAFE_FIRM only for N3DS and only if ENABLESAFEFIRMROSALINA is on
    u32 maxModuleSize = (firmType == NATIVE_FIRM || firmType == SAFE_FIRM) ? 0x80000 : 0x600000;
    for(u32 i = 0, dstModuleSize; i < nbModules; i++, dst += dstModuleSize, maxModuleSize -= dstModuleSize)
    {
        if(loadFromStorage)
        {
            char fileName[24];

            //Read modules from files if they exist
            sprintf(fileName, "sysmodules/%.8s.cxi", moduleList[i].name);

            dstModuleSize = getFileSize(fileName);

            if(dstModuleSize != 0)
            {
                if(dstModuleSize > maxModuleSize) error(extModuleSizeError);

                if(dstModuleSize <= sizeof(Cxi) + 0x200 ||
                   fileRead(dst, fileName, dstModuleSize) != dstModuleSize ||
                   memcmp(((Cxi *)dst)->ncch.magic, "NCCH", 4) != 0 ||
                   memcmp(moduleList[i].name, ((Cxi *)dst)->exHeader.systemControlInfo.appTitle, sizeof(((Cxi *)dst)->exHeader.systemControlInfo.appTitle)) != 0)
                    error("An external FIRM module is invalid or corrupted.");

                continue;
            }
        }

        dstModuleSize = moduleList[i].size;

        if(dstModuleSize > maxModuleSize) error(extModuleSizeError);

        memcpy(dst, moduleList[i].src, dstModuleSize);
    }

    //4) Patch NATIVE_FIRM/SAFE_FIRM (N3DS) if necessary
    if(nbModules == 6)
    {
        if(patchK11ModuleLoading(firm->section[0].size, dst - firm->section[0].address, (u8 *)firm + firm->section[1].offset, firm->section[1].size) != 0)
            error("Failed to inject custom sysmodule");
    }
}

u32 patchNativeFirm(u32 firmVersion, FirmwareSource nandType, bool loadFromStorage, bool isFirmProtEnabled, bool needToInitSd, bool doUnitinfoPatch)
{
    u8 *arm9Section = (u8 *)firm + firm->section[2].offset,
       *arm11Section1 = (u8 *)firm + firm->section[1].offset;

    if(ISN3DS)
    {
        //Decrypt Arm9Bin and patch Arm9 entrypoint to skip kernel9loader
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
        *arm11ExceptionsPage,
        *arm11SvcTable = getKernel11Info(arm11Section1, firm->section[1].size, &baseK11VA, &freeK11Space, &arm11SvcHandler, &arm11ExceptionsPage);

    u32 kernel9Size = (u32)(process9Offset - arm9Section) - sizeof(Cxi) - 0x200,
        ret = 0;

    //Skip on FIRMs < 4.0
    if(ISN3DS || firmVersion >= 0x1D)
    {
        ret += installK11Extension(arm11Section1, firm->section[1].size, needToInitSd, baseK11VA, arm11ExceptionsPage, &freeK11Space);
        ret += patchKernel11(arm11Section1, firm->section[1].size, baseK11VA, arm11SvcTable, arm11ExceptionsPage);
    }

    //Apply signature patches
    ret += patchSignatureChecks(process9Offset, process9Size);

    //Apply EmuNAND patches
    if(nandType != FIRMWARE_SYSNAND) ret += patchEmuNand(arm9Section, kernel9Size, process9Offset, process9Size, firm->section[2].address, firmVersion);

    //Apply FIRM0/1 writes patches on SysNAND to protect A9LH
    else if(isFirmProtEnabled) ret += patchFirmWrites(process9Offset, process9Size);

    //Apply firmlaunch patches
    ret += patchFirmlaunches(process9Offset, process9Size, process9MemAddr);

    //Apply dev unit check patches related to NCCH encryption
    if(!ISDEVUNIT)
    {
        ret += patchZeroKeyNcchEncryptionCheck(process9Offset, process9Size);
        ret += patchNandNcchEncryptionCheck(process9Offset, process9Size);
    }

    //Apply anti-anti-DG patches on 11.0+
    if(firmVersion >= (ISN3DS ? 0x21 : 0x52)) ret += patchTitleInstallMinVersionChecks(process9Offset, process9Size, firmVersion);

    //Patch P9 AM ticket wrapper on 11.8+ to use 0 Key and IV, only with UNITINFO patch on to prevent NIM from actually sending any
    if(doUnitinfoPatch && firmVersion >= (ISN3DS ? 0x35 : 0x64)) ret += patchP9AMTicketWrapperZeroKeyIV(process9Offset, process9Size, firmVersion);

    //Apply UNITINFO patches
    if(doUnitinfoPatch)
    {
        ret += patchUnitInfoValueSet(arm9Section, kernel9Size);
        if(!ISDEVUNIT) ret += patchCheckForDevCommonKey(process9Offset, process9Size);
    }

    //Arm9 exception handlers
    ret += patchArm9ExceptionHandlersInstall(arm9Section, kernel9Size);
    ret += patchSvcBreak9(arm9Section, kernel9Size, (u32)firm->section[2].address);
    ret += patchKernel9Panic(arm9Section, kernel9Size);

    ret += patchP9AccessChecks(process9Offset, process9Size);

    mergeSection0(NATIVE_FIRM, firmVersion, loadFromStorage);
    firm->section[0].size = 0;

    return ret;
}

u32 patchTwlFirm(u32 firmVersion, bool loadFromStorage, bool doUnitinfoPatch)
{
    u8 *arm9Section = (u8 *)firm + firm->section[3].offset;

    // Below 3.0, do not actually do anything.
    if(!ISN3DS && firmVersion < 0xC)
        return 0;

    //On N3DS, decrypt Arm9Bin and patch Arm9 entrypoint to skip kernel9loader
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
        mergeSection0(TWL_FIRM, 0, true);
        firm->section[0].size = 0;
    }

    return ret;
}

u32 patchAgbFirm(bool loadFromStorage, bool doUnitinfoPatch)
{
    u8 *arm9Section = (u8 *)firm + firm->section[3].offset;

    //On N3DS, decrypt Arm9Bin and patch Arm9 entrypoint to skip kernel9loader
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
        mergeSection0(AGB_FIRM, 0, true);
        firm->section[0].size = 0;
    }

    return ret;
}

u32 patch1x2xNativeAndSafeFirm(void)
{
    u8 *arm9Section = (u8 *)firm + firm->section[2].offset;

    if(ISN3DS)
    {
        //Decrypt Arm9Bin and patch Arm9 entrypoint to skip kernel9loader
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

    //Arm9 exception handlers
    ret += patchArm9ExceptionHandlersInstall(arm9Section, kernel9Size);
    ret += patchSvcBreak9(arm9Section, kernel9Size, (u32)firm->section[2].address);

    //Apply firmlaunch patches
    //Doesn't work here if Luma is on SD. If you want to use SAFE_FIRM on 1.0, use Luma from NAND & uncomment this line:
    //ret += patchFirmlaunches(process9Offset, process9Size, process9MemAddr);

    if(ISN3DS && CONFIG(ENABLESAFEFIRMROSALINA))
    {
        u8 *arm11Section1 = (u8 *)firm + firm->section[1].offset;
        //Find the Kernel11 SVC table and handler, exceptions page and free space locations
        u32 baseK11VA;
        u8 *freeK11Space;
        u32 *arm11SvcHandler,
            *arm11ExceptionsPage,
            *arm11SvcTable = getKernel11Info(arm11Section1, firm->section[1].size, &baseK11VA, &freeK11Space, &arm11SvcHandler, &arm11ExceptionsPage);

        ret += installK11Extension(arm11Section1, firm->section[1].size, false, baseK11VA, arm11ExceptionsPage, &freeK11Space);
        ret += patchKernel11(arm11Section1, firm->section[1].size, baseK11VA, arm11SvcTable, arm11ExceptionsPage);

        // Add some other patches to the mix, as we can now launch homebrew on SAFE_FIRM:

        ret += patchKernel9Panic(arm9Section, kernel9Size);
        ret += patchP9AccessChecks(process9Offset, process9Size);

        mergeSection0(NATIVE_FIRM, 0x45, false); // may change in the future
        firm->section[0].size = 0;
    }

    return ret;
}

void launchFirm(int argc, char **argv)
{
    prepareArm11ForFirmlaunch();
    chainload(argc, argv, firm);
}
