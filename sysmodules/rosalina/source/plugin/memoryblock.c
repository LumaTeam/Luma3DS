#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include "plugin.h"
#include "ifile.h"
#include "utils.h"

#define MEMPERM_RW (MEMPERM_READ | MEMPERM_WRITE)

u32  g_encDecSwapArgs[4];
char g_swapFileName[256];

Result      MemoryBlock__IsReady(void)
{
    PluginLoaderContext *ctx = &PluginLoaderCtx;
    MemoryBlock *memblock = &ctx->memblock;

    if (memblock->isReady)
        return 0;

    Result  res;

    if (isN3DS)
    {
        s64     appRegionSize = 0;
        s64     appRegionUsed = 0;
        u32     appRegionFree;
        u32     gameReserveSize;
        vu32*   appMemAllocPtr = (vu32 *)PA_FROM_VA_PTR(0x1FF80040);
        u32     appMemAlloc = *appMemAllocPtr;
        u32     temp;

        svcGetSystemInfo(&appRegionSize, 0x10000, 6);
        svcGetSystemInfo(&appRegionUsed, 0, 1);

        appRegionFree = appRegionSize - appRegionUsed;

        // Check if appmemalloc reports the entire available memory
        if ((u32)appRegionSize == appMemAlloc)
            *appMemAllocPtr -= MemBlockSize; ///< Remove plugin share from available memory

        gameReserveSize = appRegionFree - MemBlockSize;

        // First reserve the game memory size (to avoid heap relocation)
        res = svcControlMemoryUnsafe((u32 *)&temp, 0x30000000,
                                    gameReserveSize, MEMOP_REGION_APP | MEMOP_ALLOC | MEMOP_LINEAR_FLAG, MEMPERM_RW);

        // Then allocate our plugin memory block
        if (R_SUCCEEDED(res))
            res = svcControlMemoryUnsafe((u32 *)&memblock->memblock, 0x07000000,
                                        MemBlockSize, MEMOP_REGION_APP | MEMOP_ALLOC | MEMOP_LINEAR_FLAG, MEMPERM_RW);

        // Finally release game reserve block
        if (R_SUCCEEDED(res))
            res = svcControlMemoryUnsafe((u32 *)&temp, temp, gameReserveSize, MEMOP_FREE, 0);
    }
    else
    {
        res = svcControlMemoryUnsafe((u32 *)&memblock->memblock, 0x07000000,
                                    MemBlockSize, MEMOP_REGION_SYSTEM | MEMOP_ALLOC | MEMOP_LINEAR_FLAG, MEMPERM_RW);
    }

    if (R_FAILED(res)) {
        PluginLoader__DisableNotificationLED();
        if (isN3DS)
            PluginLoader__Error("Cannot map plugin memory.", res);
        else
            PluginLoader__Error("A console reboot is needed to\nclose extended memory games.\n\nPress [B] to reboot.", res);
        svcKernelSetState(7);
    }
    else
    {
        // Clear the memblock
        memset(memblock->memblock, 0, MemBlockSize);
        memblock->isReady = true;

        /*if (isN3DS)
        {
            // Check if appmemalloc reports the entire available memory
            s64     appRegionSize = 0;
            vu32*   appMemAlloc = (vu32 *)PA_FROM_VA_PTR(0x1FF80040);

            svcGetSystemInfo(&appRegionSize, 0x10000, 6);
            if ((u32)appRegionSize == *appMemAlloc)
                *appMemAlloc -= MemBlockSize; ///< Remove plugin share from available memory
        } */
    }

    return res;
}

Result      MemoryBlock__Free(void)
{
    MemoryBlock *memblock = &PluginLoaderCtx.memblock;

    if (!memblock->isReady)
        return 0;

    MemOp   memRegion = isN3DS ? MEMOP_REGION_APP : MEMOP_REGION_SYSTEM;
    Result  res = svcControlMemoryUnsafe((u32 *)&memblock->memblock, (u32)memblock->memblock,
                                    MemBlockSize, memRegion | MEMOP_FREE, 0);

    memblock->isReady = false;
    memblock->memblock = NULL;

    if (R_FAILED(res))
        PluginLoader__Error("Couldn't free memblock", res);

    return res;
}

#define FS_OPEN_RWC (FS_OPEN_READ | FS_OPEN_WRITE | FS_OPEN_CREATE)

