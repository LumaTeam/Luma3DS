// License for this file: ctrulib's license
// Copyright AuroraWright, TuxSH 2019

#pragma once

#include <3ds/services/pmapp.h>
#include <3ds/services/pmdbg.h>

Result PMDBG_GetCurrentAppTitleIdAndPid(u64 *outTitleId, u32 *outPid);
Result PMDBG_DebugNextApplicationByForce(bool debug);
Result PMDBG_LaunchTitleDebug(Handle *outDebug, const FS_ProgramInfo *programInfo, u32 launchFlags);
