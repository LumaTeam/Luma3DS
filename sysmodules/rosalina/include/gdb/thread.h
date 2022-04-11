/*
*   This file is part of Luma3DS.
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   SPDX-License-Identifier: (MIT OR GPL-2.0-or-later)
*/

#pragma once

#include "gdb.h"

static inline u32 GDB_ConvertFromRealPid(u32 pid)
{
    return pid + 1;
}

static inline u32 GDB_ConvertToRealPid(u32 pid)
{
    return pid - 1;
}

const char *GDB_ParseThreadId(GDBContext *ctx, u32 *outPid, u32 *outTid, const char *str, char lastSep);
u32 GDB_ParseDecodeSingleThreadId(GDBContext *ctx, const char *str, char lastSep);
int GDB_EncodeThreadId(GDBContext *ctx, char *outbuf, u32 tid);

u32 GDB_GetCurrentThreadFromList(GDBContext *ctx, u32 *threadIds, u32 nbThreads);
u32 GDB_GetCurrentThread(GDBContext *ctx);

GDB_DECLARE_HANDLER(SetThreadId);
GDB_DECLARE_HANDLER(IsThreadAlive);

GDB_DECLARE_QUERY_HANDLER(CurrentThreadId);
GDB_DECLARE_QUERY_HANDLER(fThreadInfo);
GDB_DECLARE_QUERY_HANDLER(sThreadInfo);
GDB_DECLARE_QUERY_HANDLER(ThreadEvents);
GDB_DECLARE_QUERY_HANDLER(ThreadExtraInfo);
GDB_DECLARE_QUERY_HANDLER(GetTLSAddr);
