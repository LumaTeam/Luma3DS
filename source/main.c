/*
*   main.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*
*   Minimalist CFW for (N)3DS
*/

#include "fs.h"
#include "firm.h"

void main(void){
    mountSD();
    setupCFW();
    loadFirm();
    patchFirm();
    launchFirm();
}