/*
*   draw.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "draw.h"
#include "fs.h"
#include "memory.h"
#include "types.h"

void shutdownLCD(void){
    
    //Clear LCDs
    *(vu32*)0x10202A44 = 0;
    *(vu32*)0x10202244 = 0;
    *(vu32*)0x10202014 = 0;
    
    //Clear ARM11 entry offset and wait for ARM11 to set it, and jumps
    *(vu32*)0x1FFFFFF8 = 0;
    while (!*(vu32*)0x1FFFFFF8);
    ((void (*)())*(vu32*)0x1FFFFFF8)();
}

void clearScreen(void){
    memset(fb->top_left, 0, 0x38400);
    memset(fb->top_right, 0, 0x38400);
    memset(fb->bottom, 0, 0x38400);
}

void loadSplash(void){
    clearScreen();
    fileRead(fb->top_left, "/rei/splash.bin", 0);
    u64 i = 0xFFFFFF; while(--i) __asm("mov r0, r0"); //Less Ghetto sleep func
}