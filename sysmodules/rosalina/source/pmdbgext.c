// License for this file: ctrulib's license
// Copyright AuroraWright, TuxSH 2019-2020

#include <string.h>
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/svc.h>
#include <3ds/srv.h>
#include <3ds/synchronization.h>
#include <3ds/services/pmdbg.h>
#include <3ds/ipc.h>

Result PMDBG_GetCurrentAppInfo(FS_ProgramInfo *outProgramInfo, u32 *outPid, u32 *outLaunchFlags)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x100, 0, 0);
    if(R_FAILED(ret = svcSendSyncRequest(*pmDbgGetSessionHandle()))) return ret;

    memcpy(outProgramInfo, cmdbuf + 2, sizeof(FS_ProgramInfo));
    *outPid = cmdbuf[6];
    *outLaunchFlags = cmdbuf[7];
    return cmdbuf[1];
}

Result PMDBG_DebugNextApplicationByForce(bool debug)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x101, 1, 0);
    cmdbuf[1] = (u32)debug;

    if(R_FAILED(ret = svcSendSyncRequest(*pmDbgGetSessionHandle()))) return ret;
    return cmdbuf[1];
}

Result PMDBG_LaunchTitleDebug(Handle *outDebug, const FS_ProgramInfo *programInfo, u32 launchFlags)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x102, 5, 0);
    memcpy(&cmdbuf[1], programInfo, sizeof(FS_ProgramInfo));
    cmdbuf[5] = launchFlags;

    if(R_FAILED(ret = svcSendSyncRequest(*pmDbgGetSessionHandle()))) return ret;

    *outDebug = cmdbuf[3];
    return (Result)cmdbuf[1];
}

Result PMDBG_PrepareToChainloadHomebrew(u64 titleId)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x103, 2, 0);
    memcpy(&cmdbuf[1], &titleId, 8);

    if(R_FAILED(ret = svcSendSyncRequest(*pmDbgGetSessionHandle()))) return ret;

    return (Result)cmdbuf[1];
}
