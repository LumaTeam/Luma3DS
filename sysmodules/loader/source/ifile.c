#include <3ds.h>
#include "ifile.h"

Result IFile_Open(IFile *file, FS_ArchiveID archiveId, FS_Path archivePath, FS_Path filePath, u32 flags)
{
  Result res;

  res = FSUSER_OpenFileDirectly(&file->handle, archiveId, archivePath, filePath, flags, 0);
  file->pos = 0;
  file->size = 0;
  return res;
}

Result IFile_Close(IFile *file)
{
  return FSFILE_Close(file->handle);
}

Result IFile_GetSize(IFile *file, u64 *size)
{
  Result res;

  res = FSFILE_GetSize(file->handle, size);
  file->size = *size;
  return res;
}

Result IFile_Read(IFile *file, u64 *total, void *buffer, u32 len)
{
  u32 read;
  u32 left;
  char *buf;
  u64 cur;
  Result res;

  if (len == 0)
  {
    *total = 0;
    return 0;
  }

  buf = (char *)buffer;
  cur = 0;
  left = len;
  while (1)
  {
    res = FSFILE_Read(file->handle, &read, file->pos, buf, left);
    if (R_FAILED(res))
    {
      break;
    }

    cur += read;
    file->pos += read;
    if (read == left)
    {
      break;
    }
    buf += read;
    left -= read;
  }

  *total = cur;
  return res;
}
