/*
*   fs.h
*/

#pragma once

#include "types.h"

u32 mountFs(void);
u32 fileRead(void *dest, const char *path, u32 size);
u32 fileWrite(const void *buffer, const char *path, u32 size);
u32 defPayloadExists(void);
void findDumpFile(const char *path, char *fileName);
void firmRead(void *dest, const char *firmFolder);