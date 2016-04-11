/*
*   emunand.h
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

#define NCSD_MAGIC (0x4453434E)

void getEmunandSect(u32 *off, u32 *head, u32 *emuNAND);
u32 getSDMMC(u8 *pos, u32 size);
void getEmuRW(u8 *pos, u32 size, u32 *readOff, u32 *writeOff);
u32 *getMPU(u8 *pos, u32 size);
void *getEmuCode(u8 *proc9Offset);