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
#include "../build/emunandpatch.h"

#define O3DS_TOSHIBA_NAND	0x1DD000

#define O3DS_LEGACY_FAT		0x200000
#define O3DS_DEFAULT_FAT	0x1DE000
#define O3DS_MINIMUM_FAT	0x1D8000

#define N3DS_LEGACY_FAT		0x400000
#define N3DS_DEFAULT_FAT	0x3B2000
#define N3DS_MINIMUM_FAT	0x26E000

void locateEmuNand(u32 *off, u32 *head, FirmwareSource *emuNand)
{
    static u8 temp[0x200];
	
    const u32 nandSize = getMMCDevice(0)->total_size;
	const u32 nandLayoutO3DS[3] = { O3DS_LEGACY_FAT, O3DS_DEFAULT_FAT, O3DS_MINIMUM_FAT }; // Legacy, Default, Minimum
	const u32 nandLayoutN3DS[3] = { N3DS_LEGACY_FAT, N3DS_DEFAULT_FAT, N3DS_MINIMUM_FAT }; // Legacy, Default, Minimum
	
	u8 i;
	u32 nandOffset;
	bool found = false;
	
	for (i = 0; i < 3; i++)
	{
		 if (i > 0 && *emuNand != FIRMWARE_EMUNAND2) break;
		
		// Check for 'Legacy', 'Default' and 'Minimum' partition layouts when checking for the 2nd EmuNAND
		nandOffset = (*emuNand == FIRMWARE_EMUNAND ? 0 : ((isN3DS || nandSize > O3DS_TOSHIBA_NAND) ? nandLayoutN3DS[i] : nandLayoutO3DS[i]));
		
		// Exception for 2DS
		if (i == 2 && !isN3DS && nandOffset == N3DS_MINIMUM_FAT) nandOffset = O3DS_MINIMUM_FAT;
		
		//Check for RedNAND
		if(!sdmmc_sdcard_readsectors(nandOffset + 1, 1, temp) && *(u32 *)(temp + 0x100) == NCSD_MAGIC)
		{
			*off = nandOffset + 1;
			*head = nandOffset + 1;
			found = true;
			break;
		}
		
		//Check for Gateway emuNAND
		else if(!sdmmc_sdcard_readsectors(nandOffset + nandSize, 1, temp) && *(u32 *)(temp + 0x100) == NCSD_MAGIC)
		{
			*off = nandOffset;
			*head = nandOffset + nandSize;
			found = true;
			break;
		}
	}
	
    /* Fallback to the first emuNAND if there's no second one,
       or to SysNAND if there isn't any */
	if (!found)
	{
        *emuNand = (*emuNand == FIRMWARE_EMUNAND2) ? FIRMWARE_EMUNAND : FIRMWARE_SYSNAND;
        if(*emuNand) locateEmuNand(off, head, emuNand);
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
    const u16 nandRedir[2] = {0x4C00, 0x47A0};

    //Look for read/write code
    const u8 pattern[] = {0x1E, 0x00, 0xC8, 0x05};

    u16 *readOffset = (u16 *)memsearch(pos, pattern, size, sizeof(pattern)) - 3,
        *writeOffset = (u16 *)memsearch((u8 *)(readOffset + 5), pattern, 0x100, sizeof(pattern)) - 3;

    *readOffset = nandRedir[0];
    readOffset[1] = nandRedir[1];
    ((u32 *)readOffset)[1] = branchOffset;
    *writeOffset = nandRedir[0];
    writeOffset[1] = nandRedir[1];
    ((u32 *)writeOffset)[1] = branchOffset;
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
    //Copy emuNAND code
    u8 *freeK9Space = getFreeK9Space(arm9Section, arm9SectionSize);
    memcpy(freeK9Space, emunand, emunand_size);

    //Add the data of the found emuNAND
    u32 *posOffset = (u32 *)memsearch(freeK9Space, "NAND", emunand_size, 4),
        *posHeader = (u32 *)memsearch(freeK9Space, "NCSD", emunand_size, 4);
    *posOffset = emuOffset;
    *posHeader = emuHeader;

    //Find and add the SDMMC struct
    u32 *posSdmmc = (u32 *)memsearch(freeK9Space, "SDMC", emunand_size, 4);
    *posSdmmc = getSdmmc(process9Offset, process9Size);

    //Add emuNAND hooks
    u32 branchOffset = (u32)freeK9Space - branchAdditive;
    patchNandRw(process9Offset, process9Size, branchOffset);

    //Set MPU for emu code region
    patchMpu(arm9Section, arm9SectionSize);
}