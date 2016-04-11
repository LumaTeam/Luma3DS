#include <3ds.h>
#include <string.h>
#include "patcher.h"
#include "ifile.h"

#ifndef PATH_MAX
#define PATH_MAX 255
#endif

static u32 config = 0;
static u8 secureinfo[0x111] = {0};

//Quick Search algorithm, adapted from http://igm.univ-mlv.fr/~lecroq/string/node19.html#SECTION00190
static u8 *memsearch(u8 *startPos, const void *pattern, u32 size, u32 patternSize)
{
    const u8 *patternc = (const u8 *)pattern;

    //Preprocessing
    u32 table[256];

    for(u32 i = 0; i < 256; ++i)
        table[i] = patternSize + 1;
    for(u32 i = 0; i < patternSize; ++i)
        table[patternc[i]] = patternSize - i;

    //Searching
    u32 j = 0;

    while(j <= size - patternSize)
    {
        if(memcmp(patternc, startPos + j, patternSize) == 0)
            return startPos + j;
        j += table[startPos[j + patternSize]];
    }

    return NULL;
}

static u32 patchMemory(u8 *start, u32 size, const void *pattern, u32 patSize, int offset, const void *replace, u32 repSize, u32 count)
{
    u32 i;

    for(i = 0; i < count; i++)
    {
        u8 *found = memsearch(start, pattern, size, patSize);

        if(found == NULL) break;

        memcpy(found + offset, replace, repSize);

        u32 at = (u32)(found - start);

        if(at + patSize > size) break;

        size -= at + patSize;
        start = found + patSize;
    }

    return i;
}

static int fileOpen(IFile *file, FS_ArchiveID id, const char *path, int flags)
{
    FS_Archive archive;
    FS_Path ppath;

    size_t len = strnlen(path, PATH_MAX);
    archive.id = id;
    archive.lowPath.type = PATH_EMPTY;
    archive.lowPath.size = 1;
    archive.lowPath.data = (u8 *)"";
    ppath.type = PATH_ASCII;
    ppath.data = path;
    ppath.size = len + 1;

    return IFile_Open(file, archive, ppath, flags);
}

static int loadSecureinfo()
{
    IFile file;
    Result ret;
    u64 total;

    if(secureinfo[0] == 0xFF)
        return 0;

    ret = fileOpen(&file, ARCHIVE_NAND_RW, "/sys/SecureInfo_C", FS_OPEN_READ);
    if(R_SUCCEEDED(ret))
    {
        ret = IFile_Read(&file, &total, secureinfo, sizeof(secureinfo));
        IFile_Close(&file);
        if(R_SUCCEEDED(ret) && total == sizeof(secureinfo))
            secureinfo[0] = 0xFF;
    }

    return ret;
}

static int loadConfig()
{
    IFile file;
    Result ret;
    u64 total;

    if(config)
        return 0;

    ret = fileOpen(&file, ARCHIVE_SDMC, "/aurei/config.bin", FS_OPEN_READ);
    if(R_SUCCEEDED(ret))
    {
        ret = IFile_Read(&file, &total, &config, 3);
        IFile_Close(&file);
    }

    return ret;
}

