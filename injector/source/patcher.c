#include <3ds.h>
#include "patcher.h"
#include "memory.h"
#include "strings.h"
#include "ifile.h"
#include "CFWInfo.h"

static CFWInfo info;

static void patchMemory(u8 *start, u32 size, const void *pattern, u32 patSize, int offset, const void *replace, u32 repSize, u32 count)
{
    for(u32 i = 0; i < count; i++)
    {
        u8 *found = memsearch(start, pattern, size, patSize);

        if(found == NULL) break;

        memcpy(found + offset, replace, repSize);

        u32 at = (u32)(found - start);

        if(at + patSize > size) break;

        size -= at + patSize;
        start = found + patSize;
    }
}

static int fileOpen(IFile *file, FS_ArchiveID archiveId, const char *path, int flags)
{
    FS_Path filePath = {PATH_ASCII, strnlen(path, 255) + 1, path},
            archivePath = {PATH_EMPTY, 1, (u8 *)""};

    return IFile_Open(file, archiveId, archivePath, filePath, flags);
}

static void loadCFWInfo(void)
{
    static bool infoLoaded = false;

    if(!infoLoaded)
    {
        svcGetCFWInfo(&info);

        IFile file;
        if(BOOTCFG_SAFEMODE != 0 && R_SUCCEEDED(fileOpen(&file, ARCHIVE_SDMC, "/", FS_OPEN_READ))) //Init SD card if SAFE_MODE is being booted
            IFile_Close(&file);

        infoLoaded = true;
    }
}

static bool secureInfoExists(void)
{
    static bool exists = false;

    if(!exists)
    {
        IFile file;
        if(R_SUCCEEDED(fileOpen(&file, ARCHIVE_NAND_RW, "/sys/SecureInfo_C", FS_OPEN_READ)))
        {
            exists = true;
            IFile_Close(&file);
        }
    }

    return exists;
}

static void loadCustomVerString(u16 *out, u32 *verStringSize)
{
    static const char path[] = "/luma/customversion.txt";

    IFile file;

    if(R_SUCCEEDED(fileOpen(&file, ARCHIVE_SDMC, path, FS_OPEN_READ)))
    {
        u64 fileSize;

        if(R_SUCCEEDED(IFile_GetSize(&file, &fileSize)) && fileSize <= 62)
        {
            u8 buf[fileSize];
            u64 total;

            if(R_SUCCEEDED(IFile_Read(&file, &total, buf, fileSize)))
            {
                static const u8 bom[] = {0xEF, 0xBB, 0xBF};
                u32 finalSize = 0;

                //Convert from UTF-8 to UTF-16 (Nintendo doesn't support 4-byte UTF-16, so 4-byte UTF-8 is unsupported)
                for(u32 increase, fileSizeTmp = (u32)fileSize, i = (fileSizeTmp > 2 && memcmp(buf, bom, 3) == 0) ? 3 : 0;
                    i < fileSizeTmp && finalSize < 19; i += increase, finalSize++)
                {
                    if((buf[i] & 0x80) == 0 && !(buf[i] == 0xA || buf[i] == 0xD))
                    {
                        increase = 1;
                        out[finalSize] = (u16)buf[i];
                    }
                    else if((buf[i] & 0xE0) == 0xC0 && i + 1 < fileSizeTmp && (buf[i + 1] & 0xC0) == 0x80)
                    {
                        increase = 2;
                        out[finalSize] = (u16)(((buf[i] & 0x1F) << 6) | (buf[i + 1] & 0x3F));
                    }
                    else if((buf[i] & 0xF0) == 0xE0 && i + 2 < fileSizeTmp && (buf[i + 1] & 0xC0) == 0x80 && (buf[i + 2] & 0xC0) == 0x80)
                    {
                        increase = 3;
                        out[finalSize] = (u16)(((buf[i] & 0xF) << 12) | ((buf[i + 1] & 0x3F) << 6) | (buf[i + 2] & 0x3F));
                    }
                    else break;
                }

                if(finalSize > 0)
                {
                    if(finalSize > 5 && finalSize < 19) out[finalSize++] = 0;
                    *verStringSize = finalSize * 2;
                }
            }
        }

        IFile_Close(&file);
    }
}

