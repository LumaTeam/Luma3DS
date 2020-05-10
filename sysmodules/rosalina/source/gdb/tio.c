/*
*   This file is part of Luma3DS.
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   SPDX-License-Identifier: (MIT OR GPL-2.0-or-later)
*/

#include <string.h>

#include "gdb/tio.h"
#include "gdb/hio.h"
#include "gdb/net.h"
#include "gdb/mem.h"
#include "gdb/debug.h"
#include "fmt.h"

#include <errno.h>
#include <fcntl.h>

#include <3ds/util/utf.h>

#define GDBHIO_O_RDONLY           0x0
#define GDBHIO_O_WRONLY           0x1
#define GDBHIO_O_RDWR             0x2
#define GDBHIO_O_ACCMODE          0x3
#define GDBHIO_O_APPEND           0x8
#define GDBHIO_O_CREAT          0x200
#define GDBHIO_O_TRUNC          0x400
#define GDBHIO_O_EXCL           0x800
#define GDBHIO_O_SUPPORTED	(GDBHIO_O_RDONLY | GDBHIO_O_WRONLY| \
                 GDBHIO_O_RDWR   | GDBHIO_O_APPEND| \
                 GDBHIO_O_CREAT  | GDBHIO_O_TRUNC| \
                 GDBHIO_O_EXCL)

#define GDBHIO_S_IFREG        0100000
#define GDBHIO_S_IFDIR         040000
#define GDBHIO_S_IFCHR         020000
#define GDBHIO_S_IRUSR           0400
#define GDBHIO_S_IWUSR           0200
#define GDBHIO_S_IXUSR           0100
#define GDBHIO_S_IRWXU           0700
#define GDBHIO_S_IRGRP            040
#define GDBHIO_S_IWGRP            020
#define GDBHIO_S_IXGRP            010
#define GDBHIO_S_IRWXG            070
#define GDBHIO_S_IROTH             04
#define GDBHIO_S_IWOTH             02
#define GDBHIO_S_IXOTH             01
#define GDBHIO_S_IRWXO             07
#define GDBHIO_S_SUPPORTED         (GDBHIO_S_IFREG|GDBHIO_S_IFDIR|  \
                                    GDBHIO_S_IRWXU|GDBHIO_S_IRWXG|  \
                                    GDBHIO_S_IRWXO)

#define GDBHIO_SEEK_SET             0
#define GDBHIO_SEEK_CUR             1
#define GDBHIO_SEEK_END             2

#define GDBHIO_EPERM                1
#define GDBHIO_ENOENT               2
#define GDBHIO_EINTR                4
#define GDBHIO_EIO                  5
#define GDBHIO_EBADF                9
#define GDBHIO_EACCES              13
#define GDBHIO_EFAULT              14
#define GDBHIO_EBUSY               16
#define GDBHIO_EEXIST              17
#define GDBHIO_ENODEV              19
#define GDBHIO_ENOTDIR             20
#define GDBHIO_EISDIR              21
#define GDBHIO_EINVAL              22
#define GDBHIO_ENFILE              23
#define GDBHIO_EMFILE              24
#define GDBHIO_EFBIG               27
#define GDBHIO_ENOSPC              28
#define GDBHIO_ESPIPE              29
#define GDBHIO_EROFS               30
#define GDBHIO_ENOSYS              88
#define GDBHIO_ENAMETOOLONG        91
#define GDBHIO_EUNKNOWN          9999

#define GDB_TIO_HANDLER(name)               GDB_HANDLER(Tio##name)
#define GDB_DECLARE_TIO_HANDLER(name)       GDB_DECLARE_HANDLER(Tio##name)

// https://sourceware.org/gdb/onlinedocs/gdb/struct-stat.html#struct-stat
typedef u32 gdbhio_time_t;
typedef int gdbhio_mode_t;

