#pragma once

#include <3ds/types.h>

Result fsldrInit(void);
void fsldrExit(void);
Result FSLDR_InitializeWithSdkVersion(Handle session, u32 version);
Result FSLDR_SetPriority(u32 priority);
Result FSLDR_OpenFileDirectly(Handle* out, FS_ArchiveID archiveId, FS_Path archivePath, FS_Path filePath, u32 openFlags, u32 attributes);
