/*
*   fs.c
*/

#include <stddef.h>
#include "fs.h"
#include "fatfs/ff.h"

static FATFS fs;

int mountSD(void){
    if(f_mount(&fs, "0:", 1) != FR_OK) return 0;
    return 1;
}

int fileRead(u8 *dest, const char *path, u32 size){
    FRESULT fr;
    FIL fp;
    unsigned int br = 0;

    fr = f_open(&fp, path, FA_READ);
    if(fr == FR_OK){
        if(!size) size = f_size(&fp);
        fr = f_read(&fp, dest, size, &br);
    }

    f_close(&fp);
    return fr ? 0 : 1;
}

int fileWrite(const u8 *buffer, const char *path, u32 size){
    FRESULT fr;
    FIL fp;
    unsigned int br = 0;

    f_unlink(path);

    fr = f_open(&fp, path, FA_WRITE | FA_OPEN_ALWAYS);
    if(fr == FR_OK) fr = f_write(&fp, buffer, size, &br);

    f_close(&fp);
    return fr ? 0 : 1;
}

int fileSize(const char* path){
    FIL fp;
    int size = 0;

    if(f_open(&fp, path, FA_READ) == FR_OK)
        size = f_size(&fp);

    f_close(&fp);
    return size;
}

int fileExists(const char* path){
    FIL fp;
    int exists = 0;

    if(f_open(&fp, path, FA_READ) == FR_OK) exists = 1;

    f_close(&fp);
    return exists;
}