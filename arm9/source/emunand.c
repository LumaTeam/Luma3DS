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

/*
*   Code for locating the SDMMC struct by Normmatt
*/


#include "emunand.h"
#include "memory.h"
#include "utils.h"
#include "fatfs/sdmmc/sdmmc.h"
#include "large_patches.h"

u32 emuOffset,
    emuHeader;

void locateEmuNand(FirmwareSource *nandType, u32 *emunandIndex, bool configureCtrNandParams)
{
    static u8 __attribute__((aligned(4))) temp[0x200];
    static u32 nandSize = 0,
               fatStart;

    if(!nandSize)
    {
        nandSize = getMMCDevice(0)->total_size;
        sdmmc_sdcard_readsectors(0, 1, temp);
        fatStart = *(u32 *)(temp + 0x1C6); //First sector of the FAT partition
    }

    /*if (*nandType == FIRMWARE_SYSNAND)
        return;*/

    for(u32 i = 0; i < 3; i++)  // Test the different kinds of multi-EmuNAND there are, unless we are looking for the first one
    {
        static const u32 roundedMinsizes[] = {0x1D8000, 0x26E000};

        u32 nandOffset;
        switch(i)
        {
            case 1:
                nandOffset = ROUND_TO_4MB(nandSize + 1); //"Default" layout
                break;
            case 2:
                nandOffset = roundedMinsizes[ISN3DS ? 1 : 0]; //"Minsize" layout
                break;
            case 0:
                nandOffset = nandSize > 0x200000 ? 0x400000 : 0x200000; //"Legacy" layout
                break;
        }

        nandOffset *= *emunandIndex; // always 0 for 1st EmuNAND

        if(fatStart >= nandOffset + roundedMinsizes[ISN3DS ? 1 : 0])
        {
            //Check for RedNAND
            if(!sdmmc_sdcard_readsectors(nandOffset + 1, 1, temp) && memcmp(temp + 0x100, "NCSD", 4) == 0)
            {
                if (configureCtrNandParams)
                {
                    emuOffset = nandOffset + 1;
                    emuHeader = 0;
                }
                return;
            }

            //Check for Gateway EmuNAND
            else if(i != 2 && !sdmmc_sdcard_readsectors(nandOffset + nandSize, 1, temp) && memcmp(temp + 0x100, "NCSD", 4) == 0)
            {
                if (configureCtrNandParams)
                {
                    emuOffset = nandOffset;
                    emuHeader = nandSize;
                }
                return;
            }
        }

        if(*emunandIndex == 0) break; // See above comments
    }

    //Fallback to the first EmuNAND if there's no second/third/fourth one, or to SysNAND if there isn't any
    if(*emunandIndex != 0)
    {
        *emunandIndex = 0;
        locateEmuNand(nandType, emunandIndex, configureCtrNandParams);
    }
    else *nandType = FIRMWARE_SYSNAND;
}

static inline u32 getProtoSdmmc(u32 *sdmmc, u32 firmVersion)
{
    switch(firmVersion)
    {
        case 243: // SDK 0.9.x (0.9.7?)
            *sdmmc = (0x080AAA28 + 0x4e0);
            break;
        case 238: // SDK 0.10
            *sdmmc = (0x080BEA70 + 0x690);
            break;
        default:
            return 1;
    }

    return 0;
}

static inline u32 getOldSdmmc(u32 *sdmmc, u32 firmVersion)
{
    switch(firmVersion)
    {
        case 0x18:
            *sdmmc = 0x080D91D8;
            break;
        case 0x1D:
        case 0x1F:
            *sdmmc = 0x080D8CD0;
            break;
        default:
            return 1;
    }

    return 0;
}

static inline u32 getSdmmc(u8 *pos, u32 size, u32 *sdmmc)
{
    //Look for struct code
    static const u8 pattern[] = {0x21, 0x20, 0x18, 0x20};

    const u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    *sdmmc = *(u32 *)(off + 9) + *(u32 *)(off + 0xD);

    return 0;
}

static inline u32 patchNandRw(u8 *pos, u32 size, u32 hookAddr)
{
    //Look for read/write code
    static const u8 pattern[] = {0x1E, 0x00, 0xC8, 0x05};

    u16 *readOffset = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(readOffset == NULL) return 1;

    readOffset -= 3;

    u16 *writeOffset = (u16 *)memsearch((u8 *)(readOffset + 5), pattern, 0x100, sizeof(pattern));

    if(writeOffset == NULL) return 1;

    writeOffset -= 3;
    *readOffset = *writeOffset = 0x4C00;
    readOffset[1] = writeOffset[1] = 0x47A0;
    ((u32 *)writeOffset)[1] = ((u32 *)readOffset)[1] = hookAddr;

    return 0;
}

