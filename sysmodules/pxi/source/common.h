/*
common.h:
    Common types and global variables.

(c) TuxSH, 2016-2020
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include <3ds.h>

typedef enum SessionState
{
    STATE_IDLE = 0,
    STATE_RECEIVED_FROM_ARM11 = 1,
    STATE_SENT_TO_ARM9 = 2,
    STATE_RECEIVED_FROM_ARM9 = 3
} SessionState;

typedef struct SessionData
{
    SessionState state;
    u32 buffer[0x100/4];

    Handle handle;
    u32 usedStaticBuffers;

    RecursiveLock lock;
} SessionData;

#define NB_STATIC_BUFFERS 21

typedef struct SessionManager
{
    Handle sendAllBuffersToArm9Event, replySemaphore, PXISRV11CommandReceivedEvent, PXISRV11ReplySentEvent;
    u32 latest_PXI_MC5_val, pendingArm9Commands;
    u32 receivedServiceId;
    RecursiveLock senderLock;
    bool sendingDisabled;
    SessionData sessionData[10];

    u32 currentlyProvidedStaticBuffers, freeStaticBuffers;
} SessionManager;

//Page alignment is mandatory there
extern u32 CTR_ALIGN(0x1000) staticBuffers[NB_STATIC_BUFFERS][0x1000/4];

extern Handle PXISyncInterrupt, PXITransferMutex;
extern Handle terminationRequestedEvent;
extern bool shouldTerminate;
extern SessionManager sessionManager;

extern const u32 nbStaticBuffersByService[10];

static inline Result assertSuccess(Result res)
{
    if(R_FAILED(res)) svcBreak(USERBREAK_PANIC);
    return res;
}

static inline s32 getMSBPosition(u32 val)
{
    return 31 - (s32) __builtin_clz(val);
}

static inline s32 getLSBPosition(u32 val)
{
    return __builtin_ffs(val) - 1;
}

static inline u32 clearMSBs(u32 val, u32 nb)
{
    for(u32 i = 0; i < nb; i++)
    {
        s32 pos = getMSBPosition(val);
        if(pos == -1) break;
        val &= ~(1 << pos);
    }

    return val;
}

static inline u32 countNbBitsSet(u32 val)
{
    u32 nb = 0;
    while(val != 0)
    {
        val = clearMSBs(val, 1);
        nb++;
    }

    return nb;
}
