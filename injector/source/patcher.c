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

        if(found == NULL) svcBreak(USERBREAK_ASSERT);

        memcpy(found + offset, replace, repSize);

        u32 at = (u32)(found - start);

        if(at + patSize > size) break;

        size -= at + patSize;
        start = found + patSize;
    }
}

static Result fileOpen(IFile *file, FS_ArchiveID archiveId, const char *path, int flags)
{
    FS_Path filePath = {PATH_ASCII, strnlen(path, 255) + 1, path},
            archivePath = {PATH_EMPTY, 1, (u8 *)""};

    return IFile_Open(file, archiveId, archivePath, filePath, flags);
}

static Result openLumaFile(IFile *file, const char *path)
{
    Result res = fileOpen(file, ARCHIVE_SDMC, path, FS_OPEN_READ);

    if((u32)res == 0xC88044AB) res = fileOpen(file, ARCHIVE_NAND_RW, path, FS_OPEN_READ); //Returned if SD is not mounted

    return res;
}

static inline void loadCFWInfo(void)
{
    static bool infoLoaded = false;

    if(!infoLoaded)
    {
        svcGetCFWInfo(&info);

        IFile file;
        if(LOADERFLAG(ISSAFEMODE) && R_SUCCEEDED(fileOpen(&file, ARCHIVE_SDMC, "/", FS_OPEN_READ))) //Init SD card if SAFE_MODE is being booted
            IFile_Close(&file);

        infoLoaded = true;
    }
}

static inline bool secureInfoExists(void)
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

static inline void loadCustomVerString(u16 *out, u32 *verStringSize, u32 currentNand)
{
    static const char *paths[] = { "/luma/customversion_sys.txt",
                                   "/luma/customversion_emu.txt",
                                   "/luma/customversion_emu2.txt",
                                   "/luma/customversion_emu3.txt",
                                   "/luma/customversion_emu4.txt" };

    IFile file;

    if(R_SUCCEEDED(openLumaFile(&file, paths[currentNand])))
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

static inline void loadTitleCodeSection(u64 progId, u8 *code, u32 size)
{
    /* Here we look for "/luma/code_sections/[u64 titleID in hex, uppercase].bin"
       If it exists it should be a decompressed binary code file */

    char path[] = "/luma/code_sections/0000000000000000.bin";
    progIdToStr(path + 35, progId);

    IFile file;

    if(R_SUCCEEDED(openLumaFile(&file, path)))
    {
        u64 fileSize;

        if(R_SUCCEEDED(IFile_GetSize(&file, &fileSize)) && fileSize <= size)
        {
            u64 total;
            IFile_Read(&file, &total, code, fileSize);

            if(total != fileSize) svcBreak(USERBREAK_ASSERT);
        }

        IFile_Close(&file);
    }
}

static inline void loadTitleLocaleConfig(u64 progId, u8 *regionId, u8 *languageId)
{
    /* Here we look for "/luma/locales/[u64 titleID in hex, uppercase].txt"
       If it exists it should contain, for example, "EUR IT" */

    char path[] = "/luma/locales/0000000000000000.txt";
    progIdToStr(path + 29, progId);

    IFile file;

    if(R_SUCCEEDED(openLumaFile(&file, path)))
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

static inline u8 *getCfgOffsets(u8 *code, u32 size, u32 *CFGUHandleOffset)
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

static inline void patchCfgGetLanguage(u8 *code, u32 size, u8 languageId, u8 *CFGU_GetConfigInfoBlk2_endPos)
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
                        *((u32 *)instr - 1)  = 0xE3A00000 | languageId; //mov    r0, sp                 => mov r0, =languageId
                        *(u32 *)instr        = 0xE5CD0000;              //bl     CFGU_GetConfigInfoBlk2 => strb r0, [sp]
                        *((u32 *)instr + 1)  = 0xE3B00000;              //(1 or 2 instructions)         => movs r0, 0             (result code)

                        //We're done
                        return;
                    }
                }
            }
        }
    }
}

