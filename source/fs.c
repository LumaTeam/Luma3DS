/*
*   fs.c
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
    FRESULT result;
    FIL file;

    result = f_open(&file, path, FA_READ);
    if(result == FR_OK)
    {
        unsigned int read;
        if(!size) size = f_size(&file);
        result = f_read(&file, dest, size, &read);
    }

    f_close(&file);

    return result;
}

u32 fileWrite(const void *buffer, const char *path, u32 size)
{
    FRESULT result;
    FIL file;

    result = f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS);
    if(result == FR_OK)
    {
        unsigned int read;
        result = f_write(&file, buffer, size, &read);
    }

    f_close(&file);

    return result;
}

u32 defPayloadExists(void)
{
    DIR dir;
    FILINFO info;

    FRESULT result = f_findfirst(&dir, &info, "/luma/payloads", "def_*.bin");

    f_closedir(&dir);

    return (result == FR_OK && info.fname[0]);
}

void findDumpFile(const char *path, char *fileName)
{
    DIR dir;
    FILINFO info;
    u32 n = 0;

    while(f_findfirst(&dir, &info, path, fileName) == FR_OK && info.fname[0])
    {
        u32 i = 18,
            tmp = ++n;

        while(tmp)
        {
            fileName[i--] = '0' + (tmp % 10);
            tmp /= 10;
        }
    }

    f_closedir(&dir);
}

void firmRead(void *dest, const char *firmFolder)
{
    char path[48] = "1:/title/00040138/00000000/content";
    memcpy(&path[18], firmFolder, 8);

    DIR dir;
    FILINFO info;

    f_opendir(&dir, path);

    u32 id = 0xFFFFFFFF;

    //Parse the target directory
    while(f_readdir(&dir, &info) == FR_OK && info.fname[0])
    {
        //Not a cxi
        if(info.altname[9] != 'A') continue;

        //Convert the .app name to an integer
        u32 tempId = 0;
        for(char *tmp = info.altname; *tmp != '.'; tmp++)
        {
            tempId <<= 4;
            tempId += *tmp > '9' ? *tmp - 'A' + 10 : *tmp - '0';
        }

        //Found an older cxi
        if(tempId < id) id = tempId;
    }

    f_closedir(&dir);

    //Complete the string with the .app name
    memcpy(&path[34], "/00000000.app", 14);

    //Last digit of the .app
    u32 i = 42;

    //Convert back the .app name from integer to array
    while(id)
    {
        static const char hexDigits[] = "0123456789ABCDEF";
        path[i--] = hexDigits[id & 0xF];
        id >>= 4;
    }

    fileRead(dest, path, 0);
}