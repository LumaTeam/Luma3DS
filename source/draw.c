/*
*   draw.c
*       by Reisyukaku / Aurora Wright
*   Code to print to the screen by mid-kid @CakesFW
*
*   Copyright (c) 2016 All Rights Reserved
*/

#include "draw.h"
#include "screeninit.h"
#include "fs.h"
#include "memory.h"

#define SCREEN_TOP_WIDTH 400
#define SCREEN_TOP_HEIGHT 240

static const struct fb {
    u8 *top_left;
    u8 *top_right;
    u8 *bottom;
} *const fb = (struct fb *)0x23FFFE00;

void clearScreens(void)
{
    memset32(fb->top_left, 0, 0x46500);
    memset32(fb->top_right, 0, 0x46500);
    memset32(fb->bottom, 0, 0x38400);
}

void loadSplash(void)
{
    initScreens();

    //Don't delay boot if no splash image is on the SD
    if(fileRead(fb->top_left, "/SaltFW/splash.bin", 0x46500) +
       fileRead(fb->bottom, "/SaltFW/splashbottom.bin", 0x38400))
    {
        u64 i = 0x1400000;
        while(i--) __asm("mov r0, r0"); //Less Ghetto sleep func
    }
}