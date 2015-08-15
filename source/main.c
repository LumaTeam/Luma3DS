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
    while(((~*(unsigned *)0x10146000) & 0xFFF) == (1 << 3) ? 0 : 1); //Press start to boot
    loadFirm();
    patchFirm();
    launchFirm();
    return 0;
}