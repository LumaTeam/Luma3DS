/*
common.h

(c) TuxSH, 2017
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include <3ds.h>
#include <string.h>

#define IS_PRE_7X (osGetFirmVersion() < SYSTEM_VERSION(2, 39, 4))
#define IS_PRE_93 (osGetFirmVersion() < SYSTEM_VERSION(2, 48, 3))

extern u32 nbSection0Modules;
extern Handle resumeGetServiceHandleOrPortRegisteredSemaphore;

struct SessionDataList;

typedef struct SessionData
{
    struct SessionData *prev, *next;
    struct SessionDataList *parent;
    u32 pid;
    u32 replayCmdbuf[4];
    Handle busyClientPortHandle;
    Handle handle;
    bool isSrvPm;
} SessionData;

typedef struct SessionDataList
{
    SessionData *first, *last;
} SessionDataList;

extern SessionDataList sessionDataInUseList, freeSessionDataList;
extern SessionDataList sessionDataWaitingForServiceOrPortRegisterList, sessionDataToWakeUpAfterServiceOrPortRegisterList;
extern SessionDataList sessionDataWaitingPortReadyList;

static inline void panic(void)
{
     svcBreak(USERBREAK_PANIC);
     for(;;) svcSleepThread(0);
}

static inline void assertSuccess(Result res)
{
    if(R_FAILED(res))
        panic();
}