static void loadTitleCodeSection(u64 progId, u8 *code, u32 size)
{
    /* Here we look for "/luma/code_sections/[u64 titleID in hex, uppercase].bin"
       If it exists it should be a decompressed binary code file */

    char path[] = "/luma/code_sections/0000000000000000.bin";
    progIdToStr(path + 35, progId);

    IFile file;

    if(R_SUCCEEDED(fileOpen(&file, ARCHIVE_SDMC, path, FS_OPEN_READ)))
    {
        u64 fileSize;

        if(R_SUCCEEDED(IFile_GetSize(&file, &fileSize)) && fileSize <= size)
        {
            u64 total;
            IFile_Read(&file, &total, code, fileSize);
        }

        IFile_Close(&file);
    }
}

static void loadTitleLocaleConfig(u64 progId, u8 *regionId, u8 *languageId)
{
    /* Here we look for "/luma/locales/[u64 titleID in hex, uppercase].txt"
       If it exists it should contain, for example, "EUR IT" */

    char path[] = "/luma/locales/0000000000000000.txt";
    progIdToStr(path + 29, progId);

    IFile file;

    if(R_SUCCEEDED(fileOpen(&file, ARCHIVE_SDMC, path, FS_OPEN_READ)))
    {
        u64 fileSize;

        if(R_SUCCEEDED(IFile_GetSize(&file, &fileSize)) && fileSize > 5 && fileSize < 9)
        {
            char buf[fileSize];
            u64 total;

            if(R_SUCCEEDED(IFile_Read(&file, &total, buf, fileSize)))
            {
                for(u32 i = 0; i < 7; i++)
                {
                    static const char *regions[] = {"JPN", "USA", "EUR", "AUS", "CHN", "KOR", "TWN"};

                    if(memcmp(buf, regions[i], 3) == 0)
                    {
                        *regionId = (u8)i;
                        break;
                    }
                }

                for(u32 i = 0; i < 12; i++)
                {
                    static const char *languages[] = {"JP", "EN", "FR", "DE", "IT", "ES", "ZH", "KO", "NL", "PT", "RU", "TW"};

                    if(memcmp(buf + 4, languages[i], 2) == 0)
                    {
                        *languageId = (u8)i;
                        break;
                    }
                }
            }
        }

        IFile_Close(&file);
    }
}

static u8 *getCfgOffsets(u8 *code, u32 size, u32 *CFGUHandleOffset)
{
    /* HANS:
       Look for error code which is known to be stored near cfg:u handle
       this way we can find the right candidate
       (handle should also be stored right after end of candidate function) */

    u32 n = 0,
        possible[24];

    for(u8 *pos = code + 4; n < 24 && pos < code + size - 4; pos += 4)
    {
        if(*(u32 *)pos == 0xD8A103F9)
        {
            for(u32 *l = (u32 *)pos - 4; n < 24 && l < (u32 *)pos + 4; l++)
                if(*l <= 0x10000000) possible[n++] = *l;
        }
    }

    for(u8 *CFGU_GetConfigInfoBlk2_endPos = code; CFGU_GetConfigInfoBlk2_endPos < code + size - 8; CFGU_GetConfigInfoBlk2_endPos += 4)
    {
        static const u32 CFGU_GetConfigInfoBlk2_endPattern[] = {0xE8BD8010, 0x00010082};

        //There might be multiple implementations of GetConfigInfoBlk2 but let's search for the one we want
        u32 *cmp = (u32 *)CFGU_GetConfigInfoBlk2_endPos;

        if(cmp[0] == CFGU_GetConfigInfoBlk2_endPattern[0] && cmp[1] == CFGU_GetConfigInfoBlk2_endPattern[1])
        {
            *CFGUHandleOffset = *((u32 *)CFGU_GetConfigInfoBlk2_endPos + 2);

            for(u32 i = 0; i < n; i++)
                if(possible[i] == *CFGUHandleOffset) return CFGU_GetConfigInfoBlk2_endPos;

            CFGU_GetConfigInfoBlk2_endPos += 4;
        }
    }

    return NULL;
}

