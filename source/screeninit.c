/*
*   screeninit.c
*
*   Screen init code by dark_samus, bil1s, Normmatt, delebile and others.
*   Screen deinit code by tiniVi.
*/

#include "screeninit.h"
#include "config.h"
#include "memory.h"
#include "draw.h"
#include "i2c.h"
#include "../build/screeninit.h"

vu32 *arm11Entry = (u32 *)0x1FFFFFF8;

void deinitScreens(void)
{
    void __attribute__((naked)) ARM11(void)
    {
        //Disable interrupts
        __asm(".word 0xF10C01C0");

        //Clear ARM11 entry offset
        *arm11Entry = 0;

        //Shutdown LCDs
        *(vu32 *)0x10202A44 = 0;
        *(vu32 *)0x10202244 = 0;
        *(vu32 *)0x10202014 = 0;

        //Wait for the entry to be set
        while(!*arm11Entry);

        //Jump to it
        ((void (*)())*arm11Entry)();
    }

    if(PDN_GPU_CNT != 1)
    {
        *arm11Entry = (u32)ARM11;
        while(*arm11Entry);

        PDN_GPU_CNT = 1;
    }
}

u32 initScreens(void)
{
    u32 needToInit = PDN_GPU_CNT == 1;

    if(needToInit)
    {
        u32 *const screenInitAddress = (u32 *)0x24FFFC00;

        memcpy(screenInitAddress, screeninit, screeninit_size);

        //Write brightness level for the stub to pick up
        screenInitAddress[2] = MULTICONFIG(0);

        *arm11Entry = (u32)screenInitAddress;
        while(*arm11Entry);

        //Turn on backlight
        i2cWriteRegister(I2C_DEV_MCU, 0x22, 0x2A);
    }

    clearScreens();

    return needToInit;
}