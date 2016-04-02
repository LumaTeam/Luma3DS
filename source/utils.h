/*
*   utils.h
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

#define CFG_BOOTENV (*(vu32 *)0x10010000)

void configureCFW(const char *configPath, const char *firm90Path);
void deleteFirms(const char *firmPaths[], u32 firms);
void error(const char *message);