/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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
*
*/

#pragma once

#include <3ds/types.h>
#include "ifile.h"

#define FTP_PATH_MAX 256
#define FTP_NAME_MAX 256

typedef struct FtpDirEntry
{
    char name[FTP_NAME_MAX];
    bool isDir;
    u64  size;
} FtpDirEntry;

typedef bool (*FtpDirCb)(const FtpDirEntry *entry, void *user);

bool   ftpFsResolve(const char *cwd, const char *arg, char *out);
bool   ftpFsIsDir(const char *path);
Result ftpFsListDir(const char *path, FtpDirCb cb, void *user);

// flags : FS_OPEN_READ, ou FS_OPEN_WRITE | FS_OPEN_CREATE
Result ftpFsOpenFile(const char *path, u32 flags, IFile *file);
Result ftpFsFileSize(const char *path, u64 *size);
Result ftpFsDeleteFile(const char *path);
Result ftpFsDeleteDir(const char *path);
Result ftpFsMakeDir(const char *path);
Result ftpFsRename(const char *from, const char *to);