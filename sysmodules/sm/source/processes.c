/*
processes.c

(c) TuxSH, 2017
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#include "list.h"
#include "processes.h"
#include "services.h"

ProcessDataList processDataInUseList = { NULL, NULL }, freeProcessDataList = { NULL, NULL };
char (*serviceAccessListBuffers)[34][8];

// The kernel limits the number of processes to 47 anyways...
static u64 freeServiceAccessListBuffersIds = (1ULL << 59) - 1;

ProcessData *findProcessData(u32 pid)
{
    for(ProcessData *node = processDataInUseList.first; node != NULL; node = node->next)
    {
        if(node->pid == pid)
            return node;
    }

    return NULL;
}

ProcessData *doRegisterProcess(u32 pid, char (*serviceAccessList)[8], u32 serviceAccessListSize)
{
    ProcessData *processData = (ProcessData *)allocateNode(&processDataInUseList, &freeProcessDataList, sizeof(ProcessData), false);
    if(serviceAccessListSize != 0)
    {
        s32 bufferId = 63 - __builtin_clzll(freeServiceAccessListBuffersIds);
        if(bufferId == -1)
            panic();
        else
        {
            freeServiceAccessListBuffersIds &= ~(1ULL << bufferId);
            processData->serviceAccessList = serviceAccessListBuffers[bufferId];
            processData->serviceAccessListSize = serviceAccessListSize;
            memcpy(processData->serviceAccessList, serviceAccessList, serviceAccessListSize);
        }
    }

    assertSuccess(svcCreateSemaphore(&processData->notificationSemaphore, 0, 0x10));
    processData->pid = pid;

    return processData;
}

Result RegisterProcess(u32 pid, char (*serviceAccessList)[8], u32 serviceAccessListSize)
{
    serviceAccessListSize = serviceAccessListSize - (serviceAccessListSize % 8);
    if(findProcessData(pid) != NULL)
        return 0xD9006403;
    else if(serviceAccessListSize > 8 * (IS_PRE_93 ? 32 : 34))
        return 0xD9006405;
    else
    {
        doRegisterProcess(pid, serviceAccessList, serviceAccessListSize);
        return 0;
    }
}

Result UnregisterProcess(u32 pid)
{
    ProcessData *processData = findProcessData(pid);
    if(processData == NULL)
        return 0xD8806404;

    svcCloseHandle(processData->notificationSemaphore);

    // Unregister the services registered by the process
    for(u32 i = 0; i < nbServices; i++)
    {
        if(servicesInfo[i].pid == pid)
        {
            svcCloseHandle(servicesInfo[i].clientPort);
            servicesInfo[i] = servicesInfo[--nbServices];
        }
    }

    freeServiceAccessListBuffersIds |= 1ULL << (u32)((processData->serviceAccessList - serviceAccessListBuffers[0]) / 34);

    moveNode(processData, &freeProcessDataList, false);
    return 0;
}