static inline void patchCfgGetRegion(u8 *code, u32 size, u8 regionId, u32 CFGUHandleOffset)
{
    for(u8 *cmdPos = code; cmdPos < code + size - 28; cmdPos += 4)
    {
        static const u32 cfgSecureInfoGetRegionCmdPattern[] = {0xEE1D4F70, 0xE3A00802, 0xE5A40080};

        u32 *cmp = (u32 *)cmdPos;

        if(cmp[0] == cfgSecureInfoGetRegionCmdPattern[0] && cmp[1] == cfgSecureInfoGetRegionCmdPattern[1] &&
           cmp[2] == cfgSecureInfoGetRegionCmdPattern[2] && *((u16 *)cmdPos + 7) == 0xE59F &&
           *(u32 *)(cmdPos + 20 + *((u16 *)cmdPos + 6)) == CFGUHandleOffset)
        {
            *((u32 *)cmdPos + 4) = 0xE3A00000 | regionId; //mov    r0, =regionId
            *((u32 *)cmdPos + 5) = 0xE5C40008;            //strb   r0, [r4, 8]
            *((u32 *)cmdPos + 6) = 0xE3B00000;            //movs   r0, 0            (result code) ('s' not needed but nvm)
            *((u32 *)cmdPos + 7) = 0xE5840004;            //str    r0, [r4, 4]

            //The remaining, not patched, function code will do the rest for us
            break;
        }
    }
}

