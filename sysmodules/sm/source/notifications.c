/*
notifications.c

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#include "notifications.h"
#include "processes.h"

#include <stdatomic.h>

static bool isNotificationInhibited(const ProcessData *processData, u32 notificationId)
{
    (void)processData;
    switch(notificationId)
    {
        default:
            return false;
    }
}

static bool doPublishNotification(ProcessData *processData, u32 notificationId, u32 flags)
{
    if((flags & 1) && processData->nbPendingNotifications != 0) // only send if not already pending
    {
        for(u16 i = 0; i < processData->nbPendingNotifications; i++)
        {
            if(processData->pendingNotifications[(processData->receivedNotificationIndex + i) % 16] == notificationId)
                return true;
        }
    }

    if(processData->nbPendingNotifications < 0x10)
    {
        s32 count;

        processData->pendingNotifications[processData->pendingNotificationIndex] = notificationId;
        processData->pendingNotificationIndex = (processData->pendingNotificationIndex + 1) % 16;
        ++processData->nbPendingNotifications;
        assertSuccess(svcReleaseSemaphore(&count, processData->notificationSemaphore, 1));

        return true;
    }
    else
        return (flags & 2) != 0;
}

Result EnableNotification(SessionData *sessionData, Handle *notificationSemaphore)
{
    ProcessData *processData = findProcessData(sessionData->pid);

    if(processData == NULL)
    {
        // Section 0 modules have access to all services, so we need to register them here for notifications
        if(sessionData->pid < nbSection0Modules)
            processData = doRegisterProcess(sessionData->pid, NULL, 0);
        else
            return 0xD8806404;
    }

    processData->notificationEnabled = true;
    *notificationSemaphore = processData->notificationSemaphore;

    return 0;
}

Result Subscribe(SessionData *sessionData, u32 notificationId)
{
    ProcessData *processData = findProcessData(sessionData->pid);


    if(processData == NULL || !processData->notificationEnabled)
        return 0xD8806404;

    for(u16 i = 0; i < processData->nbSubscribed; i++)
    {
        if(processData->subscribedNotifications[i] == notificationId)
            return 0xD9006403;
    }

    if(processData->nbSubscribed < 0x11)
    {
        processData->subscribedNotifications[processData->nbSubscribed++] = notificationId;
        return 0;
    }
    else
        return 0xD9006405;
}

Result Unsubscribe(SessionData *sessionData, u32 notificationId)
{
    ProcessData *processData = findProcessData(sessionData->pid);

    if(processData == NULL || !processData->notificationEnabled)
        return 0xD8806404;

    u16 i;
    for(i = 0; i < processData->nbSubscribed && processData->subscribedNotifications[i] != notificationId; i++);

    if(i == processData->nbSubscribed)
        return 0xD8806404;
    else
    {
        processData->subscribedNotifications[i] = processData->subscribedNotifications[--processData->nbSubscribed];
        return 0;
    }
}

Result ReceiveNotification(SessionData *sessionData, u32 *notificationId)
{
    ProcessData *processData = findProcessData(sessionData->pid);

    if(processData == NULL || !processData->notificationEnabled || processData->nbPendingNotifications == 0)
    {
        if(processData != NULL && processData->nbPendingNotifications)
            *notificationId = 0;
        return 0xD8806404;
    }
    else
    {
        --processData->nbPendingNotifications;
        *notificationId = processData->pendingNotifications[processData->receivedNotificationIndex];
        processData->receivedNotificationIndex = (processData->receivedNotificationIndex + 1) % 16;
        return 0;
    }
}

Result PublishToSubscriber(u32 notificationId, u32 flags)
{
    for(ProcessData *node = processDataInUseList.first; node != NULL; node = node->next)
    {
        if(!node->notificationEnabled || isNotificationInhibited(node, notificationId))
            continue;

        u16 i;
        for(i = 0; i < node->nbSubscribed && node->subscribedNotifications[i] != notificationId; i++);
        if(i >= node->nbSubscribed)
            continue;

        if(!doPublishNotification(node, notificationId, flags))
            return 0xD8606408;
    }

    return 0;
}

Result PublishAndGetSubscriber(u32 *pidCount, u32 *pidList, u32 notificationId, u32 flags)
{
    u32 nb = 0;
    for(ProcessData *node = processDataInUseList.first; node != NULL; node = node->next)
    {
        if(!node->notificationEnabled || isNotificationInhibited(node, notificationId))
            continue;

        u16 i;
        for(i = 0; i < node->nbSubscribed && node->subscribedNotifications[i] != notificationId; i++);
        if(i >= node->nbSubscribed)
            continue;

        if(!doPublishNotification(node, notificationId, flags))
            return 0xD8606408;
        else if(pidList != NULL && nb < 60)
            pidList[nb++] = node->pid;
    }

    if(pidCount != NULL)
        *pidCount = nb;

    return 0;
}

Result PublishToProcess(Handle process, u32 notificationId)
{
    u32 pid;
    Result res = svcGetProcessId(&pid, process);
    if(R_FAILED(res))
        return res;

    ProcessData *processData = findProcessData(pid);
    if(processData == NULL || !processData->notificationEnabled)
        res = 0xD8806404;
    else if(!doPublishNotification(processData, notificationId, 0))
        res = 0xD8606408;
    else
        res = 0;

    svcCloseHandle(process);
    return res;
}

Result PublishToAll(u32 notificationId)
{
    for(ProcessData *node = processDataInUseList.first; node != NULL; node = node->next)
    {
        if(!node->notificationEnabled)
            continue;
        else if(!doPublishNotification(node, notificationId, 0))
            return 0xD8606408;
    }

    return 0;
}
