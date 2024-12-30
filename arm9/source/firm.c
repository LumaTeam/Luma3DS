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
u32 firmProtoVersion = 0;

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
    u32 firmVersion = 0xFFFFFFFF,
        firmSize;

    bool ctrNandError = true;
    bool loadedFromStorage = false;
    bool storageLoadError = false;

    // Try loading FIRM from sdmc first if specified.
    if (loadFromStorage) {
        firmSize = loadFirmFromStorage(*firmType);
        if (firmSize != 0) loadedFromStorage = true;
        else storageLoadError = true;
    }

    // Remount ctrnand and load FIRM from it if loading from sdmc failed.
    if (!loadedFromStorage) {
        ctrNandError = isSdMode && !remountCtrNandPartition(false);
    }

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
    // If CTRNAND load failed, and it wasn't tried yet, load FIRM from sdmc.
    if (ctrNandError && !storageLoadError)
    {
        u32 result = loadFirmFromStorage(*firmType);

        if(result != 0)
        {
            loadedFromStorage = true;
            firmSize = result;
        }
        else storageLoadError = true;
    }
    // If all attempts failed, panic.
    if(ctrNandError && storageLoadError) error("Unable to mount CTRNAND or load the CTRNAND FIRM.\nPlease use an external one.");

    //Check that the FIRM is right for the console from the Arm9 section address
    bool isO3dsFirm = firm->section[3].offset == 0 && firm->section[2].address == (u8 *)0x8006800;

    if(loadedFromStorage || ISDEVUNIT)
    {
        firmVersion = 0xFFFFFFFF;

        if(isO3dsFirm && (*firmType == NATIVE_FIRM || *firmType == NATIVE_FIRM1X2X))
        {
            __attribute__((aligned(4))) static const u8 hashes[5][0x20] = {
                {0xD7, 0x43, 0x0F, 0x27, 0x8D, 0xC9, 0x3F, 0x4C, 0x96, 0xB5, 0xA8, 0x91, 0x48, 0xDB, 0x08, 0x8A,
                 0x7E, 0x46, 0xB3, 0x95, 0x65, 0xA2, 0x05, 0xF1, 0xF2, 0x41, 0x21, 0xF1, 0x0C, 0x59, 0x6A, 0x9D},
                {0x93, 0xDF, 0x49, 0xA1, 0x24, 0x86, 0xBB, 0x6F, 0xAF, 0x49, 0x99, 0x2D, 0xD0, 0x8D, 0xB1, 0x88,
                 0x8A, 0x00, 0xB6, 0xDD, 0x36, 0x89, 0xC0, 0xE2, 0xC9, 0xA9, 0x99, 0x62, 0x57, 0x5E, 0x6C, 0x23},
                {0x39, 0x75, 0xB5, 0x28, 0x24, 0x5E, 0x8B, 0x56, 0xBC, 0x83, 0x79, 0x41, 0x09, 0x2C, 0x42, 0xE6,
                 0x26, 0xB6, 0x80, 0x59, 0xA5, 0x56, 0xF9, 0xF9, 0x6E, 0xF3, 0x63, 0x05, 0x58, 0xDF, 0x35, 0xEF},
                {0x81, 0x9E, 0x71, 0x58, 0xE5, 0x44, 0x73, 0xF7, 0x48, 0x78, 0x7C, 0xEF, 0x5E, 0x30, 0xE2, 0x28,
                 0x78, 0x0B, 0x21, 0x23, 0x94, 0x63, 0xE8, 0x4E, 0x06, 0xBB, 0xD6, 0x8D, 0xA0, 0x99, 0xAE, 0x98},
                {0x1D, 0xD5, 0xB0, 0xC2, 0xD9, 0x4A, 0x4A, 0xF3, 0x23, 0xDD, 0x2F, 0x65, 0x21, 0x95, 0x9B, 0x7E,
                 0xF2, 0x71, 0x7E, 0xB6, 0x7A, 0x3A, 0x74, 0x78, 0x0D, 0xE3, 0xB5, 0x0C, 0x2B, 0x7F, 0x85, 0x37},
            };

            u32 i;
            for(i = 0; i < sizeof(hashes)/sizeof(hashes[0]); i++)
            {
                if(memcmp(firm->section[1].hash, hashes[i], 0x20) == 0) break;
            }

            switch(i)
            {
                // Beta
                case 0:
                    firmVersion = 0x0;
                    firmProtoVersion = 243;
                    *firmType = NATIVE_PROTOTYPE;
                    break;
                case 1:
                    firmVersion = 0x0;
                    firmProtoVersion = 238;
                    *firmType = NATIVE_PROTOTYPE;
                    break;
                // Release
                case 2:
                    firmVersion = 0x18;
                    break;
                case 3:
                    firmVersion = 0x1D;
                    break;
                case 4:
                    firmVersion = 0x1F;
                    break;
                default:
                    break;
            }
        }
    }

    if(*firmType != NATIVE_PROTOTYPE && (firm->section[3].offset != 0 ? firm->section[3].address : firm->section[2].address) != (ISN3DS ? (u8 *)0x8006000 : (u8 *)0x8006800))
        error("The %s FIRM is not for this console.", loadedFromStorage ? "external" : "CTRNAND");

    if(!ISN3DS && *firmType == NATIVE_FIRM && firm->section[0].address == (u8 *)0x1FF80000)
    {
        //We can't boot < 3.x EmuNANDs
        if(nandType != FIRMWARE_SYSNAND) error("An old unsupported EmuNAND has been detected.\nLuma3DS is unable to boot it.");

        //If you want to use SAFE_FIRM on 1.0, use Luma from NAND & comment this line:
        if(isSafeMode) error("SAFE_MODE is not supported on 1.x/2.x FIRM.");

        *firmType = NATIVE_FIRM1X2X;
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

static int lzss_decompress(u8 *end)
{
    unsigned int v1; // r1@2
    u8 *v2; // r2@2
    u8 *v3; // r3@2
    u8 *v4; // r1@2
    char v5; // r5@4
    char v6; // t1@4
    signed int v7; // r6@4
    int v9; // t1@7
    u8 *v11; // r3@8
    int v12; // r12@8
    int v13; // t1@8
    int v14; // t1@8
    unsigned int v15; // r7@8
    int v16; // r12@8
    int ret;

    ret = 0;
    if ( end )
    {
        v1 = *((u32 *)end - 2);
        v2 = &end[*((u32 *)end - 1)];
        v3 = &end[-(v1 >> 24)];
        v4 = &end[-(v1 & 0xFFFFFF)];
        while ( v3 > v4 )
        {
            v6 = *(v3-- - 1);
            v5 = v6;
            v7 = 8;
            while ( 1 )
            {
                if ( (v7-- < 1) )
                    break;
                if ( v5 & 0x80 )
                {
                    v13 = *(v3 - 1);
                    v11 = v3 - 1;
                    v12 = v13;
                    v14 = *(v11 - 1);
                    v3 = v11 - 1;
                    v15 = ((v14 | (v12 << 8)) & 0xFFFF0FFF) + 2;
                    v16 = v12 + 32;
                    do
                    {
                        ret = v2[v15];
                        *(v2-- - 1) = ret;
                        v16 -= 16;
                    }
                    while ( !(v16 < 0) );
                }
                else
                {
                    v9 = *(v3-- - 1);
                    ret = v9;
                    *(v2-- - 1) = v9;
                }
                v5 *= 2;
                if ( v3 <= v4 )
                    return ret;
            }
        }
    }
    return ret;
}

typedef struct CopyKipResult {
    u32 cxiSize;
    u8 *codeDstAddr;
    u32 codeSize;
} CopyKipResult;

// Copy a KIP, decompressing it in place if necessary (TwlBg)
static CopyKipResult copyKip(u8 *dst, const u8 *src, u32 maxSize, bool decompress)
{
    const char *extModuleSizeError = "The external FIRM modules are too large.";
    CopyKipResult res = { 0 };
    Cxi *dstCxi = (Cxi *)dst;
    const Cxi *srcCxi = (const Cxi *)src;

    u32 mediaUnitShift = 9 + srcCxi->ncch.flags[6];
    u32 totalSizeCompressed = srcCxi->ncch.contentSize << mediaUnitShift;

    if (totalSizeCompressed > maxSize)
        error(extModuleSizeError);

    // First, copy the compressed KIP to the destination
    memcpy(dst, src, totalSizeCompressed);

    ExHeader *exh = &dstCxi->exHeader;
    bool isCompressed = (exh->systemControlInfo.flag & 1) != 0;
    ExeFsHeader *exefs = (ExeFsHeader *)(dst + (dstCxi->ncch.exeFsOffset << mediaUnitShift));
    ExeFsFileHeader *fh = &exefs->fileHeaders[0];
    u8 *codeAddr = (u8 *)exefs + sizeof(ExeFsHeader) + fh->offset;

    if (memcmp(fh->name, ".code\0\0\0", 8) != 0 || fh->offset != 0 || exefs->fileHeaders[1].size != 0)
        error("One of the external FIRM modules have invalid layout.");

    // If it's already decompressed or we don't need to, there is not much left to do
    if (!decompress || !isCompressed)
    {
        res.cxiSize = totalSizeCompressed;
        res.codeDstAddr = codeAddr;
        res.codeSize = fh->size;
    }
    else
    {
        u32 codeSize = exh->systemControlInfo.textCodeSet.size;
        codeSize += exh->systemControlInfo.roCodeSet.size;
        codeSize += exh->systemControlInfo.dataCodeSet.size;

        u32 codeSizePadded = ((codeSize + (1 << mediaUnitShift) - 1) >> mediaUnitShift) << mediaUnitShift;
        u32 newTotalSize = (codeAddr + codeSizePadded) - dst;
        if (newTotalSize > maxSize)
            error(extModuleSizeError);

        // Decompress in place
        lzss_decompress(codeAddr + fh->size);

        // Fill padding just in case
        memset(codeAddr + codeSize, 0, codeSizePadded - codeSize);

        // Fix fields
        fh->size = codeSize;
        dstCxi->ncch.exeFsSize = codeSizePadded >> mediaUnitShift;
        exh->systemControlInfo.flag &= ~1;
        dstCxi->ncch.contentSize = newTotalSize >> mediaUnitShift;

        res.cxiSize = newTotalSize;
        res.codeDstAddr = codeAddr;
        res.codeSize = codeSize;
    }

    return res;
}
static void mergeSection0(FirmwareType firmType, u32 firmVersion, bool loadFromStorage)
{
    u32 srcModuleSize,
        nbModules = 0;

    bool isLgyFirm = firmType == TWL_FIRM || firmType == AGB_FIRM;

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
    if((firmType == NATIVE_FIRM || firmType == SAFE_FIRM) && (ISN3DS || firmVersion >= 0x25))
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
    u32 maxModuleSize = !isLgyFirm ? 0x80000 : 0x600000;
    u32 dstModuleSize = 0;
    for(u32 i = 0; i < nbModules; i++)
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

                dst += dstModuleSize;
                maxModuleSize -= dstModuleSize;
                continue;
            }
        }

        // If not successfully loaded from storage, then...

        // Decompress stock TwlBg so that we can patch it
        bool isStockTwlBg = firmType == TWL_FIRM && strcmp(moduleList[i].name, "TwlBg") == 0;

        CopyKipResult copyRes = copyKip(dst, moduleList[i].src, maxModuleSize, isStockTwlBg);

        if (isStockTwlBg)
            patchTwlBg(copyRes.codeDstAddr, copyRes.codeSize);

        dst += copyRes.cxiSize;
        maxModuleSize -= copyRes.cxiSize;
    }

    //4) Patch kernel to take module size into account
    u32 newKipSectionSize = dst - firm->section[0].address;
    u32 oldKipSectionSize = firm->section[0].size;
    u8 *kernel11Addr = (u8 *)firm + firm->section[1].offset;
    u32 kernel11Size = firm->section[1].size;
    if (isLgyFirm)
    {
        if (patchK11ModuleLoadingLgy(newKipSectionSize, kernel11Addr, kernel11Size) != 0)
            error("Failed to load sysmodules");
    }
    else
    {
        if (patchK11ModuleLoading(oldKipSectionSize, newKipSectionSize, nbModules, kernel11Addr, kernel11Size) != 0)
            error("Failed to load sysmodules");
    }
}

