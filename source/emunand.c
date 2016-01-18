/*
*   emunand.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "emunand.h"
#include "fatfs/ff.h"
#include "fatfs/sdmmc/sdmmc.h"

static u8 *temp = (u8*)0x24300000;

void getEmunand(u32 *off, u32 *head){
    u32 nandSize = getMMCDevice(0)->total_size;
    if (sdmmc_sdcard_readsectors(nandSize, 1, temp) == 0) {
        if (*(u32*)(temp + 0x100) == NCSD_MAGIC) {
            *off = 0;
            *head = nandSize;
        }
    }
}