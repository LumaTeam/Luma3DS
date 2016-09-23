#include "memory.h"

void memcpy(void *dest, const void *src, u32 size)
{
    u8 *destc = (u8 *)dest;
    const u8 *srcc = (const u8 *)src;

    for(u32 i = 0; i < size; i++)
        destc[i] = srcc[i];
}

int memcmp(const void *buf1, const void *buf2, u32 size)
{
    const u8 *buf1c = (const u8 *)buf1,
             *buf2c = (const u8 *)buf2;

    for(u32 i = 0; i < size; i++)
    {
        int cmp = buf1c[i] - buf2c[i];
        if(cmp != 0) return cmp;
    }

    return 0;
}

//Boyer-Moore Horspool algorithm, adapted from http://www-igm.univ-mlv.fr/~lecroq/string/node18.html#SECTION00180
u8 *memsearch(u8 *startPos, const void *pattern, u32 size, u32 patternSize)
{
    const u8 *patternc = (const u8 *)pattern;
    u32 table[256];

    //Preprocessing
    for(u32 i = 0; i < 256; i++)
        table[i] = patternSize;
    for(u32 i = 0; i < patternSize - 1; i++)
        table[patternc[i]] = patternSize - i - 1;

    //Searching
    u32 j = 0;
    while(j <= size - patternSize)
    {
        u8 c = startPos[j + patternSize - 1];
        if(patternc[patternSize - 1] == c && memcmp(pattern, startPos + j, patternSize - 1) == 0)
            return startPos + j;
        j += table[c];
    }

    return NULL;
}