Result      MemoryBlock__ToSwapFile(void)
{
    MemoryBlock *memblock = &PluginLoaderCtx.memblock;
    PluginLoaderContext *ctx = &PluginLoaderCtx;

    u64     written = 0;
    u64     toWrite = MemBlockSize;
    IFile   file;
    Result  res = 0;

    svcFlushDataCacheRange(memblock->memblock, MemBlockSize);
    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                    fsMakePath(PATH_ASCII, g_swapFileName), FS_OPEN_RWC);

    if (R_FAILED(res)) {
        PluginLoader__DisableNotificationLED();
        PluginLoader__Error("CRITICAL: Failed to open swap file.\n\nConsole will now reboot.", res);
        svcKernelSetState(7);
    }
    
    if (!ctx->isSwapFunctionset) {
        PluginLoader__DisableNotificationLED();
        PluginLoader__Error("CRITICAL: Swap encrypt function\nis not set.\n\nConsole will now reboot.", res);
        svcKernelSetState(7);
    }
    ctx->swapDecChecksum = encSwapFunc(memblock->memblock, memblock->memblock + MemBlockSize, g_encDecSwapArgs);
    
    res = IFile_Write(&file, &written, memblock->memblock, toWrite, FS_WRITE_FLUSH);

    if (R_FAILED(res) || written != toWrite) {
        PluginLoader__DisableNotificationLED();
        PluginLoader__Error("CRITICAL: Couldn't write swap to SD.\n\nConsole will now reboot.", res);
        svcKernelSetState(7);
    }

    IFile_Close(&file);
    return res;
}

Result      MemoryBlock__FromSwapFile(void)
{
    MemoryBlock *memblock = &PluginLoaderCtx.memblock;

    u64     read = 0;
    u64     toRead = MemBlockSize;
    IFile   file;
    Result  res = 0;

    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                    fsMakePath(PATH_ASCII, g_swapFileName), FS_OPEN_READ);

    if (R_FAILED(res)) {
        PluginLoader__DisableNotificationLED();
        PluginLoader__Error("CRITICAL: Failed to open swap file.\n\nConsole will now reboot.", res);
        svcKernelSetState(7);
    }

    res = IFile_Read(&file, &read, memblock->memblock, toRead);

    if (R_FAILED(res) || read != toRead) {
        PluginLoader__DisableNotificationLED();
        PluginLoader__Error("CRITICAL: Couldn't read swap from SD.\n\nConsole will now reboot.", res);
        svcKernelSetState(7);
    }
    
    u32 checksum = decSwapFunc(memblock->memblock, memblock->memblock + MemBlockSize, g_encDecSwapArgs);
    
    PluginLoaderContext *ctx = &PluginLoaderCtx;
    if (checksum != ctx->swapDecChecksum) {
        res = -1;
        PluginLoader__DisableNotificationLED();
        PluginLoader__Error("CRITICAL: Swap file is corrupted.\n\nConsole will now reboot.", res);
        svcKernelSetState(7); 
    }
    
    svcFlushDataCacheRange(memblock->memblock, MemBlockSize);
    IFile_Close(&file);
    return res;
}

Result     MemoryBlock__MountInProcess(void)
{
    Handle          target = PluginLoaderCtx.target;
    Error           *error = &PluginLoaderCtx.error;
    PluginHeader    *header = &PluginLoaderCtx.header;
    MemoryBlock     *memblock = &PluginLoaderCtx.memblock;

    Result       res = 0;

    // Executable
    if (R_FAILED((res = svcMapProcessMemoryEx(target, 0x07000000, CUR_PROCESS_HANDLE, (u32)memblock->memblock, header->exeSize))))
    {
        error->message = "Couldn't map exe memory block";
        error->code = res;
        return res;
    }

    // Heap (to be used by the plugin)
    if (R_FAILED((res = svcMapProcessMemoryEx(target, header->heapVA, CUR_PROCESS_HANDLE, (u32)memblock->memblock + header->exeSize, header->heapSize))))
    {
        error->message = "Couldn't map heap memory block";
        error->code = res;
    }

    return res;
}

Result     MemoryBlock__UnmountFromProcess(void)
{
    Handle          target = PluginLoaderCtx.target;
    PluginHeader    *header = &PluginLoaderCtx.header;

    Result  res = 0;

    res = svcUnmapProcessMemoryEx(target, 0x07000000, header->exeSize);
    res |= svcUnmapProcessMemoryEx(target, header->heapVA, header->heapSize);

    return res;
}

Result    MemoryBlock__SetSwapSettings(u32* func, bool isDec, u32* params)
{
    u32* physAddr = PA_FROM_VA_PTR(isDec ? (u32)decSwapFunc : (u32)encSwapFunc); //Bypass mem permissions

	memcpy(g_encDecSwapArgs, params, sizeof(g_encDecSwapArgs));
    
    int i = 0;
    for (; i < 32 && func[i] != 0xE320F000; i++)
        physAddr[i] = func[i];

    if (i >= 32) {
        return -1;
    }
    return 0;
}

void       MemoryBlock__ResetSwapSettings(void)
{
	u32* encPhysAddr = PA_FROM_VA_PTR((u32)encSwapFunc); //Bypass mem permissions
	u32* decPhysAddr = PA_FROM_VA_PTR((u32)decSwapFunc);
    PluginLoaderContext *ctx = &PluginLoaderCtx;

	memset(g_encDecSwapArgs, 0, sizeof(g_encDecSwapArgs));

	encPhysAddr[0] = decPhysAddr[0] = 0xE12FFF1E; // BX LR

	for (int i = 1; i < 32; i++) {
		encPhysAddr[i] = decPhysAddr[i] = 0xE320F000; // NOP
	}

	strcpy(g_swapFileName, "/luma/plugins/.swap");
    ctx->isSwapFunctionset = false;

	svcInvalidateEntireInstructionCache();
}