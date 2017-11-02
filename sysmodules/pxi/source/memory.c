/*
memory.c:
    Memory functions

(c) TuxSH, 2016-2017
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#include "memory.h"

//Adpated from CakesFW
void memcpy(void *dest, const void *src, u32 size)
{
    u8 *destc = (u8 *)dest;
    const u8 *srcc = (const u8 *)src;

    for(u32 i = 0; i < size; i++)
        destc[i] = srcc[i];
}
