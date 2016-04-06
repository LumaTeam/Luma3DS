/*
*   config.h
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

#define CFG_BOOTENV (*(vu32 *)0x10010000)
#define CONFIG(a, b) ((config >> a) & b)

u32 config; 

void configureCFW(const char *configPath, const char *patchedFirms[]);