/*
*   loader.c
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "loader.h"
#include "fs.h"
#include "screeninit.h"
#include "draw.h"

#define PAYLOAD_ADDRESS	0x24F00000

void loadPayload(void){
    if(fileExists("aurei/payloads/default.bin") &&
       fileRead((u8 *)PAYLOAD_ADDRESS, "aurei/loader.bin", 0)){
        if(PDN_GPU_CNT == 0x1){
            initScreens();
            clearScreens();
        }
        ((void (*)())PAYLOAD_ADDRESS)();
    }
}