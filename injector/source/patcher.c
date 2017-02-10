#include <3ds.h>
#include "patcher.h"
#include "memory.h"
#include "strings.h"
#include "ifile.h"
#include "CFWInfo.h"
#include "../build/bundled.h"

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

static u32 openLumaFile(IFile *file, const char *path)
{
    Result res = fileOpen(file, ARCHIVE_SDMC, path, FS_OPEN_READ);

    if(R_SUCCEEDED(res)) return ARCHIVE_SDMC;

    //Returned if SD is not mounted
    return (u32)res == 0xC88044AB && R_SUCCEEDED(fileOpen(file, ARCHIVE_NAND_RW, path, FS_OPEN_READ)) ? ARCHIVE_NAND_RW : 0;
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

    if(!openLumaFile(&file, paths[currentNand])) return;

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

static inline u8 *getCfgOffsets(u8 *code, u32 size, u32 *CFGUHandleOffset)
{
    /* HANS:
       Look for error code which is known to be stored near cfg:u handle
       this way we can find the right candidate
       (handle should also be stored right after end of candidate function) */

    u32 n = 0,
        possible[24];

    for(u8 *pos = code + 16; n < 24 && pos <= code + size - 16; pos += 4)
    {
        if(*(u32 *)pos != 0xD8A103F9) continue;

        for(u32 *l = (u32 *)pos - 4; n < 24 && l < (u32 *)pos + 4; l++)
            if(*l <= 0x10000000) possible[n++] = *l;
    }

    if(!n) return NULL;

    for(u8 *CFGU_GetConfigInfoBlk2_endPos = code; CFGU_GetConfigInfoBlk2_endPos <= code + size - 12; CFGU_GetConfigInfoBlk2_endPos += 4)
    {
        static const u32 CFGU_GetConfigInfoBlk2_endPattern[] = {0xE8BD8010, 0x00010082};

        //There might be multiple implementations of GetConfigInfoBlk2 but let's search for the one we want
        u32 *cmp = (u32 *)CFGU_GetConfigInfoBlk2_endPos;

        if(cmp[0] != CFGU_GetConfigInfoBlk2_endPattern[0] || cmp[1] != CFGU_GetConfigInfoBlk2_endPattern[1]) continue;

        for(u32 i = 0; i < n; i++)
            if(possible[i] == cmp[2])
        {
            *CFGUHandleOffset = cmp[2];
            return CFGU_GetConfigInfoBlk2_endPos;
        }

        CFGU_GetConfigInfoBlk2_endPos += 4;
    }

    return NULL;
}

static inline bool patchCfgGetLanguage(u8 *code, u32 size, u8 languageId, u8 *CFGU_GetConfigInfoBlk2_endPos)
{
    u8 *CFGU_GetConfigInfoBlk2_startPos; //Let's find STMFD SP (there might be a NOP before, but nevermind)

    for(CFGU_GetConfigInfoBlk2_startPos = CFGU_GetConfigInfoBlk2_endPos - 4;
        *((u16 *)CFGU_GetConfigInfoBlk2_startPos + 1) != 0xE92D; CFGU_GetConfigInfoBlk2_startPos -= 4)
        if(CFGU_GetConfigInfoBlk2_startPos < code + 4) return false;

    for(u8 *languageBlkIdPos = code; languageBlkIdPos <= code + size - 4; languageBlkIdPos += 4)
    {
        if(*(u32 *)languageBlkIdPos != 0xA0002) continue;

        for(u8 *instr = languageBlkIdPos - 8; instr >= languageBlkIdPos - 0x1008 && instr >= code + 4; instr -= 4) //Should be enough
        {
            if(instr[3] != 0xEB) continue; //We're looking for BL

            u8 *calledFunction = instr;
            u32 i = 0;

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
                    return true;
                }

                i++;
            }
            while(i < 2 && calledFunction[3] == 0xEA);
        }
    }

    return false;
}

