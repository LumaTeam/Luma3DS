#include <3ds.h>
#include "patcher.h"
#include "memory.h"
#include "strings.h"
#include "ifile.h"
#include "CFWInfo.h"

static CFWInfo info;

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

    if(infoLoaded) return;

    svcGetCFWInfo(&info);

    IFile file;
    if(LOADERFLAG(ISSAFEMODE) && R_SUCCEEDED(fileOpen(&file, ARCHIVE_SDMC, "/", FS_OPEN_READ))) //Init SD card if SAFE_MODE is being booted
        IFile_Close(&file);

    infoLoaded = true;
}

static inline bool secureInfoExists(void)
{
    static bool exists = false;

    if(exists) return true;

    IFile file;
    if(R_SUCCEEDED(fileOpen(&file, ARCHIVE_NAND_RW, "/sys/SecureInfo_C", FS_OPEN_READ)))
    {
        exists = true;
        IFile_Close(&file);
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

    if(R_FAILED(openLumaFile(&file, paths[currentNand]))) return;

    u64 fileSize;

    if(R_FAILED(IFile_GetSize(&file, &fileSize)) || fileSize > 62) goto exit;

    u8 buf[62];
    u64 total;

    if(R_FAILED(IFile_Read(&file, &total, buf, fileSize))) goto exit;

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

exit:
    IFile_Close(&file);
}

static inline u32 loadTitleCodeSection(u64 progId, u8 *code, u32 size)
{
    /* Here we look for "/luma/code_sections/[u64 titleID in hex, uppercase].bin"
       If it exists it should be a decompressed binary code file */

    char path[] = "/luma/code_sections/0000000000000000.bin";
    progIdToStr(path + 35, progId);

    IFile file;

    if(R_FAILED(openLumaFile(&file, path))) return 0;

    u32 ret;
    u64 fileSize;

    if(R_FAILED(IFile_GetSize(&file, &fileSize)) || fileSize > size) ret = 1;
    else
    {
        u64 total;

        if(R_FAILED(IFile_Read(&file, &total, code, fileSize)) || total != fileSize) ret = 1;
        else ret = 0;
    }

    IFile_Close(&file);

    return ret;
}

static inline u32 loadTitleLocaleConfig(u64 progId, u8 *regionId, u8 *languageId)
{
    /* Here we look for "/luma/locales/[u64 titleID in hex, uppercase].txt"
       If it exists it should contain, for example, "EUR IT" */

    char path[] = "/luma/locales/0000000000000000.txt";
    progIdToStr(path + 29, progId);

    IFile file;

    if(R_FAILED(openLumaFile(&file, path))) return 0;

    u32 ret;
    u64 fileSize;

    if(R_FAILED(IFile_GetSize(&file, &fileSize)) || fileSize < 6 || fileSize > 8)
    {
        ret = 1;
        goto exit;
    }

    char buf[8];
    u64 total;

    if(R_FAILED(IFile_Read(&file, &total, buf, fileSize)))
    {
        ret = 1;
        goto exit;
    }

    u32 i,
        j;

    for(i = 0; i < 7; i++)
    {
        static const char *regions[] = {"JPN", "USA", "EUR", "AUS", "CHN", "KOR", "TWN"};

        if(memcmp(buf, regions[i], 3) == 0)
        {
            *regionId = (u8)i;
            break;
        }
    }

    for(j = 0; j < 12; j++)
    {
        static const char *languages[] = {"JP", "EN", "FR", "DE", "IT", "ES", "ZH", "KO", "NL", "PT", "RU", "TW"};

        if(memcmp(buf + 4, languages[j], 2) == 0)
        {
            *languageId = (u8)j;
            break;
        }
    }

    if(i == 7 || j == 12) ret = 1;
    else ret = 0;

exit:
    IFile_Close(&file);

    return ret;
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
        if(*(u32 *)pos != 0xD8A103F9) continue;

        for(u32 *l = (u32 *)pos - 4; n < 24 && l < (u32 *)pos + 4; l++)
            if(*l <= 0x10000000) possible[n++] = *l;
    }

    if(!n) return NULL;

    for(u8 *CFGU_GetConfigInfoBlk2_endPos = code; CFGU_GetConfigInfoBlk2_endPos < code + size - 8; CFGU_GetConfigInfoBlk2_endPos += 4)
    {
        static const u32 CFGU_GetConfigInfoBlk2_endPattern[] = {0xE8BD8010, 0x00010082};

        //There might be multiple implementations of GetConfigInfoBlk2 but let's search for the one we want
        u32 *cmp = (u32 *)CFGU_GetConfigInfoBlk2_endPos;

        if(cmp[0] != CFGU_GetConfigInfoBlk2_endPattern[0] || cmp[1] != CFGU_GetConfigInfoBlk2_endPattern[1]) continue;

        *CFGUHandleOffset = *((u32 *)CFGU_GetConfigInfoBlk2_endPos + 2);

        for(u32 i = 0; i < n; i++)
            if(possible[i] == *CFGUHandleOffset) return CFGU_GetConfigInfoBlk2_endPos;

        CFGU_GetConfigInfoBlk2_endPos += 4;
    }

    return NULL;
}

