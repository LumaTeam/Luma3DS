/*
*   emunand.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "emunand.h"
#include "fatfs/ff.h"
#include "fatfs/sdmmc/sdmmc.h"

typedef struct emunand {
    u32 offset;
    u32 header;
    const char* name;
} emunand;

emunand emunands[] = {
        {.offset = 1, .header = 1, .name = "redNAND"},
        {.offset = 0, .header = 0x1D7800, .name = "Toshiba GW/MT"},
        {.offset = 0, .header = 0x1DD000, .name = "Samsung GW/MT"},
        {.offset = 0, .header = 0x26C000, .name = "Samsung N3DS GW"},
        {.offset = 0, .header = 0x3B0000, .name = "Unknown N3DS GW"},
        {.offset = 0, .header = 0, .name = 0},
};

static u8 *temp = (u8 *)0x24300000;

u8 getEmunand(u32 *off, u32 *head){
    u8 ret = 0;
    for(int i = 0; emunands[i].name; i++){
        if (sdmmc_sdcard_readsectors(emunands[i].header, 1, temp) == 0) {
            if (*(u32 *)(temp + 0x100) == NCSD_MAGIC) {
                *off = (u32)&emunands[i].offset;
                *head = (u32)&emunands[i].header;
                ret = 1;
                break;
            }
        }
    }
    return ret;
}