/*
*   This file is part of Luma3DS.
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   SPDX-License-Identifier: (MIT OR GPL-2.0-or-later)
*/

#include "gdb/remote_command.h"
#include "gdb/net.h"
#include "csvc.h"
#include "fmt.h"
#include "gdb/breakpoints.h"
#include "utils.h"

#include "../utils.h"

struct
{
    const char *name;
    GDBCommandHandler handler;
} remoteCommandHandlers[] =
{
    { "convertvatopa"     , GDB_REMOTE_COMMAND_HANDLER(ConvertVAToPA) },
    { "syncrequestinfo"   , GDB_REMOTE_COMMAND_HANDLER(SyncRequestInfo) },
    { "translatehandle"   , GDB_REMOTE_COMMAND_HANDLER(TranslateHandle) },
    { "listallhandles"    , GDB_REMOTE_COMMAND_HANDLER(ListAllHandles) },
    { "getmmuconfig"      , GDB_REMOTE_COMMAND_HANDLER(GetMmuConfig) },
    { "getmemregions"     , GDB_REMOTE_COMMAND_HANDLER(GetMemRegions) },
    { "flushcaches"       , GDB_REMOTE_COMMAND_HANDLER(FlushCaches) },
    { "toggleextmemaccess", GDB_REMOTE_COMMAND_HANDLER(ToggleExternalMemoryAccess) },
    { "catchsvc"          , GDB_REMOTE_COMMAND_HANDLER(CatchSvc) },
    { "getthreadpriority" , GDB_REMOTE_COMMAND_HANDLER(GetThreadPriority)}
};

static const char *GDB_SkipSpaces(const char *pos)
{
    const char *nextpos;
    for(nextpos = pos; *nextpos != 0 && ((*nextpos >= 9 && *nextpos <= 13) || *nextpos == ' '); nextpos++);
    return nextpos;
}