void patchCode(u64 progId, u8 *code, u32 size)
{
    switch(progId)
    {
        case 0x0004003000008F02LL: // USA Menu
        case 0x0004003000008202LL: // EUR Menu
        case 0x0004003000009802LL: // JPN Menu
        case 0x000400300000A102LL: // CHN Menu
        case 0x000400300000A902LL: // KOR Menu
        case 0x000400300000B102LL: // TWN Menu
        {
            static const u8 regionFreePattern[] = {
                0x00, 0x00, 0x55, 0xE3, 0x01, 0x10, 0xA0, 0xE3
            };
            static const u8 regionFreePatch[] = {
                0x01, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1
            };

            //Patch SMDH region checks
            patchMemory(code, size, 
                regionFreePattern, 
                sizeof(regionFreePattern), -16, 
                regionFreePatch, 
                sizeof(regionFreePatch), 1
            );

            break;
        }

        case 0x0004013000002C02LL: // NIM
        {
            static const u8 blockAutoUpdatesPattern[] = {
                0x25, 0x79, 0x0B, 0x99
            };
            static const u8 blockAutoUpdatesPatch[] = {
                0xE3, 0xA0
            };
            static const u8 skipEshopUpdateCheckPattern[] = {
                0x30, 0xB5, 0xF1, 0xB0
            };
            static const u8 skipEshopUpdateCheckPatch[] = {
                0x00, 0x20, 0x08, 0x60, 0x70, 0x47
            };

            //Block silent auto-updates
            patchMemory(code, size, 
                blockAutoUpdatesPattern, 
                sizeof(blockAutoUpdatesPattern), 0, 
                blockAutoUpdatesPatch, 
                sizeof(blockAutoUpdatesPatch), 1
            );

            //Skip update checks to access the EShop
            patchMemory(code, size, 
                skipEshopUpdateCheckPattern, 
                sizeof(skipEshopUpdateCheckPattern), 0, 
                skipEshopUpdateCheckPatch, 
                sizeof(skipEshopUpdateCheckPatch), 1
            );

            break;
        }

        case 0x0004013000003202LL: // FRIENDS
        {
            static const u8 fpdVerPattern[] = {
                0xE0, 0x1E, 0xFF, 0x2F, 0xE1, 0x01, 0x01, 0x01
            };

            u8 *fdpVer = memsearch(code, fpdVerPattern, size, sizeof(fpdVerPattern));

            if(fdpVer != NULL)
            {
                fdpVer += sizeof(fpdVerPattern) + 1;

                //Allow online access to work with old friends modules
                if(*fdpVer < 5) *fdpVer = 5;
            }

            break;
        }
        
        case 0x0004001000021000LL: // USA MSET
        case 0x0004001000020000LL: // JPN MSET
        case 0x0004001000022000LL: // EUR MSET
        case 0x0004001000026000LL: // CHN MSET
        case 0x0004001000027000LL: // KOR MSET
        case 0x0004001000028000LL: // TWN MSET
        {
            if(R_SUCCEEDED(loadConfig()) && ((config >> 4) & 1))
            {
                static const u16 verPattern[] = u"Ver.";
                const u32 currentNand = ((config >> 16) & 3);
                const u32 matchingFirm = ((config >> 18) & 1) == (currentNand != 0);

                //Patch Ver. string
                patchMemory(code, size,
                    verPattern,
                    sizeof(verPattern) - sizeof(u16), 0,
                    !currentNand ? ((matchingFirm) ? u" Sys" : u"SysA") :
                                   ((currentNand == 1) ? (matchingFirm ? u" Emu" : u"EmuA") : ((matchingFirm) ? u"Emu2" : u"Em2A")),
                    sizeof(verPattern) - sizeof(u16), 1
                );
            }

            break;
        }

        case 0x0004013000008002LL: // NS
        {
            static const u8 stopCartUpdatesPattern[] = {
                0x0C, 0x18, 0xE1, 0xD8
            };
            static const u8 stopCartUpdatesPatch[] = {
                0x0B, 0x18, 0x21, 0xC8
            };

            //Disable updates from foreign carts (makes carts region-free)
            patchMemory(code, size, 
                stopCartUpdatesPattern, 
                sizeof(stopCartUpdatesPattern), 0, 
                stopCartUpdatesPatch,
                sizeof(stopCartUpdatesPatch), 2
            );

            break;
        }

        case 0x0004013000001702LL: // CFG
        {
            static const u8 secureinfoSigCheckPattern[] = {
                0x06, 0x46, 0x10, 0x48, 0xFC
            };
            static const u8 secureinfoSigCheckPatch[] = {
                0x00, 0x26
            };

            //Disable SecureInfo signature check
            patchMemory(code, size, 
                secureinfoSigCheckPattern, 
                sizeof(secureinfoSigCheckPattern), 0, 
                secureinfoSigCheckPatch, 
                sizeof(secureinfoSigCheckPatch), 1
            );

            if(R_SUCCEEDED(loadSecureinfo()))
            {
                static const u16 secureinfoFilenamePattern[] = u"SecureInfo_";
                static const u16 secureinfoFilenamePatch[] = u"C";

                //Use SecureInfo_C
                patchMemory(code, size, 
                    secureinfoFilenamePattern, 
                    sizeof(secureinfoFilenamePattern) - sizeof(u16),
                    sizeof(secureinfoFilenamePattern) - sizeof(u16), 
                    secureinfoFilenamePatch, 
                    sizeof(secureinfoFilenamePatch) - sizeof(u16), 2
                );
            }

            break;
        }
    }
}