/*
*   screeninit.h
*
*   Screen init code by dark_samus, bil1s, Normmatt, delebile and others.
*   Screen deinit code by tiniVi.
*/

#pragma once

#include "types.h"

#define PDN_GPU_CNT        (*(vu8 *)0x10141200)
#define SCREENINIT_ADDRESS 0x24F02000

void deinitScreens(void);
void initScreens(void);