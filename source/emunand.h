/*
*   emunand.h
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

#define NCSD_MAGIC (0x4453434E)

void getEmunandSect(u32 *off, u32 *head, u32 emuNAND);
u32 getSDMMC(void *pos, u32 size);
void getEmuRW(void *pos, u32 size, u32 *readOff, u32 *writeOff);
void *getMPU(void *pos, u32 size);
void *getEmuCode(void *pos, u32 size, u8 *proc9Offset);