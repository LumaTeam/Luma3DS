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

#define VRAM (unsigned char*)0x18000000
#define FCRAM (unsigned char*)0x20000000
#define FCRAM_EXT (unsigned char*)0x28000000
#define ARM9_MEM (unsigned char*)0x8000000
#define AXIWRAM (unsigned char*)0x1FF80000
#define TOP_FRAME 0
#define BOT_FRAME 1

unsigned char handle[32];
unsigned char bmpHead[] = {	
	0x42, 0x4D, 0x36, 0x65, 0x04, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xF0, 0x00, 
	0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x65, 0x04, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void memdump(void* filename, void* buf, unsigned int size){
	unsigned int br = 0;
    memset(&handle, 0, 32);
	fopen9(&handle, filename, 6);
	fwrite9(&handle, &br, buf, size);
	fclose9(&handle);
	memset(VRAM+0x1E6000, 0xFF, 0x46500);
}

void transpose (void * dst, const void * src, unsigned dim1, unsigned dim2, unsigned item_length) {
  char * ptr_write;
  const char * ptr_read;
  unsigned x, y, z;
  for (x = 0; x < dim1; x ++) for (y = 0; y < dim2; y ++) {
    ptr_write = ((char *) dst) + item_length * (y * dim1 + x);
    ptr_read = ((const char *) src) + item_length * (x * dim2 + y);
    for (z = 0; z < item_length; z ++) *(ptr_write ++) = *(ptr_read ++);
  }
}

void screenShot(int frame){
    unsigned int br;
    short width = frame == 0 ? 400 : 320;
    short height = 240;
    int frameOff = frame == 0 ? 0x1E6000 : 0x48F000;  //<- Defaults
    int length = frame == 0 ? 0x46500 : 0x38400;
    memset(&handle, 0, 32);
	fopen9(&handle, frame == 0 ? L"sdmc:/screen_top.bmp" : L"sdmc:/screen_bot.bmp", 6);
    transpose(FCRAM+0xF80000, VRAM+frameOff, width, height, 3);
    bmpHead[18] = frame == 0 ? 0x90 : 0x40;
    fwrite9(&handle, &br, bmpHead, 0x36);
    fwrite9(&handle, &br, FCRAM+0xF80000, length);
    fclose9(&handle);
    memset(VRAM+frameOff, 0xFF, 0x46500);
}

void patches(void){
    //Change version string
    for(int i = 0; i < 0x600000; i+=4){
		if(strcomp((void*)0x27B00000  - i, (void*)L"Ver.", 4)) strcopy((void*)0x27B00000 - i, (void*)L"\uE024Rei", 4);
	}
}

void thread(void){
	while(1){
		if(isPressed(BUTTON_SELECT | BUTTON_X)){
            screenShot(TOP_FRAME);
            screenShot(BOT_FRAME);
        }
        if(isPressed(BUTTON_START | BUTTON_X)){ 
            memdump(L"sdmc:/AXIWRAM.bin", AXIWRAM, 0x00080000);
            memdump(L"sdmc:/FCRAM.bin", FCRAM, 0x010000000); 
        }
        patches();
	}
	__asm("SVC 0x09");
}