/*
*   draw.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "draw.h"
#include "fs.h"
#include "memory.h"
#include "types.h"

static struct fb *fb = (struct fb *)0x23FFFE00;

void shutdownLCD(void){

    vu32 *arm11 = (vu32*)0x1FFFFFF8;

    //Clear ARM11 entry offset
    *arm11 = 0;

    //Shutdown LCDs
    *(vu32*)0x10202A44 = 0;
    *(vu32*)0x10202244 = 0;
    *(vu32*)0x10202014 = 0;
    
    //Wait for the ARM11 entrypoint to be set
    while (!*arm11);
    //Jump to it
    ((void (*)())*arm11)();
}

void clearScreen(void){
    memset(fb->top_left, 0, 0x46500);
    memset(fb->top_right, 0, 0x46500);
    memset(fb->bottom, 0, 0x38400);
}

void loadSplash(void){
    //Check if it's a no-screen-init A9LH boot via PDN_GPU_CNT
    if (*((u8*)0x10141200) == 0x1) return;
    clearScreen();
    if(fileRead(fb->top_left, "/rei/splash.bin", 0x46500) != 0) return;
    u64 i = 0xFFFFFF; while(--i) __asm("mov r0, r0"); //Less Ghetto sleep func
}