static inline void patchCfgGetRegion(u8 *code, u32 size, u8 regionId, u32 CFGUHandleOffset)
{
    for(u8 *cmdPos = code; cmdPos <= code + size - 28; cmdPos += 4)
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

static u32 findNearestStmfd(u8* code, u32 pos)
{
    while(pos >= 4)
    {
        pos -= 4;
        if(*(u16 *)(code + pos + 2) == 0xE92D) return pos;
    }

    return 0xFFFFFFFF;
}

static u32 findFunctionCommand(u8* code, u32 size, u32 command)
{
    u32 func;

    for(func = 4; *(u32 *)(code + func) != command; func += 4)
        if(func > size - 8) return 0xFFFFFFFF;

    return findNearestStmfd(code, func);
}

static inline u32 findThrowFatalError(u8* code, u32 size)
{
    u32 connectToPort;

    for(connectToPort = 0; *(u32 *)(code + connectToPort + 4) != 0xEF00002D; connectToPort += 4)
        if(connectToPort > size - 12) return 0xFFFFFFFF;

    u32 func = 0xFFFFFFFF;

    for(u32 i = 4; func == 0xFFFFFFFF && i <= size - 4; i += 4)
    {
        if(*(u32 *)(code + i) != MAKE_BRANCH_LINK(i, connectToPort)) continue;

        func = findNearestStmfd(code, i);

        for(u32 pos = func + 4; func != 0xFFFFFFFF && pos <= size - 4 && *(u16 *)(code + pos + 2) != 0xE92D; pos += 4)
            if(*(u32 *)(code + pos) == 0xE200167E) func = 0xFFFFFFFF;
    }

    return func;
}

static inline bool applyCodeIpsPatch(u64 progId, u8 *code, u32 size)
{
    /* Here we look for "/luma/titles/[u64 titleID in hex, uppercase]/code.ips"
       If it exists it should be an IPS format patch */

    char path[] = "/luma/titles/0000000000000000/code.ips";
    progIdToStr(path + 28, progId);

    IFile file;

    if(!openLumaFile(&file, path)) return true;

    bool ret = false;
    u8 buffer[5];
    u64 total;

    if(R_FAILED(IFile_Read(&file, &total, buffer, 5)) || total != 5 || memcmp(buffer, "PATCH", 5) != 0) goto exit;

    while(R_SUCCEEDED(IFile_Read(&file, &total, buffer, 3)) && total == 3)
    {
        if(memcmp(buffer, "EOF", 3) == 0)
        {
            ret = true;
            break;
        }

        u32 offset = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];

        if(R_FAILED(IFile_Read(&file, &total, buffer, 2)) || total != 2) break;

        u32 patchSize = (buffer[0] << 8) | buffer[1];

        if(!patchSize)
        {
            if(R_FAILED(IFile_Read(&file, &total, buffer, 2)) || total != 2) break;

            u32 rleSize = (buffer[0] << 8) | buffer[1];

            if(offset + rleSize > size) break;

            if(R_FAILED(IFile_Read(&file, &total, buffer, 1)) || total != 1) break;

            for(u32 i = 0; i < rleSize; i++)
                code[offset + i] = buffer[0];

            continue;
        }

        if(offset + patchSize > size) break;

        if(R_FAILED(IFile_Read(&file, &total, code + offset, patchSize)) || total != patchSize) break;
    }

exit:
    IFile_Close(&file);

    return ret;
}

static inline bool loadTitleCodeSection(u64 progId, u8 *code, u32 size)
{
    /* Here we look for "/luma/titles/[u64 titleID in hex, uppercase]/code.bin"
       If it exists it should be a decrypted and decompressed binary code file */

    char path[] = "/luma/titles/0000000000000000/code.bin";
    progIdToStr(path + 28, progId);

    IFile file;

    if(!openLumaFile(&file, path)) return true;

    bool ret;
    u64 fileSize;

    if(R_FAILED(IFile_GetSize(&file, &fileSize)) || fileSize > size) ret = false;
    else
    {
        u64 total;

        ret = R_SUCCEEDED(IFile_Read(&file, &total, code, fileSize)) && total == fileSize;
    }

    IFile_Close(&file);

    return ret;
}

static inline bool loadTitleLocaleConfig(u64 progId, u8 *regionId, u8 *languageId)
{
    /* Here we look for "/luma/titles/[u64 titleID in hex, uppercase]/locale.txt"
       If it exists it should contain, for example, "EUR IT" */

    char path[] = "/luma/titles/0000000000000000/locale.txt";
    progIdToStr(path + 28, progId);

    IFile file;

    if(!openLumaFile(&file, path)) return true;

    bool ret = false;
    u64 fileSize;

    if(R_FAILED(IFile_GetSize(&file, &fileSize)) || fileSize < 6 || fileSize > 8) goto exit;

    char buf[8];
    u64 total;

    if(R_FAILED(IFile_Read(&file, &total, buf, fileSize))) goto exit;

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

    ret = i != 7 && j != 12;

exit:
    IFile_Close(&file);

    return ret;
}