static void patchCfgGetLanguage(u8 *code, u32 size, u8 languageId, u8 *CFGU_GetConfigInfoBlk2_endPos)
{
    u8 *CFGU_GetConfigInfoBlk2_startPos; //Let's find STMFD SP (there might be a NOP before, but nevermind)

    for(CFGU_GetConfigInfoBlk2_startPos = CFGU_GetConfigInfoBlk2_endPos - 4;
        CFGU_GetConfigInfoBlk2_startPos >= code && *((u16 *)CFGU_GetConfigInfoBlk2_startPos + 1) != 0xE92D;
        CFGU_GetConfigInfoBlk2_startPos -= 2);

    for(u8 *languageBlkIdPos = code; languageBlkIdPos < code + size; languageBlkIdPos += 4)
    {
        if(*(u32 *)languageBlkIdPos == 0xA0002)
        {
            for(u8 *instr = languageBlkIdPos - 8; instr >= languageBlkIdPos - 0x1008 && instr >= code + 4; instr -= 4) //Should be enough
            {
                if(instr[3] == 0xEB) //We're looking for BL
                {
                    u8 *calledFunction = instr;
                    u32 i = 0;
                    bool found;

                    do
                    {
                        u32 low24 = (*(u32 *)calledFunction & 0x00FFFFFF) << 2;
                        u32 signMask = (u32)(-(low24 >> 25)) & 0xFC000000; //Sign extension
                        s32 offset = (s32)(low24 | signMask) + 8;          //Branch offset + 8 for prefetch

                        calledFunction += offset;

                        found = calledFunction >= CFGU_GetConfigInfoBlk2_startPos - 4 && calledFunction <= CFGU_GetConfigInfoBlk2_endPos;
                        i++;
                    }
                    while(i < 2 && !found && calledFunction[3] == 0xEA);

                    if(found) 
                    {
                        *((u32 *)instr - 1)  = 0xE3A00000 | languageId; // mov    r0, sp                 => mov r0, =languageId
                        *(u32 *)instr        = 0xE5CD0000;              // bl     CFGU_GetConfigInfoBlk2 => strb r0, [sp]
                        *((u32 *)instr + 1)  = 0xE3B00000;              // (1 or 2 instructions)         => movs r0, 0             (result code)

                        //We're done
                        return;
                    }
                }
            }
        }
    }
}

static void patchCfgGetRegion(u8 *code, u32 size, u8 regionId, u32 CFGUHandleOffset)
{
    for(u8 *cmdPos = code; cmdPos < code + size - 28; cmdPos += 4)
    {
        static const u32 cfgSecureInfoGetRegionCmdPattern[] = {0xEE1D4F70, 0xE3A00802, 0xE5A40080};

        u32 *cmp = (u32 *)cmdPos;

        if(cmp[0] == cfgSecureInfoGetRegionCmdPattern[0] && cmp[1] == cfgSecureInfoGetRegionCmdPattern[1] &&
           cmp[2] == cfgSecureInfoGetRegionCmdPattern[2] && *((u16 *)cmdPos + 7) == 0xE59F &&
           *(u32 *)(cmdPos + 20 + *((u16 *)cmdPos + 6)) == CFGUHandleOffset)
        {
            *((u32 *)cmdPos + 4) = 0xE3A00000 | regionId; // mov    r0, =regionId
            *((u32 *)cmdPos + 5) = 0xE5C40008;            // strb   r0, [r4, 8]
            *((u32 *)cmdPos + 6) = 0xE3B00000;            // movs   r0, 0            (result code) ('s' not needed but nvm)
            *((u32 *)cmdPos + 7) = 0xE5840004;            // str    r0, [r4, 4]

            //The remaining, not patched, function code will do the rest for us
            break;
        }
    }
}

