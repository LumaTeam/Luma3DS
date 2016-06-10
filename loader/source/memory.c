#include "memory.h"

void memcpy(void *dest, const void *src, u32 size)
{
    u8 *destc = (u8 *)dest;
    const u8 *srcc = (const u8 *)src;

    for(u32 i = 0; i < size; i++)
        destc[i] = srcc[i];
}

void cleanInvalidateDCacheAndDMB(void)
{
    ((void (*)())0xFFFF0830)(); //Why write our own code when it's well implemented in the unprotected bootROM?
}