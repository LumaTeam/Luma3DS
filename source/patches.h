/*
*   patches.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/
#ifndef PATCHES_INC
#define PATCHES_INC

#include "types.h"

/**************************************************
*                   Patches
**************************************************/
u8 mpu[0x2C];       
u8 nandRedir[0x08];
u8 sigPat1[2];
u8 sigPat2[4];
u8 th1[4];
u8 th2[4];

/**************************************************
*                   Functions
**************************************************/
u32 emuCode(u32 kver);
u32 mpuCode(u32 kver);
u32 threadCode(u32 kver);
u32 threadHook(u8 val, u32 kver);
u32 emuHook(u8 val, u32 kver);
u32 sigPatch(u8 val, u32 kver);

#endif