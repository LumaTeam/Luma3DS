/*
*   fs.h
*/

#pragma once

#include "types.h"

#define PATTERN(a)      a "_*.bin"

bool mountFs(void);
u32 fileRead(void *dest, const char *path);
void fileWrite(const void *buffer, const char *path, u32 size);
void loadPayload(u32 pressed);
void firmRead(void *dest, const char *firmFolder);