/*
*   patches.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "patches.h"
#include "memory.h"

/**************************************************
*                   Patches
**************************************************/

const u8 mpu[0x2C] = {
    0x03, 0x00, 0x36, 0x00, 0x00, 0x00, 0x10, 0x10, 0x01, 0x00, 0x00, 0x01, 0x03, 0x00, 0x36, 0x00, 
    0x00, 0x00, 0x00, 0x20, 0x01, 0x01, 0x01, 0x01, 0x03, 0x06, 0x20, 0x00, 0x00, 0x00, 0x00, 0x08, 
    0x01, 0x01, 0x01, 0x01, 0x03, 0x06, 0x1C, 0x00, 0x00, 0x00, 0x02, 0x08
};

//Branch to emunand function. To be filled in
u8 nandRedir[0x08] = {0x00, 0x4C, 0xA0, 0x47, 0x00, 0x00, 0x00, 0x00};

const u8 sigPat1[2] = {0x00, 0x20};
const u8 sigPat2[4] = {0x00, 0x20, 0x70, 0x47};

const u8 writeBlock[4] = {0x00, 0x20, 0xC0, 0x46};

/**************************************************
*                   Functions
**************************************************/

u8 *getProc9(void *pos, u32 size){
    return (u8 *)memsearch(pos, "ess9", size, 4);
}

void getSignatures(void *pos, u32 size, u32 *off, u32 *off2){
    //Look for signature checks
    const unsigned char pattern[] = {0xC0, 0x1C, 0x76, 0xE7};
    const unsigned char pattern2[] = {0xB5, 0x22, 0x4D, 0x0C};

    *off = (u32)memsearch(pos, pattern, size, 4);
    *off2 = (u32)memsearch(pos, pattern2, size, 4) - 1;
}

void *getReboot(void *pos, u32 size){
    //Look for FIRM reboot code
    const unsigned char pattern[] = {0xDE, 0x1F, 0x8D, 0xE2};

    return (u8 *)memsearch(pos, pattern, size, 4) - 0x10;
}

u32 getfOpen(void *pos, u32 size, u8 *proc9Offset){
    //Offset Process9 code gets loaded to in memory (defined in ExHeader)
    u32 p9MemAddr = *(u32 *)(proc9Offset + 0xC);
    //Start of Process9 .code section (start of NCCH + ExeFS offset + ExeFS header size)
    u32 p9CodeOff = (u32)(proc9Offset - 0x204) + (*(u32 *)(proc9Offset - 0x64) * 0x200) + 0x200;

    //Calculate fOpen
    const unsigned char pattern[] = {0xB0, 0x04, 0x98, 0x0D};

    return (u32)memsearch(pos, pattern, size, 4) - 2 - p9CodeOff + p9MemAddr;
}

void *getFirmWrite(void *pos, u32 size){
    //Look for FIRM writing code
    u8 *firmwrite = (u8 *)memsearch(pos, "exe:", size, 4);
    const unsigned char pattern[] = {0x00, 0x28, 0x01, 0xDA};

    return memsearch(firmwrite - 0x100, pattern, 0x100, 4);
}