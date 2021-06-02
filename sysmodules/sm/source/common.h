/*
common.h

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include <3ds.h>
#include <string.h>

#define KERNEL_VERSION_MINOR    (GET_VERSION_MINOR(osGetKernelVersion()))

#define IS_PRE_7X               (KERNEL_VERSION_MINOR < 39)
#define IS_PRE_93               (KERNEL_VERSION_MINOR < 48)

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

#ifdef XDS
static void hexItoa(u64 number, char *out, u32 digits, bool uppercase)
{
    const char hexDigits[] = "0123456789ABCDEF";
    const char hexDigitsLowercase[] = "0123456789abcdef";
    u32 i = 0;

    while(number > 0)
    {
        out[digits - 1 - i++] = uppercase ? hexDigits[number & 0xF] : hexDigitsLowercase[number & 0xF];
        number >>= 4;
    }

    while(i < digits) out[digits - 1 - i++] = '0';
}

static inline void debugOutputHex(u64 number, u32 digits)
{
    char buf[16+2];
    hexItoa(number, buf, digits, false);
    buf[digits] = '\n';
    buf[digits + 1] = '\0';

    svcOutputDebugString(buf, digits + 1);
}
#endif

static void __attribute__((noinline)) panic(Result res)
{
#ifndef XDS
    (void)res;
    __builtin_trap();
#else
    debugOutputHex(res, 8);
    svcBreak(USERBREAK_PANIC);
#endif
}

static inline void assertSuccess(Result res)
{
    if(R_FAILED(res))
        panic(res);
}