static inline u32 patchCfgGetLanguage(u8 *code, u32 size, u8 languageId, u8 *CFGU_GetConfigInfoBlk2_endPos)
{
    u8 *CFGU_GetConfigInfoBlk2_startPos; //Let's find STMFD SP (there might be a NOP before, but nevermind)

    for(CFGU_GetConfigInfoBlk2_startPos = CFGU_GetConfigInfoBlk2_endPos - 4;
        CFGU_GetConfigInfoBlk2_startPos >= code && *((u16 *)CFGU_GetConfigInfoBlk2_startPos + 1) != 0xE92D;
        CFGU_GetConfigInfoBlk2_startPos -= 4);

    if(CFGU_GetConfigInfoBlk2_startPos < code) return 1;

    for(u8 *languageBlkIdPos = code; languageBlkIdPos < code + size; languageBlkIdPos += 4)
    {
        if(*(u32 *)languageBlkIdPos != 0xA0002) continue;

        for(u8 *instr = languageBlkIdPos - 8; instr >= languageBlkIdPos - 0x1008 && instr >= code + 4; instr -= 4) //Should be enough
        {
            if(instr[3] != 0xEB) continue; //We're looking for BL

            u8 *calledFunction = instr;
            u32 i = 0;
            bool found;

            do
            {
                u32 low24 = (*(u32 *)calledFunction & 0x00FFFFFF) << 2;
                u32 signMask = (u32)(-(low24 >> 25)) & 0xFC000000; //Sign extension
                s32 offset = (s32)(low24 | signMask) + 8;          //Branch offset + 8 for prefetch

                calledFunction += offset;

                if(calledFunction >= CFGU_GetConfigInfoBlk2_startPos - 4 && calledFunction <= CFGU_GetConfigInfoBlk2_endPos)
                {
                    *((u32 *)instr - 1)  = 0xE3A00000 | languageId; //mov    r0, sp                 => mov r0, =languageId
                    *(u32 *)instr        = 0xE5CD0000;              //bl     CFGU_GetConfigInfoBlk2 => strb r0, [sp]
                    *((u32 *)instr + 1)  = 0xE3B00000;              //(1 or 2 instructions)         => movs r0, 0             (result code)

                    //We're done
                    return 0;
                }

                i++;
            }
            while(i < 2 && !found && calledFunction[3] == 0xEA);
        }
    }

    return 1;
}

