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
u8 FIRMblock[4];
u8 emuInstr[5];

/**************************************************
*                   Functions
**************************************************/
void getSignatures(void *pos, u32 size, u32 *off, u32 *off2);
void getReboot(void *pos, u32 size, u32 *off);
void getfOpen(void *pos, u32 size, u32 *off);
void getFIRMWrite(void *pos, u32 size, u32 *off);

#endif