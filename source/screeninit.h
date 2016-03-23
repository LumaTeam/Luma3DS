/*
*   screeninit.h
*       by Aurora Wright
*   Screen init code by dark_samus, bil1s, Normmatt, delebile and others.
*   Screen deinit code by tiniVi.
*
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

#define PDN_GPU_CNT (*(vu8 *)0x10141200)

void deinitScreens(void);
void initScreens(void);