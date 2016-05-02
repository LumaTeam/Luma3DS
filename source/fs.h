/*
*   fs.h
*/

#pragma once

#include "types.h"

#define PAYLOAD_ADDRESS	0x24F00000
#define PATTERN(a)      a "_*.bin"

u32 mountFs(void);
u32 fileRead(void *dest, const char *path, u32 size);
u32 fileWrite(const void *buffer, const char *path, u32 size);
void loadPayload(u32 pressed);
void firmRead(void *dest, const char *firmFolder);