void patchCode(u64 progId, u8 *code, u32 size)
{
    loadCFWInfo();

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
                0x00, 0x00, 0x55, 0xE3, 0x01, 0x10, 0xA0
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

            //Block silent auto-updates
            patchMemory(code, size, 
                blockAutoUpdatesPattern, 
                sizeof(blockAutoUpdatesPattern), 0, 
                blockAutoUpdatesPatch, 
                sizeof(blockAutoUpdatesPatch), 1
            );

            //Apply only if the user booted with R
            if((BOOTCFG_NAND != 0) != (BOOTCFG_FIRM != 0))
            {
                static const u8 skipEshopUpdateCheckPattern[] = {
                    0x30, 0xB5, 0xF1, 0xB0
                };
                static const u8 skipEshopUpdateCheckPatch[] = {
                    0x00, 0x20, 0x08, 0x60, 0x70, 0x47
                };

                //Skip update checks to access the EShop
                patchMemory(code, size, 
                    skipEshopUpdateCheckPattern, 
                    sizeof(skipEshopUpdateCheckPattern), 0, 
                    skipEshopUpdateCheckPatch, 
                    sizeof(skipEshopUpdateCheckPatch), 1
                );
            }

            break;
        }

        case 0x0004013000003202LL: // FRIENDS
        {
            static const u8 fpdVerPattern[] = {
                0xE0, 0x1E, 0xFF, 0x2F, 0xE1, 0x01, 0x01
            };

            u8 mostRecentFpdVer = 7;

            u8 *fpdVer = memsearch(code, fpdVerPattern, size, sizeof(fpdVerPattern));

            //Allow online access to work with old friends modules
            if(fpdVer != NULL && fpdVer[9] < mostRecentFpdVer) fpdVer[9] = mostRecentFpdVer;

            break;
        }
        
        case 0x0004001000021000LL: // USA MSET
        case 0x0004001000020000LL: // JPN MSET
        case 0x0004001000022000LL: // EUR MSET
        case 0x0004001000026000LL: // CHN MSET
        case 0x0004001000027000LL: // KOR MSET
        case 0x0004001000028000LL: // TWN MSET
        {
            if(CONFIG(PATCHVERSTRING))
            {
                static const u16 verPattern[] = u"Ver.";
                static u16 *verString;
                u32 verStringSize = 0;

                u16 customVerString[19];
                loadCustomVerString(customVerString, &verStringSize);

                if(verStringSize != 0) verString = customVerString;
                else
                {
                    verStringSize = 8;
                    u32 currentNand = BOOTCFG_NAND,
                        currentFirm = BOOTCFG_FIRM;
                    bool matchingFirm = (currentFirm != 0) == (currentNand != 0);

                    static u16 verStringEmu[] = u"Emu ",
                               verStringEmuSys[] = u"Em S",
                               verStringSysEmu[] = u"SyE ";

                    switch(currentNand)
                    {
                        case 1:
                            verString = matchingFirm ? u" Emu" : u"EmuS";
                            break;
                        case 2:
                        case 3:
                        case 4:
                        {
                            if(matchingFirm)
                            {
                                verStringEmu[3] = '0' + currentNand;
                                verString = verStringEmu;
                            }
                            else
                            {
                                verStringEmuSys[2] = '0' + currentNand;
                                verString = verStringEmuSys;
                            }
                            break;
                        }
                        default:
                            if(matchingFirm) verString = u" Sys";
                            else
                            {
                                if(currentFirm == 1) verString = u"SysE";
                                else
                                {
                                    verStringSysEmu[3] = '0' + currentFirm;
                                    verString = verStringSysEmu;
                                }
                            }
                            break;
                    }
                }

                //Patch Ver. string
                patchMemory(code, size,
                    verPattern,
                    sizeof(verPattern) - 2, 0,
                    verString,
                    verStringSize, 1
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

            u32 cpuSetting = MULTICONFIG(NEWCPU);

            if(cpuSetting != 0)
            {
                static const u8 cfgN3dsCpuPattern[] = {
                    0x00, 0x40, 0xA0, 0xE1, 0x07
                };

                u32 *cfgN3dsCpuLoc = (u32 *)memsearch(code, cfgN3dsCpuPattern, size, sizeof(cfgN3dsCpuPattern));

                //Patch N3DS CPU Clock and L2 cache setting
                if(cfgN3dsCpuLoc != NULL)
                {
                    *(cfgN3dsCpuLoc + 1) = 0xE1A00000;
                    *(cfgN3dsCpuLoc + 8) = 0xE3A00000 | cpuSetting;
                }
            }

            break;
        }

        case 0x0004013000001702LL: // CFG
        {
            static const u8 secureinfoSigCheckPattern[] = {
                0x06, 0x46, 0x10, 0x48
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

            if(secureInfoExists())
            {
                static const u16 secureinfoFilenamePattern[] = u"SecureInfo_";
                static const u16 secureinfoFilenamePatch[] = u"C";

                //Use SecureInfo_C
                patchMemory(code, size, 
                    secureinfoFilenamePattern, 
                    sizeof(secureinfoFilenamePattern) - 2,
                    sizeof(secureinfoFilenamePattern) - 2, 
                    secureinfoFilenamePatch, 
                    sizeof(secureinfoFilenamePatch) - 2, 2
                );
            }

            break;
        }
        
        case 0x0004013000003702LL: // RO
        {
            static const u8 sigCheckPattern[] = {
                0x30, 0x40, 0x2D, 0xE9, 0x02
            };
            static const u8 sha256ChecksPattern1[] = {
                0x30, 0x40, 0x2D, 0xE9, 0x24
            };
            static const u8 sha256ChecksPattern2[] = {
                0xF8, 0x4F, 0x2D, 0xE9, 0x01
            };
            
            static const u8 stub[] = {
                0x00, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1 // mov r0, #0; bx lr
            };
            
            //Disable CRR0 signature (RSA2048 with SHA256) check
            patchMemory(code, size, 
                sigCheckPattern, 
                sizeof(sigCheckPattern), 0, 
                stub,
                sizeof(stub), 1
            );
            
            //Disable CRO0/CRR0 SHA256 hash checks (section hashes, and hash table)
            patchMemory(code, size, 
                sha256ChecksPattern1, 
                sizeof(sha256ChecksPattern1), 0, 
                stub,
                sizeof(stub), 1
            );
            
            patchMemory(code, size, 
                sha256ChecksPattern2, 
                sizeof(sha256ChecksPattern2), 0, 
                stub,
                sizeof(stub), 1
            );
            
            break;
        }

        case 0x0004003000008A02LL: // ErrDisp
        {
            if(MULTICONFIG(DEVOPTIONS) == 1)
            {
                static const u8 unitinfoCheckPattern1[] = { 
                    0x14, 0x00, 0xD0, 0xE5, 0xDB
                };

                static const u8 unitinfoCheckPattern2[] = {
                    0x14, 0x00, 0xD0, 0xE5, 0x01
                } ;

                static const u8 unitinfoCheckPatch[] = {
                    0x00, 0x00, 0xA0, 0xE3
                } ;

                patchMemory(code, size, 
                    unitinfoCheckPattern1, 
                    sizeof(unitinfoCheckPattern1), 0, 
                    unitinfoCheckPatch, 
                    sizeof(unitinfoCheckPatch), 1
                );

                patchMemory(code, size, 
                    unitinfoCheckPattern2, 
                    sizeof(unitinfoCheckPattern2), 0,
                    unitinfoCheckPatch, 
                    sizeof(unitinfoCheckPatch), 3
                );
            }

            break;
        }

        default:
            if(CONFIG(USELANGEMUANDCODE))
            {
                if((u32)((progId & 0xFFFFFFF000000000LL) >> 0x24) == 0x0004000)
                {
                    //External .code section loading
                    loadTitleCodeSection(progId, code, size);

                    //Language emulation
                    u8 regionId = 0xFF,
                       languageId = 0xFF;
                    loadTitleLocaleConfig(progId, &regionId, &languageId);

                    if(regionId != 0xFF || regionId != 0xFF)
                    {
                        u32 CFGUHandleOffset;
                        u8 *CFGU_GetConfigInfoBlk2_endPos = getCfgOffsets(code, size, &CFGUHandleOffset);

                        if(CFGU_GetConfigInfoBlk2_endPos != NULL)
                        {
                            if(languageId != 0xFF) patchCfgGetLanguage(code, size, languageId, CFGU_GetConfigInfoBlk2_endPos);
                            if(regionId != 0xFF) patchCfgGetRegion(code, size, regionId, CFGUHandleOffset);
                        }
                    }
                }
            }

        break;
    }
}