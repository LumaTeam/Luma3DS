#pragma once


#include <3ds/types.h>
#include "menu.h"

#define MAX_FILES 256

#define nullptr (void *)0

typedef struct {
    char fileNames[128][128];
	char Path[512];
	
	char PathDir[512];
	bool FoFext[128];
	
}Dir_Name;

void Explorer(void);