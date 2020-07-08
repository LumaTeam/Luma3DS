#pragma once

#include "process_data.h"
#include <3ds/exheader.h>

Result listAndTerminateDependencies(ProcessData *process, ExHeader_Info *exheaderInfo);
ProcessData *terminateAllProcesses(u32 callerPid, s64 timeout); // callerPid = -1 for firmlaunch

Result TerminateApplication(s64 timeout);
Result TerminateTitle(u64 titleId, s64 timeout);
Result TerminateProcess(u32 pid, s64 timeout);
Result PrepareForReboot(u32 pid, s64 timeout);
