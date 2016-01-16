/*
*   memory.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/
#include "memory.h"

void memcpy32(u32 *dest, u32 *src, u32 size){
    for (u32 i = 0; i < size; i++) dest[i] = src[i];
}

void memcpy(void *dest, const void *src, u32 size){
    char *destc = (char *)dest;
    const char *srcc = (const char *)src;
    u32 i; for (i = 0; i < size; i++) {
        destc[i] = srcc[i];
    }
}

void memset(void *dest, int filler, u32 size){
    char *destc = (char *)dest;
    u32 i; for (i = 0; i < size; i++) {
        destc[i] = filler;
    }
}

int memcmp(const void *buf1, const void *buf2, u32 size){
    const char *buf1c = (const char *)buf1;
    const char *buf2c = (const char *)buf2;
    u32 i; for (i = 0; i < size; i++) {
        int cmp = buf1c[i] - buf2c[i];
        if (cmp) return cmp;
    }
    return 0;
}

void *memsearch(void *start_pos, void *search, u32 size, u32 size_search){
    for (void *pos = start_pos + size - size_search; pos >= start_pos; pos--) {
        if (memcmp(pos, search, size_search) == 0) return pos;
    }
    return NULL;
}