/*
*   patches.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "patches.h"

#define FIRM 0x24000000

#define KERNEL9 (FIRM + 0x66A00)
#define PROC9 (FIRM + 0x7D700)
#define v9_6_Offset 0x1600

#define K9_ADDR 0x08006000
#define P9_ADDR 0x08028000

/**************************************************
*                   Patches
**************************************************/

/*
*   Emunand
*/
u8 mpu[0x2C] = {    //MPU shit
    0x03, 0x00, 0x36, 0x00, 0x00, 0x00, 0x10, 0x10, 0x01, 0x00, 0x00, 0x01, 0x03, 0x00, 0x36, 0x00, 
    0x00, 0x00, 0x00, 0x20, 0x01, 0x01, 0x01, 0x01, 0x03, 0x06, 0x20, 0x00, 0x00, 0x00, 0x00, 0x08, 
    0x01, 0x01, 0x01, 0x01, 0x03, 0x06, 0x1C, 0x00, 0x00, 0x00, 0x02, 0x08
    };
       
u8 nandRedir[0x08] = {0x00, 0x4C, 0xA0, 0x47, 0xC0, 0xA4, 0x01, 0x08};    //Branch to emunand function

/*
*   Sig checks
*/
u8 sigPat1[2] = {0x00, 0x20};
u8 sigPat2[4] = {0x00, 0x20, 0x70, 0x47};

/*
*   Arm9 thread
*/
u8 th1[4] = {0x2C, 0xF0, 0x9F, 0xE5};   //ldr pc, =0x0801A6E0
u8 th2[4] = {0xE0, 0xA6, 0x01, 0x08};   //0x0801A6E0



/**************************************************
*                   Functions
**************************************************/

//Where the emunand code is stored in firm
u32 emuCode(u32 kver){
    u32 ret = NULL;
    switch(kver){
        case 0x04:
        case 0x0C:
        case 0x0F:
            ret = KERNEL9 + (0x0801A4C0 - K9_ADDR);
            break;
        case 0x18:
            ret = KERNEL9 + v9_6_Offset + (0x0801A4C0 - K9_ADDR);
            break;
    }
    return ret;
}

//Where thread code is stored in firm
u32 threadCode(u32 kver){
    u32 ret = NULL;
    switch(kver){
        case 0x04:
        case 0x0C:
        case 0x0F:
            ret = KERNEL9 + (0x0801A6E0 - K9_ADDR);
        case 0x18:
            ret = KERNEL9 + v9_6_Offset + (0x0801A6E0 - K9_ADDR);
            break;
    }
    return ret;
}

//Area of MPU setting code
u32 mpuCode(u32 kver){
    u32 ret = NULL;
    switch(kver){
        case 0x04:
        case 0x0C:
        case 0x0F:
            ret = KERNEL9 + (0x0801B3D4 - K9_ADDR);
            break;
        case 0x18:
            ret = KERNEL9 + v9_6_Offset + (0x0801B3D4 - K9_ADDR);
            break;
    }
    return ret;
}

//Offsets to redirect to thread code
u32 threadHook(u8 val, u32 kver){
    u32 ret = NULL;
    switch(kver){
        case 0x04:
            ret = val == 1 ? 
            PROC9 + (0x0808690C - P9_ADDR) : 
            PROC9 + (0x08086940 - P9_ADDR);
            break;
        case 0x0C:
            //TODO: find
            break;
        case 0x0F:
            ret = val == 1 ? 
            PROC9 + (0x080860B0 - P9_ADDR) : 
            PROC9 + (0x080860E4 - P9_ADDR);
            break;
        case 0x18:
            ret = val == 1 ? 
            PROC9 + v9_6_Offset + (0x08086140 - P9_ADDR) : 
            PROC9 + v9_6_Offset + (0x08086174 - P9_ADDR);
            break;
    }
    return ret;
}

//Offsets to redirect to Emunand code
u32 emuHook(u8 val, u32 kver){ //latest only
    u32 ret = NULL;
    switch(kver){
        case 0x04:
            //???
            break;
        case 0x0C:
            //???
            break;
        case 0x0F:
            if(val == 1) ret = PROC9 + (0x0807882C - P9_ADDR); 
            else if(val == 2) ret = PROC9 + (0x0807886C - P9_ADDR);
            break;
        case 0x18:
            if(val == 1) ret = PROC9 + v9_6_Offset + (0x0807882C - P9_ADDR); 
            else if(val == 2) ret = PROC9 + v9_6_Offset + (0x0807886C - P9_ADDR);
            break;
    }
    return ret;
}

//Offsets to redirect to thread code
u32 sigPatch(u8 val, u32 kver){
    u32 ret = NULL;
    switch(kver){
        case 0x04:
            ret = val == 1 ?
            PROC9 + (0x08063C28 - P9_ADDR) :
            PROC9 + (0x0805E2D4 - P9_ADDR);
            break;
        case 0x0C:
            ret = val == 1 ?
            0 :
            0; //TODO: find
            break;
        case 0x0F:
            ret = val == 1 ?
            PROC9 + (0x08063374 - P9_ADDR) :
            PROC9 + (0x0805D498 - P9_ADDR);
            break;
        case 0x18:
            ret = val == 1 ?
            PROC9 + v9_6_Offset + (0x080632B8 - P9_ADDR) :
            PROC9 + v9_6_Offset + (0x0805D628 - P9_ADDR);
            break;
    } 
    return ret;
}