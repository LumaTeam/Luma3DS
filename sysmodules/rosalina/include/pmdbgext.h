// License for this file: ctrulib's license
// Copyright AuroraWright, TuxSH 2019-2020

#pragma once

#include <3ds/services/pmapp.h>
#include <3ds/services/pmdbg.h>

/// Custom launch flags for PM launch commands.
enum {
    PMLAUNCHFLAGEXT_FAKE_DEPENDENCY_LOADING = BIT(24),
};

Result PMDBG_GetCurrentAppInfo(FS_ProgramInfo *outProgramInfo, u32 *outPid, u32 *outLaunchFlags);
Result PMDBG_DebugNextApplicationByForce(bool debug);
Result PMDBG_LaunchTitleDebug(Handle *outDebug, const FS_ProgramInfo *programInfo, u32 launchFlags);
Result PMDBG_PrepareToChainloadHomebrew(u64 titleId);
