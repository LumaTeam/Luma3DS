/*
*   thread.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include <wchar.h>
#include <stdio.h>
#include "thread.h"
#include "lib.h"
#include "FS.h"

//ram stuff
#define VRAM (unsigned char*)0x18000000
#define FCRAM (unsigned char*)0x20000000
#define FCRAM_EXT (unsigned char*)0x28000000

//file stuff
#define READ 0
#define WRITE 1

unsigned char handle[32];

void fileReadWrite(void *buf, void *path, int size, char rw){
    unsigned int br = 0;
    memset(&handle, 0, 32);
    fopen9(&handle, path, 6);
    if(rw == 0) fread9(&handle, &br, buf, size);
    else fwrite9(&handle, &br, buf, size);
    fclose9(&handle);
}

void memdump(void* filename, void* buf, unsigned int size){
	fileReadWrite(buf, filename, size, WRITE);
	memset(VRAM+0x1E6000, 0xFF, 0x46500);
}

void patches(void){
    //Change version string
    for(int i = 0; i < 0x600000; i+=4){
        if(strcomp((void*)0x27B00000  - i, (void*)L"Ver.", 4)){
            if(strcomp((void*)0x27B00000  - i + 0x0A, (void*)L"%d.%d.%d-%d", 11)) strcopy((void*)0x27B00000 - i, (void*)L"\uE024Rei", 4);
        }
	}
}

void thread(void){
	while(1){
        if(isPressed(BUTTON_START | BUTTON_X)){
            unsigned char buf[0x10] = {0};
            int loc = 0;
            fileReadWrite(buf, L"sdmc:/rei/RAM.txt", 0x20, READ);
            loc = atoi(buf);
            memdump(L"sdmc:/RAMdmp.bin", (void*)loc, 0x500000);
        }
        patches();
	}
	__asm("SVC 0x09");
}