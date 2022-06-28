/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2022 Aurora Wright, TuxSH
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
