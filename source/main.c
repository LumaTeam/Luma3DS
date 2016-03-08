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

void main(){
    mountSD();
    setupCFW();
}

void startCFW(){
    loadSplash();
    if(!loadFirm()) return;
    if(!patchFirm()) return;
    launchFirm();
}