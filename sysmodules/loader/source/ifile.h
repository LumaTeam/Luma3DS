#pragma once

#include <3ds/services/fs.h>

#define PATH_MAX 255

typedef struct
{
    Handle handle;
    u64 pos;
    u64 size;
} IFile;

Result IFile_Open(IFile *file, FS_ArchiveID archiveId, FS_Path archivePath, FS_Path filePath, u32 flags);
Result IFile_OpenFromArchive(IFile *file, FS_Archive archive, FS_Path filePath, u32 flags);
Result IFile_Close(IFile *file);
Result IFile_GetSize(IFile *file, u64 *size);
Result IFile_SetSize(IFile *file, u64 size);
Result IFile_Read(IFile *file, u64 *total, void *buffer, u32 len);
Result IFile_Write(IFile *file, u64 *total, const void *buffer, u32 len, u32 flags);
