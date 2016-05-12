/*
*   patches.h
*/

#pragma once

#include "types.h"

/**************************************************
*                   Patches
**************************************************/
const u32 mpuPatch[3];       
const u16 nandRedir[2],
          sigPatch[2],
          writeBlock[2],
          writeBlockSafe[2];
const u8  svcBackdoor[40];

/**************************************************
*                   Functions
**************************************************/
u8 *getProcess9(u8 *pos, u32 size, u32 *process9Size, u32 *process9MemAddr);
void getSigChecks(u8 *pos, u32 size, u32 *off, u32 *off2);
void *getReboot(u8 *pos, u32 size, u32 process9MemAddr, u32 *fOpenOffset);
u16 *getFirmWrite(u8 *pos, u32 size);
u16 *getFirmWriteSafe(u8 *pos, u32 size);
u32 getLoader(u8 *pos, u32 *loaderSize);
u32 *getSvcAndExceptions(u8 *pos, u32 size, u32 **exceptionsPage);