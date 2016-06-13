/*
*   memory.h
*
*   Quick Search algorithm adapted from http://igm.univ-mlv.fr/~lecroq/string/node19.html#SECTION00190
*/

#pragma once

#include "types.h"

void memcpy(void *dest, const void *src, u32 size);
void memset32(void *dest, u32 filler, u32 size);
int memcmp(const void *buf1, const void *buf2, u32 size);
u8 *memsearch(u8 *startPos, const void *pattern, u32 size, u32 patternSize);