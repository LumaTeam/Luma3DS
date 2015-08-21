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
u8 eh1[0x14];        
u8 eh2[0x0A];
u8 eh3[0x0A];
u8 p1[2];
u8 p2[4];
u8 th1[4];
u8 th2[4];

/**************************************************
*                   Functions
**************************************************/
u32 emuCode(void);
u32 mpuCode(u32 kver);
u32 threadCode(u32 kver);
u32 threadHook(u8 val, u32 kver);
u32 emuHook(u8 val);
u32 sigPatch(u8 val, u32 kver);

#endif