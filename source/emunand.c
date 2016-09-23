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

#include "emunand.h"
#include "memory.h"
#include "fatfs/sdmmc/sdmmc.h"
#include "../build/bundled.h"

void locateEmuNand(u32 *emuHeader, FirmwareSource *nandType)
{
    static u8 __attribute__((aligned(4))) temp[0x200];
    static u32 nandSize = 0,
               fatStart;
    bool found = false;

    if(!nandSize)
    {
        nandSize = getMMCDevice(0)->total_size;
        sdmmc_sdcard_readsectors(0, 1, temp);
        fatStart = *(u32 *)(temp + 0x1C6); //First sector of the FAT partition
    }

    for(u32 i = 0; i < 3 && !found; i++)
    {
        static const u32 roundedMinsizes[] = {0x1D8000, 0x26E000};

        u32 nandOffset;
        switch(i)
        {
            case 1:
                nandOffset = ROUND_TO_4MB(nandSize + 1); //"Default" layout
                break;
            case 2:
                nandOffset = roundedMinsizes[isN3DS ? 1 : 0]; //"Minsize" layout
                break;
            default:
                nandOffset = *nandType == FIRMWARE_EMUNAND ? 0 : (nandSize > 0x200000 ? 0x400000 : 0x200000); //"Legacy" layout
                break;
        }

        if(*nandType != FIRMWARE_EMUNAND) nandOffset *= ((u32)*nandType - 1);

        if(fatStart >= nandOffset + roundedMinsizes[isN3DS ? 1 : 0])
        {
            //Check for RedNAND
            if(!sdmmc_sdcard_readsectors(nandOffset + 1, 1, temp) && *(u32 *)(temp + 0x100) == NCSD_MAGIC)
            {
                emuOffset = nandOffset + 1;
                *emuHeader = nandOffset + 1;
                found = true;
            }

            //Check for Gateway EmuNAND
            else if(i != 2 && !sdmmc_sdcard_readsectors(nandOffset + nandSize, 1, temp) && *(u32 *)(temp + 0x100) == NCSD_MAGIC)
            {
                emuOffset = nandOffset;
                *emuHeader = nandOffset + nandSize;
                found = true;
            }
        }

        if(*nandType == FIRMWARE_EMUNAND) break;
    }

    //Fallback to the first EmuNAND if there's no second/third/fourth one, or to SysNAND if there isn't any
    if(!found)
    {
        if(*nandType != FIRMWARE_EMUNAND)
        {
            *nandType = FIRMWARE_EMUNAND;
            locateEmuNand(emuHeader, nandType);
        }
        else *nandType = FIRMWARE_SYSNAND;
    }
}

static inline u8 *getFreeK9Space(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

    //Looking for the last free space before Process9
    return memsearch(pos + 0x13500, pattern, size - 0x13500, sizeof(pattern)) + 0x455;
}

static inline u32 getSdmmc(u8 *pos, u32 size)
{
    //Look for struct code
    const u8 pattern[] = {0x21, 0x20, 0x18, 0x20};
    const u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    return *(u32 *)(off + 9) + *(u32 *)(off + 0xD);
}

static inline void patchNandRw(u8 *pos, u32 size, u32 branchOffset)
{
    //Look for read/write code
    const u8 pattern[] = {0x1E, 0x00, 0xC8, 0x05};

    u16 *readOffset = (u16 *)memsearch(pos, pattern, size, sizeof(pattern)) - 3,
        *writeOffset = (u16 *)memsearch((u8 *)(readOffset + 5), pattern, 0x100, sizeof(pattern)) - 3;

    *readOffset = *writeOffset = 0x4C00;
    readOffset[1] = writeOffset[1] = 0x47A0;
    ((u32 *)writeOffset)[1] = ((u32 *)readOffset)[1] = branchOffset;
}

static inline void patchMpu(u8 *pos, u32 size)
{
    //Look for MPU pattern
    const u8 pattern[] = {0x03, 0x00, 0x24, 0x00};

    u32 *off = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));

    off[0] = 0x00360003;
    off[6] = 0x00200603;
    off[9] = 0x001C0603;
}

void patchEmuNand(u8 *arm9Section, u32 arm9SectionSize, u8 *process9Offset, u32 process9Size, u32 emuHeader, u32 branchAdditive)
{
    //Copy EmuNAND code
    u8 *freeK9Space = getFreeK9Space(arm9Section, arm9SectionSize);
    memcpy(freeK9Space, emunand_bin, emunand_bin_size);

    //Add the data of the found EmuNAND
    u32 *posOffset = (u32 *)memsearch(freeK9Space, "NAND", emunand_bin_size, 4),
        *posHeader = (u32 *)memsearch(freeK9Space, "NCSD", emunand_bin_size, 4);
    *posOffset = emuOffset;
    *posHeader = emuHeader;

    //Find and add the SDMMC struct
    u32 *posSdmmc = (u32 *)memsearch(freeK9Space, "SDMC", emunand_bin_size, 4);
    *posSdmmc = getSdmmc(process9Offset, process9Size);

    //Add EmuNAND hooks
    u32 branchOffset = (u32)freeK9Space - branchAdditive;
    patchNandRw(process9Offset, process9Size, branchOffset);

    //Set MPU
    patchMpu(arm9Section, arm9SectionSize);
}