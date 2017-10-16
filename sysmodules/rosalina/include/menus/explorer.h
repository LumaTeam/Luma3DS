#pragma once


#include <3ds/types.h>
#include "menu.h"


#define MAP_BASE_1		0x08000000
#define MAP_BASE_2 		0x10000000
#define MAP_BASE_SIZE	  0x200000

#define MEMOP_FREE 		1
#define MEMOP_ALLOC 	3
#define MEMPERM_READ 	1
#define MEMPERM_WRITE 	2

#define TICKS_PER_MSEC 268111.856

#define MAX_FILES 256

#define nullptr (void *)0

typedef struct {
    char fileNames[128][128];
	char Path[512];
	
	char PathDir[512];
	bool FoFext[128];
	
}Dir_Name;

void Explorer(void);