/*
*   emunand.c
*/

#include "emunand.h"
#include "memory.h"
#include "fatfs/sdmmc/sdmmc.h"

void getEmunandSect(u32 *off, u32 *head, u32 *emuNAND)
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
        if(*emuNAND) getEmunandSect(off, head, emuNAND);
    }
}

u32 getSDMMC(u8 *pos, u32 size)
{
    //Look for struct code
    const u8 pattern[] = {0x21, 0x20, 0x18, 0x20};
    const u8 *off = memsearch(pos, pattern, size, 4);

    return *(u32 *)(off + 9) + *(u32 *)(off + 0xD);
}

void getEmuRW(u8 *pos, u32 size, u32 *readOffset, u32 *writeOffset)
{
    //Look for read/write code
    const u8 pattern[] = {0x1E, 0x00, 0xC8, 0x05};

    *readOffset = (u32)memsearch(pos, pattern, size, 4) - 6;
    *writeOffset = (u32)memsearch((u8 *)(*readOffset + 0xA), pattern, 0x100, 4) - 6;
}

u32 *getMPU(u8 *pos, u32 size)
{
    //Look for MPU pattern
    const u8 pattern[] = {0x03, 0x00, 0x24, 0x00};

    return (u32 *)memsearch(pos, pattern, size, 4);
}

void *getEmuCode(u8 *pos)
{
    const u8 pattern[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

    //Looking for the last free space before Process9
    return memsearch(pos + 0x13500, pattern, 0x1000, 6) + 0x455;
}