/*
*   exceptions.c
*       by TuxSH
*   Copyright (c) 2016 All Rights Reserved
*/

#include "exceptions.h"
#include "fs.h"
#include "fatfs/ff.h"
#include "memory.h"
#include "utils.h"
#include "../build/arm9_exceptions.h"

#define PAYLOAD_ADDRESS 0x01FF8000

void installARM9Handlers(void)
{
    memcpy((void *)PAYLOAD_ADDRESS, arm9_exceptions, arm9_exceptions_size);
    ((void (*)())PAYLOAD_ADDRESS)();
}

static char* itoa8(unsigned int n, char* out)
{
    for(int i = 0; i < 8; ++i) out[i] = '0';
    int i = 7;
    do
    {
        out[i--] = '0'+(n%10);
        n /= 10;
    }
    while(n != 0 && i >= 0);
    
    for(int j = 0; j < 7-i; ++j)
        out[j] = out[i+1+j];
    
    out[7-i] = 0;
    return out + 7 - i;
}

void detectAndProcessExceptionDumps(void)
{
    vu32* dump = (vu32 *)0x25000000;
    if(dump[0] != 0xDEADC0DE || dump[1] != 0xDEADCAFE || dump[3] != 9) return;
    u32 sz = dump[5];
    
    DIR dir;
    FILINFO info;
    char fileName[] = "crash_dump_00000000.dmp", fileNameExt[] = ".dmp";
    unsigned int n = 0;
    FRESULT result = FR_NOT_READY;
    
    result = FR_NOT_READY;
    do
    {
        char* pos = itoa8(n++, fileName + 11);
        memcpy(pos, fileNameExt, sizeof(fileNameExt));
        result = f_findfirst(&dir, &info, "/aurei/dumps/arm9", fileName);
    }
    while (result == FR_OK && info.fname[0]);
    f_closedir(&dir);

    char completePath[] = "/aurei/dumps/arm9/crash_dump_00000000.dmp";
    memcpy(completePath+18, fileName, sizeof(fileName));
    
    fileWrite((void *)dump, completePath, sz);
    
    error("An ARM9 exception occured.\nPlease check your /aurei/dumps/arm9 folder.");
}