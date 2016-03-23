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
const u8 mpu[0x2C];       
u8 nandRedir[0x08];
const u8 sigPat1[2];
const u8 sigPat2[4];
const u8 writeBlock[4];

/**************************************************
*                   Functions
**************************************************/
u8 *getProc9(void *pos, u32 size);
void getSignatures(void *pos, u32 size, u32 *off, u32 *off2);
void *getReboot(void *pos, u32 size);
u32 getfOpen(void *pos, u32 size, u8 *proc9Offset);
void *getFirmWrite(void *pos, u32 size);