/*
*   memory.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/
#ifndef MEM_INC
#define MEM_INC

#include "types.h"

void memcpy(u8 *dest, u8 *src, u32 size);
void memcpy32(u32 *dest, u32 *src, u32 size);
void memset(u8 *dest, u32 fill, u32 size);
int memcmp(u8 *buf1, u8 *buf2, u32 size);
u32 *memsearch(u8 *start_pos, u8 *search, u32 size, u32 size_search);

#endif