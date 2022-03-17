#include <3ds.h>
#include "patcher.h"
#include "bps_patcher.h"
#include "memory.h"
#include "strings.h"
#include "romfsredir.h"

static u32 patchMemory(u8 *start, u32 size, const void *pattern, u32 patSize, s32 offset, const void *replace, u32 repSize, u32 count)
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

static Result fileOpen(IFile *file, FS_ArchiveID archiveId, const char *path, u32 flags)
{
    return IFile_Open(file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), flags);
}

static bool dirCheck(FS_ArchiveID archiveId, const char *path)
{
    bool ret;
    Handle handle;
    FS_Archive archive;

    if(R_FAILED(FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, "")))) ret = false;
    else
    {
        ret = R_SUCCEEDED(FSUSER_OpenDirectory(&handle, archive, fsMakePath(PATH_ASCII, path)));
        if(ret) FSDIR_Close(handle);
        FSUSER_CloseArchive(archive);
    }

    return ret;
}

static bool openLumaFile(IFile *file, const char *path)
{
    FS_ArchiveID archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;

    return R_SUCCEEDED(fileOpen(file, archiveId, path, FS_OPEN_READ));
}

static u32 checkLumaDir(const char *path)
{
    FS_ArchiveID archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;

    return dirCheck(archiveId, path) ? archiveId : 0;
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

static u32 findFunctionStart(u8 *code, u32 pos)
{
    while(pos >= 4)
    {
        pos -= 4;
        if(*(u16 *)(code + pos + 2) == 0xE92D) return pos;
    }

    return 0xFFFFFFFF;
}

static inline bool findLayeredFsSymbols(u8 *code, u32 size, u32 *fsMountArchive, u32 *fsRegisterArchive, u32 *fsTryOpenFile, u32 *fsOpenFileDirectly)
{
    u32 found = 0,
        *temp = NULL;

    for(u32 addr = 0; addr <= size - 4; addr += 4)
    {
        u32 *addr32 = (u32 *)(code + addr);

        switch(*addr32)
        {
            case 0xE5970010:
                if(addr <= size - 12 && *fsMountArchive == 0xFFFFFFFF && addr32[1] == 0xE1CD20D8 && (addr32[2] & 0xFFFFFF) == 0x008D0000) temp = fsMountArchive;
                break;
            case 0xE24DD028:
                if(addr <= size - 16 && *fsMountArchive == 0xFFFFFFFF && addr32[1] == 0xE1A04000 && addr32[2] == 0xE59F60A8 && addr32[3] == 0xE3A0C001) temp = fsMountArchive;
                break;
            case 0xE3500008:
                if(addr <= size - 12 && *fsRegisterArchive == 0xFFFFFFFF && (addr32[1] & 0xFFF00FF0) == 0xE1800400 && (addr32[2] & 0xFFF00FF0) == 0xE1800FC0) temp = fsRegisterArchive;
                break;
            case 0xE351003A:
                if(addr <= size - 0x40 && *fsTryOpenFile == 0xFFFFFFFF && addr32[1] == 0x1AFFFFFC && addr32[0xD] == 0xE590C000 && addr32[0xF] == 0xE12FFF3C) temp = fsTryOpenFile;
                break;
            case 0x08030204:
                if(*fsOpenFileDirectly == 0xFFFFFFFF) temp = fsOpenFileDirectly;
                break;
        }

        if(temp != NULL)
        {
            *temp = findFunctionStart(code, addr);

            if(*temp != 0xFFFFFFFF)
            {
                found++;
                if(found == 4) break;
            }

            temp = NULL;
        }
    }

    return found == 4;
}

static inline bool findLayeredFsPayloadOffset(u8 *code, u32 size, u32 roSize, u32 dataSize, u32 roAddress, u32 dataAddress, u32 *payloadOffset, u32 *pathOffset, u32 *pathAddress)
{
    u32 roundedTextSize = ((size + 4095) & 0xFFFFF000),
        roundedRoSize = ((roSize + 4095) & 0xFFFFF000),
        roundedDataSize = ((dataSize + 4095) & 0xFFFFF000);

    //First check for sufficient padding at the end of the .text segment
    if(roundedTextSize - size >= romfsRedirPatchSize) *payloadOffset = size;
    else
    {
        //If there isn't enough padding look for the "throwFatalError" function to replace
        u32 svcConnectToPort = 0xFFFFFFFF;

        for(u32 addr = 4; svcConnectToPort == 0xFFFFFFFF && addr <= size - 4; addr += 4)
        {
            if(*(u32 *)(code + addr) == 0xEF00002D)
                svcConnectToPort = addr - 4;
        }

        if(svcConnectToPort != 0xFFFFFFFF)
        {
            u32 func = 0xFFFFFFFF;

            for(u32 i = 4; func == 0xFFFFFFFF && i <= size - 4; i += 4)
            {
                if(*(u32 *)(code + i) != MAKE_BRANCH_LINK(i, svcConnectToPort)) continue;

                func = findFunctionStart(code, i);

                for(u32 pos = func + 4; func != 0xFFFFFFFF && pos <= size - 4 && *(u16 *)(code + pos + 2) != 0xE92D; pos += 4)
                    if(*(u32 *)(code + pos) == 0xE200167E) func = 0xFFFFFFFF;
            }

            if(func != 0xFFFFFFFF) *payloadOffset = func;
        }
    }

    if(roundedRoSize - roSize >= 39)
    {
        *pathOffset = roundedTextSize + roSize;
        *pathAddress = roAddress + roSize;
    }
    else if(roundedDataSize - dataSize >= 39)
    {
        *pathOffset = roundedTextSize + roundedRoSize + dataSize;
        *pathAddress = dataAddress + dataSize;
    }
    else
    {
        u32 strSpace = 0xFFFFFFFF;

        for(u32 addr = 0; strSpace == 0xFFFFFFFF && addr <= size - 4; addr += 4)
        {
            if(*(u32 *)(code + addr) == 0xE3A00B42)
                strSpace = findFunctionStart(code, addr);
        }

        if(strSpace != 0xFFFFFFFF)
        {
            *pathOffset = strSpace;
            *pathAddress = 0x100000 + strSpace;
        }
    }

    return *payloadOffset != 0 && *pathOffset != 0;
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

bool loadTitleCodeSection(u64 progId, u8 *code, u32 size)
{
    /* Here we look for "/luma/titles/[u64 titleID in hex, uppercase]/code.bin"
       If it exists it should be a decrypted and decompressed binary code file */

    char path[] = "/luma/titles/0000000000000000/code.bin";
    progIdToStr(path + 28, progId);

    IFile file;

    if(!openLumaFile(&file, path)) return false;

    u64 fileSize;

    if(R_FAILED(IFile_GetSize(&file, &fileSize)) || fileSize > size) goto error;
    else
    {
        u64 total;

        if(R_FAILED(IFile_Read(&file, &total, code, fileSize)) || total != fileSize) goto error;
    }

    IFile_Close(&file);

    return true;

error:
    IFile_Close(&file);

    svcBreak(USERBREAK_ASSERT);
    while(true);
}

bool loadTitleExheaderInfo(u64 progId, ExHeader_Info *exheaderInfo)
{
    /* Here we look for "/luma/titles/[u64 titleID in hex, uppercase]/exheader.bin"
       If it exists it should be a decrypted exheader / exheader info */

    char path[] = "/luma/titles/0000000000000000/exheader.bin";
    progIdToStr(path + 28, progId);

    IFile file;

    if(!openLumaFile(&file, path)) return false;

    u64 fileSize;

    if(R_FAILED(IFile_GetSize(&file, &fileSize)) || (fileSize != sizeof(ExHeader_Info) && fileSize != sizeof(ExHeader))) goto error;
    else
    {
        u64 total;

        if(R_FAILED(IFile_Read(&file, &total, exheaderInfo, sizeof(ExHeader_Info))) || total != sizeof(ExHeader_Info)) goto error;
    }

    IFile_Close(&file);

    return true;

error:
    IFile_Close(&file);

    svcBreak(USERBREAK_ASSERT);
    while(true);
}

static inline bool loadTitleLocaleConfig(u64 progId, u8 *mask, u8 *regionId, u8 *languageId, u8 *countryId, u8 *stateId)
{
    /* Here we look for "/luma/titles/[u64 titleID in hex, uppercase]/locale.txt"
       If it exists it should contain, for example, "EUR IT" */

    char path[] = "/luma/titles/0000000000000000/locale.txt";
    progIdToStr(path + 28, progId);
    *mask = *regionId = *languageId = *countryId = *stateId = 0;

    IFile file;

    if(!openLumaFile(&file, path)) return false;

    bool ret = false;
    u64 fileSize;

    if(R_FAILED(IFile_GetSize(&file, &fileSize)) || fileSize < 3) goto exit;
    if(fileSize >= 12) fileSize = 12;

    char buf[12] = "------------";
    u64 total;

    if(R_FAILED(IFile_Read(&file, &total, buf, fileSize))) goto exit;

    ret = true;

    static const char *regions[]   = {"---", "JPN", "USA", "EUR", "AUS", "CHN", "KOR", "TWN"},
                      *languages[] = {"--", "JP", "EN", "FR", "DE", "IT", "ES", "ZH", "KO", "NL", "PT", "RU", "TW"},
                      *countries[] = {"--", "JP", "--", "--", "--", "--", "--", "--", "AI", "AG", "AR", "AW",
                                      "BS", "BB", "BZ", "BO", "BR", "VG", "CA", "KY", "CL", "CO", "CR", "DM",
                                      "DO", "EC", "SV", "GF", "GD", "GP", "GT", "GY", "HT", "HN", "JM", "MQ",
                                      "MX", "MS", "AN", "NI", "PA", "PY", "PE", "KN", "LC", "VC", "SR", "TT",
                                      "TC", "US", "UY", "VI", "VE", "--", "--", "--", "--", "--", "--", "--",
                                      "--", "--", "--", "--", "AL", "AU", "AT", "BE", "BA", "BW", "BG", "HR",
                                      "CY", "CZ", "DK", "EE", "FI", "FR", "DE", "GR", "HU", "IS", "IE", "IT",
                                      "LV", "LS", "LI", "LT", "LU", "MK", "MT", "ME", "MZ", "NA", "NL", "NZ",
                                      "NO", "PL", "PT", "RO", "RU", "RS", "SK", "SI", "ZA", "ES", "SZ", "SE",
                                      "CH", "TR", "GB", "ZM", "ZW", "AZ", "MR", "ML", "NE", "TD", "SD", "ER",
                                      "DJ", "SO", "AD", "GI", "GG", "IM", "JE", "MC", "TW", "--", "--", "--",
                                      "--", "--", "--", "--", "KR", "--", "--", "--", "--", "--", "--", "--",
                                      "HK", "MO", "--", "--", "--", "--", "--", "--", "ID", "SG", "TH", "PH",
                                      "MY", "--", "--", "--", "CN", "--", "--", "--", "--", "--", "--", "--",
                                      "AE", "IN", "EG", "OM", "QA", "KW", "SA", "SY", "BH", "JO", "--", "--",
                                      "--", "--", "--", "--", "SM", "VA"};

    u32 i;
    for(i = 0; i < sizeof(regions) / sizeof(char *); i++)
    {
        if(memcmp(buf, regions[i], 3) == 0 && i != 0)
        {
            *regionId = (u8)(i - 1);
            *mask |= 1;
            break;
        }
    }

    for(i = 0; fileSize >= 6 && i < sizeof(languages) / sizeof(char *); i++)
    {
        if(memcmp(buf + 4, languages[i], 2) == 0 && i != 0)
        {
            *languageId = (u8)(i - 1);
            *mask |= 2;
            break;
        }
    }

    for(i = 0; fileSize >= 9 && i < sizeof(countries) / sizeof(char *); i++)
    {
        if(memcmp(buf + 7, countries[i], 2) == 0 && i != 0)
        {
            *countryId = (u8)i;
            *mask |= 4;
            break;
        }
    }

    if(fileSize >= 12 &&
        ((buf[10] >= '0' && buf[10] <= '9') || (buf[10] >= 'a' && buf[10] <= 'f') || (buf[10] >= 'A' && buf[10] <= 'F')) &&
        ((buf[11] >= '0' && buf[11] <= '9') || (buf[11] >= 'a' && buf[11] <= 'f') || (buf[11] >= 'A' && buf[11] <= 'F')))
    {
        if     (buf[10] >= '0' && buf[10] <= '9') *stateId = 16 * (buf[10] - '0');
        else if(buf[10] >= 'a' && buf[10] <= 'f') *stateId = 16 * (buf[10] - 'a' + 10);
        else if(buf[10] >= 'A' && buf[10] <= 'F') *stateId = 16 * (buf[10] - 'A' + 10);

        if     (buf[11] >= '0' && buf[11] <= '9') *stateId += buf[11] - '0';
        else if(buf[11] >= 'a' && buf[11] <= 'f') *stateId += buf[11] - 'a' + 10;
        else if(buf[11] >= 'A' && buf[11] <= 'F') *stateId += buf[11] - 'A' + 10;

        *mask |= 8;
    }

exit:
    IFile_Close(&file);

    return ret;
}

static inline bool patchLayeredFs(u64 progId, u8 *code, u32 size, u32 textSize, u32 roSize, u32 dataSize, u32 roAddress, u32 dataAddress)
{
    /* Here we look for "/luma/titles/[u64 titleID in hex, uppercase]/romfs"
       If it exists it should be a folder containing ROMFS files */

    char path[] = "/luma/titles/0000000000000000/romfs";
    progIdToStr(path + 28, progId);

    u32 archiveId = checkLumaDir(path);

    if(!archiveId) return true;

    u32 fsMountArchive = 0xFFFFFFFF,
        fsRegisterArchive = 0xFFFFFFFF,
        fsTryOpenFile = 0xFFFFFFFF,
        fsOpenFileDirectly = 0xFFFFFFFF,
        payloadOffset = 0,
        pathOffset = 0,
        pathAddress = 0xDEADCAFE;

    if(!findLayeredFsSymbols(code, textSize, &fsMountArchive, &fsRegisterArchive, &fsTryOpenFile, &fsOpenFileDirectly) ||
       !findLayeredFsPayloadOffset(code, textSize, roSize, dataSize, roAddress, dataAddress, &payloadOffset, &pathOffset, &pathAddress)) return false;

    static const char *updateRomFsMounts[] = { "rom2:",
                                               "rex:",
                                               "patch:",
                                               "ext:",
                                               "rom:" };
    u32 updateRomFsIndex;

    //Locate update RomFSes
    for(updateRomFsIndex = 0; updateRomFsIndex < sizeof(updateRomFsMounts) / sizeof(char *) - 1; updateRomFsIndex++)
    {
        u32 patternSize = strlen(updateRomFsMounts[updateRomFsIndex]);
        u8 temp[7];
        temp[0] = 0;
        memcpy(temp + 1, updateRomFsMounts[updateRomFsIndex], patternSize);
        if(memsearch(code, temp, size, patternSize + 1) != NULL) break;
    }

    //Setup the payload
    u8 *payload = code + payloadOffset;

    romfsRedirPatchSubstituted1 = *(u32 *)(code + fsOpenFileDirectly);
    romfsRedirPatchHook1 = MAKE_BRANCH(payloadOffset + (u32)&romfsRedirPatchHook1 - (u32)romfsRedirPatch, fsOpenFileDirectly + 4);
    romfsRedirPatchSubstituted2 = *(u32 *)(code + fsTryOpenFile);
    romfsRedirPatchHook2 = MAKE_BRANCH(payloadOffset + (u32)&romfsRedirPatchHook2 - (u32)romfsRedirPatch, fsTryOpenFile + 4);
    romfsRedirPatchCustomPath = pathAddress;
    romfsRedirPatchFsMountArchive = 0x100000 + fsMountArchive;
    romfsRedirPatchFsRegisterArchive = 0x100000 + fsRegisterArchive;
    romfsRedirPatchArchiveId = archiveId;
    memcpy(&romfsRedirPatchUpdateRomFsMount, updateRomFsMounts[updateRomFsIndex], 4);

    memcpy(payload, romfsRedirPatch, romfsRedirPatchSize);

    memcpy(code + pathOffset, "lf:", 3);
    memcpy(code + pathOffset + 3, path, sizeof(path));

    //Place the hooks
    *(u32 *)(code + fsOpenFileDirectly) = MAKE_BRANCH(fsOpenFileDirectly, payloadOffset);
    *(u32 *)(code + fsTryOpenFile) = MAKE_BRANCH(fsTryOpenFile, payloadOffset + 12);

    return true;
}

void patchCode(u64 progId, u16 progVer, u8 *code, u32 size, u32 textSize, u32 roSize, u32 dataSize, u32 roAddress, u32 dataAddress)
{
    bool isHomeMenu = progId == 0x0004003000008F02LL || //USA Home Menu
                      progId == 0x0004003000008202LL || //JPN Home Menu
                      progId == 0x0004003000009802LL || //EUR Home Menu
                      progId == 0x000400300000A902LL || //KOR Home Menu
                      progId == 0x000400300000A102LL || //CHN Home Menu
                      progId == 0x000400300000B102LL;   //TWN Home Menu

    if(isHomeMenu)
    {
        bool applyRegionFreePatch = true;

        switch(progId)
        {
            case 0x0004003000008F02LL: //USA Home Menu
            case 0x0004003000008202LL: //JPN Home Menu
            case 0x0004003000009802LL: //EUR Home Menu
                if(progVer <= 4) applyRegionFreePatch = false;
                break;
            case 0x000400300000A902LL: //KOR Home Menu
                if(!progVer) applyRegionFreePatch = false;
                break;
        }

        if(applyRegionFreePatch)
        {
            static const u8 pattern[] = {
                0x0A, 0x0C, 0x00, 0x10
            },
                            patch[] = {
                0x01, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1
            };

            //Patch SMDH region check
            if(!patchMemory(code, textSize,
                    pattern,
                    sizeof(pattern), -31,
                    patch,
                    sizeof(patch), 1
                )) goto error;
        }

        //Patch SMDH region check for manuals
        u32 i;
        for(i = 4; i < textSize; i += 4)
        {
            u32 *code32 = (u32 *)(code + i);
            if(code32[1] == 0xE1A0000D && (*code32 & 0xFFFFFF00) == 0x0A000000 && (code32[-1] & 0xFFFFFF00) == 0xE1110000)
                {
                    *code32 = 0xE320F000;
                    break;
                }
        }

        if(i == textSize) goto error;

        //Patch DS flashcart whitelist check
        static const u8 pattern[] = {
            0x10, 0xD1, 0xE5, 0x08, 0x00, 0x8D
        };

        u8 *temp = memsearch(code, pattern, textSize, sizeof(pattern));

        if(temp == NULL) goto error;

        u32 additive = findFunctionStart(code, (u32)(temp - code - 1));

        if(additive == 0xFFFFFFFF) goto error;

        u32 *off = (u32 *)(code + additive);

        off[0] = 0xE3A00000; //mov r0, #0
        off[1] = 0xE12FFF1E; //bx lr
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
        if(!patchMemory(code, textSize,
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
            u32 ret = patchMemory(code, textSize,
                          pattern,
                          sizeof(pattern), 0,
                          patch,
                          sizeof(patch), 2
                      );

            if(ret == 0 || (ret == 1 && progVer > 0xB)) goto error;
        }

        if(isN3DS)
        {
            u32 cpuSetting = MULTICONFIG(NEWCPU);

            if(cpuSetting != 0)
            {
                static const u8 pattern[] = {
                    0x0C, 0x00, 0x94, 0x15
                };

                u32 *off = (u32 *)memsearch(code, pattern, textSize, sizeof(pattern));

                if(off == NULL) goto error;

                //Patch N3DS CPU Clock and L2 cache setting
                *(off - 4) = *(off - 3);
                *(off - 3) = *(off - 1);
                memmove(off - 1, off, 16);
                *(off + 3) = 0xE3800000 | cpuSetting;
            }
        }

        if(progVer > 0x12)
        {
            static const u8 pattern[] = {
                0x00, 0xB1, 0x15, 0x00
            };

            u8 *roStart = code + ((textSize + 4095) & 0xFFFFF000),
               *start = memsearch(roStart, pattern, roSize, sizeof(pattern));

            if(start == NULL) goto error;

            start++;
            u8 *end;
            for(end = start + 8; *(u32 *)end != 0xCC010000; end += 8)
                if(end >= roStart + roSize - 12) goto error;

            memset(start, 0, end - start);
        }
    }

    else if((progId & ~0xF0000001ULL) == 0x0004013000001702LL) //CFG, SAFE_FIRM CFG
    {
        static const u8 pattern[] = {
            0x06, 0x46, 0x10, 0x48
        },
                        patch[] = {
            0x00, 0x26
        };

        //Disable SecureInfo signature check
        if(!patchMemory(code, textSize,
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
            if(patchMemory(code + ((textSize + 4095) & 0xFFFFF000), roSize,
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
        if(!patchMemory(code, textSize,
                pattern,
                sizeof(pattern), -9,
                patch,
                sizeof(patch), 1
            ) ||
           !patchMemory(code, textSize,
                pattern2,
                sizeof(pattern2), 1,
                patch,
                sizeof(patch), 1
            ) ||
           !patchMemory(code, textSize,
                pattern3,
                sizeof(pattern3), -2,
                patch,
                sizeof(patch), 1
            )) goto error;
    }

    else if(progId == 0x0004013000002802LL) //DLP
    {
        static const u8 pattern[] = {
            0x0C, 0xAC, 0xC0, 0xD8
        },
                        patch[] = {
            0x00, 0x00, 0x00, 0x00
        };

        static const u8 pattern2[] = {
            0x20, 0x82, 0xa8, 0x7e, 0x00, 0x28, 0x00, 0xd0, 0x01, 0x20, 0xa0, 0x77
        },
                        pattern3[] = {
            0x42
        },
                        patch2[] = {
            0xC0, 0x46 // mov r8, r8
        };

        //Patch DLP region check
        if(!patchMemory(code, textSize,
                pattern,
                sizeof(pattern), 0,
                patch,
                sizeof(patch), 1
            )) goto error;

        // Patch DLP client region check
        u8 *found = memsearch(code, pattern2, textSize, sizeof(pattern2));

        if (!patchMemory(found, textSize,
               pattern3,
               sizeof(pattern3), 1,
               patch2,
               sizeof(patch2), 1
            )) goto error;
    }

    else if((progId & ~0xF0000001ULL) == 0x0004013000001A02LL) //DSP, SAFE_FIRM DSP
    {
        static const u8 pattern[] = {
            0xE3, 0x10, 0x10, 0x80, 0xE2
        },
                        patch[] = {
            0x00, 0x20, 0xA0, 0xE3
        };

        //Patch DSP signature check
        if(!patchMemory(code, textSize,
                pattern,
                sizeof(pattern), -3,
                patch,
                sizeof(patch), 1
            )) goto error;
    }

    if(CONFIG(PATCHGAMES) && !nextGamePatchDisabled)
    {
        if(!patcherApplyCodeBpsPatch(progId, code, size)) goto error;
        if(!applyCodeIpsPatch(progId, code, size)) goto error;

        if((u32)((progId >> 0x20) & 0xFFFFFFEDULL) == 0x00040000 || isHomeMenu)
        {
            u8 mask,
               regionId,
               languageId,
               countryId,
               stateId;

            if(loadTitleLocaleConfig(progId, &mask, &regionId, &languageId, &countryId, &stateId))
                svcKernelSetState(0x10001, ((u32)stateId << 24) | ((u32)countryId << 16) | ((u32)languageId << 8) | ((u32)regionId << 4) | (u32)mask , progId);
            if(!patchLayeredFs(progId, code, size, textSize, roSize, dataSize, roAddress, dataAddress)) goto error;
        }
    }

    nextGamePatchDisabled = false;
    return;

error:
    svcBreak(USERBREAK_ASSERT);
}