struct PACKED ALIGN(4) gdbhio_stat {
    u32  gst_dev;               /* device */
    u32  gst_ino;               /* inode */
    gdbhio_mode_t gst_mode;     /* protection */
    u32 gst_nlink;              /* number of hard links */
    u32 gst_uid;                /* user ID of owner */
    u32 gst_gid;                /* group ID of owner */
    u32 gst_rdev;               /* device type (if inode device) */
    u64 gst_size;               /* total size, in bytes */
    u64 gst_blksize;            /* blocksize for filesystem I/O */
    u64 gst_blocks;             /* number of blocks allocated */
    gdbhio_time_t gst_atime;    /* time of last access */
    gdbhio_time_t gst_mtime;    /* time of last modification */
    gdbhio_time_t gst_ctime;    /* time of last change */
};

static void GDB_TioMakeStructStat(struct gdbhio_stat *out, const struct gdbhio_stat *in)
{
    memset(out, 0, sizeof(struct gdbhio_stat));
    out->gst_dev = __builtin_bswap32(in->gst_dev);
    out->gst_ino = __builtin_bswap32(in->gst_ino);
    out->gst_mode = __builtin_bswap32(in->gst_dev);
    out->gst_nlink = __builtin_bswap32(in->gst_nlink);
    out->gst_uid = __builtin_bswap32(in->gst_uid);
    out->gst_gid = __builtin_bswap32(in->gst_gid);
    out->gst_rdev = __builtin_bswap32(in->gst_rdev);
    out->gst_size = __builtin_bswap64(in->gst_size);
    out->gst_blksize = __builtin_bswap64(in->gst_blksize);
    out->gst_blocks = __builtin_bswap64(in->gst_blocks);
    out->gst_atime = __builtin_bswap32(in->gst_atime);
    out->gst_mtime = __builtin_bswap32(in->gst_mtime);
    out->gst_ctime = __builtin_bswap32(in->gst_ctime);
}

// Inspired from https://github.com/smealum/ctrulib/blob/master/libctru/source/sdmc_dev.c

static int GDB_TioConvertResult(Result res)
{
    switch(res)
    {
        case 0:          return 0;
        case 0x082044BE: return GDBHIO_EEXIST;
        case 0x086044D2: return GDBHIO_ENOSPC;
        case 0xC8804478:
        case 0xC92044FA: return GDBHIO_ENOENT;
        case 0xE0E046BE: return GDBHIO_EINVAL;
        case 0xE0E046BF: return GDBHIO_ENAMETOOLONG;
        default:         return GDBHIO_EUNKNOWN;
    }
}

static GdbTioFileInfo *GDB_TioConvertFd(GDBContext *ctx, int fd)
{
    if (fd < 3 || fd - 3 >= MAX_TIO_OPEN_FILE)
        return NULL;

    GdbTioFileInfo *slot = &ctx->openTioFileInfos[fd - 3];
    return slot->f.handle == 0 ? NULL : slot;
}

static int GDB_TioRegisterFile(GDBContext *ctx, Handle h, int gdbOpenFlags)
{
    int fd;
    for (fd = 3; fd < 3 + MAX_TIO_OPEN_FILE && GDB_TioConvertFd(ctx, fd) != NULL; fd++);

    if (fd >= 3 + MAX_TIO_OPEN_FILE)
        return -1;

    GdbTioFileInfo *slot = &ctx->openTioFileInfos[fd - 3];
    memset(slot, 0, sizeof(GdbTioFileInfo));
    slot->f.handle = h;
    slot->flags = gdbOpenFlags;

    ctx->numOpenTioFiles++;
    return fd;
}

static int GDB_MakeUtf16Path(FS_Path *outPath, const char *pathData)
{
    size_t pathDataLen = strlen(pathData);
    if (pathDataLen % 2 == 1) return GDBHIO_EINVAL;

    char path[PATH_MAX + 1];
    u32 count = GDB_DecodeHex(path, pathData, pathDataLen / 2);
    path[count] = 0;

    u16 *p16 = (u16 *)outPath->data;
    outPath->type = PATH_UTF16;

    ssize_t units = utf8_to_utf16(p16, (const u8 *) path, count);
    if (units < 0) return GDBHIO_EINVAL;
    else if (units >= PATH_MAX) return  GDBHIO_ENAMETOOLONG;

    p16[units] = 0;
    outPath->size = 2 * (units + 1);
    return 0;
}

