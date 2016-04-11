/*
*   fs.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "fs.h"
#include "memory.h"
#include "fatfs/ff.h"

static FATFS sdFs,
             nandFs;

u32 mountFs(void)
{
    if(f_mount(&sdFs, "0:", 1) != FR_OK) return 0;
    f_mount(&nandFs, "1:", 0);
    return 1;
}

u32 fileRead(void *dest, const char *path, u32 size)
{
    FRESULT fr;
    FIL fp;
    unsigned int br = 0;

    fr = f_open(&fp, path, FA_READ);
    if(fr == FR_OK)
    {
        if(!size) size = f_size(&fp);
        fr = f_read(&fp, dest, size, &br);
    }

    f_close(&fp);
    return fr ? 0 : 1;
}

u32 fileWrite(const void *buffer, const char *path, u32 size)
{
    FRESULT fr;
    FIL fp;
    unsigned int br = 0;

    fr = f_open(&fp, path, FA_WRITE | FA_OPEN_ALWAYS);
    if(fr == FR_OK) fr = f_write(&fp, buffer, size, &br);

    f_close(&fp);
    return fr ? 0 : 1;
}

u32 fileSize(const char *path)
{
    FIL fp;
    u32 size = 0;

    if(f_open(&fp, path, FA_READ) == FR_OK)
        size = f_size(&fp);

    f_close(&fp);
    return size;
}

u32 fileExists(const char *path)
{
    FIL fp;
    u32 exists = 0;

    if(f_open(&fp, path, FA_READ) == FR_OK) exists = 1;

    f_close(&fp);
    return exists;
}

void firmRead(void *dest, const char *firmFolder)
{
    char path[48] = "1:/title/00040138/00000000/content";
    memcpy(&path[18], firmFolder, 8);

    DIR dir;
    FILINFO info = { .lfname = NULL };

    f_opendir(&dir, path);

    u32 id = 0;

    //Parse the target directory
    while(f_readdir(&dir, &info) == FR_OK)
    {
        //We've parsed the whole folder
        if(!info.fname[0]) break;

        //Not a cxi
        if(info.fname[9] != 'a' && info.fname[9] != 'A') continue;

        //Convert the .app name to an integer
        u32 tempId = 0;
        for(char *tmp = info.fname; (*tmp) != '.'; tmp++)
        {
            if((*tmp) > '9') tempId = (tempId << 4) + ((*tmp) - 'A') + 9;
            else tempId = (tempId << 4) + (*tmp) - '0';
        }

        //Found a newer cxi
        if(tempId > id) id = tempId;
    }

    f_closedir(&dir);

    //Complete the string with the .app name
    memcpy(&path[34], "/00000000.app", 14);

    //Last digit of the .app
    u32 i = 42;

    //Convert back the .app name from integer to array
    while(id > 15)
    {
        u32 remainder = (id % 16);
        if(remainder > 9) path[i] = remainder - 9 + 'A';
        else path[i] = remainder + '0';
        id /= 16;
        i--;
    }
    if(id > 9) path[i] = id - 9 + 'A';
    else path[i] = id + '0';

    fileRead(dest, path, 0);
}