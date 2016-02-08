/*
*   emunand.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "emunand.h"
#include "memory.h"
#include "fatfs/ff.h"
#include "fatfs/sdmmc/sdmmc.h"

static u8 *temp = (u8*)0x24300000;

void getEmunandSect(u32 *off, u32 *head){
    u32 nandSize = getMMCDevice(0)->total_size;
    if (sdmmc_sdcard_readsectors(nandSize, 1, temp) == 0) {
        if (*(u32*)(temp + 0x100) == NCSD_MAGIC) {
            *off = 0;
            *head = nandSize;
        }
    }
}

void getSDMMC(void *pos, u32 *off, u32 size){
    //Look for struct code
    unsigned char pattern[] = {0x01, 0x21, 0x20, 0x18, 0x20, 0x30};
    *off = (u32)memsearch(pos, pattern, size, 6);
    
    //Get DCD values
    unsigned char buf[4];
    int p;
    u32 addr = 0;
    u32 additive = 0;
    memcpy((void*)buf, (void*)(*off+0x0A), 4);
    for (p = 0; p < 4; p++) addr |= ((u32) buf[p]) << (8 * p);
    memcpy((void*)buf, (void*)(*off+0x0E), 4);
    for (p = 0; p < 4; p++) additive |= ((u32) buf[p]) << (8 * p);
    
    //Return result
	*off = addr + additive;
}

void getEmuRW(void *pos, u32 size, u32 *readOff, u32 *writeOff){
    //Look for read/write code
    unsigned char pattern[] = {0x04, 0x00, 0x0D, 0x00, 0x17, 0x00, 0x1E, 0x00, 0xC8, 0x05};

    *writeOff = (u32)memsearch(pos, pattern, size, 10);
    *readOff = (u32)memsearch((void *)(*writeOff - 0x1000), pattern, 0x1000, 10);
}

void getMPU(void *pos, u32 *off, u32 size){
    //Look for MPU code
    unsigned char pattern[] = {0x03, 0x00, 0x24, 0x00, 0x00, 0x00, 0x10};

    *off = (u32)memsearch(pos, pattern, size, 7);
}

void getEmuCode(void *pos, u32 *off, u32 size){
    void *proc9 = memsearch(pos, "Process9", size, 8);
    unsigned char pattern[] = {0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF};

    //We're looking for the last spot before Process9
    *off = (u32)memsearch(pos, pattern, size - (size - (u32)(proc9 - pos)), 6) + 0xF;
}