/*
*   draw.c
*       by Reisyukaku
*/

#include "draw.h"
#include "fs.h"

void loadSplash(void){
    fileRead(fb->top_left, "/rei/splash.bin", 0x46500);
}