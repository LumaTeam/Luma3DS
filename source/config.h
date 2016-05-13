/*
*   config.h
*/

#pragma once

#include "types.h"

#define CONFIG(a) ((config >> (a + 16)) & 1)
#define MULTICONFIG(a) ((config >> (a * 2 + 6)) & 3)
#define BOOTCONFIG(a, b) ((config >> a) & b)

#define DEVMODE CONFIG(8)

extern u32 config;

void configureCFW(const char *configPath);