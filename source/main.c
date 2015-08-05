/*
*   main.c
*       by Reisyukaku
*
*   Minimalist CFW for N3DS
*/

#include "fs.h"
#include "firm.h"

int main(){
    mountSD();
    loadSplash();
    loadFirm();
    patchFirm();
    launchFirm();
    return 0;
}