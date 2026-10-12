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
            if(n < 0) // nom invalide ou trop long : ignoré
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