/*
*   draw.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#ifndef DRAW_INC
#define DRAW_INC

#include "types.h"

struct fb {
    u8 *top_left;
    u8 *top_right;
    u8 *bottom;
};

void loadSplash(void);
void shutdownLCD(void);

#endif