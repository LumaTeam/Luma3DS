#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void* (*reiNand)() = 0x08000030;

void main()
{
	/*int fbfound = 0;
	unsigned char* screen = 0x20000000;
	for(int i = 0; i < 0x30; i++){
		if( *((unsigned int*)(screen + i + 0)) == 0xABADF00D &&
			*((unsigned int*)(screen + i + 4)) == 0xDEADBEEF	){
			fbfound = 1;
			screen += i;
		}
	}
	if(!fbfound){
		screen = 0x20046500;
		for(int i = 0; i < 0x30; i++){
			if( *((unsigned int*)(screen + i + 0)) == 0xABADF00D &&
				*((unsigned int*)(screen + i + 4)) == 0xDEADBEEF	){
				fbfound = 1;
				screen += i;
			}
		}
	}
	*/
	*((unsigned int*)0x080FFFC0) = 0x20000000;
	*((unsigned int*)0x080FFFC4) = 0x20046500;
	*((unsigned int*)0x080FFFD8) = 0;

	unsigned int* buf = 0x20400000;
	unsigned int base = 0x67893421;
	unsigned int seed = 0x12756342;
	for(int i = 0; i < 400*1024/4; i++){
		buf[i] ^= base;
		base += seed;
	}

	unsigned char*src = 0x20400000;
	unsigned char*dst = 0x08000000;
	for(int i = 0; i < 320*1024; i++){
		dst[i] = src[i];
	}

	*(unsigned int*)0x10000020 = 0;
    *(unsigned int*)0x10000020 = 0x340;
	reiNand();
}
