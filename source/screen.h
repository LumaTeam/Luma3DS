/*
*   screen.h
*
*   Screen init code by dark_samus, bil1s, Normmatt, delebile and others.
*   Screen deinit code by tiniVi.
*/

#pragma once

#include "types.h"

#define PDN_GPU_CNT         (*(vu8 *)0x10141200)
#define ARM11_STUB_ADDRESS  (0x25000000 - 0x40) //It's currently only 0x28 bytes large. We're putting 0x40 just to be sure here
#define WAIT_FOR_ARM9()     *arm11Entry = 0; while(!*arm11Entry); ((void (*)())*arm11Entry)();

struct fb {
    u8 *top_left;
    u8 *top_right;
    u8 *bottom;
};

void deinitScreens(void);
void updateBrightness(u32 brightnessIndex);
void clearScreens(void);
u32 initScreens(void);