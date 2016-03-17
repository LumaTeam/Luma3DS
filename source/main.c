/*
*   main.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*
*   Minimalist CFW for N3DS
*/

#include "fs.h"
#include "firm.h"

void main(void){
    mountSD();
    setupCFW();
}

void startCFW(void){
    if(!loadFirm()) return;
    if(!patchFirm()) return;
    launchFirm();
}