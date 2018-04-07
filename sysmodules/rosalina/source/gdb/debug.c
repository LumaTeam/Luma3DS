/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include "gdb/debug.h"
#include "gdb/verbose.h"
#include "gdb/net.h"
#include "gdb/thread.h"
#include "gdb/mem.h"
#include "gdb/watchpoints.h"
#include "fmt.h"

#include <stdlib.h>
#include <signal.h>

/*
    Since we can't select particular threads to continue (and that's uncompliant behavior):
        - if we continue the current thread, continue all threads
        - otherwise, leaves all threads stopped but make the client believe it's continuing
*/

GDB_DECLARE_HANDLER(Detach)
{
    ctx->state = GDB_STATE_CLOSING;
    return GDB_ReplyOk(ctx);
}

GDB_DECLARE_HANDLER(Kill)
{
    ctx->state = GDB_STATE_CLOSING;
    ctx->flags |= GDB_FLAG_TERMINATE_PROCESS;
    return 0;
}

GDB_DECLARE_HANDLER(Break)
{
    if(!(ctx->flags & GDB_FLAG_PROCESS_CONTINUING))
        return GDB_SendPacket(ctx, "S02", 3);
    else
    {
        ctx->flags &= ~GDB_FLAG_PROCESS_CONTINUING;
        return 0;
    }
}

static void GDB_ContinueExecution(GDBContext *ctx)
{
    ctx->selectedThreadId = ctx->selectedThreadIdForContinuing = 0;
    svcContinueDebugEvent(ctx->debug, ctx->continueFlags);
    ctx->flags |= GDB_FLAG_PROCESS_CONTINUING;
}

GDB_DECLARE_HANDLER(Continue)
{
    char *addrStart = NULL;
    u32 addr = 0;

    if(ctx->selectedThreadIdForContinuing != 0 && ctx->selectedThreadIdForContinuing != ctx->currentThreadId)
        return 0;

    if(ctx->commandData[-1] == 'C')
    {
        if(ctx->commandData[0] == 0 || ctx->commandData[1] == 0 || (ctx->commandData[2] != 0 && ctx->commandData[2] == ';'))
            return GDB_ReplyErrno(ctx, EILSEQ);

        // Signal ignored...

        if(ctx->commandData[2] == ';')
            addrStart = ctx->commandData + 3;
    }
    else
    {
        if(ctx->commandData[0] != 0)
            addrStart = ctx->commandData;
    }

    if(addrStart != NULL && ctx->currentThreadId != 0)
    {
        ThreadContext regs;
        if(GDB_ParseHexIntegerList(&addr, ctx->commandData + 3, 1, 0) == NULL)
            return GDB_ReplyErrno(ctx, EILSEQ);

        Result r = svcGetDebugThreadContext(&regs, ctx->debug, ctx->currentThreadId, THREADCONTEXT_CONTROL_CPU_SPRS);
        if(R_SUCCEEDED(r))
        {
            regs.cpu_registers.pc = addr;
            r = svcSetDebugThreadContext(ctx->debug, ctx->currentThreadId, &regs, THREADCONTEXT_CONTROL_CPU_SPRS);
        }
    }

    GDB_ContinueExecution(ctx);
    return 0;
}

GDB_DECLARE_VERBOSE_HANDLER(Continue)
{
    char *pos = ctx->commandData;
    bool currentThreadFound = false;
    while(pos != NULL && *pos != 0 && !currentThreadFound)
    {
        if(*pos != 'c' && *pos != 'C')
            return GDB_ReplyErrno(ctx, EPERM);

        pos += *pos == 'C' ? 3 : 1;

        if(*pos++ != ':') // default action found
        {
            currentThreadFound = true;
            break;
        }

        char *nextpos = (char *)strchr(pos, ';');
        if(strncmp(pos, "-1", 2) == 0)
            currentThreadFound = true;
        else
        {
            u32 threadId;
            if(GDB_ParseHexIntegerList(&threadId, pos, 1, ';') == NULL)
                return GDB_ReplyErrno(ctx, EILSEQ);
            currentThreadFound = currentThreadFound || threadId == ctx->currentThreadId;
        }

        pos = nextpos;
    }

    if(ctx->currentThreadId == 0 || currentThreadFound)
        GDB_ContinueExecution(ctx);

    return 0;
}

GDB_DECLARE_HANDLER(GetStopReason)
{
    return GDB_SendStopReply(ctx, &ctx->latestDebugEvent);
}

