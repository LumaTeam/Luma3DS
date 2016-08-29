#pragma once

#include <3ds/types.h>

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

int svcGetCFWInfo(CFWInfo *info);