/*
*   emunand.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "emunand.h"
#include "memory.h"
#include "fatfs/sdmmc/sdmmc.h"

void getEmunandSect(u32 *off, u32 *head, u32 emuNAND){
    u8 *const temp = (u8 *)0x24300000;

    u32 nandSize = getMMCDevice(0)->total_size;
    u32 nandOffset = emuNAND == 1 ? 0 :
                                  (nandSize > 0x200000 ? 0x400000 : 0x200000);

    if(sdmmc_sdcard_readsectors(nandOffset + nandSize, 1, temp) == 0){
        if(*(u32 *)(temp + 0x100) == NCSD_MAGIC){
            *off = nandOffset;
            *head = nandOffset + nandSize;
        }
        //Fallback to the first emuNAND if there's no second one
        else if(emuNAND == 2) getEmunandSect(off, head, 1);
        //Check if a RedNAND is present
        else if(sdmmc_sdcard_readsectors(1, 1, temp) == 0){
            if(*(u32 *)(temp + 0x100) != NCSD_MAGIC)
                *head = 0;
        }
    }
}

void getSDMMC(void *pos, u32 *off, u32 size){
    //Look for struct code
    const unsigned char pattern[] = {0x21, 0x20, 0x18, 0x20};
    *off = (u32)memsearch(pos, pattern, size, 4) - 1;
    
    //Get DCD values
    u8 buf[4];
    u32 addr = 0,
        additive = 0,
        p;
    memcpy(buf, (void *)(*off+0x0A), 4);
    for (p = 0; p < 4; p++) addr |= ((u32) buf[p]) << (8 * p);
    memcpy(buf, (void *)(*off+0x0E), 4);
    for (p = 0; p < 4; p++) additive |= ((u32) buf[p]) << (8 * p);
    
    //Return result
    *off = addr + additive;
}

void getEmuRW(void *pos, u32 size, u32 *readOff, u32 *writeOff){
    //Look for read/write code
    const unsigned char pattern[] = {0x1E, 0x00, 0xC8, 0x05};

    *writeOff = (u32)memsearch(pos, pattern, size, 4) - 6;
    *readOff = (u32)memsearch((void *)(*writeOff - 0x1000), pattern, 0x1000, 4) - 6;
}

void getMPU(void *pos, u32 *off, u32 size){
    //Look for MPU pattern
    const unsigned char pattern[] = {0x03, 0x00, 0x24, 0x00};

    *off = (u32)memsearch(pos, pattern, size, 4);
}

void getEmuCode(void *pos, u32 *off, u32 size){
    u8 *proc9 = (u8 *)memsearch(pos, "ess9", size, 4);
    const unsigned char pattern[] = {0x00, 0xFF, 0xFF, 0xFF};
 
    //Looking for the last spot before Process9
    *off = (u32)memsearch(pos, pattern, size - (size - (u32)(proc9 - (u8 *)pos)), 4) + 0xD;
}