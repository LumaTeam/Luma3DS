/*
memory.h

(c) TuxSH, 2017
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include <3ds/types.h>

//void memcpy(void *dest, const void *src, u32 size);
#define memcpy __builtin_memcpy
#define memset __builtin_memset
#define strncmp __builtin_strncmp
#define strncpy __builtin_strncpy

s32 strnlen(const char *string, s32 maxlen);
