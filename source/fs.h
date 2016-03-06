/*
*   fs.h
*/

#ifndef FS_INC
#define FS_INC

#include "types.h"

u8 mountSD(void);
u8 fileRead(u8 *dest, const char *path, u32 size);
u8 fileWrite(const u8 *buffer, const char *path, u32 size);
u32 fileSize(const char* path);
u8 fileExists(const char* path);

#endif