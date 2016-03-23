/*
*   memory.h
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

void memcpy(void *dest, const void *src, u32 size);
void memset(void *dest, int filler, u32 size);
void memset32(void *dest, u32 filler, u32 size);
int memcmp(const void *buf1, const void *buf2, u32 size);
void *memsearch(void *start_pos, const void *search, u32 size, u32 size_search);