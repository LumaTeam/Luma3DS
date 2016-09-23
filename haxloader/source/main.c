/*
*   This pathFile is part of Luma3DS
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
*   Additional Terms 7.b of GPLv3 applies to this pathFile: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

#include "memory.h"
#include "cache.h"
#include "types.h"
#include "fatfs/ff.h"
#include "../../build/bundled.h"

static FATFS fs;

void main()
{
    if(f_mount(&fs, "0:", 0) == FR_OK)
    {
        u32 *loaderAddress = (u32 *)0x24FFFF00;
        u8 *payloadAddress = (u8 *)0x24F00000;
        FIL pathFile,
            payload;
        bool useDefault = true;

        memcpy(loaderAddress, loader_bin, loader_bin_size);

        if(f_open(&pathFile, "/luma/path.txt", FA_READ) == FR_OK)
        {
            u32 pathSize = f_size(&pathFile);

            if(pathSize > 5 && pathSize < 40)
            {
                char path[pathSize];
                unsigned int read;
                f_read(&pathFile, path, pathSize, &read);
                if(path[pathSize - 1] == 0xA) pathSize--;
                if(path[pathSize - 1] == 0xD) pathSize--;

                if(pathSize > 5 && pathSize < 38 && path[0] == '/' && memcmp(&path[pathSize - 4], ".bin", 4) == 0)
                {
                    path[pathSize] = 0;
                    useDefault = f_open(&payload, path, FA_READ) == FR_OK;
                }
            }

            f_close(&pathFile);
        }

        if(!useDefault) f_open(&payload, "arm9loaderhax.bin", FA_READ);

        u32 payloadSize = f_size(&payload);

        if(payloadSize > 0)
        {
            unsigned int read;
            f_read(&payload, payloadAddress, payloadSize, &read);
            loaderAddress[1] = payloadSize;

            flushDCacheRange(loaderAddress, loader_bin_size);
            flushICacheRange(loaderAddress, loader_bin_size);

            ((void (*)())loaderAddress)();
        }
    }

    while(true);
}