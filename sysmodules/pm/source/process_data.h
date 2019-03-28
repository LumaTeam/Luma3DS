#pragma once

#include <3ds/types.h>
#include <3ds/synchronization.h>
#include "intrusive_list.h"

#define FOREACH_PROCESS(list, process) \
for (process = ProcessList_GetFirst(list); !ProcessList_TestEnd(list, process); process = ProcessList_GetNext(process))

enum {
    PROCESSFLAG_NOTIFY_TERMINATION              = BIT(0),
    PROCESSFLAG_KIP                             = BIT(1),
    PROCESSFLAG_DEPENDENCIES_LOADED             = BIT(2),
    PROCESSFLAG_AUTOLOADED                      = BIT(3),
    PROCESSFLAG_NOTIFY_TERMINATION_TERMINATED   = BIT(4),
    PROCESSFLAG_NORMAL_APPLICATION              = BIT(5), // Official PM doesn't have this
};

typedef enum TerminationStatus {
    TERMSTATUS_RUNNING              = 0,
    TERMSTATUS_NOTIFICATION_SENT    = 1,
    TERMSTATUS_NOTIFICATION_FAILED  = 2,
    TERMSTATUS_TERMINATED           = 3,
} TerminationStatus;

typedef struct ProcessData {
    IntrusiveNode node;
    Handle handle;
    u32 pid;
    u64 titleId;
    u64 programHandle;
    u8 flags;
    u8 terminatedNotificationVariation;
    TerminationStatus terminationStatus;
    u8 refcount;
} ProcessData;

typedef struct ProcessList {
    RecursiveLock lock;
    IntrusiveList list;
    IntrusiveList freeList;
} ProcessList;

static inline void ProcessList_Init(ProcessList *list, void *buf, size_t num)
{
    IntrusiveList_Init(&list->list);
    IntrusiveList_CreateFromBuffer(&list->freeList, buf, sizeof(ProcessData), sizeof(ProcessData) * num);
    RecursiveLock_Init(&list->lock);
}

static inline void ProcessList_Lock(ProcessList *list)
{
    RecursiveLock_Lock(&list->lock);
}

static inline void ProcessList_Unlock(ProcessList *list)
{
    RecursiveLock_Unlock(&list->lock);
}

static inline ProcessData *ProcessList_GetNext(const ProcessData *process)
{
    return (ProcessData *)process->node.next;
}

static inline ProcessData *ProcessList_GetPrev(const ProcessData *process)
{
    return (ProcessData *)process->node.next;
}

static inline ProcessData *ProcessList_GetFirst(const ProcessList *list)
{
    return (ProcessData *)list->list.first;
}

static inline ProcessData *ProcessList_GetLast(const ProcessList *list)
{
    return (ProcessData *)list->list.last;
}

static inline bool ProcessList_TestEnd(const ProcessList *list, const ProcessData *process)
{
    return IntrusiveList_TestEnd(&list->list, &process->node);
}

ProcessData *ProcessList_New(ProcessList *list);
void ProcessList_Delete(ProcessList *list, ProcessData *process);

ProcessData *ProcessList_FindProcessById(const ProcessList *list, u32 pid);
ProcessData *ProcessList_FindProcessByTitleId(const ProcessList *list, u64 titleId);
ProcessData *ProcessList_FindProcessByHandle(const ProcessList *list, Handle handle);

void ProcessData_Incref(ProcessData *process, u32 amount);
Result ProcessData_Notify(const ProcessData *process, u32 notificationId);
Result ProcessData_SendTerminationNotification(ProcessData *process);