void patchCode(u64 progId, u16 progVer, u8 *code, u32 size)
{
    loadCFWInfo();

    if(((progId == 0x0004003000008F02LL || //USA Home Menu
         progId == 0x0004003000008202LL || //JPN Home Menu
         progId == 0x0004003000009802LL) //EUR Home Menu
        && progVer > 4) ||
       (progId == 0x000400300000A902LL //KOR Home Menu
        && progVer > 0) ||
        progId == 0x000400300000A102LL || //CHN Home Menu
        progId == 0x000400300000B102LL) //TWN Home Menu
    {
        static const u8 regionFreePattern[] = {
            0x0A, 0x0C, 0x00, 0x10
        },
                        regionFreePatch[] = {
            0x01, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1
        };

        //Patch SMDH region checks
        patchMemory(code, size, 
            regionFreePattern,
            sizeof(regionFreePattern), -31, 
            regionFreePatch, 
            sizeof(regionFreePatch), 1
        );
    }

    else if(progId == 0x0004013000003202LL) //FRIENDS
    {
        static const u8 fpdVerPattern[] = {
            0x42, 0xE0, 0x1E, 0xFF
        };

        u8 mostRecentFpdVer = 8;

        u8 *off = memsearch(code, fpdVerPattern, size, sizeof(fpdVerPattern));

        if(off == NULL) svcBreak(USERBREAK_ASSERT);

        //Allow online access to work with old friends modules
        if(off[0xA] < mostRecentFpdVer) off[0xA] = mostRecentFpdVer;
    }

    else if((progId == 0x0004001000021000LL || //USA MSET
             progId == 0x0004001000020000LL || //JPN MSET
             progId == 0x0004001000022000LL || //EUR MSET
             progId == 0x0004001000026000LL || //CHN MSET
             progId == 0x0004001000027000LL || //KOR MSET
             progId == 0x0004001000028000LL) //TWN MSET
            && CONFIG(PATCHVERSTRING)) 
    {
        static const u16 verPattern[] = u"Ve";
        static u16 *verString;
        u32 verStringSize = 0,
        currentNand = BOOTCFG_NAND;

        u16 customVerString[19];
        loadCustomVerString(customVerString, &verStringSize, currentNand);

        if(verStringSize != 0) verString = customVerString;
        else
        {
            verStringSize = 8;
            u32 currentFirm = BOOTCFG_FIRM;

            static u16 *verStringsNands[] = { u" Sys",
                                              u" Emu",
                                              u"Emu2",
                                              u"Emu3",
                                              u"Emu4" },

                       *verStringsEmuSys[] = { u"EmuS",
                                               u"Em2S",
                                               u"Em3S",
                                               u"Em4S" },

                       *verStringsSysEmu[] = { u"SysE",
                                               u"SyE2",
                                               u"SyE3",
                                               u"SyE4" };

            verString = (currentFirm != 0) == (currentNand != 0) ? verStringsNands[currentNand] :
                                              (!currentNand ? verStringsSysEmu[currentFirm - 1] : verStringsEmuSys[currentNand - 1]);
        }

        //Patch Ver. string
        patchMemory(code, size,
            verPattern,
            sizeof(verPattern) - 2, 0,
            verString,
            verStringSize, 1
        );
    }

    else if(progId == 0x0004013000008002LL) //NS
    {
        if(progVer > 4)
        {
            static const u8 stopCartUpdatesPattern[] = {
                0x0C, 0x18, 0xE1, 0xD8
            },
                            stopCartUpdatesPatch[] = {
                0x0B, 0x18, 0x21, 0xC8
            };

            //Disable updates from foreign carts (makes carts region-free)
            patchMemory(code, size, 
                stopCartUpdatesPattern, 
                sizeof(stopCartUpdatesPattern), 0, 
                stopCartUpdatesPatch,
                sizeof(stopCartUpdatesPatch), 2
            );
        }

        if(LOADERFLAG(ISN3DS))
        {
            u32 cpuSetting = MULTICONFIG(NEWCPU);

            if(cpuSetting != 0)
            {
                static const u8 cfgN3dsCpuPattern[] = {
                    0x0C, 0x00, 0x94, 0x15
                };

                u32 *off = (u32 *)memsearch(code, cfgN3dsCpuPattern, size, sizeof(cfgN3dsCpuPattern));

                if(off == NULL) svcBreak(USERBREAK_ASSERT);

                //Patch N3DS CPU Clock and L2 cache setting
                *(off - 4) = 0xE1A00000;
                *(off + 3) = 0xE3A00000 | cpuSetting;
            }
        }
    }

    else if(progId == 0x0004013000001702LL) //CFG
    {
        static const u8 secureinfoSigCheckPattern[] = {
            0x06, 0x46, 0x10, 0x48
        },
                        secureinfoSigCheckPatch[] = {
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
            static const u16 secureinfoFilenamePattern[] = u"SecureInfo_",
                             secureinfoFilenamePatch[] = u"C";

            //Use SecureInfo_C
            patchMemory(code, size, 
                secureinfoFilenamePattern, 
                sizeof(secureinfoFilenamePattern) - 2,
                sizeof(secureinfoFilenamePattern) - 2, 
                secureinfoFilenamePatch, 
                sizeof(secureinfoFilenamePatch) - 2, 2
            );
        }
    }
        
    else if(progId == 0x0004013000003702LL && progVer > 0) //RO
    {
        static const u8 sigCheckPattern[] = {
            0x20, 0xA0, 0xE1, 0x8B
        },
                        sha256ChecksPattern1[] = {
            0xE1, 0x30, 0x40, 0x2D
        },
                        sha256ChecksPattern2[] = {
            0x2D, 0xE9, 0x01, 0x70
        },
                        stub[] = {
            0x00, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1 //mov r0, #0; bx lr
        };

        //Disable CRR0 signature (RSA2048 with SHA256) check
        patchMemory(code, size, 
            sigCheckPattern, 
            sizeof(sigCheckPattern), -9, 
            stub,
            sizeof(stub), 1
        );

        //Disable CRO0/CRR0 SHA256 hash checks (section hashes, and hash table)
        patchMemory(code, size, 
            sha256ChecksPattern1, 
            sizeof(sha256ChecksPattern1), 1, 
            stub,
            sizeof(stub), 1
        );

        patchMemory(code, size, 
            sha256ChecksPattern2, 
            sizeof(sha256ChecksPattern2), -2, 
            stub,
            sizeof(stub), 1
        );
    }

    else if(progId == 0x0004003000008A02LL && MULTICONFIG(DEVOPTIONS) == 1) //ErrDisp
    {
        static const u8 unitinfoCheckPattern1[] = { 
            0x00, 0xD0, 0xE5, 0xDB
        },
                        unitinfoCheckPattern2[] = {
            0x14, 0x00, 0xD0, 0xE5, 0x01
        },
                        unitinfoCheckPatch[] = {
            0x00, 0x00, 0xA0, 0xE3
        };

        patchMemory(code, size, 
            unitinfoCheckPattern1, 
            sizeof(unitinfoCheckPattern1), -1, 
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

    else if(CONFIG(USELANGEMUANDCODE) && (u32)((progId & 0xFFFFFFF000000000LL) >> 0x24) == 0x0004000)
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