#pragma once

#include <3ds/exheader.h>
#include <3ds/services/fs.h>
#include "process_data.h"

Result registerProgram(u64 *programHandle, const FS_ProgramInfo *programInfo, const FS_ProgramInfo *programInfoUpdate);
Result getAndListDependencies(u64 *dependencies, u32 *numDeps, ProcessData *process, ExHeader_Info *exheaderInfo);
Result listDependencies(u64 *dependencies, u32 *numDeps, const ExHeader_Info *exheaderInfo);
Result listMergeUniqueDependencies(ProcessData **procs, u64 *dependencies, u32 *remrefcounts, u32 *numDeps, const ExHeader_Info *exheaderInfo);

Result GetTitleExHeaderFlags(ExHeader_Arm11CoreInfo *outCoreInfo, ExHeader_SystemInfoFlags *outSiFlags, const FS_ProgramInfo *programInfo);

// Custom
Result GetCurrentAppInfo(FS_ProgramInfo *outProgramInfo, u32 *outPid, u32 *outLaunchFlags);
