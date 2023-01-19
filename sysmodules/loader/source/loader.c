#include <3ds.h>
#include "memory.h"
#include "patcher.h"
#include "paslr.h"
#include "ifile.h"
#include "util.h"
#include "hbldr.h"

extern u32 config, multiConfig, bootConfig;
extern bool isN3DS, isSdMode, nextGamePatchDisabled;

static u64 g_cached_programHandle;
static ExHeader_Info g_exheaderInfo;

typedef struct ContentPath {
    u32 contentType;
    char fileName[8]; // for exefs
} ContentPath;

static const ContentPath codeContentPath = {
    .contentType = 1, // ExeFS (with code)
    .fileName = ".code", // last 3 bytes have to be 0, but this is guaranteed here.
};

typedef struct prog_addrs_t
{
    u32 text_addr;
    u32 text_size;
    u32 ro_addr;
    u32 ro_size;
    u32 data_addr;
    u32 data_size;
    u32 total_size;
} prog_addrs_t;

static int lzss_decompress(u8 *end)
{
    unsigned int v1; // r1@2
    u8 *v2; // r2@2
    u8 *v3; // r3@2
    u8 *v4; // r1@2
    char v5; // r5@4
    char v6; // t1@4
    signed int v7; // r6@4
    int v9; // t1@7
    u8 *v11; // r3@8
    int v12; // r12@8
    int v13; // t1@8
    int v14; // t1@8
    unsigned int v15; // r7@8
    int v16; // r12@8
    int ret;

    ret = 0;
    if ( end )
    {
        v1 = *((u32 *)end - 2);
        v2 = &end[*((u32 *)end - 1)];
        v3 = &end[-(v1 >> 24)];
        v4 = &end[-(v1 & 0xFFFFFF)];
        while ( v3 > v4 )
        {
            v6 = *(v3-- - 1);
            v5 = v6;
            v7 = 8;
            while ( 1 )
            {
                if ( (v7-- < 1) )
                    break;
                if ( v5 & 0x80 )
                {
                    v13 = *(v3 - 1);
                    v11 = v3 - 1;
                    v12 = v13;
                    v14 = *(v11 - 1);
                    v3 = v11 - 1;
                    v15 = ((v14 | (v12 << 8)) & 0xFFFF0FFF) + 2;
                    v16 = v12 + 32;
                    do
                    {
                        ret = v2[v15];
                        *(v2-- - 1) = ret;
                        v16 -= 16;
                    }
                    while ( !(v16 < 0) );
                }
                else
                {
                    v9 = *(v3-- - 1);
                    ret = v9;
                    *(v2-- - 1) = v9;
                }
                v5 *= 2;
                if ( v3 <= v4 )
                    return ret;
            }
        }
    }
    return ret;
}

static Result allocateProgramMemoryWrapper(prog_addrs_t *mapped, const ExHeader_Info *exhi, const prog_addrs_t *vaddr)
{
    memcpy(mapped, vaddr, sizeof(prog_addrs_t));
    mapped->text_addr = 0x10000000;
    mapped->ro_addr = mapped->text_addr + (mapped->text_size << 12);
    mapped->data_addr = mapped->ro_addr + (mapped->ro_size << 12);
    return allocateProgramMemory(exhi, mapped->text_addr, mapped->total_size << 12);
}

