#include <3ds.h>
#include <string.h>
#include "process_data.h"
#include "util.h"

ProcessData *ProcessList_FindProcessById(const ProcessList *list, u32 pid)
{
    ProcessData *process;

    FOREACH_PROCESS(list, process) {
        if (process->pid == pid) {
            return process;
        }
    }

    return NULL;
}

ProcessData *ProcessList_FindProcessByHandle(const ProcessList *list, Handle handle)
{
    ProcessData *process;

    FOREACH_PROCESS(list, process) {
        if (process->handle == handle) {
            return process;
        }
    }

    return NULL;
}

ProcessData *ProcessList_FindProcessByTitleId(const ProcessList *list, u64 titleId)
{
    ProcessData *process;

    FOREACH_PROCESS(list, process) {
        if ((process->titleId & ~0xFFULL) == (titleId & ~0xFFULL)) {
            return process;
        }
    }

    return NULL;
}

Result ProcessData_Notify(const ProcessData *process, u32 notificationId)
{
    Result res = SRVPM_PublishToProcess(notificationId, process->handle);
    if (res == (Result)0xD8606408) {
        panic(res);
    }

    return res;
}

Result ProcessData_SendTerminationNotification(ProcessData *process)
{
    Result res = ProcessData_Notify(process, 0x100);
    process->terminationStatus = R_SUCCEEDED(res) ? TERMSTATUS_NOTIFICATION_SENT : TERMSTATUS_NOTIFICATION_FAILED;
    return res;
}

void ProcessData_Incref(ProcessData *process, u32 amount)
{
    if (process->flags & PROCESSFLAG_AUTOLOADED) {
        if (process->refcount + amount > 0xFF) {
            panic(3);
        }

        process->refcount += amount;
    }
}

ProcessData *ProcessList_New(ProcessList *list)
{
    if (IntrusiveList_TestEnd(&list->freeList, list->freeList.first)) {
        return NULL;
    }

    IntrusiveNode *nd = list->freeList.first;
    IntrusiveList_Erase(nd);
    memset(nd, 0, sizeof(ProcessData));
    IntrusiveList_InsertAfter(list->list.last, nd);
    return (ProcessData *)nd;
}

void ProcessList_Delete(ProcessList *list, ProcessData *process)
{
    IntrusiveList_Erase(&process->node);
    IntrusiveList_InsertAfter(list->freeList.first, &process->node);
}
