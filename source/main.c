/*
*   main.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*
*   Minimalist CFW for N3DS
*/

#include "fs.h"
#include "firm.h"
#include "linux.h"

u16 pressed;

void main(){
    mountSD();
    
    pressed = HID_PAD;
    if(pressed & 32){
        loadLinux();
        runLinux();
    }else{
        setupCFW();
    }
}

void startCFW(){
    if(!loadFirm()) return;
    if(!patchFirm()) return;
    launchFirm();
}