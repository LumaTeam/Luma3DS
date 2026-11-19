#pragma once
#include <3ds/types.h>
#include "ifile.h"

#define _3GX_MAGIC (0x3330303024584733) /* "3GX$0003" */

typedef struct CTR_PACKED
{
    u32             authorLen;
    const char*     authorMsg;
    u32             titleLen;
    const char*     titleMsg;
    u32             summaryLen;
    const char*     summaryMsg;
    u32             descriptionLen;
    const char*     descriptionMsg;
    union {
        u32         flags;
        struct {
            u32     embeddedExeLoadFunc : 1;
            u32     embeddedSwapSaveLoadFunc : 1;
            u32     memoryRegionSize : 2;
            u32     compatibility : 2;
            u32     eventsSelfManaged : 1;
            u32     swapNotNeeded : 1;
            u32     usePrivateMemory : 1;
            u32     allowHomebrewLoad : 1;
            u32     unused : 22;
        };
    };
    u32             exeLoadChecksum;
    u32             builtInLoadExeArgs[4];
    u32             builtInSwapSaveLoadArgs[4];
} _3gx_Infos;

typedef struct CTR_PACKED
{
    u32             count;
    u32           * titles;
}   _3gx_Targets;

typedef struct CTR_PACKED
{
    u32             nbSymbols;
    u32             symbolsOffset;
    u32             nameTableOffset;
}   _3gx_Symtable;

typedef struct CTR_PACKED
{
    u32             builtInDataOffset;
    u32             builtInDataSize;
}   _3gx_BuiltInData;

typedef struct CTR_PACKED
{
    u32             codeOffset;
    u32             rodataOffset;
    u32             dataOffset;
    u32             codeSize;
    u32             rodataSize;
    u32             dataSize;
    u32             bssSize;
    u32             exeLoadFuncOffset; // NOP terminated
    u32             swapSaveFuncOffset; // NOP terminated
    u32             swapLoadFuncOffset; // NOP terminated
} _3gx_Executable;

typedef struct CTR_PACKED
{
    u64                 magic;
    u32                 version;
    u16                 headerSize;
    u16                 reserved;
    _3gx_Infos          infos;
    _3gx_Executable     executable;
    _3gx_Targets        targets;
    _3gx_Symtable       symtable;
    _3gx_BuiltInData    builtinData;
} _3gx_Header;


enum _3gx_Compatibility {
    PLG_COMPAT_CONSOLE = 0,
    PLG_COMPAT_EMULATOR = 1,
    PLG_COMPAT_CONSOLE_EMULATOR = 2,
};

Result  Check_3gx_Magic(IFile *file, u16* headerSizeOut);
Result  Read_3gx_Header(IFile *file, _3gx_Header *header, u16 headerSize);
Result  Read_3gx_String(IFile *file, u32 offset, u32 len, char *out, u32 outSize);
Result  Read_3gx_IsTargetCompatible(IFile *file, _3gx_Header *header, u32 titleId, bool *compatible);
Result  Read_3gx_LoadSegments(IFile *file, _3gx_Header *header, void *dst);
Result  Read_3gx_EmbeddedPayloads(IFile *file, _3gx_Header *header);
Result  Set_3gx_LoadParams(u32* loadFunc, u32* params);
void	Reset_3gx_LoadParams(void);