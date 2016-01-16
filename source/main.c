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

int main(){
    mountSD();
    loadSplash();
    loadFirm();
    loadEmu();
    patchFirm();
    launchFirm();
    return 0;
}