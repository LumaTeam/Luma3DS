/*
*   fs.h
*/

#ifndef __fs_h__
#define __fs_h__

#include "types.h"

int mountSD(void);
int fileRead(u8 *dest, const char *path, u32 size);
int fileWrite(const u8 *buffer, const char *path, u32 size);
int fileSize(const char* path);
int fileExists(const char* path);

#endif
