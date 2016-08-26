#pragma once

#include <3ds/types.h>

#define PATH_MAX 255

#define CONFIG(a)        (((info.config >> (a + 16)) & 1) != 0)
#define MULTICONFIG(a)   ((info.config >> (a * 2 + 6)) & 3)
#define BOOTCONFIG(a, b) ((info.config >> a) & b)

typedef struct __attribute__((packed))
{
    char magic[4];
    
    u8 versionMajor;
    u8 versionMinor;
    u8 versionBuild;
    u8 flags; /* bit 0: dev branch; bit 1: is release */

    u32 commitHash;

    u32 config;
} CFWInfo;

void patchCode(u64 progId, u8 *code, u32 size);