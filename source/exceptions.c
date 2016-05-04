/*
*   exceptions.c
*       by TuxSH
*/

#include "exceptions.h"
#include "fs.h"
#include "memory.h"
#include "screeninit.h"
#include "draw.h"
#include "i2c.h"
#include "utils.h"
#include "../build/arm9_exceptions.h"

void installArm9Handlers(void)
{
    void *payloadAddress = (void *)0x01FF8000;

    memcpy(payloadAddress, arm9_exceptions, arm9_exceptions_size);
    ((void (*)())payloadAddress)();
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

        initScreens();

        drawString("An ARM9 exception occurred", 10, 10, COLOR_RED);
        int posY = drawString("You can find a dump in the following file:", 10, 30, COLOR_WHITE);
        posY = drawString(path, 10, posY + SPACING_Y, COLOR_WHITE);
        drawString("Press any button to shutdown", 10, posY + 2 * SPACING_Y, COLOR_WHITE);

        waitInput();

        i2cWriteRegister(I2C_DEV_MCU, 0x20, 1);
        while(1);
    }
}