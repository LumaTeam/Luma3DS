/*
*   memory.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#ifndef MEMORY_INC
#define MEMORY_INC

#include "types.h"

void memcpy(void *dest, const void *src, u32 size);
void memset(void *dest, int filler, u32 size);
int memcmp(const void *buf1, const void *buf2, u32 size);
void *memsearch(void *start_pos, void *search, u32 size, u32 size_search);

#endif