static Result loadCode(const ExHeader_Info *exhi, u64 programHandle, const prog_addrs_t *mapped)
{
    IFile file;
    FS_Path archivePath;
    FS_Path filePath;
    u64 size;
    u64 total;

    u64 titleId = exhi->aci.local_caps.title_id;
    const ExHeader_CodeSetInfo *csi = &exhi->sci.codeset_info;
    bool isCompressed = csi->flags.compress_exefs_code;

    if(!CONFIG(PATCHGAMES) || !loadTitleCodeSection(titleId, (u8 *)mapped->text_addr, (u64)mapped->total_size << 12))
    {
        archivePath.type = PATH_BINARY;
        archivePath.data = &programHandle;
        archivePath.size = sizeof(programHandle);

        filePath.type = PATH_BINARY;
        filePath.data = &codeContentPath;
        filePath.size = sizeof(codeContentPath);

        assertSuccess(IFile_Open(&file, ARCHIVE_SAVEDATA_AND_CONTENT2, archivePath, filePath, FS_OPEN_READ));
        assertSuccess(IFile_GetSize(&file, &size));

        // check size
        if (size > (u64)mapped->total_size << 12)
        {
            IFile_Close(&file);
            return 0xC900464F;
        }

        // read code
        assertSuccess(IFile_Read(&file, &total, (void *)mapped->text_addr, size));
        IFile_Close(&file); // done reading

        // decompress
        if (isCompressed)
            lzss_decompress((u8 *)mapped->text_addr + size);
    }

    patchCode(titleId, csi->flags.remaster_version, (u8 *)mapped->text_addr, mapped->total_size << 12, csi->text.size, csi->rodata.size, csi->data.size, csi->rodata.address, csi->data.address);

    return 0;
}

static u32 plgldrRefcount = 0;
static Handle plgldrHandle = 0;

Result plgldrInit(void)
{
    Result res;
    if (AtomicPostIncrement(&plgldrRefcount)) return 0;

    for(res = 0xD88007FA; res == (Result)0xD88007FA; svcSleepThread(500 * 1000LL)) {
        res = svcConnectToPort(&plgldrHandle, "plg:ldr");
        if(R_FAILED(res) && res != (Result)0xD88007FA) {
            AtomicDecrement(&plgldrRefcount);
            return res;
        }
    }

    return 0;
}

void plgldrExit(void)
{
    if (AtomicDecrement(&plgldrRefcount)) return;
        svcCloseHandle(plgldrHandle);
}

// Try to load a plugin for the game
static Result PLGLDR_LoadPlugin(Handle *process)
{
    // Special case handling: games rebooting the 3DS on old models
    if (!isN3DS && g_exheaderInfo.aci.local_caps.core_info.o3ds_system_mode > 0)
    {
        // Check if the plugin loader is enabled, otherwise skip the loading part
        s64 out;

        svcGetSystemInfo(&out, 0x10000, 0x180);
        if ((out & 1) == 0)
            return 0;
    }

    u32* cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(1, 0, 2);
    cmdbuf[1] = IPC_Desc_SharedHandles(1);
    cmdbuf[2] = *process;
    return svcSendSyncRequest(plgldrHandle);
}

static inline bool IsHioId(u64 id)
{
    // FS loads HIO titles at boot when it can. For HIO titles, title/programId and "program handle"
    // are the same thing, although some of them can be aliased with their "real" titleId (i.e. in ExHeader).

    if (id >> 32 == 0xFFFF0000u)
        return true;
    else
        return R_LEVEL(FSREG_CheckHostLoadId(id)) == RL_SUCCESS; // check if this is an alias to an HIO-loaded title
}

static Result GetProgramInfoImpl(ExHeader_Info *exheaderInfo, u64 programHandle)
{
    Result res;
    TRY(IsHioId(programHandle) ? FSREG_GetProgramInfo(exheaderInfo, 1, programHandle) : PXIPM_GetProgramInfo(exheaderInfo, programHandle));

    // Tweak 3dsx placeholder title exheaderInfo
    if (hbldrIs3dsxTitle(exheaderInfo->aci.local_caps.title_id))
    {
        hbldrPatchExHeaderInfo(exheaderInfo);
    }
    else
    {
        u64 originalTitleId = exheaderInfo->aci.local_caps.title_id;
        if(CONFIG(PATCHGAMES) && loadTitleExheaderInfo(exheaderInfo->aci.local_caps.title_id, exheaderInfo))
            exheaderInfo->aci.local_caps.title_id = originalTitleId;
    }

    return res;
}

