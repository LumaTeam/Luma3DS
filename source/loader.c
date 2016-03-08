/*
*   loader.c
*/

#include "loader.h"
#include "fs.h"

#define PAYLOAD_ADDRESS	0x24F00000

void loadPayload(void){
    if(fileExists("rei/arm9payload.bin") &&
       fileRead((u8 *)PAYLOAD_ADDRESS, "rei/loader.bin", 0))
        ((void (*)())PAYLOAD_ADDRESS)();
}