static inline u32 patchProtoNandRw(u8 *pos, u32 size, u32 hookAddr, u32 hookCidAddr)
{
    //Look for read/write code
    static const u8 pattern[] = {
        0x03, 0x00, 0x51, 0xE3, // cmp r1, #3
        0x02, 0xC0, 0xA0, 0xE1, // mov r12, r2
        0x04, 0x00, 0x80, 0xE2, // add r0, r0, #4
    };

    u32 *writeOffset = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(writeOffset == NULL) return 1;

    u32 *readOffset = (u32 *)memsearch((u8 *)(writeOffset + 3), pattern, 0x400, sizeof(pattern));

    if(readOffset == NULL) return 1;

    // Find the sdmmc mount/init(?) function
    static const u8 mount_pattern[] = {
        0x20, 0x00, 0x84, 0xE2, // add r0, r4, 0x20
        0x01, 0x20, 0xA0, 0xE3, // mov r2, #1
        0x00, 0x10, 0xA0, 0xE3, // mov r1, #0
    };
    u32* mountOffset = (u32*) memsearch(pos, mount_pattern, size, sizeof(mount_pattern));
    if (mountOffset == NULL) return 1;

    // Find the sdmmc read cid function.
    static const u8 readcid_pattern[] = {
        0x31, 0xFF, 0x2F, 0xE1, // blx r1
        0x20, 0x60, 0x9F, 0xE5, // ldr r6, [pc, #0x20] // =failing_result
        0x00, 0x00, 0x50, 0xE3, // cmp r0, #0
    };
    u32* readCidOffset = (u32*) memsearch(pos, readcid_pattern, size, sizeof(readcid_pattern));
    if (readCidOffset == NULL) return 1;
    readCidOffset -= 5;

    mountOffset[1] = 0xe3a02000; // mov r2, #0 // sd-card

    readOffset[0] = writeOffset[0] = 0xe52de004; // push {lr}
    readOffset[1] = writeOffset[1] = 0xe59fc000; // ldr r12, [pc, #0]
    readOffset[2] = writeOffset[2] = 0xe12fff3c; // blx r12
    readOffset[3] = writeOffset[3] = hookAddr;

    readCidOffset[0] = 0xe59fc000; // ldr r12, [pc, #0]
    readCidOffset[1] = 0xe12fff3c; // blx r12
    readCidOffset[2] = hookCidAddr;

    // Read the emmc cid into the place hook will copy it from
    sdmmc_get_cid(1, emunandPatchNandCid);

    return 0;
}

static inline u32 patchProtoNandRw238(u8 *pos, u32 size, u32 hookAddr, u32 hookCidAddr)
{
    //Look for read/write code
    static const u8 pattern[] = {
        0x03, 0x00, 0x50, 0xE3, // cmp r0, #3
        0x00, 0x00, 0xA0, 0x13, // movne r0, #0
        0x01, 0x00, 0xA0, 0x03, // moveq r0, #1
    };

    u32 *writeOffset = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(writeOffset == NULL) return 1;

    u32 *readOffset = (u32 *)memsearch((u8 *)(writeOffset + 3), pattern, 0x400, sizeof(pattern));

    if(readOffset == NULL) return 1;

    // Find the mmc static ctor...
    static const u8 mount_pattern[] = {
        0x08, // last byte of some ptr to something in P9
        0x01, 0x01, 0x00, 0x00, // emmc controller id
    };
    u8* mountOffset = (u8*) memsearch(pos, mount_pattern, size, sizeof(mount_pattern));
    if (mountOffset == NULL) return 1;
    mountOffset++;

    // Find the sdmmc read cid function.
    static const u8 readcid_pattern[] = {
        0x31, 0xFF, 0x2F, 0xE1, // blx r1
        0x20, 0x60, 0x9F, 0xE5, // ldr r6, [pc, #0x20] // =failing_result
        0x00, 0x00, 0x50, 0xE3, // cmp r0, #0
    };
    u32* readCidOffset = (u32*) memsearch(pos, readcid_pattern, size, sizeof(readcid_pattern));
    if (readCidOffset == NULL) return 1;
    readCidOffset -= 5;

    *(u32*)mountOffset = 0x300; // sd card

    readOffset[0] = writeOffset[0] = 0xe59fc000; // ldr r12, [pc, #0]
    readOffset[1] = writeOffset[1] = 0xe12fff3c; // blx r12
    readOffset[2] = writeOffset[2] = hookAddr;

    readCidOffset[0] = 0xe59fc000; // ldr r12, [pc, #0]
    readCidOffset[1] = 0xe12fff3c; // blx r12
    readCidOffset[2] = hookCidAddr;

    // Read the emmc cid into the place hook will copy it from
    sdmmc_get_cid(1, emunandPatchNandCid);

    return 0;
}

u32 patchEmuNand(u8 *process9Offset, u32 process9Size, u32 firmVersion)
{
    u32 ret = 0;

    //Add the data of the found EmuNAND
    emunandPatchNandOffset = emuOffset;
    emunandPatchNcsdHeaderOffset = emuHeader;

    //Find and add the SDMMC struct
    u32 sdmmc;
    ret += !ISN3DS && firmVersion < 0x25 ? getOldSdmmc(&sdmmc, firmVersion) : getSdmmc(process9Offset, process9Size, &sdmmc);
    if(!ret) emunandPatchSdmmcStructPtr = sdmmc;

    //Add EmuNAND hooks
    ret += patchNandRw(process9Offset, process9Size, (u32)emunandPatch);

    return ret;
}

u32 patchProtoEmuNand(u8 *process9Offset, u32 process9Size)
{
    extern u32 firmProtoVersion;
    u32 ret = 0;

    // Add the data of the found EmuNAND
    emunandPatchNandOffset = emuOffset;
    emunandPatchNcsdHeaderOffset = emuHeader;

    // Find and add the SDMMC struct
    u32 sdmmc;
    ret += getProtoSdmmc(&sdmmc, firmProtoVersion);
    if(!ret) emunandPatchSdmmcStructPtr = sdmmc;

    // Add EmuNAND hooks
    switch (firmProtoVersion) {
        case 243: // SDK 0.9.x (0.9.7?)
            ret += patchProtoNandRw(process9Offset, process9Size, (u32)emunandProtoPatch, (u32)emunandProtoCidPatch);
            break;
        case 238: // SDK 0.10.x
            ret += patchProtoNandRw238(process9Offset, process9Size, (u32)emunandProtoPatch238, (u32)emunandProtoCidPatch);
            break;
        default:
            ret++;
            break;
    }

    return ret;
}