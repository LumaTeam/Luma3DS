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
u32 emuCode(void);
u32 mpuCode(void);
u32 threadCode(void);
u32 threadHook(u8 val);
u32 emuHook(u8 val);
u32 sigPatch(u8 val);

#endif