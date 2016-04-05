/*
*   utils.h
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

u32 waitInput(void);
void deleteFirms(const char *firmPaths[], u32 firms);
void error(const char *message);