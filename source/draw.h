/*
*   draw.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "types.h"

static struct fb {
    u8 *top_left;
    u8 *top_right;
    u8 *bottom;
} *fb = (struct fb *)0x23FFFE00;

void clearScreen(void);
void loadSplash(void);