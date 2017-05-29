#pragma once

#include <3ds/types.h>

typedef struct __attribute__((packed))
{
    char magic[4];
    
    u8 versionMajor;
    u8 versionMinor;
    u8 versionBuild;
    u8 flags;

    u32 commitHash;

    u32 config;
} CFWInfo;

u32 svcGetCFWInfo(CFWInfo *info);