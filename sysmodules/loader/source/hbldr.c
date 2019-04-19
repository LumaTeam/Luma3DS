#include <3ds.h>
#include <string.h>
#include "hbldr.h"

static u32 hbldrRefcount = 0;
static Handle hbldrHandle = 0;

Result hbldrInit(void)
{
    Result res;
    if (AtomicPostIncrement(&hbldrRefcount)) return 0;

    for(res = 0xD88007FA; res == (Result)0xD88007FA; svcSleepThread(500 * 1000LL)) {
        res = svcConnectToPort(&hbldrHandle, "hb:ldr");
        if(R_FAILED(res) && res != (Result)0xD88007FA) {
            AtomicDecrement(&hbldrRefcount);
            return res;
        }
    }

    return 0;
}

void hbldrExit(void)
{
    if (AtomicDecrement(&hbldrRefcount)) return;
    svcCloseHandle(hbldrHandle);
}

Result HBLDR_LoadProcess(Handle *outCodeSet, u32 textAddr, u32 kernelFlags, u64 titleId, const char *name)
{
    u32* cmdbuf = getThreadCommandBuffer(); // 0x11800
    cmdbuf[0] = IPC_MakeHeader(1, 6, 0);
    cmdbuf[1] = textAddr;
    cmdbuf[2] = kernelFlags & 0xF00;
    memcpy(&cmdbuf[3], &titleId, 8);
    strncpy((char *)&cmdbuf[5], name, 8);
    Result rc = svcSendSyncRequest(hbldrHandle);
    if (R_SUCCEEDED(rc)) rc = cmdbuf[1];

    *outCodeSet = R_SUCCEEDED(rc) ? cmdbuf[3] : 0;
    return rc;
}

Result HBLDR_SetTarget(const char* path)
{
    u32 pathLen = strlen(path) + 1;
    u32* cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(2, 0, 2); // 0x20002
    cmdbuf[1] = IPC_Desc_StaticBuffer(pathLen, 0);
    cmdbuf[2] = (u32)path;

    Result rc = svcSendSyncRequest(hbldrHandle);
    if (R_SUCCEEDED(rc)) rc = cmdbuf[1];
    return rc;
}

Result HBLDR_SetArgv(const void* buffer, size_t size)
{
    u32* cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(3, 0, 2); // 0x30002
    cmdbuf[1] = IPC_Desc_StaticBuffer(size, 1);
    cmdbuf[2] = (u32)buffer;

    Result rc = svcSendSyncRequest(hbldrHandle);
    if (R_SUCCEEDED(rc)) rc = cmdbuf[1];
    return rc;
}

Result HBLDR_PatchExHeaderInfo(ExHeader_Info *exheaderInfo)
{
    u32* cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(4, 0, 2); // 0x40002
    cmdbuf[1] = IPC_Desc_Buffer(sizeof(*exheaderInfo), IPC_BUFFER_RW);
    cmdbuf[2] = (u32)exheaderInfo;

    Result rc = svcSendSyncRequest(hbldrHandle);
    if (R_SUCCEEDED(rc)) rc = cmdbuf[1];

    return rc;
}

Result HBLDR_DebugNextApplicationByForce(bool dryRun)
{
    u32* cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(5, 1, 0); // 0x50040
    cmdbuf[1] = dryRun ? 1 : 0;

    Result rc = svcSendSyncRequest(hbldrHandle);
    if (R_SUCCEEDED(rc)) rc = cmdbuf[1];

    return rc;
}