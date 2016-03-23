/*
*   main.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*
*   Minimalist CFW for (N)3DS
*/

#include "fs.h"
#include "firm.h"
#include "i2c.h"

void main(void){
    mountSD();
    setupCFW();
    if(!loadFirm()) return;
    if(!patchFirm()) return;
    launchFirm();
}

void shutdown(void){
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1);
}