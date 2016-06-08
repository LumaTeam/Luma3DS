/*
*   handlers.h
*       by TuxSH
*
*   This is part of Luma3DS, see LICENSE.txt for details
*/

#pragma once
#include "types.h"

void FIQHandler(void);
void undefinedInstructionHandler(void);
void dataAbortHandler(void);
void prefetchAbortHandler(void);

typedef struct __attribute__((packed))
{
    u32 magic[2];
    u16 versionMinor, versionMajor;
    
    u16 processor, core;
    u32 type;
    
    u32 totalSize;
    u32 registerDumpSize;
    u32 codeDumpSize;
    u32 stackDumpSize;
    u32 additionalDataSize;
} ExceptionDumpHeader;