static Result GetProgramInfo(u64 programHandle)
{
    Result res = 0;
    u64 cachedTitleId = g_exheaderInfo.aci.local_caps.title_id;

    if (programHandle != g_cached_programHandle || hbldrIs3dsxTitle(cachedTitleId))
    {
        res = GetProgramInfoImpl(&g_exheaderInfo, programHandle);
        g_cached_programHandle = R_SUCCEEDED(res) ? programHandle : 0;
    }

    return res;
}

static Result LoadProcessImpl(Handle *outProcessHandle, const ExHeader_Info *exhi, u64 programHandle)
{
    const ExHeader_CodeSetInfo *csi = &exhi->sci.codeset_info;

    Result res = 0;
    u32 dummy;
    prog_addrs_t mapped;
    prog_addrs_t vaddr;
    Handle codeset;
    CodeSetInfo codesetinfo;
    u32 dataMemSize;

    u32 region = 0;
    u32 count;
    for (count = 0; count < 28; count++)
    {
        u32 desc = exhi->aci.kernel_caps.descriptors[count];
        if (0x1FE == desc >> 23)
            region = desc & 0xF00;
    }
    if (region == 0)
            return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, 1, 2);

    // allocate process memory
    vaddr.text_addr = csi->text.address;
    vaddr.text_size = (csi->text.size + 4095) >> 12;
    vaddr.ro_addr = csi->rodata.address;
    vaddr.ro_size = (csi->rodata.size + 4095) >> 12;
    vaddr.data_addr = csi->data.address;
    vaddr.data_size = (csi->data.size + 4095) >> 12;
    dataMemSize = (csi->data.size + csi->bss_size + 4095) >> 12;
    vaddr.total_size = vaddr.text_size + vaddr.ro_size + vaddr.data_size;
    TRY(allocateProgramMemoryWrapper(&mapped, exhi, &vaddr));

    // load code
    u64 titleId = exhi->aci.local_caps.title_id;
    if (R_SUCCEEDED(res = loadCode(exhi, programHandle, &mapped)))
    {
        u32     *code = (u32 *)sharedAddr.text_addr;
        bool    isHomebrew = code[0] == 0xEA000006 && code[8] == 0xE1A0400E;

        memcpy(&codesetinfo.name, csi->name, 8);
        codesetinfo.program_id = titleId;
        codesetinfo.text_addr = vaddr.text_addr;
        codesetinfo.text_size = vaddr.text_size;
        codesetinfo.text_size_total = vaddr.text_size;
        codesetinfo.ro_addr = vaddr.ro_addr;
        codesetinfo.ro_size = vaddr.ro_size;
        codesetinfo.ro_size_total = vaddr.ro_size;
        codesetinfo.rw_addr = vaddr.data_addr;
        codesetinfo.rw_size = vaddr.data_size;
        codesetinfo.rw_size_total = dataMemSize;
        res = svcCreateCodeSet(&codeset, &codesetinfo, (void *)mapped.text_addr, (void *)mapped.ro_addr, (void *)mapped.data_addr);
        if (R_SUCCEEDED(res))
        {
            // There are always 28 descriptors
            res = svcCreateProcess(outProcessHandle, codeset, exhi->aci.kernel_caps.descriptors, count);
            svcCloseHandle(codeset);
            res = R_SUCCEEDED(res) ? 0 : res;
            
            // check for plugin
            if (!res && !isHomebrew && ((u32)((titleId >> 0x20) & 0xFFFFFFEDULL) == 0x00040000))
            {
                assertSuccess(plgldrInit());
                assertSuccess(PLGLDR_LoadPlugin(process));
                plgldrExit();
            }
        }
    }

    svcControlMemory(&dummy, mapped.text_addr, 0, mapped.total_size << 12, MEMOP_FREE, 0);
    return res;
}

