/*
*   main.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*
*   Minimalist CFW for N3DS
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