u32 patchNativeFirm(u32 firmVersion, FirmwareSource nandType, bool loadFromStorage, bool isFirmProtEnabled, bool needToInitSd, bool doUnitinfoPatch)
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

#ifndef BUILD_FOR_EXPLOIT_DEV
    //Skip on FIRMs < 5.0
    if(ISN3DS || firmVersion >= 0x25)
    {
        //Find the Kernel11 SVC table and handler, exceptions page and free space locations
        u8 *arm11Section1 = (u8 *)firm + firm->section[1].offset;
        u32 baseK11VA;
        u8 *freeK11Space;
        u32 *arm11SvcHandler,
            *arm11ExceptionsPage,
            *arm11SvcTable = getKernel11Info(arm11Section1, firm->section[1].size, &baseK11VA, &freeK11Space, &arm11SvcHandler, &arm11ExceptionsPage);

        ret += installK11Extension(arm11Section1, firm->section[1].size, needToInitSd, baseK11VA, arm11ExceptionsPage, &freeK11Space);
        ret += patchKernel11(arm11Section1, firm->section[1].size, baseK11VA, arm11SvcTable, arm11ExceptionsPage);
    }
#else
    (void)needToInitSd;
#endif

    //Apply signature patches
    ret += patchSignatureChecks(process9Offset, process9Size);

    //Apply EmuNAND patches
    if(nandType != FIRMWARE_SYSNAND) ret += patchEmuNand(process9Offset, process9Size, firmVersion);

    //Apply FIRM0/1 writes patches on SysNAND to protect A9LH
    else if(isFirmProtEnabled) ret += patchFirmWrites(process9Offset, process9Size);

