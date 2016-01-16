/*
*   draw.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "draw.h"
#include "fs.h"
#include "memory.h"

void clearScreen(void){
    memset(fb->top_left, 0, 0x38400);
    memset(fb->top_right, 0, 0x38400);
    memset(fb->bottom, 0, 0x38400);
}

void loadSplash(void){
    clearScreen();
    fileRead(fb->top_left, "/rei/splash.bin", 0x46500);
    unsigned i,t; for(t=120;t>0;t--){for(i=0xFFFF;i>0;i--);}; //Ghetto sleep func
}