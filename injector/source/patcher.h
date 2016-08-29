#pragma once

#include <3ds/types.h>

#define PATH_MAX 255

#define CONFIG(a)        (((info.config >> (a + 16)) & 1) != 0)
#define MULTICONFIG(a)   ((info.config >> (a * 2 + 6)) & 3)
#define BOOTCONFIG(a, b) ((info.config >> a) & b)

void patchCode(u64 progId, u8 *code, u32 size);