static int GDB_ParseCommonThreadInfo(char *out, GDBContext *ctx, int sig)
{
    u32 threadId = ctx->currentThreadId;
    ThreadContext regs;
    s64 dummy;
    u32 core;
    Result r = svcGetDebugThreadContext(&regs, ctx->debug, threadId, THREADCONTEXT_CONTROL_ALL);
    int n = sprintf(out, "T%02xthread:%x;", sig, threadId);

    if(R_FAILED(r))
        return n;

    r = svcGetDebugThreadParam(&dummy, &core, ctx->debug, ctx->currentThreadId, DBGTHREAD_PARAMETER_CPU_CREATOR); // Creator = "first ran, and running the thread"

    if(R_SUCCEEDED(r))
        n += sprintf(out + n, "core:%x;", core);

    for(u32 i = 0; i <= 12; i++)
        n += sprintf(out + n, "%x:%08x;", i, __builtin_bswap32(regs.cpu_registers.r[i]));

    n += sprintf(out + n, "d:%08x;e:%08x;f:%08x;19:%08x;",
        __builtin_bswap32(regs.cpu_registers.sp), __builtin_bswap32(regs.cpu_registers.lr), __builtin_bswap32(regs.cpu_registers.pc),
        __builtin_bswap32(regs.cpu_registers.cpsr));

    for(u32 i = 0; i < 16; i++)
    {
        u64 val;
        memcpy(&val, &regs.fpu_registers.d[i], 8);
        n += sprintf(out + n, "%x:%016llx;", 26 + i, __builtin_bswap64(val));
    }

    n += sprintf(out + n, "2a:%08x;2b:%08x;", __builtin_bswap32(regs.fpu_registers.fpscr), __builtin_bswap32(regs.fpu_registers.fpexc));

    return n;
}

void GDB_PreprocessDebugEvent(GDBContext *ctx, DebugEventInfo *info)
{
    switch(info->type)
    {
        case DBGEVENT_ATTACH_THREAD:
        {
            if(ctx->nbThreads == MAX_DEBUG_THREAD)
                svcBreak(USERBREAK_ASSERT);
            else
            {
                ctx->threadInfos[ctx->nbThreads].id = info->thread_id;
                ctx->threadInfos[ctx->nbThreads++].tls = info->attach_thread.thread_local_storage;
            }

            break;
        }

        case DBGEVENT_EXIT_THREAD:
        {
            u32 i;
            for(i = 0; i < ctx->nbThreads && ctx->threadInfos[i].id != info->thread_id; i++);
            if(i == ctx->nbThreads ||  ctx->threadInfos[i].id != info->thread_id)
                svcBreak(USERBREAK_ASSERT);
            else
            {
                for(u32 j = i; j < ctx->nbThreads - 1; j++)
                    memcpy(ctx->threadInfos + j, ctx->threadInfos + j + 1, sizeof(ThreadInfo));
                memset(ctx->threadInfos + --ctx->nbThreads, 0, sizeof(ThreadInfo));
            }

            break;
        }

        case DBGEVENT_EXIT_PROCESS:
        {
            ctx->processEnded = true;
            ctx->processExited = info->exit_process.reason == EXITPROCESS_EVENT_EXIT;

            break;
        }

        case DBGEVENT_OUTPUT_STRING:
        {
            if(info->output_string.string_addr >= 0xFFFFFFFE)
            {
                u32 sz = info->output_string.string_size, addr = info->output_string.string_addr, threadId = info->thread_id;
                memset(info, 0, sizeof(DebugEventInfo));
                info->type = (addr == 0xFFFFFFFF) ? DBGEVENT_SYSCALL_OUT : DBGEVENT_SYSCALL_IN;
                info->thread_id = threadId;
                info->flags = 1;
                info->syscall.syscall = sz;
            }

            break;
        }

        case DBGEVENT_EXCEPTION:
        {
            switch(info->exception.type)
            {
                case EXCEVENT_UNDEFINED_INSTRUCTION:
                {
                    // kernel bugfix for thumb mode
                    ThreadContext regs;
                    Result r = svcGetDebugThreadContext(&regs, ctx->debug, info->thread_id, THREADCONTEXT_CONTROL_CPU_SPRS);
                    if(R_SUCCEEDED(r) && (regs.cpu_registers.cpsr & 0x20) != 0)
                    {
                        regs.cpu_registers.pc += 2;
                        r = svcSetDebugThreadContext(ctx->debug, info->thread_id, &regs, THREADCONTEXT_CONTROL_CPU_SPRS);
                    }

                    break;
                }

                default:
                    break;
            }
        }
        default:
            break;
    }
}

