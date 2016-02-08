/*
*   draw.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "draw.h"
#include "fs.h"
#include "memory.h"

static struct fb* fb = (struct fb*) 0x23FFFE00;

void clearScreen(void){
    memset(fb->top_left, 0, 0x38400);
    memset(fb->top_right, 0, 0x38400);
    memset(fb->bottom, 0, 0x38400);
}

void loadSplash(void){
    clearScreen();
    if(fileRead(fb->top_left, "/rei/splash.bin", 0x46500) != 0) return;
    unsigned i,t; for(t=220;t>0;t--){for(i=0xFFFF;i>0;i--);}; //Ghetto sleep func
}