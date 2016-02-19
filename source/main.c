/*
*   main.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*
*   Minimalist CFW for N3DS
*/

#include "fs.h"
#include "firm.h"
#include "draw.h"

u8 a9lh = 0;

u8 main(){
    mountSD();
    //Detect A9LH mode checking PDN_GPU_CNT2 register.
    if (!*((u32*)0x10141204)) a9lh = 1;
    else loadSplash();
    if (loadFirm(a9lh)) return 1;
    if (patchFirm()) return 1;
    launchFirm();
    return 0;
}