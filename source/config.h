/*
*   config.h
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

#define CFG_BOOTENV (*(vu32 *)0x10010000)

#define CONFIG(a) ((config >> (a + 16)) & 1)
#define MULTICONFIG(a) ((config >> (a * 2 + 6)) & 3)
#define BOOTCONFIG(a, b) ((config >> a) & b)

extern u32 config; 

void configureCFW(const char *configPath);
