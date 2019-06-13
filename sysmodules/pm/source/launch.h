#pragma once

#include <3ds/exheader.h>
#include <3ds/services/fs.h>
#include "process_data.h"

Result LaunchTitle(u32 *outPid, const FS_ProgramInfo *programInfo, u32 launchFlags);
Result LaunchTitleUpdate(const FS_ProgramInfo *programInfo, const FS_ProgramInfo *programInfoUpdate, u32 launchFlags);
Result LaunchApp(const FS_ProgramInfo *programInfo, u32 launchFlags);
Result RunQueuedProcess(Handle *outDebug);
Result LaunchAppDebug(Handle *outDebug, const FS_ProgramInfo *programInfo, u32 launchFlags);

Result autolaunchSysmodules(void);

// Custom
Result DebugNextApplicationByForce(bool debug);
Result LaunchTitleDebug(Handle *outDebug, const FS_ProgramInfo *programInfo, u32 launchFlags);
