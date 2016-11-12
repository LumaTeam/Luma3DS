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
#include "strings.h"
#include "crypto.h"
#include "cache.h"
#include "screen.h"
#include "fatfs/ff.h"
#include "buttons.h"
#include "../build/bundled.h"

static FATFS sdFs,
             nandFs;

static bool switchToMainDir(bool isSd)
{
    const char *mainDir = isSd ? "/luma" : "/rw/luma";
    bool ret;

    switch(f_chdir(mainDir))
    {
        case FR_OK:
            ret = true;
            break;
        case FR_NO_PATH:
            f_mkdir(mainDir);
            ret = switchToMainDir(isSd);
            break;
        default:
            ret = false;
            break;
    }

    return ret;
}

bool mountFs(bool isSd, bool switchToCtrNand)
{
    return isSd ? f_mount(&sdFs, "0:", 1) == FR_OK && switchToMainDir(true) :
                  f_mount(&nandFs, "1:", 1) == FR_OK && (!switchToCtrNand || (f_chdrive("1:") == FR_OK && switchToMainDir(false)));
}

u32 fileRead(void *dest, const char *path, u32 maxSize)
{
    FIL file;
    u32 ret;

    if(f_open(&file, path, FA_READ) != FR_OK) ret = 0;
    else
    {
        u32 size = f_size(&file);
        if(dest == NULL) ret = size;
        else if(size <= maxSize)
            f_read(&file, dest, size, (unsigned int *)&ret);
        f_close(&file);
    }

    return ret;
}

u32 getFileSize(const char *path)
{
    return fileRead(NULL, path, 0);
}

bool fileWrite(const void *buffer, const char *path, u32 size)
{
    FIL file;
    bool ret;

    switch(f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS))
    {
        case FR_OK:
        {
            unsigned int written;
            f_write(&file, buffer, size, &written);
            f_truncate(&file);
            f_close(&file);

            ret = (u32)written == size;
            break;
        }
        case FR_NO_PATH:
            for(u32 i = 1; path[i] != 0; i++)
                if(path[i] == '/')
                {
                    char folder[i + 1];
                    memcpy(folder, path, i);
                    folder[i] = 0;
                    f_mkdir(folder);
                }

            ret = fileWrite(buffer, path, size);
            break;
        default:
            ret = false;
            break;
    }

    return ret;
}

void fileDelete(const char *path)
{
    f_unlink(path);
}

void loadPayload(u32 pressed)
{
    const char *pattern;

    if(pressed & BUTTON_LEFT) pattern = PATTERN("left");
    else if(pressed & BUTTON_RIGHT) pattern = PATTERN("right");
    else if(pressed & BUTTON_UP) pattern = PATTERN("up");
    else if(pressed & BUTTON_DOWN) pattern = PATTERN("down");
    else if(pressed & BUTTON_START) pattern = PATTERN("start");
    else if(pressed & BUTTON_B) pattern = PATTERN("b");
    else if(pressed & BUTTON_X) pattern = PATTERN("x");
    else if(pressed & BUTTON_Y) pattern = PATTERN("y");
    else if(pressed & BUTTON_R1) pattern = PATTERN("r");
    else if(pressed & BUTTON_A) pattern = PATTERN("a");
    else pattern = PATTERN("select");

    DIR dir;
    FILINFO info;
    char path[22] = "payloads";

    FRESULT result = f_findfirst(&dir, &info, path, pattern);

    if(result == FR_OK)
    {
        f_closedir(&dir);

        if(info.fname[0] != 0)
        {
            u32 *loaderAddress = (u32 *)0x24FFFE00;
            u8 *payloadAddress = (u8 *)0x24F00000;

            memcpy(loaderAddress, loader_bin, loader_bin_size);

            concatenateStrings(path, "/");
            concatenateStrings(path, info.altname);

            u32 payloadSize = fileRead(payloadAddress, path, (u32)((u8 *)loaderAddress - payloadAddress));

            if(payloadSize > 0)
            {
                loaderAddress[1] = payloadSize;

                backupAndRestoreShaHash(true);
                initScreens();

                flushDCacheRange(loaderAddress, loader_bin_size);
                flushICacheRange(loaderAddress, loader_bin_size);

                ((void (*)())loaderAddress)();
            }
        }
    }
}

u32 firmRead(void *dest, u32 firmType)
{
    const char *firmFolders[][2] = {{ "00000002", "20000002" },
                                    { "00000102", "20000102" },
                                    { "00000202", "20000202" },
                                    { "00000003", "20000003" },
                                    { "00000001", "20000001" }};

    char path[48] = "1:/title/00040138/";
    concatenateStrings(path, firmFolders[firmType][ISN3DS ? 1 : 0]);
    concatenateStrings(path, "/content");

    DIR dir;
    u32 firmVersion = 0xFFFFFFFF;

    if(f_opendir(&dir, path) == FR_OK)
    {
        FILINFO info;

        //Parse the target directory
        while(f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0)
        {
            //Not a cxi
            if(info.fname[9] != 'a' || strlen(info.fname) != 12) continue;

            u32 tempVersion = hexAtoi(info.altname, 8);

            //Found an older cxi
            if(tempVersion < firmVersion) firmVersion = tempVersion;
        }

        f_closedir(&dir);

        if(firmVersion != 0xFFFFFFFF)
        {
            //Complete the string with the .app name
            concatenateStrings(path, "/00000000.app");

            //Convert back the .app name from integer to array
            hexItoa(firmVersion, path + 35, 8, false);

            if(fileRead(dest, path, 0x400000 + sizeof(Cxi) + 0x200) <= sizeof(Cxi) + 0x200) firmVersion = 0xFFFFFFFF;
        }
    }

    return firmVersion;
}

void findDumpFile(const char *path, char *fileName)
{
    DIR dir;
    FRESULT result;
    u32 n = 0;

    while(n <= 99999999)
    {
        FILINFO info;

        result = f_findfirst(&dir, &info, path, fileName);

        if(result != FR_OK || !info.fname[0]) break;

        decItoa(++n, fileName + 11, 8);
    }

    if(result == FR_OK) f_closedir(&dir);
}