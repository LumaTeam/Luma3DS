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

u8 a9lhBoot = 0;

u8 main(){
    mountSD();
    //Detect an A9LH boot checking PDN_GPU_CNT register
    if (*((u8*)0x10141200) == 0x1) a9lhBoot = 1;
    else loadSplash();
    if (loadFirm(a9lhBoot)) return 1;
    if (patchFirm()) return 1;
    launchFirm();
    return 0;
}