#ifndef BUILD_FOR_EXPLOIT_DEV
    //Apply firmlaunch patches
    ret += patchFirmlaunches(process9Offset, process9Size, process9MemAddr);
#endif

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
    u8 *section1 = (u8 *)firm + firm->section[1].offset;
    u32 section1Size = firm->section[1].size;
    u8 *section2 = (u8 *)firm + firm->section[2].offset;
    u32 section2Size = firm->section[2].size;

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

    ret += patchLgyK11(section1, section1Size, section2, section2Size);

    // Also patch TwlBg here
    mergeSection0(TWL_FIRM, 0, loadFromStorage);
    firm->section[0].size = 0;

    return ret;
}

u32 patchAgbFirm(bool loadFromStorage, bool doUnitinfoPatch)
{
    u8 *arm9Section = (u8 *)firm + firm->section[3].offset;

    u8 *section1 = (u8 *)firm + firm->section[1].offset;
    u32 section1Size = firm->section[1].size;
    u8 *section2 = (u8 *)firm + firm->section[2].offset;
    u32 section2Size = firm->section[2].size;

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
    ret += patchLgyK11(section1, section1Size, section2, section2Size);

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

u32 patchPrototypeNative(FirmwareSource nandType)
{
    u8 *arm9Section = (u8 *)firm + firm->section[2].offset;

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9Info(arm9Section, firm->section[2].size, &process9Size, &process9MemAddr);

    u32 kernel9Size = (u32)(process9Offset - arm9Section) - sizeof(Cxi) - 0x200,
        ret = 0;

    ret += patchProtoNandSignatureCheck(process9Offset, process9Size);

    //Arm9 exception handlers
    ret += patchArm9ExceptionHandlersInstall(arm9Section, kernel9Size);

    //Apply EmuNAND patches
    if(nandType != FIRMWARE_SYSNAND) ret += patchProtoEmuNand(process9Offset, process9Size);

    return ret;
}

void launchFirm(int argc, char **argv)
{
    prepareArm11ForFirmlaunch();
    chainload(argc, argv, firm);
}
