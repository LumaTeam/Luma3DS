/*
*   memory.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "memory.h"

void memcpy(void *dest, const void *src, u32 size){
    u8 *destc = (u8 *)dest;
    const u8 *srcc = (const u8 *)src;
    for(u32 i = 0; i < size; i++)
        destc[i] = srcc[i];
}

void memset(void *dest, int filler, u32 size){
    u8 *destc = (u8 *)dest;
    for(u32 i = 0; i < size; i++)
        destc[i] = (u8)filler;
}

void memset32(void *dest, u32 filler, u32 size){
    u32 *dest32 = (u32 *)dest;
    for (u32 i = 0; i < size / 4; i++)
        dest32[i] = filler;
}

int memcmp(const void *buf1, const void *buf2, u32 size){
    const u8 *buf1c = (const u8 *)buf1;
    const u8 *buf2c = (const u8 *)buf2;
    for(u32 i = 0; i < size; i++){
        int cmp = buf1c[i] - buf2c[i];
        if(cmp) return cmp;
    }
    return 0;
}

void *memsearch(void *start_pos, const void *search, u32 size, u32 size_search){
    for(u8 *pos = (u8 *)start_pos + size - size_search; pos >= (u8 *)start_pos; pos--)
        if(memcmp(pos, search, size_search) == 0) return pos;
    return NULL;
}