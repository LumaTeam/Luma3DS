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

static void hexItoa(u32 n, char *out)
{
    const char hexDigits[] = "0123456789ABCDEF";
    u32 i = 0;

    while(n > 0)
    {
        out[7 - i++] = hexDigits[n & 0xF];
        n >>= 4;
    }

    for(; i < 8; i++) out[7 - i] = '0';
}

void detectAndProcessExceptionDumps(void)
{
    const char *handledExceptionNames[] = { 
        "FIQ", "undefined instruction", "prefetch abort", "data abort"
    };
    
    const char *registerNames[] = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12",
        "SP", "LR", "PC", "CPSR"
    };
    
    char hexstring[] = "00000000";
    
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

        drawString("An exception occurred", 10, 10, COLOR_RED);
        int posY = drawString("Processor:      ARM9", 10, 30, COLOR_WHITE) + SPACING_Y;
        posY = drawString("Exception type: ", 10, posY, COLOR_WHITE);
        posY = drawString(handledExceptionNames[dump[4]], 10 + 16 * SPACING_X, posY, COLOR_WHITE);

        posY += 3 * SPACING_Y;
        for(u32 i = 0; i < 17; i += 2)
        {
            posY = drawString(registerNames[i], 10, posY, COLOR_WHITE);
            hexItoa(dump[10 + i], hexstring);
            posY = drawString(hexstring, 10 + 7 * SPACING_X, posY, COLOR_WHITE);

            if(i != 16)
            {
                posY = drawString(registerNames[i + 1], 10 + 22 * SPACING_X, posY, COLOR_WHITE);
                hexItoa(dump[10 + i + 1], hexstring);
                posY = drawString(hexstring, 10 + 29 * SPACING_X, posY, COLOR_WHITE);
            }

            posY += SPACING_Y;
        }

        posY += 2 * SPACING_Y;
        posY = drawString("You can find a dump in the following file:", 10, posY, COLOR_WHITE) + SPACING_Y;
        posY = drawString(path, 10, posY, COLOR_WHITE) + 2 * SPACING_Y;
        drawString("Press any button to shutdown", 10, posY, COLOR_WHITE);

        waitInput();

        i2cWriteRegister(I2C_DEV_MCU, 0x20, 1);
        while(1);
    }
}