int GDB_SendStopReply(GDBContext *ctx, const DebugEventInfo *info)
{
    char buffer[GDB_BUF_LEN + 1];

    switch(info->type)
    {
        case DBGEVENT_ATTACH_PROCESS:
            break; // Dismissed

        case DBGEVENT_ATTACH_THREAD:
        {
            if(info->attach_thread.creator_thread_id == 0 || !ctx->catchThreadEvents)
                break; // Dismissed
            else
            {
                ctx->currentThreadId = info->thread_id;
                return GDB_SendPacket(ctx, "T05create:;", 10);
            }
        }

        case DBGEVENT_EXIT_THREAD:
        {
            if(ctx->catchThreadEvents && info->exit_thread.reason < EXITTHREAD_EVENT_EXIT_PROCESS)
            {
                // no signal, SIGTERM, SIGQUIT (process exited), SIGTERM (process terminated)
                static int threadExitRepliesSigs[] = { 0, SIGTERM, SIGQUIT, SIGTERM };
                return GDB_SendFormattedPacket(ctx, "w%02x;%x", threadExitRepliesSigs[(u32)info->exit_thread.reason], info->thread_id);
            }
            break;
        }

        case DBGEVENT_EXIT_PROCESS:
        {
            // exited (no error / unhandled exception), SIGTERM (process terminated) * 2
            static const char *processExitReplies[] = { "W00", "X0f", "X0f" };
            return GDB_SendPacket(ctx, processExitReplies[(u32)info->exit_process.reason], 3);
        }

        case DBGEVENT_EXCEPTION:
        {
            ExceptionEvent exc = info->exception;

            switch(exc.type)
            {
                case EXCEVENT_UNDEFINED_INSTRUCTION:
                case EXCEVENT_PREFETCH_ABORT: // doesn't include hardware breakpoints
                case EXCEVENT_DATA_ABORT:     // doesn't include hardware watchpoints
                case EXCEVENT_UNALIGNED_DATA_ACCESS:
                case EXCEVENT_UNDEFINED_SYSCALL:
                {
                    u32 signum = exc.type == EXCEVENT_UNDEFINED_INSTRUCTION ? SIGILL :
                                (exc.type == EXCEVENT_UNDEFINED_SYSCALL ? SIGSYS : SIGSEGV);

                    ctx->currentThreadId = info->thread_id;
                    GDB_ParseCommonThreadInfo(buffer, ctx, signum);
                    return GDB_SendFormattedPacket(ctx, "%s", buffer);
                }

                case EXCEVENT_ATTACH_BREAK:
                    return GDB_SendPacket(ctx, "S00", 3);

                case EXCEVENT_STOP_POINT:
                {
                    ctx->currentThreadId = info->thread_id;

                    switch(exc.stop_point.type)
                    {
                        case STOPPOINT_SVC_FF:
                        {
                            GDB_ParseCommonThreadInfo(buffer, ctx, SIGTRAP);
                            return GDB_SendFormattedPacket(ctx, "%sswbreak:;", buffer);
                            break;
                        }

                        case STOPPOINT_BREAKPOINT:
                        {
                            // /!\ Not actually implemented (and will never be)
                            GDB_ParseCommonThreadInfo(buffer, ctx, SIGTRAP);
                            return GDB_SendFormattedPacket(ctx, "%shwbreak:;", buffer);
                            break;
                        }

                        case STOPPOINT_WATCHPOINT:
                        {
                            const char *kinds[] = { "", "r", "", "a" };
                            WatchpointKind kind = GDB_GetWatchpointKind(ctx, exc.stop_point.fault_information);
                            if(kind == WATCHPOINT_DISABLED)
                                GDB_SendDebugString(ctx, "Warning: unknown watchpoint encountered!\n");

                            GDB_ParseCommonThreadInfo(buffer, ctx, SIGTRAP);
                            return GDB_SendFormattedPacket(ctx, "%s%swatch:%08x;", buffer, kinds[(u32)kind], exc.stop_point.fault_information);
                            break;
                        }

                        default:
                            break;
                    }
                }

                case EXCEVENT_USER_BREAK:
                {
                    ctx->currentThreadId = info->thread_id;
                    GDB_ParseCommonThreadInfo(buffer, ctx, SIGINT);
                    return GDB_SendFormattedPacket(ctx, "%s", buffer);
                    //TODO
                }

                case EXCEVENT_DEBUGGER_BREAK:
                {
                    u32 threadIds[4];
                    u32 nbThreads = 0;

                    for(u32 i = 0; i < 4; i++)
                    {
                        if(exc.debugger_break.thread_ids[i] > 0)
                            threadIds[nbThreads++] = (u32)exc.debugger_break.thread_ids[i];
                    }

                    u32 currentThreadId = nbThreads > 0 ? GDB_GetCurrentThreadFromList(ctx, threadIds, nbThreads) : GDB_GetCurrentThread(ctx);
                    s64 dummy;
                    u32 mask = 0;

                    svcGetDebugThreadParam(&dummy, &mask, ctx->debug, currentThreadId, DBGTHREAD_PARAMETER_SCHEDULING_MASK_LOW);

                    if(mask == 1)
                    {
                        ctx->currentThreadId = currentThreadId;
                        GDB_ParseCommonThreadInfo(buffer, ctx, SIGINT);
                        return GDB_SendFormattedPacket(ctx, "%s", buffer);
                    }
                    else
                        return GDB_SendPacket(ctx, "S02", 3);
                }

                default:
                    break;
            }

            break;
        }

        case DBGEVENT_SYSCALL_IN:
        {
            ctx->currentThreadId = info->thread_id;
            GDB_ParseCommonThreadInfo(buffer, ctx, SIGTRAP);
            return GDB_SendFormattedPacket(ctx, "%ssyscall_entry:%02x;", buffer, info->syscall.syscall);
        }

        case DBGEVENT_SYSCALL_OUT:
        {
            ctx->currentThreadId = info->thread_id;
            GDB_ParseCommonThreadInfo(buffer, ctx, SIGTRAP);
            return GDB_SendFormattedPacket(ctx, "%ssyscall_return:%02x;", buffer, info->syscall.syscall);
        }

        case DBGEVENT_OUTPUT_STRING:
        {
            u32 addr = info->output_string.string_addr;
            u32 remaining = info->output_string.string_size;
            u32 sent = 0;
            int total = 0;
            while(remaining > 0)
            {
                u32 pending = (GDB_BUF_LEN - 1) / 2;
                pending = pending < remaining ? pending : remaining;

                int res = GDB_SendMemory(ctx, "O", 1, addr + sent, pending);
                if(res < 0 || (u32) res != 5 + 2 * pending)
                    break;

                sent += pending;
                remaining -= pending;
                total += res;
            }

            return total;
        }
        default:
            break;
    }

    return 0;
}

