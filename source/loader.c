/*
*   loader.c
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "loader.h"
#include "fs.h"
#include "memory.h"
#include "screeninit.h"
#include "../build/loader.h"

#define PAYLOAD_ADDRESS	0x24F00000

void loadPayload(void){
    if(fileExists("aurei/payloads/default.bin")){
        initScreens();
        memcpy((void *)PAYLOAD_ADDRESS, loader, loader_size);
        ((void (*)())PAYLOAD_ADDRESS)();
    }
}