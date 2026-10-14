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

#include <3ds.h>
#include <string.h>
#include "ftp_fs.h"

static bool ftpFsApply(const char *in, char *out, u32 *len)
{
    const char *p = in;

    while(*p != 0)
    {
        while(*p == '/')
            p++;

        const char *comp = p;
        while(*p != 0 && *p != '/')
            p++;

        u32 clen = (u32)(p - comp);
        if(clen == 0 || (clen == 1 && comp[0] == '.'))
            continue;

        if(clen == 2 && comp[0] == '.' && comp[1] == '.')
        {
            while(*len > 1 && out[*len - 1] != '/')
                (*len)--;
            if(*len > 1)
                (*len)--;
            out[*len] = 0;
            continue;
        }

        u32 need = clen + (*len > 1 ? 1 : 0);
        if(*len + need >= FTP_PATH_MAX)
            return false;

        if(*len > 1)
            out[(*len)++] = '/';
        memcpy(out + *len, comp, clen);
        *len += clen;
        out[*len] = 0;
    }

    return true;
}

bool ftpFsResolve(const char *cwd, const char *arg, char *out)
{
    u32 len = 1;
    out[0] = '/';
    out[1] = 0;

    if(arg[0] != '/' && !ftpFsApply(cwd, out, &len))
        return false;
    return ftpFsApply(arg, out, &len);
}

static bool ftpFsToUtf16(const char *path, u16 *out, u32 outCount)
{
    ssize_t n = utf8_to_utf16(out, (const u8 *)path, outCount - 1);
    if(n < 0)
        return false;
    out[n] = 0;
    return true;
}

static Result ftpFsOpenDir(const char *path, FS_Archive *archive, Handle *dir)
{
    u16 path16[FTP_PATH_MAX];
    if(!ftpFsToUtf16(path, path16, FTP_PATH_MAX))
        return -1;

    Result res = FSUSER_OpenArchive(archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if(R_FAILED(res))
        return res;

    res = FSUSER_OpenDirectory(dir, *archive, fsMakePath(PATH_UTF16, path16));
    if(R_FAILED(res))
        FSUSER_CloseArchive(*archive);

    return res;
}

bool ftpFsIsDir(const char *path)
{
    FS_Archive archive;
    Handle dir;

    if(R_FAILED(ftpFsOpenDir(path, &archive, &dir)))
        return false;

    FSDIR_Close(dir);
    FSUSER_CloseArchive(archive);
    return true;
}

Result ftpFsListDir(const char *path, FtpDirCb cb, void *user)
{
    FS_Archive archive;
    Handle dir;

    Result res = ftpFsOpenDir(path, &archive, &dir);
    if(R_FAILED(res))
        return res;

    FS_DirectoryEntry entries[4];
    bool stop = false;

    while(!stop)
    {
        u32 count = 0;
        res = FSDIR_Read(dir, &count, 4, entries);
        if(R_FAILED(res) || count == 0)
            break;

        for(u32 i = 0; i < count && !stop; i++)
        {
            FtpDirEntry e;
            ssize_t n = utf16_to_utf8((u8 *)e.name, entries[i].name, sizeof(e.name) - 1);
            if(n < 0) // Invalid or too-long name: ignored
                continue;

            e.name[n] = 0;
            e.isDir = (entries[i].attributes & FS_ATTRIBUTE_DIRECTORY) != 0;
            e.size = entries[i].fileSize;

            if(!cb(&e, user))
                stop = true;
        }
    }

    FSDIR_Close(dir);
    FSUSER_CloseArchive(archive);
    return res;
}

// files

Result ftpFsOpenFile(const char *path, u32 flags, IFile *file)
{
    u16 path16[FTP_PATH_MAX];
    if(!ftpFsToUtf16(path, path16, FTP_PATH_MAX))
        return -1;

    return IFile_Open(file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_UTF16, path16), flags);
}

Result ftpFsFileSize(const char *path, u64 *size)
{
    IFile file;

    Result res = ftpFsOpenFile(path, FS_OPEN_READ, &file);
    if(R_FAILED(res))
        return res;

    res = IFile_GetSize(&file, size);
    IFile_Close(&file);
    return res;
}

Result ftpFsDeleteFile(const char *path)
{
    u16 path16[FTP_PATH_MAX];
    FS_Archive archive;

    if(!ftpFsToUtf16(path, path16, FTP_PATH_MAX))
        return -1;

    Result res = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if(R_FAILED(res))
        return res;

    res = FSUSER_DeleteFile(archive, fsMakePath(PATH_UTF16, path16));
    FSUSER_CloseArchive(archive);
    return res;
}

Result ftpFsDeleteDir(const char *path)
{
    u16 path16[FTP_PATH_MAX];
    FS_Archive archive;

    if(!ftpFsToUtf16(path, path16, FTP_PATH_MAX))
        return -1;

    Result res = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if(R_FAILED(res))
        return res;

    res = FSUSER_DeleteDirectory(archive, fsMakePath(PATH_UTF16, path16));
    FSUSER_CloseArchive(archive);
    return res;
}

Result ftpFsMakeDir(const char *path)
{
    u16 path16[FTP_PATH_MAX];
    FS_Archive archive;

    if(!ftpFsToUtf16(path, path16, FTP_PATH_MAX))
        return -1;

    Result res = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if(R_FAILED(res))
        return res;

    res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_UTF16, path16), 0);
    FSUSER_CloseArchive(archive);
    return res;
}

Result ftpFsRename(const char *from, const char *to)
{
    u16 from16[FTP_PATH_MAX];
    u16 to16[FTP_PATH_MAX];
    FS_Archive archive;

    if(!ftpFsToUtf16(from, from16, FTP_PATH_MAX) || !ftpFsToUtf16(to, to16, FTP_PATH_MAX))
        return -1;

    bool isDir = ftpFsIsDir(from);

    Result res = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if(R_FAILED(res))
        return res;

    FS_Path src = fsMakePath(PATH_UTF16, from16);
    FS_Path dst = fsMakePath(PATH_UTF16, to16);

    if(isDir)
        res = FSUSER_RenameDirectory(archive, src, archive, dst);
    else
        res = FSUSER_RenameFile(archive, src, archive, dst);

    FSUSER_CloseArchive(archive);
    return res;
}