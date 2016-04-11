/*
*   patches.h
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

/**************************************************
*                   Patches
**************************************************/
const u32 mpuPatch[3];       
const u16 nandRedir[2];
const u16 sigPatch[2];
const u16 writeBlock[2];
const u8 unitInfoPatch;

/**************************************************
*                   Functions
**************************************************/
u8 *getProc9(u8 *pos, u32 size);
void getSigChecks(u8 *pos, u32 size, u32 *off, u32 *off2);
void *getReboot(u8 *pos, u32 size);
u32 getfOpen(u8 *proc9Offset, void *rebootOffset);
u16 *getFirmWrite(u8 *pos, u32 size);
void getLoader(u8 *pos, u32 size, u32 *loaderOffset, u32 *loaderSize);
void getUnitInfoValueSet(u8 *pos, u32 size, u32* off);