static Result LoadProcess(Handle *process, u64 programHandle)
{
    Result res = 0;
    TRY(GetProgramInfo(programHandle));

    if (hbldrIs3dsxTitle(g_exheaderInfo.aci.local_caps.title_id))
        return hbldrLoadProcess(process, &g_exheaderInfo);
    else
        return LoadProcessImpl(process, &g_exheaderInfo, programHandle);
}

static Result RegisterProgram(u64 *programHandle, FS_ProgramInfo *title, FS_ProgramInfo *update)
{
    Result res;
    u64 titleId;

    titleId = title->programId;
    if (IsHioId(titleId))
    {
        if ((title->mediaType != update->mediaType) || (titleId != update->programId))
            panic(1);

        TRY(FSREG_LoadProgram(programHandle, title));

        if (!IsHioId(*programHandle)) // double check this is indeed HIO
            panic(2);
    }
    else
    {
        TRY(PXIPM_RegisterProgram(programHandle, title, update));
        if (IsHioId(*programHandle)) // double check this is indeed *not* HIO
            panic(0);
    }

    return res;
}

static Result UnregisterProgram(u64 programHandle)
{
    return IsHioId(programHandle) ? FSREG_UnloadProgram(programHandle) : PXIPM_UnregisterProgram(programHandle);
}

void loaderHandleCommands(void *ctx)
{
    (void)ctx;
    FS_ProgramInfo title;
    FS_ProgramInfo update;
    u32* cmdbuf;
    u16 cmdid;
    int res;
    Handle handle;
    u64 programHandle;

    cmdbuf = getThreadCommandBuffer();
    cmdid = cmdbuf[0] >> 16;
    res = 0;
    switch (cmdid)
    {
        case 1: // LoadProcess
            memcpy(&programHandle, &cmdbuf[1], 8);
            handle = 0;
            cmdbuf[1] = LoadProcess(&handle, programHandle);
            cmdbuf[0] = IPC_MakeHeader(1, 1, 2);
            cmdbuf[2] = IPC_Desc_MoveHandles(1);
            cmdbuf[3] = handle;
            break;
        case 2: // RegisterProgram
            memcpy(&title, &cmdbuf[1], sizeof(FS_ProgramInfo));
            memcpy(&update, &cmdbuf[5], sizeof(FS_ProgramInfo));
            cmdbuf[1] = RegisterProgram(&programHandle, &title, &update);
            cmdbuf[0] = IPC_MakeHeader(2, 3, 0);
            memcpy(&cmdbuf[2], &programHandle, 8);
            break;
        case 3: // UnregisterProgram
            memcpy(&programHandle, &cmdbuf[1], 8);

            if (g_cached_programHandle == programHandle)
                g_cached_programHandle = 0;
            cmdbuf[1] = UnregisterProgram(programHandle);
            cmdbuf[0] = IPC_MakeHeader(3, 1, 0);
            break;
        case 4: // GetProgramInfo
            memcpy(&programHandle, &cmdbuf[1], 8);
            res = GetProgramInfo(programHandle);
            cmdbuf[0] = IPC_MakeHeader(4, 1, 2);
            cmdbuf[1] = res;
            cmdbuf[2] = IPC_Desc_StaticBuffer(sizeof(ExHeader_Info), 0); //0x1000002;
            cmdbuf[3] = (u32)&g_exheaderInfo; // official Loader makes a copy here, but this is isn't necessary
            break;
        // Custom
        case 0x100: // DisableNextGamePatch
            nextGamePatchDisabled = true;
            cmdbuf[0] = IPC_MakeHeader(0x100, 1, 0);
            cmdbuf[1] = MAKERESULT(RL_SUCCESS, RS_SUCCESS, RM_COMMON, RD_SUCCESS);
            break;
        default: // error
            cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
            cmdbuf[1] = 0xD900182F;
            break;
    }
}
