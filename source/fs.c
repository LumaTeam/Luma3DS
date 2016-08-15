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

#include "fs.h"
#include "memory.h"
#include "cache.h"
#include "screen.h"
#include "fatfs/ff.h"
#include "buttons.h"
#include "../build/loader.h"

static FATFS sdFs,
             nandFs;

void mountFs(void)
{
    f_mount(&sdFs, "0:", 1);
    f_mount(&nandFs, "1:", 0);
}

u32 fileRead(void *dest, const char *path)
{
    FIL file;
    u32 size;

    if(f_open(&file, path, FA_READ) == FR_OK)
    {
        unsigned int read;
        size = f_size(&file);
        if(dest != NULL)
            f_read(&file, dest, size, &read);
        f_close(&file);
    }
    else size = 0;

    return size;
}

u32 getFileSize(const char *path)
{
    return fileRead(NULL, path);
}

bool fileWrite(const void *buffer, const char *path, u32 size)
{
    FIL file;

    if(f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS) == FR_OK)
    {
        unsigned int written;
        f_write(&file, buffer, size, &written);
        f_close(&file);

        return true;
    }

    return false;
}

void createDirectory(const char *path)
{
    f_mkdir(path);
}

void loadPayload(u32 pressed)
{
    const char *pattern;

    if(pressed & BUTTON_RIGHT) pattern = PATTERN("right");
    else if(pressed & BUTTON_LEFT) pattern = PATTERN("left");
    else if(pressed & BUTTON_UP) pattern = PATTERN("up");
    else if(pressed & BUTTON_DOWN) pattern = PATTERN("down");
    else if(pressed & BUTTON_X) pattern = PATTERN("x");
    else if(pressed & BUTTON_Y) pattern = PATTERN("y");
    else if(pressed & BUTTON_R1) pattern = PATTERN("r");
    else if(pressed & BUTTON_A) pattern = PATTERN("a");
    else if(pressed & BUTTON_START) pattern = PATTERN("start");
    else pattern = PATTERN("select");

    DIR dir;
    FILINFO info;
    char path[28] = "/luma/payloads";

    FRESULT result = f_findfirst(&dir, &info, path, pattern);

    f_closedir(&dir);

    if(result == FR_OK && info.fname[0])
    {
        initScreens();

        u32 *const loaderAddress = (u32 *)0x24FFFF00;

        memcpy(loaderAddress, loader, loader_size);

        path[14] = '/';
        memcpy(&path[15], info.altname, 13);

        loaderAddress[1] = fileRead((void *)0x24F00000, path);

        flushDCacheRange(loaderAddress, loader_size);
        flushICacheRange(loaderAddress, loader_size);

        ((void (*)())loaderAddress)();
    }
}

u32 firmRead(void *dest, u32 firmType)
{
    const char *firmFolders[4][2] = {{ "00000002", "20000002" },
                                    { "00000102", "20000102" },
                                    { "00000202", "20000202" },
                                    { "00000003", "20000003" }};

    char path[48] = "1:/title/00040138/00000000/content";
    memcpy(&path[18], firmFolders[firmType][isN3DS ? 1 : 0], 8);

    DIR dir;
    FILINFO info;

    f_opendir(&dir, path);

    u32 firmVersion = 0xFFFFFFFF;

    //Parse the target directory
    while(f_readdir(&dir, &info) == FR_OK && info.fname[0])
    {
        //Not a cxi
        if(info.altname[9] != 'A') continue;

        //Convert the .app name to an integer
        u32 tempVersion = 0;
        for(char *tmp = info.altname; *tmp != '.'; tmp++)
        {
            tempVersion <<= 4;
            tempVersion += *tmp > '9' ? *tmp - 'A' + 10 : *tmp - '0';
        }

        //Found an older cxi
        if(tempVersion < firmVersion) firmVersion = tempVersion;
    }

    f_closedir(&dir);

    //Complete the string with the .app name
    memcpy(&path[34], "/00000000.app", 14);

    //Last digit of the .app
    u32 i = 42;

    //Convert back the .app name from integer to array
    u32 tempVersion = firmVersion;
    while(tempVersion)
    {
        static const char hexDigits[] = "0123456789ABCDEF";
        path[i--] = hexDigits[tempVersion & 0xF];
        tempVersion >>= 4;
    }

    fileRead(dest, path);

    return firmVersion;
}