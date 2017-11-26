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

#pragma once

#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/synchronization.h>
#include <3ds/result.h>
#include "sock_util.h"
#include "memory.h"

#define MAX_DEBUG           3
#define MAX_DEBUG_THREAD    127
#define MAX_BREAKPOINT      256
// 512+24 is the ideal size as IDA will try to read exactly 0x100 bytes at a time. Add 4 to this, for $#<checksum>, see below.
// IDA seems to want additional bytes as well.
// 1024 is fine enough to put all regs in the 'T' stop reply packets
#define GDB_BUF_LEN 1024

#define GDB_HANDLER(name)           GDB_Handle##name
#define GDB_QUERY_HANDLER(name)     GDB_HANDLER(Query##name)
#define GDB_VERBOSE_HANDLER(name)   GDB_HANDLER(Verbose##name)

#define GDB_DECLARE_HANDLER(name)           int GDB_HANDLER(name)(GDBContext *ctx)
#define GDB_DECLARE_QUERY_HANDLER(name)     GDB_DECLARE_HANDLER(Query##name)
#define GDB_DECLARE_VERBOSE_HANDLER(name)   GDB_DECLARE_HANDLER(Verbose##name)

typedef struct Breakpoint
{
    u32 address;
    u32 savedInstruction;
    u8 instructionSize;
    bool persistent;
} Breakpoint;

typedef enum GDBFlags
{
    GDB_FLAG_SELECTED = 1,
    GDB_FLAG_USED  = 2,
    GDB_FLAG_PROCESS_CONTINUING = 4,
    GDB_FLAG_TERMINATE_PROCESS = 8,
} GDBFlags;

typedef enum GDBState
{
    GDB_STATE_DISCONNECTED,
    GDB_STATE_CONNECTED,
    GDB_STATE_NOACK_SENT,
    GDB_STATE_NOACK,
    GDB_STATE_CLOSING
} GDBState;

typedef struct ThreadInfo
{
    u32 id;
    u32 tls;
} ThreadInfo;

typedef struct GDBContext
{
    sock_ctx super;

    RecursiveLock lock;
    GDBFlags flags;
    GDBState state;

    u32 pid;
    Handle debug;
    ThreadInfo threadInfos[MAX_DEBUG_THREAD];
    u32 nbThreads;

    u32 currentThreadId, selectedThreadId, selectedThreadIdForContinuing;

    Handle clientAcceptedEvent, continuedEvent;
    Handle eventToWaitFor;

    bool catchThreadEvents;
    bool processEnded, processExited;

    DebugEventInfo latestDebugEvent;
    DebugFlags continueFlags;
    u32 svcMask[8];

    u32 nbBreakpoints;
    Breakpoint breakpoints[MAX_BREAKPOINT];

    u32 nbWatchpoints;
    u32 watchpoints[2];

    bool enableExternalMemoryAccess;
    char *commandData, *commandEnd;
    int latestSentPacketSize;
    char buffer[GDB_BUF_LEN + 4];

    char threadListData[0x800];
    u32 threadListDataPos;

    char memoryOsInfoXmlData[0x800];
    char processesOsInfoXmlData[0x2000];
} GDBContext;

typedef int (*GDBCommandHandler)(GDBContext *ctx);

void GDB_InitializeContext(GDBContext *ctx);
void GDB_FinalizeContext(GDBContext *ctx);
GDB_DECLARE_HANDLER(Unsupported);
