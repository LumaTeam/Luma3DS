/*
*   memory.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/
#include "memory.h"

void memcpy(u8 *dest, u8 *src, u32 size){
    for (u32 i = 0; i < size; i++) dest[i] = src[i];
}

void memcpy32(u32 *dest, u32 *src, u32 size){
    for (u32 i = 0; i < size; i++) dest[i] = src[i];
}

void memset(u8 *dest, u32 fill, u32 size){
    for (u32 i = 0; i < size; i++) dest[i] = fill;
}

int memcmp(u8 *buf1, u8 *buf2, u32 size){
    for (u32 i = 0; i < size; i++) {
        int cmp = buf1[i] - buf2[i];
        if (cmp != 0) return cmp;
    }
    return 0;
}

u32 *memsearch(u8 *start_pos, u8 *search, u32 size, u32 size_search){
    for (u8 *pos = start_pos; pos <= start_pos + size - size_search; pos++) {
        if (memcmp(pos, search, size_search) == 0) return (u32*)pos;
    }
    return NULL;
}