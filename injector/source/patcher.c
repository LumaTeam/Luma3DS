#include <3ds.h>
#include <string.h>
#include "patcher.h"
#include "ifile.h"

#ifndef PATH_MAX
#define PATH_MAX 255
#define CONFIG(a) ((config >> (a + 16)) & 1)
#define MULTICONFIG(a) ((config >> (a * 2 + 6)) & 3)
#define BOOTCONFIG(a, b) ((config >> a) & b)
#endif

static u32 config = 0;
static u8 secureInfo[0x111] = {0};

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

static int loadSecureInfo(void)
{
    if(secureInfo[0] == 0xFF)
        return 0;

    IFile file;
    Result ret = fileOpen(&file, ARCHIVE_NAND_RW, "/sys/SecureInfo_C", FS_OPEN_READ);
    if(R_SUCCEEDED(ret))
    {
        u64 total;

        ret = IFile_Read(&file, &total, secureInfo, 0x111);
        IFile_Close(&file);
        if(R_SUCCEEDED(ret) && total == 0x111)
            secureInfo[0] = 0xFF;
    }

    return ret;
}

static int loadConfig(void)
{
    if(config)
        return 0;

    IFile file;
    Result ret = fileOpen(&file, ARCHIVE_SDMC, "/aurei/config.bin", FS_OPEN_READ);
    if(R_SUCCEEDED(ret))
    {
        u64 total;

        ret = IFile_Read(&file, &total, &config, 4);
        IFile_Close(&file);
        if(R_SUCCEEDED(ret)) config |= 1 << 4;
    }

    return ret;
}

static int loadTitleLocaleConfig(u64 progId, u8 *regionId, u8 *languageId)
{
    /* Here we look for "/aurei/locales/[u64 titleID in hex, uppercase].txt"
       If it exists it should contain, for example, "EUR IT" */

    char path[] = "/aurei/locales/0000000000000000.txt";

    while(progId > 0)
    {
        static u32 i = 30;
        static const char hexDigits[] = "0123456789ABCDEF";
        path[i--] = hexDigits[(u32)(progId & 0xF)];
        progId >>= 4;
    }

    IFile file;
    Result ret = fileOpen(&file, ARCHIVE_SDMC, path, FS_OPEN_READ);
    if(R_SUCCEEDED(ret))
    {
        char buf[6];
        u64 total;

        ret = IFile_Read(&file, &total, buf, 6);
        IFile_Close(&file);

        if(!R_SUCCEEDED(ret) || total < 6) return -1;

        for(u32 i = 0; i < 7; ++i)
        {
            static const char *regions[] = {"JPN", "USA", "EUR", "AUS", "CHN", "KOR", "TWN"};

            if(memcmp(buf, regions[i], 3) == 0)
            {
                *regionId = (u8)i;
                break;
            }
        }
		
        for(u32 i = 0; i < 12; ++i)
        {
            static const char *languages[] = {"JP", "EN", "FR", "DE", "IT", "ES", "ZH", "KO", "NL", "PT", "RU", "TW"};

            if(memcmp(buf + 4, languages[i], 2) == 0)
            {
                *languageId = (u8)i;
                break;
            }
        }
    }

    return ret;
}

static u8 *getCfgOffsets(u8* code, u32 size, u32 *CFGUHandleOffset)
{
    static const u8 CFGU_GetConfigInfoBlk2_endPattern[] = {
        0x10, 0x80, 0xBD, 0xE8, 0x82, 0x00, 0x01, 0x00
    };

    u8 *CFGU_GetConfigInfoBlk2_endPos = code;

    while((CFGU_GetConfigInfoBlk2_endPos + sizeof(CFGU_GetConfigInfoBlk2_endPattern)) < (code + size) 
          && (*CFGUHandleOffset > 0x10000000UL || ((u32)CFGU_GetConfigInfoBlk2_endPos % 4) != 0)) 
    {
        //There might be multiple implementations of GetConfigInfoBlk2 but let's search for the one we want
        CFGU_GetConfigInfoBlk2_endPos += sizeof(CFGU_GetConfigInfoBlk2_endPattern);
        CFGU_GetConfigInfoBlk2_endPos = memsearch(CFGU_GetConfigInfoBlk2_endPos, CFGU_GetConfigInfoBlk2_endPattern, 
                                                  size - (u32)(CFGU_GetConfigInfoBlk2_endPos - code), sizeof(CFGU_GetConfigInfoBlk2_endPattern));

        if(CFGU_GetConfigInfoBlk2_endPos == NULL) break;

        *CFGUHandleOffset = *(u32 *)(CFGU_GetConfigInfoBlk2_endPos + 8);
    }

    return CFGU_GetConfigInfoBlk2_endPos;
}

