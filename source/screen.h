/*
*   screen.h
*
*   Screen init code by dark_samus, bil1s, Normmatt, delebile and others.
*   Screen deinit code by tiniVi.
*/

#pragma once

#include "types.h"

#define PDN_GPU_CNT         (*(vu8 *)0x10141200)

static volatile struct fb {
    u8 *top_left;
    u8 *top_right;
    u8 *bottom;
} *const fb = (volatile struct fb *)0x23FFFE00;

void deinitScreens(void);
void updateBrightness(u32 brightnessLevel);
void clearScreens(void);
u32 initScreens(void);