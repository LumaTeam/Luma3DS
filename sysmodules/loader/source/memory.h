#pragma once

#include <3ds/types.h>
#include <string.h>

u8 *memsearch(u8 *startPos, const void *pattern, u32 size, u32 patternSize);