static void patchCfgGetLanguage(u8* code, u32 size, u8 languageId, u8 *CFGU_GetConfigInfoBlk2_endPos)
{
    u8 *CFGU_GetConfigInfoBlk2_startPos = CFGU_GetConfigInfoBlk2_endPos; //Let's find STMFD SP (there might be a NOP before, but nevermind)
    while(*(u16 *)CFGU_GetConfigInfoBlk2_startPos != 0xE92D) CFGU_GetConfigInfoBlk2_startPos -= 2;
    CFGU_GetConfigInfoBlk2_startPos -= 2;

    u8 *languageBlkIdPos = code;
    u32 patched = 0;

    while((languageBlkIdPos + 4) < (code + size) && !patched)
    {
        static const u32 languageBlkId = 0xA0002;

        languageBlkIdPos += 4;
        languageBlkIdPos = memsearch(languageBlkIdPos, &languageBlkId, size - (u32)(languageBlkIdPos - code), 4);

        if(languageBlkIdPos == NULL) break;
        if(((u32)languageBlkIdPos % 4) != 0) continue;

        for(u8 *instr = languageBlkIdPos - 8; instr >= languageBlkIdPos - 0x1008; instr -= 4) //Should be enough
        {
            if(*(instr + 3) != 0xEB) continue; //We're looking for BL
            u32 low24 = (*(u32 *)instr & 0x00FFFFFF) << 2;
            s32 offset = (((low24 >> 25) != 0) ? -low24 : low24) + 8; //Branch offset + 8 for prefetch

            if((instr + offset) >= (CFGU_GetConfigInfoBlk2_startPos - 4) && (instr + offset) <= CFGU_GetConfigInfoBlk2_endPos) 
            {
                *(u32 *)(instr - 4)  = 0xE3A00000 | languageId; // mov    r0, sp                 => mov r0, =languageID
                *(u32 *)instr        = 0xE5CD0000;              // bl     CFGU_GetConfigInfoBlk2 => strb r0, [sp]
                *(u32 *)(instr + 4)  = 0xE3A00000;              // mov    r1, pc                 => mov r0, 0             (result code)

                //We're done
                patched = 1;
                break;
            }
        }
    }
}

static void patchCfgGetRegion(u8* code, u32 size, u8 regionID, u32 *CFGUHandleOffset)
{
    static const u8 cfgSecureInfoGetRegionCmdPattern[] = {
        0x70, 0x4F, 0x1D, 0xEE, 0x02, 0x08, 0xA0, 0xE3, 0x80, 0x00, 0xA4, 0xE5 
    };

    u8 *cmdPos = code;

    while((cmdPos + sizeof(cfgSecureInfoGetRegionCmdPattern)) < (code + size))
    {
        cmdPos += sizeof(cfgSecureInfoGetRegionCmdPattern);
        cmdPos = memsearch(cmdPos, cfgSecureInfoGetRegionCmdPattern, size - (u32)(cmdPos - code), sizeof(cfgSecureInfoGetRegionCmdPattern));

        if(cmdPos == NULL) break;
        if(*(u16 *)(cmdPos + 12 + 2) != 0xE59F) continue; // ldr r0, [pc, X]

        if(*(u32 *)(cmdPos + 12 + 8 + *(u16 *)(cmdPos + 12)) == *CFGUHandleOffset)
        {
            *(u32 *)(cmdPos + 16) = 0xE3A00000 | regionID; // mov    r0, =regionID
            *(u32 *)(cmdPos + 20) = 0xE5C40008;            // strb   r0, [r4, 8]
            *(u32 *)(cmdPos + 24) = 0xE3A00000;            // mov    r0, 0            (result code)
            *(u32 *)(cmdPos + 28) = 0xE5840004;            // str    r0, [r4, 4]

            //The remaining, not patched, function code will do the rest for us
            break;
        }
    }
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
            if(R_SUCCEEDED(loadConfig()) && CONFIG(6))
            {
                static const u16 verPattern[] = u"Ver.";
                const u32 currentNand = BOOTCONFIG(0, 3);
                const u32 matchingFirm = BOOTCONFIG(2, 1) == (currentNand != 0);

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

            if(R_SUCCEEDED(loadConfig()) && MULTICONFIG(1))
            {
                static const u8 cfgN3dsCpuPattern[] = {
                    0x40, 0xA0, 0xE1, 0x07, 0x00
                };

                u8 *cfgN3dsCpuLoc = memsearch(code, cfgN3dsCpuPattern, size, sizeof(cfgN3dsCpuPattern));

                //Patch N3DS CPU Clock and L2 cache setting
                if(cfgN3dsCpuLoc != NULL)
                {
                    *(u32 *)(cfgN3dsCpuLoc + 3) = 0xE1A00000;
                    *(u32 *)(cfgN3dsCpuLoc + 0x1F) = 0xE3A00000 | MULTICONFIG(1);
                }
            }

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

            if(R_SUCCEEDED(loadSecureInfo()))
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

        default:
            if((progId & 0xFFFFFFFF00000000LL) == 0x0004000000000000LL && R_SUCCEEDED(loadConfig()) && CONFIG(4))
            {
                //Language emulation
                u8 regionId = 0xFF,
                   languageId = 0xFF;

                int ret = loadTitleLocaleConfig(progId, &regionId, &languageId);

                if(R_SUCCEEDED(ret))
                {
                    u32 CFGUHandleOffset = 0xFFFFFFFF;
                    u8 *CFGU_GetConfigInfoBlk2_endPos = getCfgOffsets(code, size, &CFGUHandleOffset);

                    if(CFGU_GetConfigInfoBlk2_endPos != NULL)
                    {
                        if(languageId != 0xFF) patchCfgGetLanguage(code, size, languageId, CFGU_GetConfigInfoBlk2_endPos);
                        if(regionId != 0xFF) patchCfgGetRegion(code, size, regionId, &CFGUHandleOffset);
                    }
                }
            }

            break;
    }
}