static inline int GDB_TioReplyErrno(GDBContext *ctx, int err)
{
    return GDB_SendFormattedPacket(ctx, "F-1,%lx", (u32)err);
}

static inline FS_ArchiveID GDB_TioGetArchiveId(void)
{
    /*
    s64 out = 1;
    svcGetSystemInfo(&out, 0x10000, 0x203);
    bool isSdMode = (bool)out;
    FS_ArchiveID archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    */

    // Actually, only support SD card for security reasons.
    return ARCHIVE_SDMC;
}

GDB_DECLARE_TIO_HANDLER(Open)
{
    FS_ArchiveID archiveId = GDB_TioGetArchiveId();
    u16 fsNameBuf[PATH_MAX + 1];
    FS_Path fsPath;
    fsPath.data = fsNameBuf;

    char *comma = strchr(ctx->commandData, ',');
    if (comma == NULL)
        return GDB_ReplyErrno(ctx, EILSEQ);

    *comma = 0;
    char *fileNameData = ctx->commandData;

    u32 args[2] = {0};
    if (GDB_ParseHexIntegerList(args, comma + 1, 2, 0) == NULL)
        return GDB_ReplyErrno(ctx, EILSEQ);
    int flags = (int)args[0];
    // mode unused
    IFile f = {0};
    u32 fsFlags = 0;

    if (ctx->numOpenTioFiles >= MAX_TIO_OPEN_FILE)
        return GDB_TioReplyErrno(ctx, GDBHIO_EMFILE);

    int err = GDB_MakeUtf16Path(&fsPath, fileNameData);
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    switch (flags & GDBHIO_O_ACCMODE)
    {
        case GDBHIO_O_RDONLY:
            fsFlags |= FS_OPEN_READ;
            if (flags & GDBHIO_O_APPEND) err = GDBHIO_EINVAL;
            break;
        case GDBHIO_O_WRONLY:
            fsFlags |= FS_OPEN_WRITE;
            break;
        case GDBHIO_O_RDWR:
            fsFlags |= FS_OPEN_READ | FS_OPEN_WRITE;
            break;
        default:
            err = GDBHIO_EINVAL;
            break;
    }

    if (flags & GDBHIO_O_CREAT)
        fsFlags |= FS_OPEN_CREATE;

    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    FS_Archive ar;
    err = GDB_TioConvertResult(FSUSER_OpenArchive(&ar, archiveId, fsMakePath(PATH_EMPTY, "")));
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    if ((flags & GDBHIO_O_CREAT) && (flags & GDBHIO_O_EXCL))
    {
        err = GDB_TioConvertResult(FSUSER_CreateFile(ar, fsPath, 0, 0));
        if (err != 0)
        {
            FSUSER_CloseArchive(ar);
            return GDB_TioReplyErrno(ctx, err);
        }
    }

    err = GDB_TioConvertResult(IFile_OpenFromArchive(&f, ar, fsPath, fsFlags));
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);
    FSUSER_CloseArchive(ar);
    
    if((flags & GDBHIO_O_ACCMODE) != GDBHIO_O_RDONLY && (flags & GDBHIO_O_TRUNC))
    {
        f.size = 0;
        err = GDB_TioConvertResult(FSFILE_SetSize(f.handle, 0));
        if (err != 0)
        {
            IFile_Close(&f);
            return GDB_TioReplyErrno(ctx, err);
        }
    }

    int fd = GDB_TioRegisterFile(ctx, f.handle, flags & (GDBHIO_O_ACCMODE | GDBHIO_O_APPEND));
    if (fd < 0)
        return GDB_TioReplyErrno(ctx, GDBHIO_EMFILE);

    return GDB_SendFormattedPacket(ctx, "F%lx", (u32)fd);
}

GDB_DECLARE_TIO_HANDLER(Close)
{
    int fd;

    if (GDB_ParseHexIntegerList((u32 *)&fd, ctx->commandData, 1, 0) == NULL)
        return GDB_ReplyErrno(ctx, EILSEQ);

    GdbTioFileInfo *fi = GDB_TioConvertFd(ctx, fd);
    if (fi == NULL)
        return GDB_TioReplyErrno(ctx, GDBHIO_EBADF);

    int err = GDB_TioConvertResult(IFile_Close(&fi->f));
    memset(fi, 0, sizeof(GdbTioFileInfo));

    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);
    else
        return GDB_SendPacket(ctx, "F0", 2);
}