static inline bool patchRomfsRedirection(u64 progId, u8* code, u32 size)
{
    /* Here we look for "/luma/titles/[u64 titleID in hex, uppercase]/romfs"
       If it exists it should be a decrypted raw RomFS */

    char path[] = "/luma/titles/0000000000000000/romfs";
    progIdToStr(path + 28, progId);

    IFile file;
    u32 archive = openLumaFile(&file, path);

    if(!archive) return true;

    bool ret = false;
    u64 romfsSize;

    if(R_FAILED(IFile_GetSize(&file, &romfsSize))) goto exit;

    u64 total;
    u32 magic;

    if(R_FAILED(IFile_Read(&file, &total, &magic, 4)) || total != 4 || magic != 0x43465649) goto exit;

    u32 fsOpenFileDirectly = findFunctionCommand(code, size, 0x08030204),
        throwFatalError = findThrowFatalError(code, size);

    if(fsOpenFileDirectly == 0xFFFFFFFF || throwFatalError == 0xFFFFFFFF) goto exit;

    //Setup the payload
    u8 *payload = code + throwFatalError;
    memcpy(payload, romfsredir_bin, romfsredir_bin_size);
    memcpy(payload + romfsredir_bin_size, path, sizeof(path));
    *(u32 *)(payload + 0xC) = *(u32 *)(code + fsOpenFileDirectly);

    u32 *payloadSymbols = (u32 *)(payload + romfsredir_bin_size - 0x24);
    payloadSymbols[0] = 0x100000 + fsOpenFileDirectly + 4;
    *(u64 *)(payloadSymbols + 2) = 0x1000ULL;
    *(u64 *)(payloadSymbols + 4) = romfsSize - 0x1000ULL;
    payloadSymbols[6] = archive;
    payloadSymbols[7] = sizeof(path);
    payloadSymbols[8] = 0x100000 + throwFatalError + romfsredir_bin_size; //String pointer

    //Place the hooks
    *(u32 *)(code + fsOpenFileDirectly) = MAKE_BRANCH(fsOpenFileDirectly, throwFatalError);

    u32 fsOpenLinkFile = findFunctionCommand(code, size, 0x80C0000);

    if(fsOpenLinkFile != 0xFFFFFFFF)
    {
        *(u32 *)(code + fsOpenLinkFile) = 0xE3A03003; //mov r3, #3
        *(u32 *)(code + fsOpenLinkFile + 4) = MAKE_BRANCH(fsOpenLinkFile + 4, throwFatalError);
    }

    ret = true;

exit:
    IFile_Close(&file);

    return ret;
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

        u8 mostRecentFpdVer = 9;

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

    else if(progId == 0x0004013000002802LL && progVer > 0) //DLP
    {
        static const u8 pattern[] = {
            0x0C, 0xAC, 0xC0, 0xD8
        },
                        patch[] = {
            0x00, 0x00, 0x00, 0x00
        };

        //Patch DLP region checks
        if(!patchMemory(code, size,
                pattern,
                sizeof(pattern), 0,
                patch,
                sizeof(patch), 1
            )) goto error;
    }
   
    if(CONFIG(PATCHGAMES) && (u32)((progId >> 0x20) & 0xFFFFFFEDULL) == 0x00040000)
    {
        u8 regionId = 0xFF,
           languageId;

        if(!loadTitleCodeSection(progId, code, size) ||
           !loadTitleLocaleConfig(progId, &regionId, &languageId) ||
           !patchRomfsRedirection(progId, code, size) ||
           !applyCodeIpsPatch(progId, code, size)) goto error;

        if(regionId != 0xFF)
        {
            u32 CFGUHandleOffset;
            u8 *CFGU_GetConfigInfoBlk2_endPos = getCfgOffsets(code, size, &CFGUHandleOffset);

            if(CFGU_GetConfigInfoBlk2_endPos == NULL ||
               !patchCfgGetLanguage(code, size, languageId, CFGU_GetConfigInfoBlk2_endPos)) goto error;

            patchCfgGetRegion(code, size, regionId, CFGUHandleOffset);
        }
    }

    return;

error:
    svcBreak(USERBREAK_ASSERT);
    while(true);
}
