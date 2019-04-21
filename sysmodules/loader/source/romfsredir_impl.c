#include <3ds.h>
#include <string.h>
#include "ifile.h"
#include "util.h"

// Note: code here must be PIC

/*

romfsRedirPatchArchiveName       : .ascii "lf:\0"
romfsRedirPatchFsMountArchive    : .word 0xdead0005
romfsRedirPatchFsRegisterArchive : .word 0xdead0006
romfsRedirPatchArchiveId         : .word 0xdead0007
romfsRedirPatchRomFsMount        : .ascii "rom:"
romfsRedirPatchUpdateRomFsMount  : .word 0xdead0008
romfsRedirPatchCustomPath        : .word 0xdead0004
*/

typedef struct RomfsRedirContext
{
    char newMountPoint[4];
    Result (*mountArchive)(u32 *obj, u32 archiveId);
    Result (*registerArchive)(const char *mountPoint, u32 obj, u32 arg3, u32 arg4);
    u32 newArchiveId;
    char oldRomfsMountPoint[8];
    char oldRomfsUpdateMountPoint[8];
    u16 lumaRomfsDir[48];
    u32 lumaRomfsDirSize;
} RomfsRedirContext;

Result romfsRedirOriginalMountArchive(void *obj, const char *path, u32 pathLen, u32 archiveId);

Result doRomfsRedirMountArchive(void *obj, const char *path, u32 pathLen, u32 archiveId, RomfsRedirContext *ctx)
{
    (void)obj;
    if(archiveId == ARCHIVE_ROMFS)
    {
        strncpy(ctx->oldRomfsMountPoint, path, 7);
        ctx->oldRomfsMountPoint[7] = 0;

        u32 obj2;
        ctx->mountArchive(&obj2, archiveId);
        return ctx->registerArchive(ctx->newMountPoint, obj2, 0, 0);
    }
    // update romfs?

    else
        return romfsRedirOriginalMountArchive(obj, path, pathLen, archiveId);
}

Result romfsRedirOriginalFileOpen(IFile *file, const char *name, u32 flags);

static bool checkMountPoint(const char *str8, const u16 *str16)
{
    while (*str8 != 0)
    {
        if (*str8++ != *str16++) return false;
    }

    return true;
}

Result doRomfsRedirFsRedir(IFile *file, const u16 *name, u32 flags, RomfsRedirContext *ctx)
{
    u16 newPath[0x200];
    u32 size = ctx->lumaRomfsDirSize;
    if (!checkMountPoint(ctx->oldRomfsMountPoint, name) && !checkMountPoint(ctx->oldRomfsUpdateMountPoint, name))
        return romfsRedirOriginalFileOpen(file, name, flags);
    else
    {
        const u16 *path = name;
        while(*path != u':') path++;
        while(*path == u'/') path++;
        memcpy(newPath, ctx->lumaRomfsDir, size);
        while (*path != 0 && size < 0x200)
            newPath[size++] = *path++;
        if (size != 0x200)
            newPath[size] = 0;
        if (R_FAILED(romfsRedirOriginalFileOpen(file, newPath, flags)))
            return romfsRedirOriginalFileOpen(file, name, flags);
        return 0;
    }
}