GDB_DECLARE_TIO_HANDLER(Read)
{
    // GDB, with it code quality we're all aware of, always ask to read GDB_BUF_LEN, even if the packet can't fit...
    // "$F<num>;<data>#XX"
    char buf2[GDB_BUF_LEN - 4];
    u8 buf[sizeof(buf2) - 2 - 8];

    u32 args[3];
    if (GDB_ParseHexIntegerList(args, ctx->commandData, 3, 0) == NULL)
        return GDB_ReplyErrno(ctx, EILSEQ);

    u64 numRead = 0;
    int fd = (int)args[0];
    u32 count = args[1] > sizeof(buf) ? sizeof(buf) : args[1];
    u32 offset = args[2];

    GdbTioFileInfo *fi = GDB_TioConvertFd(ctx, fd);
    if (fi == NULL)
        return GDB_TioReplyErrno(ctx, GDBHIO_EBADF);

    fi->f.pos = offset;

    int err = GDB_TioConvertResult(IFile_Read(&fi->f, &numRead, buf, count));
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    char hdr[16];
    u32 encodedCount;
    u32 actualCount = GDB_EscapeBinaryData(&encodedCount, buf2 + 10, buf, (u32)numRead, sizeof(buf));
    sprintf(hdr, "F%08lx;", (u32)actualCount); // buffer might not fit the entire read data
    memcpy(buf2, hdr, 10);

    return GDB_SendPacket(ctx, buf2, 10 + encodedCount);
}

GDB_DECLARE_TIO_HANDLER(Write)
{
    u8 buf[GDB_BUF_LEN];
    u32 args[2];
    const char *comma = GDB_ParseHexIntegerList(args, ctx->commandData, 2, ',');
    if (comma == NULL)
        return GDB_ReplyErrno(ctx, EILSEQ);

    int fd = (int)args[0];
    u32 offset = args[1];
    const char *escData = comma + 1;

    u32 count = GDB_UnescapeBinaryData(buf, escData, ctx->commandEnd - escData);

    GdbTioFileInfo *fi = GDB_TioConvertFd(ctx, fd);
    if (fi == NULL)
        return GDB_TioReplyErrno(ctx, GDBHIO_EBADF);
    
    fi->f.pos = offset;


    u64 written;
    int err = GDB_TioConvertResult(IFile_Write(&fi->f, &written, buf, count, 0));
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);
    else
        return GDB_SendFormattedPacket(ctx, "F%lx", (u32)written);
}

