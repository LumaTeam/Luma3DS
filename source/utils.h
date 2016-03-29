/*
*   utils.h
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

void configureCFW(const char *configPath, const char *firm90Path);
void deleteFirms(const char *firmPaths[], u32 firms);
void error(const char *message);