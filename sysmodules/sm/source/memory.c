/*
memory.c

(c) TuxSH, 2017
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#include "memory.h"

/*
//Adpated from CakesFW
void memcpy(void *dest, const void *src, u32 size)
{
    u8 *destc = (u8 *)dest;
    const u8 *srcc = (const u8 *)src;

    for(u32 i = 0; i < size; i++)
        destc[i] = srcc[i];
}*/

s32 strnlen(const char *string, s32 maxlen)
{
    s32 size;
    for(size = 0; size < maxlen && *string; string++, size++);

    return size;
}
