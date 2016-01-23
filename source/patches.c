/*
*   patches.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "patches.h"

#define FIRM 0x24000000

#define KERNEL9 (FIRM + 0x68400)
#define PROC9 (FIRM + 0x7F100)

#define K9_ADDR 0x08006000
#define P9_ADDR 0x08028000

/**************************************************
*                   Patches
**************************************************/

/*
*   MPU
*/
u8 mpu[0x2C] = {    //MPU shit
    0x03, 0x00, 0x36, 0x00, 0x00, 0x00, 0x10, 0x10, 0x01, 0x00, 0x00, 0x01, 0x03, 0x00, 0x36, 0x00, 
    0x00, 0x00, 0x00, 0x20, 0x01, 0x01, 0x01, 0x01, 0x03, 0x06, 0x20, 0x00, 0x00, 0x00, 0x00, 0x08, 
    0x01, 0x01, 0x01, 0x01, 0x03, 0x06, 0x1C, 0x00, 0x00, 0x00, 0x02, 0x08
    };
u8 nandRedir[0x08] = {0x00, 0x4C, 0xA0, 0x47, 0xC0, 0xA5, 0x01, 0x08};    //Branch to emunand function

/*
*   Sig checks
*/
u8 sigPat1[2] = {0x00, 0x20};
u8 sigPat2[4] = {0x00, 0x20, 0x70, 0x47};

/*
*   Arm9 thread
*/
u8 th1[4] = {0x2C, 0xF0, 0x9F, 0xE5};   //ldr pc, =0x08006070
u8 th2[4] = {0x70, 0x60, 0x00, 0x08};   //0x08006070



/**************************************************
*                   Functions
**************************************************/

//Where the emunand code is stored in firm
u32 emuCode(void){
    return KERNEL9 + (0x0801A5C0 - K9_ADDR);
}

//Where thread code is stored in firm
u32 threadCode(void){
    return KERNEL9 + (0x08006070 - K9_ADDR);
}

//Area of MPU setting code
u32 mpuCode(void){
    return KERNEL9 + (0x0801B3D4 - K9_ADDR);
}

//Offsets to redirect to thread code
u32 threadHook(u8 val){
    return val == 1 ? 
            PROC9 + (0x08085198 - P9_ADDR): 
            PROC9 + (0x080851CC - P9_ADDR);
}

//Offsets to redirect to thread code
u32 sigPatch(u8 val){
    return val == 1 ?
            PROC9 + (0x08062B08 - P9_ADDR) :
            PROC9 + (0x0805C31C - P9_ADDR);
}