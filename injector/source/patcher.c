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
static u8 *memsearch(u8 *startPos, const void *pattern, u32 size, u32 patternSize){
    const u8 *patternc = (const u8 *)pattern;

    //Preprocessing
    u32 table[256];

    for(u32 i = 0; i < 256; ++i)
        table[i] = patternSize + 1;
    for(u32 i = 0; i < patternSize; ++i)
        table[patternc[i]] = patternSize - i;

    //Searching
    u32 j = 0;

    while(j <= size - patternSize){
        if(memcmp(patternc, startPos + j, patternSize) == 0)
            return startPos + j;
        j += table[startPos[j + patternSize]];
    }

    return NULL;
}

static u32 patch_memory(u8 *start, u32 size, const void *pattern, u32 patsize, int offset, const void *replace, u32 repsize, u32 count){
    u32 i;

    for(i = 0; i < count; i++){
        u8 *found = memsearch(start, pattern, size, patsize);
        if(found == NULL)
            break;

        memcpy(found + offset, replace, repsize);

        u32 at = (u32)(found - start);

        if(at + patsize > size) size = 0;
        else size = size - (at + patsize);

        start = found + patsize;
    }

    return i;
}

static int file_open(IFile *file, FS_ArchiveID id, const char *path, int flags){
    FS_Archive archive;
    FS_Path ppath;

    size_t len = strnlen(path, PATH_MAX);
    archive.id = id;
    archive.lowPath.type = PATH_EMPTY;
    archive.lowPath.size = 1;
    archive.lowPath.data = (u8 *)"";
    ppath.type = PATH_ASCII;
    ppath.data = path;
    ppath.size = len+1;
    return IFile_Open(file, archive, ppath, flags);
}

static int patch_secureinfo(){
    IFile file;
    Result ret;
    u64 total;

    if(secureinfo[0] == 0xFF)
        return 0;

    ret = file_open(&file, ARCHIVE_SDMC, "/SecureInfo_A", FS_OPEN_READ);
    if(R_SUCCEEDED(ret)){
        ret = IFile_Read(&file, &total, secureinfo, sizeof(secureinfo));
        IFile_Close(&file);
        if(R_SUCCEEDED(ret) && total == sizeof(secureinfo)){
            ret = file_open(&file, ARCHIVE_NAND_RW, "/sys/SecureInfo_C", FS_OPEN_WRITE | FS_OPEN_CREATE);
            if(R_SUCCEEDED(ret)){
                ret = IFile_Write(&file, &total, secureinfo, sizeof(secureinfo), FS_WRITE_FLUSH);
                IFile_Close(&file);
            }
            secureinfo[0] = 0xFF; // we repurpose this byte as status
        }
    }
    else { // get file from NAND
        ret = file_open(&file, ARCHIVE_NAND_RW, "/sys/SecureInfo_C", FS_OPEN_READ);
        if(R_SUCCEEDED(ret)){
            ret = IFile_Read(&file, &total, secureinfo, sizeof(secureinfo));
            IFile_Close(&file);
            if(R_SUCCEEDED(ret) && total == sizeof(secureinfo))
                secureinfo[0] = 0xFF;
        }
    }

    return ret;
}

static int open_config(){
    IFile file;
    Result ret;
    u64 total;

    if(config)
        return 0;

    ret = file_open(&file, ARCHIVE_SDMC, "/aurei/config.bin", FS_OPEN_READ);
    if(R_SUCCEEDED(ret)){
        ret = IFile_Read(&file, &total, (void *)&config, 3);
        IFile_Close(&file);
    }

    return ret;
}

