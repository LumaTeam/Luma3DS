#pragma once

#include <3ds/types.h>
#include "process_data.h"

typedef struct Manager {
    ProcessList processList;
    ProcessData *runningApplicationData;
    ProcessData *debugData; // note: official PM uses runningApplicationData for both, and has queuedApplicationProcessHandle
    Handle reslimits[4];
    Handle newProcessEvent;
    Handle allNotifiedTerminationEvent;
    bool waitingForTermination;
    bool preparingForReboot;
    u8 maxAppCpuTime;
    s8 cpuTimeBase;
} Manager;

extern Manager g_manager;

void Manager_Init(void *procBuf, size_t numProc);
void Manager_RegisterKips(void);
Result UnregisterProcess(u64 titleId);
