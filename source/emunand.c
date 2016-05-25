/*
*   emunand.c
*/

#include "emunand.h"
#include "memory.h"
#include "fatfs/sdmmc/sdmmc.h"
#include "../build/emunandpatch.h"

void locateEmuNAND(u32 *off, u32 *head, u32 *emuNAND)
{
    static u8 *const temp = (u8 *)0x24300000;

    const u32 nandSize = getMMCDevice(0)->total_size;
    u32 nandOffset = *emuNAND == 1 ? 0 :
                                  (nandSize > 0x200000 ? 0x400000 : 0x200000);

    //Check for RedNAND
    if(!sdmmc_sdcard_readsectors(nandOffset + 1, 1, temp) &&
       *(u32 *)(temp + 0x100) == NCSD_MAGIC)
    {
        *off = nandOffset + 1;
        *head = nandOffset + 1;
    }

    //Check for Gateway emuNAND
    else if(!sdmmc_sdcard_readsectors(nandOffset + nandSize, 1, temp) &&
            *(u32 *)(temp + 0x100) == NCSD_MAGIC)
    {
        *off = nandOffset;
        *head = nandOffset + nandSize;
    }

    /* Fallback to the first emuNAND if there's no second one,
       or to SysNAND if there isn't any */
    else
    {
        (*emuNAND)--;
        if(*emuNAND) locateEmuNAND(off, head, emuNAND);
    }
}

static inline u32 getSDMMC(u8 *pos, u32 size)
{
    //Look for struct code
    const u8 pattern[] = {0x21, 0x20, 0x18, 0x20};
    const u8 *off = memsearch(pos, pattern, size, 4);

    return *(u32 *)(off + 9) + *(u32 *)(off + 0xD);
}

static inline void getEmuRW(u8 *pos, u32 size, u16 **readOffset, u16 **writeOffset)
{
    //Look for read/write code
    const u8 pattern[] = {0x1E, 0x00, 0xC8, 0x05};

    *readOffset = (u16 *)memsearch(pos, pattern, size, 4) - 3;
    *writeOffset = (u16 *)memsearch((u8 *)(*readOffset + 5), pattern, 0x100, 4) - 3;
}

static inline u32 *getMPU(u8 *pos, u32 size)
{
    //Look for MPU pattern
    const u8 pattern[] = {0x03, 0x00, 0x24, 0x00};

    return (u32 *)memsearch(pos, pattern, size, 4);
}

static inline void *getEmuCode(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

    //Looking for the last free space before Process9
    return memsearch(pos + 0x13500, pattern, size - 0x13500, 6) + 0x455;
}

void patchEmuNAND(u8 *arm9Section, u32 arm9SectionSize, u8 *process9Offset, u32 process9Size, u32 emuHeader, u32 branchAdditive)
{
    //Copy emuNAND code
    void *emuCodeOffset = getEmuCode(arm9Section, arm9SectionSize);
    memcpy(emuCodeOffset, emunand, emunand_size);

    //Add the data of the found emuNAND
    u32 *pos_offset = (u32 *)memsearch(emuCodeOffset, "NAND", emunand_size, 4);
    u32 *pos_header = (u32 *)memsearch(emuCodeOffset, "NCSD", emunand_size, 4);
    *pos_offset = emuOffset;
    *pos_header = emuHeader;

    //Find and add the SDMMC struct
    u32 *pos_sdmmc = (u32 *)memsearch(emuCodeOffset, "SDMC", emunand_size, 4);
    *pos_sdmmc = getSDMMC(process9Offset, process9Size);

    //Add emuNAND hooks
    u16 *emuRead,
        *emuWrite;
    u32 branchOffset = (u32)emuCodeOffset - branchAdditive;
    getEmuRW(process9Offset, process9Size, &emuRead, &emuWrite);
    const u16 nandRedir[2] = {0x4C00, 0x47A0};

    *emuRead = nandRedir[0];
    emuRead[1] = nandRedir[1];
    ((u32 *)emuRead)[1] = branchOffset;
    *emuWrite = nandRedir[0];
    emuWrite[1] = nandRedir[1];
    ((u32 *)emuWrite)[1] = branchOffset;

    //Set MPU for emu code region
    u32 *mpuOffset = getMPU(arm9Section, arm9SectionSize);
    const u32 mpuPatch[3] = {0x00360003, 0x00200603, 0x001C0603};

    *mpuOffset = mpuPatch[0];
    mpuOffset[6] = mpuPatch[1];
    mpuOffset[9] = mpuPatch[2];
}