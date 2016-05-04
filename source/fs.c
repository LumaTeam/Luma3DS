/*
*   fs.c
*/

#include "fs.h"
#include "memory.h"
#include "screeninit.h"
#include "exceptions.h"
#include "fatfs/ff.h"
#include "buttons.h"
#include "../build/loader.h"

static FATFS sdFs,
             nandFs;

u32 mountFs(void)
{
    if(f_mount(&sdFs, "0:", 1) != FR_OK) return 0;
    f_mount(&nandFs, "1:", 0);

    return 1;
}

u32 fileRead(void *dest, const char *path)
{
    FIL file;
    u32 size;

    if(f_open(&file, path, FA_READ) == FR_OK)
    {
        unsigned int read;
        size = f_size(&file);
        f_read(&file, dest, size, &read);
        f_close(&file);
    }
    else size = 0;

    return size;
}

void fileWrite(const void *buffer, const char *path, u32 size)
{
    FIL file;

    if(f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS) == FR_OK)
    {
        unsigned int written;
        f_write(&file, buffer, size, &written);
        f_close(&file);
    }
}

void loadPayload(u32 pressed, u32 devMode)
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
    else if(pressed & BUTTON_SELECT) pattern = PATTERN("select");
    else pattern = "nlc.bin";

    DIR dir;
    FILINFO info;
    char path[28] = "/luma/payloads";

    FRESULT result = f_findfirst(&dir, &info, path, pattern);

    f_closedir(&dir);

    if(result == FR_OK && info.fname[0])
    {
        //Only when "Enable developer features" is set
        if(devMode) installArm9Handlers();

        initScreens();

        u32 *const loaderAddress = (u32 *)0x24FFFB00;

        memcpy(loaderAddress, loader, loader_size);

        path[14] = '/';
        memcpy(&path[15], info.altname, 13);

        loaderAddress[1] = fileRead((void *)0x24F00000, path);

        ((void (*)())loaderAddress)();
    }
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

    fileRead(dest, path);
}