/*
*   draw.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "draw.h"
#include "fs.h"
#include "memory.h"

static const struct fb {
    u8 *top_left;
    u8 *top_right;
    u8 *bottom;
} *const fb = (struct fb *)0x23FFFE00;

void __attribute__((naked)) shutdownLCD(void){

    vu32 *const arm11 = (u32 *)0x1FFFFFF8;

    //Clear ARM11 entry offset
    *arm11 = 0;

    //Shutdown LCDs
    *(vu32 *)0x10202A44 = 0;
    *(vu32 *)0x10202244 = 0;
    *(vu32 *)0x10202014 = 0;
    
    //Wait for the entry to be set
    while(!*arm11);
    //Jump to it
    ((void (*)())*arm11)();
}

static void clearScreens(void){
    memset32(fb->top_left, 0, 0x46500);
    memset32(fb->top_right, 0, 0x46500);
    memset32(fb->bottom, 0, 0x38400);
}

void loadSplash(void){
    clearScreens();
    //Don't delay boot if no splash image is on the SD
    if(fileRead(fb->top_left, "/aurei/splash.bin", 0x46500) +
       fileRead(fb->bottom, "/aurei/splashbottom.bin", 0x38400)){
        u64 i = 0x1300000; while(--i) __asm("mov r0, r0"); //Less Ghetto sleep func
    }
}