/*
    Only 1 blocking event can be enqueued at a time: they preempt all the other threads.
    The only "non-blocking" event that is implemented is EXIT PROCESS (but it's a very special case)
*/
int GDB_HandleDebugEvents(GDBContext *ctx)
{
    if(ctx->state == GDB_STATE_CLOSING)
        return -1;

    DebugEventInfo info;
    Result rdbg = svcGetProcessDebugEvent(&info, ctx->debug);

    if(R_FAILED(rdbg))
        return -1;

    GDB_PreprocessDebugEvent(ctx, &info);

    int ret = 0;
    bool continueAutomatically = info.type == DBGEVENT_OUTPUT_STRING || info.type == DBGEVENT_ATTACH_PROCESS ||
                                (info.type == DBGEVENT_ATTACH_THREAD && (info.attach_thread.creator_thread_id == 0 || !ctx->catchThreadEvents)) ||
                                (info.type == DBGEVENT_EXIT_THREAD && (info.exit_thread.reason >= EXITTHREAD_EVENT_EXIT_PROCESS || !ctx->catchThreadEvents)) ||
                                info.type == DBGEVENT_EXIT_PROCESS || !(info.flags & 1);


    if(continueAutomatically)
    {
        Result r = 0;
        ret = GDB_SendStopReply(ctx, &info);
        if(info.flags & 1)
            r = svcContinueDebugEvent(ctx->debug, ctx->continueFlags);

        if(r == (Result)0xD8A02008) // process ended
            return -2;

        return -ret - 3;
    }
    else
    {
        int ret;

        if(ctx->processEnded)
            return -2;

        ctx->latestDebugEvent = info;
        ret = GDB_SendStopReply(ctx, &info);
        ctx->flags &= ~GDB_FLAG_PROCESS_CONTINUING;
        return ret;
    }
}
