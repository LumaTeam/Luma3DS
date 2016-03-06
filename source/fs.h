/*
*   fs.h
*/

#ifndef FS_INC
#define FS_INC

#include "types.h"

u32 mountSD(void);
u32 fileRead(u8 *dest, const char *path, u32 size);
u32 fileWrite(const u8 *buffer, const char *path, u32 size);
u32 fileSize(const char *path);
u32 fileExists(const char *path);

#endif