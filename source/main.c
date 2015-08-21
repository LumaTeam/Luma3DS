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

int mode = 1;

int main(){
    mountSD();
    loadSplash();
    while(1){
        if(((~*(unsigned *)0x10146000) & 0xFFF) == (1 << 3)) break;
        else if(((~*(unsigned *)0x10146000) & 0xFFF) == ((1 << 3) | (1 << 1))) {mode = 0; break;}
    } //Start = emu; Start+B = sys
    loadFirm(mode);
    patchFirm();
    launchFirm();
    return 0;
}