GDB_DECLARE_TIO_HANDLER(Stat)
{
    char *fileNameData = ctx->commandData; 
    if (*fileNameData == 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    FS_ArchiveID archiveId = GDB_TioGetArchiveId();

    u16 fsNameBuf[PATH_MAX + 1];
    FS_Path fsPath;
    fsPath.data = fsNameBuf;

    int err = GDB_MakeUtf16Path(&fsPath, fileNameData);
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    Handle fileHandle;
    Handle dirHandle;
    FS_Archive ar;
    err = GDB_TioConvertResult(FSUSER_OpenArchive(&ar, archiveId, fsMakePath(PATH_EMPTY, "")));
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    struct gdbhio_stat gdbSt = {0};
    err = GDB_TioConvertResult(FSUSER_OpenFile(&fileHandle, ar, fsPath, FS_OPEN_READ, 0));
    if (err == 0)
    {
        u64 size = 0;
        err = GDB_TioConvertResult(FSFILE_GetSize(fileHandle, &size));
        FSFILE_Close(fileHandle);

        if (err == 0)
        {
            gdbSt.gst_nlink = 1;
            gdbSt.gst_mode = GDBHIO_S_IFREG | GDBHIO_S_IRUSR | GDBHIO_S_IWUSR |
            GDBHIO_S_IRGRP | GDBHIO_S_IWGRP | GDBHIO_S_IROTH | GDBHIO_S_IWOTH;
        }
    }
    else
    {
        err = GDB_TioConvertResult(FSUSER_OpenDirectory(&dirHandle, ar, fsPath));
        if (err == 0)
        {
            FSDIR_Close(dirHandle);
            gdbSt.gst_nlink = 1;
            gdbSt.gst_mode = GDBHIO_S_IFDIR | GDBHIO_S_IRWXU | GDBHIO_S_IRWXG | GDBHIO_S_IRWXO;
        }
    }

    FSUSER_CloseArchive(ar);

    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    struct gdbhio_stat gdbStFinal;
    GDB_TioMakeStructStat(&gdbStFinal, &gdbSt);

    char buf[3 + 2 * sizeof(struct gdbhio_stat)] = "F0;";
    u32 encodedCount;
    GDB_EscapeBinaryData(&encodedCount, buf + 3, buf, sizeof(struct gdbhio_stat), 2 * sizeof(struct gdbhio_stat));

    return GDB_SendPacket(ctx, buf, 3 + encodedCount);
}

GDB_DECLARE_TIO_HANDLER(Unlink)
{
    char *fileNameData = ctx->commandData; 
    if (*fileNameData == 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    FS_ArchiveID archiveId = GDB_TioGetArchiveId();

    u16 fsNameBuf[PATH_MAX + 1];
    FS_Path fsPath;
    fsPath.data = fsNameBuf;

    int err = GDB_MakeUtf16Path(&fsPath, fileNameData);
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    FS_Archive ar;
    err = GDB_TioConvertResult(FSUSER_OpenArchive(&ar, archiveId, fsMakePath(PATH_EMPTY, "")));
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    err = GDB_TioConvertResult(FSUSER_DeleteFile(ar, fsPath));
    FSUSER_CloseArchive(ar);
    if (err != 0)
        return GDB_TioReplyErrno(ctx, err);

    return GDB_SendPacket(ctx, "F0", 2);
}

GDB_DECLARE_TIO_HANDLER(Readlink)
{
    // Not really supported
    char *fileNameData = ctx->commandData; 
    if (*fileNameData == 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    return GDB_SendPacket(ctx, "F0;", 3);
}

GDB_DECLARE_TIO_HANDLER(Setfs)
{
    // Largely ignored
    u32 pid;
    if (GDB_ParseHexIntegerList(&pid, ctx->commandData, 1, 0) == NULL)
        return GDB_ReplyErrno(ctx, EILSEQ);
    return GDB_SendPacket(ctx, "F0", 2);
}

static const struct
{
    const char *name;
    GDBCommandHandler handler;
} gdbTioCommandHandlers[] =
{
    { "open",       GDB_TIO_HANDLER(Open)       },
    { "close",      GDB_TIO_HANDLER(Close)      },
    { "pread",      GDB_TIO_HANDLER(Read)       },
    { "pwrite",     GDB_TIO_HANDLER(Write)      },
    { "fstat",      GDB_TIO_HANDLER(Stat)       },
    { "unlink",     GDB_TIO_HANDLER(Unlink)     },
    { "readlink",   GDB_TIO_HANDLER(Readlink)   },
    { "setfs",      GDB_TIO_HANDLER(Setfs)      },
};

GDB_DECLARE_VERBOSE_HANDLER(File)
{
    if (*ctx->commandData == 0)
        return GDB_ReplyErrno(ctx, EILSEQ);

    char *colon = strchr(ctx->commandData, ':');
    if (colon == NULL)
        return GDB_ReplyErrno(ctx, EILSEQ);

    *colon = 0;

    for(u32 i = 0; i < sizeof(gdbTioCommandHandlers) / sizeof(gdbTioCommandHandlers[0]); i++)
    {
        if(strcmp(gdbTioCommandHandlers[i].name, ctx->commandData) == 0)
        {
            ctx->commandData = colon + 1;
            return gdbTioCommandHandlers[i].handler(ctx);
        }
    }

    return GDB_HandleUnsupported(ctx); // No handler found!
}
