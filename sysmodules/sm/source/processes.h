/*
processes.h

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include "common.h"

struct ProcessDataList;

typedef struct ProcessData
{
    struct ProcessData *prev, *next;
    struct ProcessDataList *parent;

    u32 pid;

    Handle notificationSemaphore;

    char (*serviceAccessList)[8];
    u32 serviceAccessListSize;

    bool notificationEnabled;
    // Circular buffer
    u16 receivedNotificationIndex;
    u16 pendingNotificationIndex;

    u16 nbPendingNotifications;
    u32 pendingNotifications[16];
    u16 nbSubscribed;
    u32 subscribedNotifications[17];
} ProcessData;

typedef struct ProcessDataList
{
    ProcessData *first, *last;
} ProcessDataList;

extern ProcessDataList processDataInUseList, freeProcessDataList;

ProcessData *findProcessData(u32 pid);
ProcessData *doRegisterProcess(u32 pid, char (*serviceAccessList)[8], u32 serviceAccessListSize);

Result RegisterProcess(u32 pid, char (*serviceAccessList)[8], u32 serviceAccessListSize);
Result UnregisterProcess(u32 pid);
