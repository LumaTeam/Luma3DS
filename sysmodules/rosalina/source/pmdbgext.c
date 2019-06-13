// License for this file: ctrulib's license
// Copyright AuroraWright, TuxSH 2019

#include <string.h>
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/svc.h>
#include <3ds/srv.h>
#include <3ds/synchronization.h>
#include <3ds/services/pmdbg.h>
#include <3ds/ipc.h>

Result PMDBG_GetCurrentAppTitleIdAndPid(u64 *outTitleId, u32 *outPid)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x100, 0, 0);
    if(R_FAILED(ret = svcSendSyncRequest(*pmDbgGetSessionHandle()))) return ret;

    memcpy(outTitleId, cmdbuf + 2, 8);
    *outPid = cmdbuf[4];
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
