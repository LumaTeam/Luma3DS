#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include "plugin.h"
#include "ifile.h"
#include "utils.h"

u32  g_decExeArgs[0x4];

static inline u32 invertEndianness(u32 val)
{
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

Result  Check_3gx_Magic(IFile *file)
{
    u64     magic;
    u64     total;
    Result  res;
    int     verDif;

    file->pos = 0;
    if (R_FAILED((res = IFile_Read(file, &total, &magic, sizeof(u64)))))
        return res;

    if ((u32)magic != (u32)_3GX_MAGIC) //Invalid file type
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, 1);

    else if ((verDif = invertEndianness((u32)(magic >> 32)) - invertEndianness((u32)(_3GX_MAGIC >> 32))) != 0) //Invalid plugin version (2 -> outdated plugin; 3 -> outdated loader)
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, (verDif < 0) ? 2 : 3);

    else return 0;
}

Result  Read_3gx_Header(IFile *file, _3gx_Header *header)
{
    u64     total;
    char *  dst;
    Result  res = 0;

    file->pos = 0;
    res = IFile_Read(file, &total, header, sizeof(_3gx_Header));
    if (R_FAILED(res))
        return res;

    // Read author
    file->pos = (u32)header->infos.authorMsg;
    dst = (char *)header + sizeof(_3gx_Header);
    res = IFile_Read(file, &total, dst, header->infos.authorLen);
    if (R_FAILED(res))
        return res;

    // Relocate ptr
    header->infos.authorMsg = dst;

    // Read title
    file->pos = (u32)header->infos.titleMsg;
    dst += header->infos.authorLen;
    res = IFile_Read(file, &total, dst, header->infos.titleLen);
    if (R_FAILED(res))
        return res;

    // Relocate ptr
    header->infos.titleMsg = dst;

    // Declare other members as null (unused in our case)
    header->infos.summaryLen = 0;
    header->infos.summaryMsg = NULL;
    header->infos.descriptionLen = 0;
    header->infos.descriptionMsg = NULL;

    // Read targets compatibility
    file->pos = (u32)header->targets.titles;
    dst += header->infos.titleLen;
    dst += 4 - ((u32)dst & 3); // 4 bytes aligned
    res = IFile_Read(file, &total, dst, header->targets.count * sizeof(u32));
    if (R_FAILED(res))
        return res;

    // Relocate ptr
    header->targets.titles = (u32 *)dst;
    
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
    
    
    if (!res && !ctx->isExeDecFunctionset) return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_NO_DATA);
    u32 checksum = 0;
    if (!res) checksum = decExeFunc(dst, dst + size, g_decExeArgs);
    if (!res && checksum != ctx->exeDecChecksum) res = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_INVALID_ADDRESS);
    Reset_3gx_DecParams();
    return res;
}

Result Read_3gx_EmbeddedPayloads(IFile *file, _3gx_Header *header)
{
    u32                 tempBuff[32];
    u32                 tempBuff2[4];
    u64                 total;
    Result              res = 0;
    PluginLoaderContext *ctx = &PluginLoaderCtx;
    
    if (header->infos.embeddedExeDecryptFunc) {
        file->pos = header->executable.exeDecOffset;
        res = IFile_Read(file, &total, tempBuff, sizeof(tempBuff));
        memcpy(tempBuff2, header->infos.builtInDecExeArgs, sizeof(tempBuff2));
        if (!res) res = Set_3gx_DecParams(tempBuff, tempBuff2);
        if (!res) ctx->isExeDecFunctionset = true;
    }
    if (!res && header->infos.embeddedSwapEncDecFunc) {
        file->pos = header->executable.swapEncOffset;
        res = IFile_Read(file, &total, tempBuff, sizeof(tempBuff));
        memcpy(tempBuff2, header->infos.builtInSwapEncDecArgs, sizeof(tempBuff2));
        if (!res) res = MemoryBlock__SetSwapSettings(tempBuff, false, tempBuff2);
        file->pos = header->executable.swapDecOffset;
        if (!res) res = IFile_Read(file, &total, tempBuff, sizeof(tempBuff));
        if (!res) res = MemoryBlock__SetSwapSettings(tempBuff, true, tempBuff2);
        if (!res) ctx->isSwapFunctionset = true;
    }
    return res;
}

Result    Set_3gx_DecParams(u32* decFunc, u32* params)
{
    u32* decExeFuncAddr = PA_FROM_VA_PTR((u32)decExeFunc); //Bypass mem permissions

	memcpy(g_decExeArgs, params, sizeof(g_decExeArgs));
    
    int i = 0;
    for (; i < 32 && decFunc[i] != 0xE320F000; i++)
        decExeFuncAddr[i] = decFunc[i];

    if (i >= 32) {
        return -1;
    }
    return 0;
}

void       Reset_3gx_DecParams(void)
{
    PluginLoaderContext *ctx = &PluginLoaderCtx;
	u32* decExeFuncAddr = PA_FROM_VA_PTR((u32)decExeFunc); //Bypass mem permissions

	memset(g_decExeArgs, 0, sizeof(g_decExeArgs));

	decExeFuncAddr[0] = 0xE12FFF1E; // BX LR

	for (int i = 1; i < 32; i++) {
		decExeFuncAddr[i] = 0xE320F000; // NOP
	}
    
    ctx->isExeDecFunctionset = false;

	svcInvalidateEntireInstructionCache();
}
