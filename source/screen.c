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

/*
*   Screen init code by dark_samus, bil1s, Normmatt, delebile and others
*   Screen deinit code by tiniVi
*/

#include "screen.h"
#include "config.h"
#include "memory.h"
#include "cache.h"
#include "draw.h"
#include "i2c.h"

vu32 *const arm11Entry = (vu32 *)0x1FFFFFF8;
static const u32 brightness[4] = {0x5F, 0x4C, 0x39, 0x26};

void  __attribute__((naked)) arm11Stub(void)
{
    //Disable interrupts
    __asm(".word 0xF10C01C0");

    //Wait for the entry to be set
    while(*arm11Entry == ARM11_STUB_ADDRESS);

    //Jump to it
    ((void (*)())*arm11Entry)();
}

static void invokeArm11Function(void (*func)())
{
    static bool hasCopiedStub = false;
    if(!hasCopiedStub)
    {
        memcpy((void *)ARM11_STUB_ADDRESS, arm11Stub, 0x30);
        flushDCacheRange((void *)ARM11_STUB_ADDRESS, 0x30);
        hasCopiedStub = true;
    }

    *arm11Entry = (u32)func;
    while(*arm11Entry);
    *arm11Entry = ARM11_STUB_ADDRESS;
}

void deinitScreens(void)
{
    void __attribute__((naked)) ARM11(void)
    {
        //Disable interrupts
        __asm(".word 0xF10C01C0");

        //Shutdown LCDs
        *(vu32 *)0x10202A44 = 0;
        *(vu32 *)0x10202244 = 0;
        *(vu32 *)0x10202014 = 0;

        WAIT_FOR_ARM9();
    }

    if(PDN_GPU_CNT != 1) invokeArm11Function(ARM11);
}

void updateBrightness(u32 brightnessIndex)
{
    static u32 brightnessLevel;
    brightnessLevel = brightness[brightnessIndex];

    void __attribute__((naked)) ARM11(void)
    {
        //Disable interrupts
        __asm(".word 0xF10C01C0");

        //Change brightness
        *(vu32 *)0x10202240 = brightnessLevel;
        *(vu32 *)0x10202A40 = brightnessLevel;

        WAIT_FOR_ARM9();
    }

    flushDCacheRange(&brightnessLevel, 4);
    invokeArm11Function(ARM11);
}

void clearScreens(bool clearTop, bool clearBottom)
{
    static bool clearTopTmp,
                clearBottomTmp;
    clearTopTmp = clearTop;
    clearBottomTmp = clearBottom;

    void __attribute__((naked)) ARM11(void)
    {
        //Disable interrupts
        __asm(".word 0xF10C01C0");

        //Setting up two simultaneous memory fills using the GPU

        vu32 *REGs_PSC0 = (vu32 *)0x10400010,
             *REGs_PSC1 = (vu32 *)0x10400020;

        if(clearTopTmp)
        {
            REGs_PSC0[0] = (u32)fb->top_left >> 3; //Start address
            REGs_PSC0[1] = (u32)(fb->top_left + SCREEN_TOP_FBSIZE) >> 3; //End address
            REGs_PSC0[2] = 0; //Fill value
            REGs_PSC0[3] = (2 << 8) | 1; //32-bit pattern; start
        }

        if(clearBottomTmp)
        {
            REGs_PSC1[0] = (u32)fb->bottom >> 3; //Start address
            REGs_PSC1[1] = (u32)(fb->bottom + SCREEN_BOTTOM_FBSIZE) >> 3; //End address
            REGs_PSC1[2] = 0; //Fill value
            REGs_PSC1[3] = (2 << 8) | 1; //32-bit pattern; start
        }

        while(!((!clearTopTmp || (REGs_PSC0[3] & 2)) && (!clearBottomTmp || (REGs_PSC1[3] & 2))));

        if(fb->top_right != fb->top_left && clearTopTmp)
        {
            REGs_PSC0[0] = (u32)fb->top_right >> 3; //Start address
            REGs_PSC0[1] = (u32)(fb->top_right + SCREEN_TOP_FBSIZE) >> 3; //End address
            REGs_PSC0[2] = 0; //Fill value
            REGs_PSC0[3] = (2 << 8) | 1; //32-bit pattern; start

            while(!(REGs_PSC0[3] & 2));
        }

        WAIT_FOR_ARM9();
    }

    flushDCacheRange(&clearTopTmp, 1);
    flushDCacheRange(&clearBottomTmp, 1);
    flushDCacheRange((void *)fb, sizeof(struct fb));
    invokeArm11Function(ARM11);
}