u32 patch_code(u64 progid, u8 *code, u32 size){
    if( progid == 0x0004003000008F02LL || // USA Menu
        progid == 0x0004003000008202LL || // JPN Menu
        progid == 0x0004003000009802LL || // EUR Menu
        progid == 0x000400300000A102LL || // CHN Menu
        progid == 0x000400300000A902LL || // KOR Menu
        progid == 0x000400300000B102LL    // TWN Menu
     ){
        static const u8 regionFreePattern[] = {
            0x00, 0x00, 0x55, 0xE3, 0x01, 0x10, 0xA0, 0xE3
        };
        static const u8 regionFreePatch[] = {
            0x01, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1
        };

        patch_memory(code, size, 
            regionFreePattern, 
            sizeof(regionFreePattern), -16, 
            regionFreePatch, 
            sizeof(regionFreePatch), 1
        );
    }
    else if(progid == 0x0004013000002C02LL){ // NIM
        static const u8 blockAutoUpdatesPattern[] = {
            0x25, 0x79, 0x0B, 0x99
        };
        static const u8 blockAutoUpdatesPatch[] = {
            0xE3, 0xA0
        };
        static const u8 blockEShopUpdateCheckPattern[] = {
            0x30, 0xB5, 0xF1, 0xB0
        };
        static const u8 blockEShopUpdateCheckPatch[] = {
            0x00, 0x20, 0x08, 0x60, 0x70, 0x47
        };
        static const u8 countryRespPattern[] = {
            0x01, 0x20, 0x01, 0x90, 0x22, 0x46, 0x06, 0x9B
        };
        static const char countryRespPatchModel[] = {
            0x06, 0x9A, 0x03, 0x20, 0x90, 0x47, 0x55, 0x21, 0x01, 0x70, 0x53, 0x21, 0x41, 0x70, 0x00, 0x21, 
            0x81, 0x70, 0x60, 0x61, 0x00, 0x20
        };
        const char *country;
        char countryRespPatch[sizeof(countryRespPatchModel)];

        patch_memory(code, size, 
            blockAutoUpdatesPattern, 
            sizeof(blockAutoUpdatesPattern), 0, 
            blockAutoUpdatesPatch, 
            sizeof(blockAutoUpdatesPatch), 1
        );
        patch_memory(code, size, 
            blockEShopUpdateCheckPattern, 
            sizeof(blockEShopUpdateCheckPattern), 0, 
            blockEShopUpdateCheckPatch, 
            sizeof(blockEShopUpdateCheckPatch), 1
        );
        if(R_SUCCEEDED(patch_secureinfo())){
            switch(secureinfo[0x100]){
                case 1: country = "US"; break;
                case 2: country = "GB"; break; // sorry rest-of-Europe, you have to change this
                case 3: country = "AU"; break;
                case 4: country = "CN"; break;
                case 5: country = "KR"; break;
                case 6: country = "TW"; break;
                default: case 0: country = "JP"; break;
            }

            // patch XML response Country
            memcpy(countryRespPatch, 
                countryRespPatchModel, 
                sizeof(countryRespPatchModel)
            );
            countryRespPatch[6] = country[0];
            countryRespPatch[10] = country[1];
            patch_memory(code, size, 
                countryRespPattern, 
                sizeof(countryRespPattern), 0, 
                countryRespPatch, 
                sizeof(countryRespPatch), 1
            );
        }
    }
    else if(
             progid == 0x0004001000021000LL || // USA MSET
             progid == 0x0004001000020000LL || // JPN MSET
             progid == 0x0004001000022000LL || // EUR MSET
             progid == 0x0004001000026000LL || // CHN MSET
             progid == 0x0004001000027000LL || // KOR MSET
             progid == 0x0004001000028000LL    // TWN MSET
    ){
        if(R_SUCCEEDED(open_config()) && ((config >> 5) & 0x1)){
            static const u16 VerPattern[] = u"Ver.";
            const u32 currentFirm = ((config >> 12) & 0x1);
            const u32 currentNand = ((config >> 13) & 0x3);

            patch_memory(code, size,
                VerPattern,
                sizeof(VerPattern) - sizeof(u16), 0,
                currentNand ? ((currentNand == 1) ? ((currentFirm == 1) ? u" Emu" : u"Emu9") : u"Emu2") :
                              ((currentFirm == 1) ? u" Sys" : u"Sys9"),
                sizeof(VerPattern) - sizeof(u16), 1
            );
        }
    }
    else if (progid == 0x0004013000008002LL){ // NS
        static const u8 stopCartUpdatesPattern[] = {
            0x0C, 0x18, 0xE1, 0xD8
        };
        static const u8 stopCartUpdatesPatch[] = {
            0x0B, 0x18, 0x21, 0xC8
        };

        patch_memory(code, size, 
            stopCartUpdatesPattern, 
            sizeof(stopCartUpdatesPattern), 0, 
            stopCartUpdatesPatch,
            sizeof(stopCartUpdatesPatch), 2
        );
    }
    else if(progid == 0x0004013000001702LL){ // CFG
        static const u8 secureinfoSigCheckPattern[] = {
            0x06, 0x46, 0x10, 0x48, 0xFC
        };
        static const u8 secureinfoSigCheckPatch[] = {
            0x00, 0x26
        };
        static const u16 secureinfoFilenamePattern[] = u"SecureInfo_";
        static const u16 secureinfoFilenamePatch[] = u"C";

        // disable SecureInfo signature check
        patch_memory(code, size, 
            secureinfoSigCheckPattern, 
            sizeof(secureinfoSigCheckPattern), 0, 
            secureinfoSigCheckPatch, 
            sizeof(secureinfoSigCheckPatch), 1
        );
        if(R_SUCCEEDED(patch_secureinfo())){
            // use SecureInfo_C
            patch_memory(code, size, 
                secureinfoFilenamePattern, 
                sizeof(secureinfoFilenamePattern) - sizeof(u16),
                sizeof(secureinfoFilenamePattern) - sizeof(u16), 
                secureinfoFilenamePatch, 
                sizeof(secureinfoFilenamePatch) - sizeof(u16), 2
            );
        }
    }

    return 0;
}
