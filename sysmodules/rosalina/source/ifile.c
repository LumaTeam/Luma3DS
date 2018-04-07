/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

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

Result IFile_Write(IFile *file, u64 *total, void *buffer, u32 len, u32 flags)
{
  u32 written;
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
    res = FSFILE_Write(file->handle, &written, file->pos, buf, left, flags);
    if (R_FAILED(res))
    {
      break;
    }

    cur += written;
    file->pos += written;
    if (written == left)
    {
      break;
    }
    buf += written;
    left -= written;
  }

  *total = cur;
  return res;
}