void initScreens(void)
{
    void __attribute__((naked)) ARM11(void)
    {
        //Disable interrupts
        __asm(".word 0xF10C01C0");

        u32 brightnessLevel = brightness[MULTICONFIG(BRIGHTNESS)];
        
        *(vu32 *)0x10141200 = 0x1007F;
        *(vu32 *)0x10202014 = 0x00000001;
        *(vu32 *)0x1020200C &= 0xFFFEFFFE;
        *(vu32 *)0x10202240 = brightnessLevel;
        *(vu32 *)0x10202A40 = brightnessLevel;
        *(vu32 *)0x10202244 = 0x1023E;
        *(vu32 *)0x10202A44 = 0x1023E;

        // Top screen
        *(vu32 *)0x10400400 = 0x000001c2;
        *(vu32 *)0x10400404 = 0x000000d1;
        *(vu32 *)0x10400408 = 0x000001c1;
        *(vu32 *)0x1040040c = 0x000001c1;
        *(vu32 *)0x10400410 = 0x00000000;
        *(vu32 *)0x10400414 = 0x000000cf;
        *(vu32 *)0x10400418 = 0x000000d1;
        *(vu32 *)0x1040041c = 0x01c501c1;
        *(vu32 *)0x10400420 = 0x00010000;
        *(vu32 *)0x10400424 = 0x0000019d;
        *(vu32 *)0x10400428 = 0x00000002;
        *(vu32 *)0x1040042c = 0x00000192;
        *(vu32 *)0x10400430 = 0x00000192;
        *(vu32 *)0x10400434 = 0x00000192;
        *(vu32 *)0x10400438 = 0x00000001;
        *(vu32 *)0x1040043c = 0x00000002;
        *(vu32 *)0x10400440 = 0x01960192;
        *(vu32 *)0x10400444 = 0x00000000;
        *(vu32 *)0x10400448 = 0x00000000;
        *(vu32 *)0x1040045C = 0x00f00190;
        *(vu32 *)0x10400460 = 0x01c100d1;
        *(vu32 *)0x10400464 = 0x01920002;
        *(vu32 *)0x10400468 = 0x18300000;
        *(vu32 *)0x10400470 = 0x80341;
        *(vu32 *)0x10400474 = 0x00010501;
        *(vu32 *)0x10400478 = 0;
        *(vu32 *)0x10400490 = 0x000002D0;
        *(vu32 *)0x1040049C = 0x00000000;

        // Disco register
        for(u32 i = 0; i < 256; i++)
           *(vu32 *)0x10400484 = 0x10101 * i;

        // Bottom screen
        *(vu32 *)0x10400500 = 0x000001c2;
        *(vu32 *)0x10400504 = 0x000000d1;
        *(vu32 *)0x10400508 = 0x000001c1;
        *(vu32 *)0x1040050c = 0x000001c1;
        *(vu32 *)0x10400510 = 0x000000cd;
        *(vu32 *)0x10400514 = 0x000000cf;
        *(vu32 *)0x10400518 = 0x000000d1;
        *(vu32 *)0x1040051c = 0x01c501c1;
        *(vu32 *)0x10400520 = 0x00010000;
        *(vu32 *)0x10400524 = 0x0000019d;
        *(vu32 *)0x10400528 = 0x00000052;
        *(vu32 *)0x1040052c = 0x00000192;
        *(vu32 *)0x10400530 = 0x00000192;
        *(vu32 *)0x10400534 = 0x0000004f;
        *(vu32 *)0x10400538 = 0x00000050;
        *(vu32 *)0x1040053c = 0x00000052;
        *(vu32 *)0x10400540 = 0x01980194;
        *(vu32 *)0x10400544 = 0x00000000;
        *(vu32 *)0x10400548 = 0x00000011;
        *(vu32 *)0x1040055C = 0x00f00140;
        *(vu32 *)0x10400560 = 0x01c100d1;
        *(vu32 *)0x10400564 = 0x01920052;
        *(vu32 *)0x10400568 = 0x18300000 + 0x46500;
        *(vu32 *)0x10400570 = 0x80301;
        *(vu32 *)0x10400574 = 0x00010501;
        *(vu32 *)0x10400578 = 0;
        *(vu32 *)0x10400590 = 0x000002D0;
        *(vu32 *)0x1040059C = 0x00000000;

        // Disco register
        for(u32 i = 0; i < 256; i++)
           *(vu32 *)0x10400584 = 0x10101 * i;

        *(vu32 *)0x10400468 = 0x18300000;
        *(vu32 *)0x1040046c = 0x18300000;
        *(vu32 *)0x10400494 = 0x18300000;
        *(vu32 *)0x10400498 = 0x18300000;
        *(vu32 *)0x10400568 = 0x18346500;
        *(vu32 *)0x1040056c = 0x18346500;

        //Set CakeBrah framebuffers
        fb->top_left = (u8 *)0x18300000;
        fb->top_right = (u8 *)0x18300000;
        fb->bottom = (u8 *)0x18346500;

        WAIT_FOR_ARM9();
    }

    if(PDN_GPU_CNT == 1)
    {
        flushDCacheRange(&configData, sizeof(CfgData));
        flushDCacheRange((void *)fb, sizeof(struct fb));
        invokeArm11Function(ARM11);

        clearScreens(true, true);

        //Turn on backlight
        i2cWriteRegister(I2C_DEV_MCU, 0x22, 0x2A);
    }
    else
    {
        clearScreens(true, true);
        updateBrightness(MULTICONFIG(BRIGHTNESS));
    }
}