GDB_DECLARE_REMOTE_COMMAND_HANDLER(ConvertVAToPA)
{
    bool    ok;
    int     n;
    u32     val;
    u32     pa;
    char *  end;
    char    outbuf[GDB_BUF_LEN / 2 + 1];

    if(ctx->commandData[0] == 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    val = xstrtoul(ctx->commandData, &end, 0, true, &ok);

    if(!ok)
        return GDB_ReplyErrno(ctx, EILSEQ);

    if (val >= 0x40000000)
        pa = svcConvertVAToPA((const void *)val, false);
    else
    {
        Handle process;
        Result r = svcOpenProcess(&process, ctx->pid);
        if(R_FAILED(r))
        {
            n = sprintf(outbuf, "Invalid process (wtf?)\n");
            goto end;
        }
        r = svcControlProcess(process, PROCESSOP_GET_PA_FROM_VA, (u32)&pa, val);
        svcCloseHandle(process);

        if (R_FAILED(r))
        {
            n = sprintf(outbuf, "An error occured: %08lX\n", r);
            goto end;
        }
    }

    n = sprintf(outbuf, "va: 0x%08lX, pa: 0x%08lX, b31: 0x%08lX\n", val, pa, pa | (1 << 31));
end:
    return GDB_SendHexPacket(ctx, outbuf, n);
}


GDB_DECLARE_REMOTE_COMMAND_HANDLER(SyncRequestInfo)
{
    char outbuf[GDB_BUF_LEN / 2 + 1];
    Result r;
    int n;

    if(ctx->commandData[0] != 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    if(ctx->selectedThreadId == 0)
        ctx->selectedThreadId = ctx->currentThreadId;

    if(ctx->selectedThreadId == 0)
    {
        n = sprintf(outbuf, "Cannot run this command without a selected thread.\n");
        goto end;
    }

    u32 id;
    u32 cmdId;
    ThreadContext regs;
    u32 instr;
    Handle process;
    r = svcOpenProcess(&process, ctx->pid);
    if(R_FAILED(r))
    {
        n = sprintf(outbuf, "Invalid process (wtf?)\n");
        goto end;
    }

    for(id = 0; id < MAX_DEBUG_THREAD && ctx->threadInfos[id].id != ctx->selectedThreadId; id++);

    r = svcGetDebugThreadContext(&regs, ctx->debug, ctx->selectedThreadId, THREADCONTEXT_CONTROL_CPU_REGS);

    if(R_FAILED(r) || id == MAX_DEBUG_THREAD)
    {
        n = sprintf(outbuf, "Invalid or running thread.\n");
        goto end;
    }

    r = svcReadProcessMemory(&cmdId, ctx->debug, ctx->threadInfos[id].tls + 0x80, 4);
    if(R_FAILED(r))
    {
        n = sprintf(outbuf, "Invalid or running thread.\n");
        goto end;
    }

    r = svcReadProcessMemory(&instr, ctx->debug, regs.cpu_registers.pc, (regs.cpu_registers.cpsr & 0x20) ? 2 : 4);

    if(R_SUCCEEDED(r) && (((regs.cpu_registers.cpsr & 0x20) && instr == BREAKPOINT_INSTRUCTION_THUMB) || instr == BREAKPOINT_INSTRUCTION_ARM))
    {
        u32 savedInstruction;
        if(GDB_GetBreakpointInstruction(&savedInstruction, ctx, regs.cpu_registers.pc) == 0)
            instr = savedInstruction;
    }

    if(R_FAILED(r) || ((regs.cpu_registers.cpsr & 0x20) && !(instr == 0xDF32 || (instr == 0xDFFE && regs.cpu_registers.r[12] == 0x32)))
                   || (!(regs.cpu_registers.cpsr & 0x20) && !(instr == 0xEF000032 || (instr == 0xEF0000FE && regs.cpu_registers.r[12] == 0x32))))
    {
        n = sprintf(outbuf, "The selected thread is not currently performing a sync request (svc 0x32).\n");
        goto end;
    }

    char name[12];
    Handle handle;
    r = svcCopyHandle(&handle, CUR_PROCESS_HANDLE, (Handle)regs.cpu_registers.r[0], process);
    if(R_FAILED(r))
    {
        n = sprintf(outbuf, "Invalid handle.\n");
        goto end;
    }

    r = svcControlService(SERVICEOP_GET_NAME, name, handle);
    if(R_FAILED(r))
        name[0] = 0;

    n = sprintf(outbuf, "%s 0x%lx, 0x%08lx\n", name, cmdId, ctx->threadInfos[id].tls + 0x80);

end:
    svcCloseHandle(handle);
    svcCloseHandle(process);
    return GDB_SendHexPacket(ctx, outbuf, n);
}

enum
{
    TOKEN_KAUTOOBJECT = 0,
    TOKEN_KSYNCHRONIZATIONOBJECT = 1,
    TOKEN_KEVENT = 0x1F,
    TOKEN_KSEMAPHORE = 0x2F,
    TOKEN_KTIMER = 0x35,
    TOKEN_KMUTEX = 0x39,
    TOKEN_KDEBUG = 0x4D,
    TOKEN_KSERVERPORT = 0x55,
    TOKEN_KDMAOBJECT = 0x59,
    TOKEN_KCLIENTPORT = 0x65,
    TOKEN_KCODESET = 0x68,
    TOKEN_KSESSION = 0x70,
    TOKEN_KTHREAD = 0x8D,
    TOKEN_KSERVERSESSION = 0x95,
    TOKEN_KCLIENTSESSION = 0xA5,
    TOKEN_KPORT = 0xA8,
    TOKEN_KSHAREDMEMORY = 0xB0,
    TOKEN_KPROCESS = 0xC5,
    TOKEN_KRESOURCELIMIT = 0xC8
};

GDB_DECLARE_REMOTE_COMMAND_HANDLER(TranslateHandle)
{
    bool ok;
    u32 val;
    char *end;
    int n;
    Result r;
    u32 kernelAddr;
    s64 token;
    Handle handle, process;
    s64 refcountRaw;
    u32 refcount;
    char classBuf[32], serviceBuf[12] = {0}, ownerBuf[50] = { 0 };
    char outbuf[GDB_BUF_LEN / 2 + 1];

    if(ctx->commandData[0] == 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    val = xstrtoul(ctx->commandData, &end, 0, true, &ok);

    if(!ok)
        return GDB_ReplyErrno(ctx, EILSEQ);

    end = (char *)GDB_SkipSpaces(end);

    if(*end != 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    r = svcOpenProcess(&process, ctx->pid);
    if(R_FAILED(r))
    {
        n = sprintf(outbuf, "Invalid process (wtf?)\n");
        goto end;
    }

    r = svcCopyHandle(&handle, CUR_PROCESS_HANDLE, (Handle)val, process);
    if(R_FAILED(r))
    {
        n = sprintf(outbuf, "Invalid handle.\n");
        goto end;
    }

    svcTranslateHandle(&kernelAddr, classBuf, handle);
    svcGetHandleInfo(&refcountRaw, handle, 1);
    svcGetHandleInfo(&token, handle, 0x10001);
    svcControlService(SERVICEOP_GET_NAME, serviceBuf, handle);
    refcount = (u32)(refcountRaw - 1);

    if(serviceBuf[0] != 0)
        n = sprintf(outbuf, "(%s *)0x%08lx /* %s handle, %lu %s */\n", classBuf, kernelAddr, serviceBuf, refcount, refcount == 1 ? "reference" : "references");
    else if (token == TOKEN_KPROCESS)
    {
        svcGetProcessInfo((s64 *)serviceBuf, handle, 0x10000);
        n = sprintf(outbuf, "(%s *)0x%08lx /* process: %s, %lu %s */\n", classBuf, kernelAddr, serviceBuf, refcount, refcount == 1 ? "reference" : "references");
    }
    else
    {
        s64 owner;

        if (R_SUCCEEDED(svcGetHandleInfo(&owner, handle, 0x10002)))
        {
            svcGetProcessInfo((s64 *)serviceBuf, (u32)owner, 0x10000);
            svcCloseHandle((u32)owner);
            sprintf(ownerBuf, " owner: %s", serviceBuf);
        }
        n = sprintf(outbuf, "(%s *)0x%08lx /* %lu %s%s */\n", classBuf, kernelAddr, refcount, refcount == 1 ? "reference" : "references", ownerBuf);
    }

end:
    svcCloseHandle(handle);
    svcCloseHandle(process);
    return GDB_SendHexPacket(ctx, outbuf, n);
}

GDB_DECLARE_REMOTE_COMMAND_HANDLER(ListAllHandles)
{
    bool    ok;
    u32     val;
    char    *end;
    int     n = 0;
    Result  r;
    s32     count = 0;
    Handle  process, procHandles[0x100];
    char    outbuf[GDB_BUF_LEN / 2 + 1];

    if(ctx->commandData[0] == 0)
        val = 0; ///< All handles
    else
    { // Get handles of specified type
        val = xstrtoul(ctx->commandData, &end, 0, true, &ok);

        if(!ok)
            return GDB_ReplyErrno(ctx, EILSEQ);

        end = (char *)GDB_SkipSpaces(end);

        if(*end != 0)
            return GDB_ReplyErrno(ctx, EILSEQ);
    }

    r = svcOpenProcess(&process, ctx->pid);
    if(R_FAILED(r))
    {
        n = sprintf(outbuf, "Invalid process (wtf?)\n");
        goto end;
    }

    if (R_FAILED(count = svcControlProcess(process, PROCESSOP_GET_ALL_HANDLES, (u32)procHandles, val)))
        n = sprintf(outbuf, "An error occured: %08lX\n", count);
    else if (count == 0)
        n = sprintf(outbuf, "Process has no handles ?\n");
    else
    {
        n = sprintf(outbuf, "Found %ld handles.\n", count);

        const char *comma = "";
        for (s32 i = 0; i < count && n < (GDB_BUF_LEN >> 1) - 20; ++i)
        {
            Handle handle = procHandles[i];

            n += sprintf(outbuf + n, "%s0x%08lX", comma, handle);

            if (((i + 1) % 8) == 0)
            {
                outbuf[n++] = '\n';
                comma = "";
            }
            else
                comma = ", ";
        }
    }
end:
    svcCloseHandle(process);
    return GDB_SendHexPacket(ctx, outbuf, n);
}

extern bool isN3DS;
GDB_DECLARE_REMOTE_COMMAND_HANDLER(GetMmuConfig)
{
    int n;
    char outbuf[GDB_BUF_LEN / 2 + 1];
    Result r;
    Handle process;

    if(ctx->commandData[0] != 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    r = svcOpenProcess(&process, ctx->pid);
    if(R_FAILED(r))
        n = sprintf(outbuf, "Invalid process (wtf?)\n");
    else
    {
        s64 TTBCR, TTBR0;
        svcGetSystemInfo(&TTBCR, 0x10002, 0);
        svcGetProcessInfo(&TTBR0, process, 0x10008);
        n = sprintf(outbuf, "TTBCR = %lu\nTTBR0 = 0x%08lx\nTTBR1 =", (u32)TTBCR, (u32)TTBR0);
        for(u32 i = 0; i < (isN3DS ? 4 : 2); i++)
        {
            s64 TTBR1;
            svcGetSystemInfo(&TTBR1, 0x10002, 1 + i);

            if(i == (isN3DS ? 3 : 1))
                n += sprintf(outbuf + n, " 0x%08lx\n", (u32)TTBR1);
            else
                n += sprintf(outbuf + n, " 0x%08lx /", (u32)TTBR1);
        }
        svcCloseHandle(process);
    }

    return GDB_SendHexPacket(ctx, outbuf, n);
}

GDB_DECLARE_REMOTE_COMMAND_HANDLER(GetMemRegions)
{
    u32         posInBuffer = 0;
    Handle      handle;
    char        outbuf[GDB_BUF_LEN / 2 + 1];

    if(R_FAILED(svcOpenProcess(&handle, ctx->pid)))
    {
        posInBuffer = sprintf(outbuf, "Invalid process (wtf?)\n");
        return GDB_SendHexPacket(ctx, outbuf, posInBuffer);
    }

    posInBuffer = formatMemoryMapOfProcess(outbuf, GDB_BUF_LEN / 2, handle);
    return GDB_SendHexPacket(ctx, outbuf, posInBuffer);
}

GDB_DECLARE_REMOTE_COMMAND_HANDLER(FlushCaches)
{
    if(ctx->commandData[0] != 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    svcFlushEntireDataCache();
    svcInvalidateEntireInstructionCache();

    return GDB_ReplyOk(ctx);
}

GDB_DECLARE_REMOTE_COMMAND_HANDLER(ToggleExternalMemoryAccess)
{
    int n;
    char outbuf[GDB_BUF_LEN / 2 + 1];

    ctx->enableExternalMemoryAccess = !ctx->enableExternalMemoryAccess;

    n = sprintf(outbuf, "External memory access %s successfully.\n", ctx->enableExternalMemoryAccess ? "enabled" : "disabled");

    return GDB_SendHexPacket(ctx, outbuf, n);
}

GDB_DECLARE_REMOTE_COMMAND_HANDLER(CatchSvc)
{
    if(ctx->commandData[0] == '0')
    {
        memset(ctx->svcMask, 0, 32);
        return R_SUCCEEDED(svcKernelSetState(0x10002, ctx->pid, false)) ? GDB_ReplyOk(ctx) : GDB_ReplyErrno(ctx, EPERM);
    }
    else if(ctx->commandData[0] == '1')
    {
        if(ctx->commandData[1] == ';')
        {
            u32 id;
            const char *pos = ctx->commandData + 1;
            memset(ctx->svcMask, 0, 32);

            do
            {
                pos = GDB_ParseHexIntegerList(&id, pos + 1, 1, ';');
                if(pos == NULL)
                    return GDB_ReplyErrno(ctx, EILSEQ);

                if(id < 0xFE)
                    ctx->svcMask[id / 32] |= 1 << (31 - (id % 32));
            }
            while(*pos != 0);
        }
        else
            memset(ctx->svcMask, 0xFF, 32);

        return R_SUCCEEDED(svcKernelSetState(0x10002, ctx->pid, true, ctx->svcMask)) ? GDB_ReplyOk(ctx) : GDB_ReplyErrno(ctx, EPERM);
    }
    else
        return GDB_ReplyErrno(ctx, EILSEQ);
}

s32 GDB_GetDynamicThreadPriority(GDBContext *ctx, u32 threadId);
GDB_DECLARE_REMOTE_COMMAND_HANDLER(GetThreadPriority)
{
    int n;
    char outbuf[GDB_BUF_LEN / 2 + 1];

    n = sprintf(outbuf, "Thread (%ld) priority: 0x%02lX\n", ctx->selectedThreadId,
                GDB_GetDynamicThreadPriority(ctx, ctx->selectedThreadId));

    return GDB_SendHexPacket(ctx, outbuf, n);
}

GDB_DECLARE_QUERY_HANDLER(Rcmd)
{
    char commandData[GDB_BUF_LEN / 2 + 1];
    char *endpos;
    const char *errstr = "Unrecognized command.\n";
    u32 len = strlen(ctx->commandData);

    if(len == 0 || (len % 2) == 1 || GDB_DecodeHex(commandData, ctx->commandData, len / 2) != len / 2)
        return GDB_ReplyErrno(ctx, EILSEQ);
    commandData[len / 2] = 0;

    for(endpos = commandData; !(*endpos >= 9 && *endpos <= 13) && *endpos != ' ' && *endpos != 0; endpos++);

    char *nextpos = (char *)GDB_SkipSpaces(endpos);
    *endpos = 0;

    for(u32 i = 0; i < sizeof(remoteCommandHandlers) / sizeof(remoteCommandHandlers[0]); i++)
    {
        if(strcmp(commandData, remoteCommandHandlers[i].name) == 0)
        {
            ctx->commandData = nextpos;
            return remoteCommandHandlers[i].handler(ctx);
        }
    }

    return GDB_SendHexPacket(ctx, errstr, strlen(errstr));
}
