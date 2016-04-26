/*
*   exceptions.c
*       by TuxSH
*/

#include "exceptions.h"
#include "fs.h"
#include "memory.h"
#include "utils.h"
#include "../build/arm9_exceptions.h"

#define PAYLOAD_ADDRESS 0x01FF8000

void installArm9Handlers(void)
{
    memcpy((void *)PAYLOAD_ADDRESS, arm9_exceptions, arm9_exceptions_size);
    ((void (*)())PAYLOAD_ADDRESS)();
}

void detectAndProcessExceptionDumps(void)
{
    vu32 *dump = (u32 *)0x25000000;

    if(dump[0] == 0xDEADC0DE && dump[1] == 0xDEADCAFE && dump[3] == 9)
    {
        char path[41] = "/luma/dumps/arm9";
        char fileName[] = "crash_dump_00000000.dmp";

        findDumpFile(path, fileName);

        path[16] = '/';
        memcpy(&path[17], fileName, sizeof(fileName));

        fileWrite((void *)dump, path, dump[5]);

        error("An ARM9 exception occured.\nPlease check your /luma/dumps/arm9 folder.");
    }
}