static inline void patchCfgGetRegion(u8 *code, u32 size, u8 regionId, u32 CFGUHandleOffset)
{
    for(u8 *cmdPos = code; cmdPos < code + size - 28; cmdPos += 4)
    {
        static const u32 cfgSecureInfoGetRegionCmdPattern[] = {0xEE1D0F70, 0xE3A00802};

        u32 *cmp = (u32 *)cmdPos;

        if(*cmp != cfgSecureInfoGetRegionCmdPattern[1]) continue;

        for(u32 i = 1; i < 3; i++)
            if((*(cmp - i) & 0xFFFF0FFF) == cfgSecureInfoGetRegionCmdPattern[0] && *((u16 *)cmdPos + 5) == 0xE59F &&
               *(u32 *)(cmdPos + 16 + *((u16 *)cmdPos + 4)) == CFGUHandleOffset)
        {
            cmp[3] = 0xE3A00000 | regionId; //mov    r0, =regionId
            cmp[4] = 0xE5C40008;            //strb   r0, [r4, #8]
            cmp[5] = 0xE3A00000;            //mov    r0, #0 (result code)
            cmp[6] = 0xE5840004;            //str    r0, [r4, #4]

            //The remaining, not patched, function code will do the rest for us
            return;
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
        static const u8 pattern[] = {
            0x0A, 0x0C, 0x00, 0x10
        },
                        patch[] = {
            0x01, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1
        };

        //Patch SMDH region checks
        if(!patchMemory(code, size,
                pattern,
                sizeof(pattern), -31,
                patch,
                sizeof(patch), 1
            )) goto error;
    }

    else if(progId == 0x0004013000003202LL) //FRIENDS
    {
        static const u8 pattern[] = {
            0x42, 0xE0, 0x1E, 0xFF
        };

        u8 mostRecentFpdVer = 8;

        u8 *off = memsearch(code, pattern, size, sizeof(pattern));

        if(off == NULL) goto error;

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
        static const u16 pattern[] = u"Ve";
        static u16 *patch;
        u32 patchSize = 0,
        currentNand = BOOTCFG_NAND;

        u16 customVerString[19];
        loadCustomVerString(customVerString, &patchSize, currentNand);

        if(patchSize != 0) patch = customVerString;
        else
        {
            patchSize = 8;
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

            patch = (currentFirm != 0) == (currentNand != 0) ? verStringsNands[currentNand] :
                                          (!currentNand ? verStringsSysEmu[currentFirm - 1] : verStringsEmuSys[currentNand - 1]);
        }

        //Patch Ver. string
        if(!patchMemory(code, size,
                pattern,
                sizeof(pattern) - 2, 0,
                patch,
                patchSize, 1
            )) goto error;
    }

    else if(progId == 0x0004013000008002LL) //NS
    {
        if(progVer > 4)
        {
            static const u8 pattern[] = {
                0x0C, 0x18, 0xE1, 0xD8
            },
                            patch[] = {
                0x0B, 0x18, 0x21, 0xC8
            };

            //Disable updates from foreign carts (makes carts region-free)
            u32 ret = patchMemory(code, size,
                          pattern,
                          sizeof(pattern), 0,
                          patch,
                          sizeof(patch), 2
                      );

            if(ret == 0 || (ret == 1 && progVer > 0xB)) goto error;
        }

        if(LOADERFLAG(ISN3DS))
        {
            u32 cpuSetting = MULTICONFIG(NEWCPU);

            if(cpuSetting != 0)
            {
                static const u8 pattern[] = {
                    0x0C, 0x00, 0x94, 0x15
                };

                u32 *off = (u32 *)memsearch(code, pattern, size, sizeof(pattern));

                if(off == NULL) goto error;

                //Patch N3DS CPU Clock and L2 cache setting
                *(off - 4) = *(off - 3);
                *(off - 3) = *(off - 1);
                memcpy(off - 1, off, 16);
                *(off + 3) = 0xE3800000 | cpuSetting;
            }
        }
    }

    else if(progId == 0x0004013000001702LL) //CFG
    {
        static const u8 pattern[] = {
            0x06, 0x46, 0x10, 0x48
        },
                        patch[] = {
            0x00, 0x26
        };

        //Disable SecureInfo signature check
        if(!patchMemory(code, size,
                pattern,
                sizeof(pattern), 0,
                patch,
                sizeof(patch), 1
            )) goto error;

        if(secureInfoExists())
        {
            static const u16 pattern[] = u"Sec",
                             patch[] = u"C";

            //Use SecureInfo_C
            if(patchMemory(code, size,
                   pattern,
                   sizeof(pattern) - 2, 22,
                   patch,
                   sizeof(patch) - 2, 2
               ) != 2) goto error;
        }
    }

    else if(progId == 0x0004013000003702LL && progVer > 0) //RO
    {
        static const u8 pattern[] = {
            0x20, 0xA0, 0xE1, 0x8B
        },
                        pattern2[] = {
            0xE1, 0x30, 0x40, 0x2D
        },
                        pattern3[] = {
            0x2D, 0xE9, 0x01, 0x70
        },
                        patch[] = {
            0x00, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1 //mov r0, #0; bx lr
        };

        //Disable CRR0 signature (RSA2048 with SHA256) check and CRO0/CRR0 SHA256 hash checks (section hashes, and hash table)
        if(!patchMemory(code, size,
                pattern,
                sizeof(pattern), -9,
                patch,
                sizeof(patch), 1
            ) ||
           !patchMemory(code, size,
                pattern2,
                sizeof(pattern2), 1,
                patch,
                sizeof(patch), 1
            ) ||
           !patchMemory(code, size,
                pattern3,
                sizeof(pattern3), -2,
                patch,
                sizeof(patch), 1
            )) goto error;
    }

    else if(progId == 0x0004003000008A02LL && MULTICONFIG(DEVOPTIONS) == 1) //ErrDisp
    {
        static const u8 pattern[] = { 
            0x00, 0xD0, 0xE5, 0xDB
        },
                        pattern2[] = {
            0x14, 0x00, 0xD0, 0xE5, 0x01
        },
                        patch[] = {
            0x00, 0x00, 0xA0, 0xE3
        };

        //Patch UNITINFO checks to make ErrDisp more verbose
        if(!patchMemory(code, size,
                pattern,
                sizeof(pattern), -1,
                patch,
                sizeof(patch), 1
            ) ||
           patchMemory(code, size,
               pattern2,
               sizeof(pattern2), 0,
               patch,
               sizeof(patch), 3
           ) != 3) goto error;
    }

    else if(CONFIG(USELANGEMUANDCODE) && (u32)((progId & 0xFFFFFFF000000000LL) >> 0x24) == 0x0004000)
    {
        u8 regionId = 0xFF,
           languageId;
        if(!loadTitleLocaleConfig(progId, &regionId, &languageId) ||
           !loadTitleCodeSection(progId, code, size)) goto error;

        if(regionId != 0xFF)
        {
            u32 CFGUHandleOffset;
            u8 *CFGU_GetConfigInfoBlk2_endPos = getCfgOffsets(code, size, &CFGUHandleOffset);

            if(CFGU_GetConfigInfoBlk2_endPos == NULL ||
               patchCfgGetLanguage(code, size, languageId, CFGU_GetConfigInfoBlk2_endPos)) goto error;

            patchCfgGetRegion(code, size, regionId, CFGUHandleOffset);
        }
    }

    return;

error:
    svcBreak(USERBREAK_ASSERT);
    while(true);
}