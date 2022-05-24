/*
*   This file is part of Luma3DS
*   Copyright (C) 2022  TuxSH
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

#include <3ds.h>
#include "MyThread.h"
#include "ifile.h"
#include "fmt.h"

//#define BOOTDIAG_ENABLED
#ifdef  BOOTDIAG_ENABLED

static MyThread bootdiagThread;
static u8 ALIGN(0x1000) bootdiagThreadStack[0x1000];

#define BOOTDIAG_WAIT_TIME      (3500 * 1000 * 1000u) // 2 seconds
#define BOOTDIAG_DUMP_PLIST     1
#define BOOTDIAG_PID            1u
#define BOOTDIAG_DUMP_TO_FILE   0

static int bootdiagDumpProcessList(char *buf)
{
    int n = 0;
    u32 pids[40];
    s32 numProcesses = 0;
    if (R_FAILED(svcGetProcessList(&numProcesses, pids, 0x40))) __builtin_trap();

    for (s32 i = 0; i < numProcesses; i++)
    {
        s64 tmp = 0;
        char name[9] = {0};
        Handle h;
        svcOpenProcess(&h, pids[i]);
        svcGetProcessInfo(&tmp, h, 0x10000);
        memcpy(name, &tmp, 8);
        svcGetHandleInfo(&tmp, h, 0);
        u32 creationTimeMs = (u32)(1000 * tmp / SYSCLOCK_ARM11);
        svcCloseHandle(h);
        n += sprintf(buf + n, "#%4lu - %8s %lums\n", pids[i], name, creationTimeMs);
    }

    return n;
}
static int bootdiagDumpThread(char *buf, Handle debug, u32 tid)
{
    ThreadContext ctx;
    if (R_FAILED(svcGetDebugThreadContext(&ctx, debug, tid, THREADCONTEXT_CONTROL_ALL)))
        return 0;

    s64 dummy;
    u32 mask = 0xFF;
    svcGetDebugThreadParam(&dummy, &mask, debug, tid, DBGTHREAD_PARAMETER_SCHEDULING_MASK_LOW);

    return sprintf(buf, "Thread %lu -- PC=%08lx LR=%08lx R0=%08lx sched=%lu\n", tid, ctx.cpu_registers.pc, ctx.cpu_registers.lr, ctx.cpu_registers.r[0], mask);
}

static int bootdiagDumpProcess(char *buf, u32 pid)
{
    Handle debug = 0;
    int n = 0;

    if (R_FAILED(svcDebugActiveProcess(&debug, pid)))
        __builtin_trap();

    while (svcWaitSynchronization(debug, 500 * 1000 * 1000) == 0)
    {
        DebugEventInfo info;
        if (R_FAILED(svcGetProcessDebugEvent(&info, debug)))
            break;

        if (info.type == DBGEVENT_ATTACH_THREAD)
            n += bootdiagDumpThread(buf + n, debug, info.thread_id);

        if (info.flags & 1)
            svcContinueDebugEvent(debug, 0);
    }

    svcCloseHandle(debug);
    return n;
}

static void bootdiagThreadMain(void)
{
    char buf[1024];

    svcSleepThread(BOOTDIAG_WAIT_TIME);
    int n = 0;
#if BOOTDIAG_DUMP_PLIST == 1
    n += bootdiagDumpProcessList(buf + n);
#else
    (void)bootdiagDumpProcessList;
#endif
    n += bootdiagDumpProcess(buf + n, BOOTDIAG_PID);

#if BOOTDIAG_DUMP_TO_FILE
    IFile file;
    FS_ArchiveID archiveId = ARCHIVE_SDMC;

    if (R_FAILED(fsInit())) __builtin_trap();

    IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/luma/bootdiag.txt"), FS_OPEN_CREATE | FS_OPEN_WRITE);
    u64 ttl = 0;
    IFile_Write(file, &ttl, buf, (u32)n, 0);
    IFile_Close(&file);

    fsExit();
#else
    (void)n;
    *(volatile char *)buf;
    __builtin_trap(); // use stack dump to extract info
#endif
}

MyThread *bootdiagCreateThread(void)
{
    if(R_FAILED(MyThread_Create(&bootdiagThread, bootdiagThreadMain, bootdiagThreadStack, sizeof(bootdiagThreadStack), 18, 0)))
        svcBreak(USERBREAK_PANIC);
    return &bootdiagThread;
}

#else

MyThread *bootdiagCreateThread(void)
{
    return NULL;
}

#endif
