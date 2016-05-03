/*
*   fs.h
*/

#pragma once

#include "types.h"

#define PATTERN(a)      a "_*.bin"

u32 mountFs(void);
u32 fileRead(void *dest, const char *path, u32 size);
u32 fileWrite(const void *buffer, const char *path, u32 size);
void findDumpFile(const char *path, char *fileName);
void loadPayload(u32 pressed, u32 devMode);
void firmRead(void *dest, const char *firmFolder);