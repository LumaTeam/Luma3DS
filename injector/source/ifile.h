#pragma once

#include <3ds/types.h>

typedef struct
{
  Handle handle;
  u64 pos;
  u64 size;
} IFile;

Result IFile_Open(IFile *file, FS_Archive archive, FS_Path path, u32 flags);
Result IFile_Close(IFile *file);
Result IFile_GetSize(IFile *file, u64 *size);
Result IFile_Read(IFile *file, u64 *total, void *buffer, u32 len);
Result IFile_Write(IFile *file, u64 *total, void *buffer, u32 len, u32 flags);
