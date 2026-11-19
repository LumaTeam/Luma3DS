#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include "plugin.h"
#include "ifile.h"
#include "utils.h"

u32  g_loadExeArgs[0x4];

#define _3GX_MAGIC_OLD_V2 (0x3230303024584733) /* "3GX$0002" */
#define _3GX_VERSION_V2_HEADER_SIZE (0x94)

static inline u32 invertEndianness(u32 val)
{
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

Result  Check_3gx_Magic(IFile *file, u16* headerSizeOut)
{
    u64     magic;
    u64     total;
    Result  res = 0;
    int     verDif;
    bool    hasHeaderSize = true;

    file->pos = 0;
    if (R_FAILED((res = IFile_Read(file, &total, &magic, sizeof(u64)))))
        return res;

    if (magic == _3GX_MAGIC_OLD_V2) {
        // Loader version 3 is backwards compatible with version 2
        magic = _3GX_MAGIC;
        hasHeaderSize = false;
        *headerSizeOut = _3GX_VERSION_V2_HEADER_SIZE;
    }

    if ((u32)magic != (u32)_3GX_MAGIC) //Invalid file type
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, 1);

    else if ((verDif = invertEndianness((u32)(magic >> 32)) - invertEndianness((u32)(_3GX_MAGIC >> 32))) != 0) //Invalid plugin version (2 -> outdated plugin; 3 -> outdated loader)
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, (verDif < 0) ? 2 : 3);

    if (hasHeaderSize) {
        file->pos = offsetof(_3gx_Header, headerSize);
        if (R_FAILED((res = IFile_Read(file, &total, headerSizeOut, sizeof(u16)))))
            return res;
        if (total != sizeof(u16) || *headerSizeOut < _3GX_VERSION_V2_HEADER_SIZE)
            return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, 4);
    }

    return res;
}

Result  Read_3gx_Header(IFile *file, _3gx_Header *header, u16 headerSize)
{
    u64     total;
    Result  res = 0;

    if (headerSize > sizeof(_3gx_Header)) {
        headerSize = sizeof(_3gx_Header);
    }

    memset(header, 0, sizeof(_3gx_Header));

    file->pos = 0;
    res = IFile_Read(file, &total, header, headerSize);
    if (R_SUCCEEDED(res) && total != headerSize)
        res = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_INVALID_SIZE);

    return res;
}

Result  Read_3gx_String(IFile *file, u32 offset, u32 len, char *out, u32 outSize)
{
    u64     total = 0;
    Result  res = 0;

    if (len >= outSize)
        len = outSize - 1;

    file->pos = offset;
    res = IFile_Read(file, &total, out, len);
    out[R_SUCCEEDED(res) ? (u32)total : 0] = '\0';

    return res;
}

Result  Read_3gx_IsTargetCompatible(IFile *file, _3gx_Header *header, u32 titleId, bool *compatible)
{
    u32     titles[32];
    u32     left = header->targets.count;
    u64     total;
    Result  res = 0;

    *compatible = left == 0;

    file->pos = (u32)header->targets.titles;
    while (left != 0 && !*compatible)
    {
        u32 count = left < 32 ? left : 32;

        res = IFile_Read(file, &total, titles, count * sizeof(u32));
        if (R_SUCCEEDED(res) && total != count * sizeof(u32))
            res = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_INVALID_SIZE);
        if (R_FAILED(res))
            return res;

        for (u32 i = 0; i < count; ++i)
        {
            if (titles[i] == titleId)
                *compatible = true;
        }
        left -= count;
    }

    return res;
}

Result  Read_3gx_LoadSegments(IFile *file, _3gx_Header *header, void *dst)
{
    u32                 size;
    u64                 total;
    Result              res = 0;
    _3gx_Executable     *exeHdr = &header->executable;
    PluginLoaderContext *ctx = &PluginLoaderCtx;

    file->pos = exeHdr->codeOffset;
    size = exeHdr->codeSize + exeHdr->rodataSize + exeHdr->dataSize;
    res = IFile_Read(file, &total, dst, size);
    
    
    if (!res && !ctx->isExeLoadFunctionset) return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_NO_DATA);
    u32 checksum = 0;
    if (!res) checksum = loadExeFunc(dst, dst + size, g_loadExeArgs);
    if (!res && checksum != ctx->exeLoadChecksum) res = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_INVALID_ADDRESS);
    Reset_3gx_LoadParams();
    return res;
}

Result Read_3gx_EmbeddedPayloads(IFile *file, _3gx_Header *header)
{
    u32                 tempBuff[32];
    u32                 tempBuff2[4];
    u64                 total;
    Result              res = 0;
    PluginLoaderContext *ctx = &PluginLoaderCtx;
    
    if (header->infos.embeddedExeLoadFunc) {
        file->pos = header->executable.exeLoadFuncOffset;
        res = IFile_Read(file, &total, tempBuff, sizeof(tempBuff));
        memcpy(tempBuff2, header->infos.builtInLoadExeArgs, sizeof(tempBuff2));
        if (!res) res = Set_3gx_LoadParams(tempBuff, tempBuff2);
        if (!res) ctx->isExeLoadFunctionset = true;
    }
    if (!res && header->infos.embeddedSwapSaveLoadFunc) {
        file->pos = header->executable.swapSaveFuncOffset;
        res = IFile_Read(file, &total, tempBuff, sizeof(tempBuff));
        memcpy(tempBuff2, header->infos.builtInSwapSaveLoadArgs, sizeof(tempBuff2));
        if (!res) res = MemoryBlock__SetSwapSettings(tempBuff, false, tempBuff2);
        file->pos = header->executable.swapLoadFuncOffset;
        if (!res) res = IFile_Read(file, &total, tempBuff, sizeof(tempBuff));
        if (!res) res = MemoryBlock__SetSwapSettings(tempBuff, true, tempBuff2);
        if (!res) ctx->isSwapFunctionset = true;
    }
    return res;
}

Result    Set_3gx_LoadParams(u32* loadFunc, u32* params)
{
    u32* loadExeFuncAddr = PA_FROM_VA_PTR((u32)loadExeFunc); //Bypass mem permissions

	memcpy(g_loadExeArgs, params, sizeof(g_loadExeArgs));
    
    int i = 0;
    for (; i < 32 && loadFunc[i] != 0xE320F000; i++)
        loadExeFuncAddr[i] = loadFunc[i];

    if (i >= 32) {
        return -1;
    }
    return 0;
}

void       Reset_3gx_LoadParams(void)
{
    PluginLoaderContext *ctx = &PluginLoaderCtx;
	u32* loadExeFuncAddr = PA_FROM_VA_PTR((u32)loadExeFunc); //Bypass mem permissions

	memset(g_loadExeArgs, 0, sizeof(g_loadExeArgs));

	loadExeFuncAddr[0] = 0xE12FFF1E; // BX LR

	for (int i = 1; i < 32; i++) {
		loadExeFuncAddr[i] = 0xE320F000; // NOP
	}
    
    ctx->isExeLoadFunctionset = false;

	svcInvalidateEntireInstructionCache();
}
