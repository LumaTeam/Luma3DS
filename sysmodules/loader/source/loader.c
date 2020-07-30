#include <3ds.h>
#include "memory.h"
#include "patcher.h"
#include "ifile.h"
#include "util.h"
#include "hbldr.h"
#include "luma_shared_config.h"

extern u32 config, multiConfig, bootConfig;
extern bool isN3DS, isSdMode;

static u8 g_ret_buf[sizeof(ExHeader_Info)];
static u64 g_cached_programHandle;
static ExHeader_Info g_exheaderInfo;

const char CODE_PATH[] = {0x01, 0x00, 0x00, 0x00, 0x2E, 0x63, 0x6F, 0x64, 0x65, 0x00, 0x00, 0x00};

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

static inline bool hbldrIs3dsxTitle(u64 tid)
{
    return Luma_SharedConfig->use_hbldr && tid == Luma_SharedConfig->hbldr_3dsx_tid;
}

static Result allocateSharedMem(prog_addrs_t *shared, prog_addrs_t *vaddr, int flags)
{
    u32 dummy;

    memcpy(shared, vaddr, sizeof(prog_addrs_t));
    shared->text_addr = 0x10000000;
    shared->ro_addr = shared->text_addr + (shared->text_size << 12);
    shared->data_addr = shared->ro_addr + (shared->ro_size << 12);
    return svcControlMemory(&dummy, shared->text_addr, 0, shared->total_size << 12, (flags & 0xF00) | MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);
}

static Result loadCode(u64 titleId, prog_addrs_t *shared, u64 programHandle, int isCompressed)
{
    IFile file;
    FS_Path archivePath;
    FS_Path filePath;
    u64 size;
    u64 total;

    if(!CONFIG(PATCHGAMES) || !loadTitleCodeSection(titleId, (u8 *)shared->text_addr, (u64)shared->total_size << 12))
    {
        archivePath.type = PATH_BINARY;
        archivePath.data = &programHandle;
        archivePath.size = 8;

        filePath.type = PATH_BINARY;
        filePath.data = CODE_PATH;
        filePath.size = sizeof(CODE_PATH);
        if (R_FAILED(IFile_Open(&file, ARCHIVE_SAVEDATA_AND_CONTENT2, archivePath, filePath, FS_OPEN_READ)))
            svcBreak(USERBREAK_ASSERT);

        // get file size
        assertSuccess(IFile_GetSize(&file, &size));

        // check size
        if (size > (u64)shared->total_size << 12)
        {
            IFile_Close(&file);
            return 0xC900464F;
        }

        // read code
        assertSuccess(IFile_Read(&file, &total, (void *)shared->text_addr, size));
        IFile_Close(&file); // done reading

        // decompress
        if (isCompressed)
            lzss_decompress((u8 *)shared->text_addr + size);
    }

    ExHeader_CodeSetInfo *csi = &g_exheaderInfo.sci.codeset_info;

    patchCode(titleId, csi->flags.remaster_version, (u8 *)shared->text_addr, shared->total_size << 12, csi->text.size, csi->rodata.size, csi->data.size, csi->rodata.address, csi->data.address);

    return 0;
}

static Result GetProgramInfo(ExHeader_Info *exheaderInfo, u64 programHandle)
{
    Result res = 0;

    if (programHandle >> 32 == 0xFFFF0000)
        res = FSREG_GetProgramInfo(exheaderInfo, 1, programHandle);
    else
    {
        res = FSREG_CheckHostLoadId(programHandle);
        //if ((res >= 0 && (unsigned)res >> 27) || (res < 0 && ((unsigned)res >> 27)-32))
        //so use PXIPM if FSREG fails OR returns "info", is the second condition a bug?
        if (R_FAILED(res) || (R_SUCCEEDED(res) && R_LEVEL(res) != RL_SUCCESS))
        {
            TRY(PXIPM_GetProgramInfo(exheaderInfo, programHandle));
        }
        else
        {
            TRY(FSREG_GetProgramInfo(exheaderInfo, 1, programHandle));
        }
    }

    // Tweak 3dsx placeholder title exheaderInfo
    if (hbldrIs3dsxTitle(exheaderInfo->aci.local_caps.title_id))
    {
        assertSuccess(hbldrInit());
        HBLDR_PatchExHeaderInfo(exheaderInfo);
        hbldrExit();
    }
    else
    {
        u64 originaltitleId = exheaderInfo->aci.local_caps.title_id;
        if(CONFIG(PATCHGAMES) && loadTitleExheaderInfo(exheaderInfo->aci.local_caps.title_id, exheaderInfo))
            exheaderInfo->aci.local_caps.title_id = originaltitleId;
    }

    return res;
}

static Result LoadProcess(Handle *process, u64 programHandle)
{
    Result res;
    int count;
    u32 flags;
    u32 desc;
    u32 dummy;
    prog_addrs_t sharedAddr;
    prog_addrs_t vaddr;
    Handle codeset;
    CodeSetInfo codesetinfo;
    u32 dataMemSize;
    u64 titleId;

    // make sure the cached info corrosponds to the current programHandle
    if (g_cached_programHandle != programHandle || hbldrIs3dsxTitle(g_exheaderInfo.aci.local_caps.title_id))
    {
        res = GetProgramInfo(&g_exheaderInfo, programHandle);
        g_cached_programHandle = programHandle;
        if (R_FAILED(res))
        {
            g_cached_programHandle = 0;
            return res;
        }
    }

    // get kernel flags
    flags = 0;
    for (count = 0; count < 28; count++)
    {
        desc = g_exheaderInfo.aci.kernel_caps.descriptors[count];
        if (0x1FE == desc >> 23)
            flags = desc & 0xF00;
    }
    if (flags == 0)
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, 1, 2);

    // check for 3dsx process
    titleId = g_exheaderInfo.aci.local_caps.title_id;
    ExHeader_CodeSetInfo *csi = &g_exheaderInfo.sci.codeset_info;

    if (hbldrIs3dsxTitle(titleId))
    {
        assertSuccess(hbldrInit());
        assertSuccess(HBLDR_LoadProcess(&codeset, csi->text.address, flags & 0xF00, titleId, csi->name));
        res = svcCreateProcess(process, codeset, g_exheaderInfo.aci.kernel_caps.descriptors, count);
        svcCloseHandle(codeset);
        hbldrExit();
        return res;
    }

    // allocate process memory
    vaddr.text_addr = csi->text.address;
    vaddr.text_size = (csi->text.size + 4095) >> 12;
    vaddr.ro_addr = csi->rodata.address;
    vaddr.ro_size = (csi->rodata.size + 4095) >> 12;
    vaddr.data_addr = csi->data.address;
    vaddr.data_size = (csi->data.size + 4095) >> 12;
    dataMemSize = (csi->data.size + csi->bss_size + 4095) >> 12;
    vaddr.total_size = vaddr.text_size + vaddr.ro_size + vaddr.data_size;
    TRY(allocateSharedMem(&sharedAddr, &vaddr, flags));

    // load code
    if (R_SUCCEEDED(res = loadCode(titleId, &sharedAddr, programHandle, csi->flags.compress_exefs_code)))
    {
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
        res = svcCreateCodeSet(&codeset, &codesetinfo, (void *)sharedAddr.text_addr, (void *)sharedAddr.ro_addr, (void *)sharedAddr.data_addr);
        if (R_SUCCEEDED(res))
        {
            res = svcCreateProcess(process, codeset, g_exheaderInfo.aci.kernel_caps.descriptors, count);
            svcCloseHandle(codeset);
            res = R_SUCCEEDED(res) ? 0 : res;
        }
    }

    svcControlMemory(&dummy, sharedAddr.text_addr, 0, sharedAddr.total_size << 12, MEMOP_FREE, 0);
    return res;
}

static Result RegisterProgram(u64 *programHandle, FS_ProgramInfo *title, FS_ProgramInfo *update)
{
    Result res;
    u64 titleId;

    titleId = title->programId;
    if (titleId >> 32 != 0xFFFF0000)
    {
        res = FSREG_CheckHostLoadId(titleId);
        //if ((res >= 0 && (unsigned)res >> 27) || (res < 0 && ((unsigned)res >> 27)-32))
        if (R_FAILED(res) || (R_SUCCEEDED(res) && R_LEVEL(res) != RL_SUCCESS))
        {
            TRY(PXIPM_RegisterProgram(programHandle, title, update));
            if (*programHandle >> 32 != 0xFFFF0000)
            {
                res = FSREG_CheckHostLoadId(*programHandle);
                //if ((res >= 0 && (unsigned)res >> 27) || (res < 0 && ((unsigned)res >> 27)-32))
                if (R_FAILED(res) || (R_SUCCEEDED(res) && R_LEVEL(res) != RL_SUCCESS))
                {
                    return 0;
                }
            }
            panic(0);
        }
    }

    if ((title->mediaType != update->mediaType) || (titleId != update->programId))
        panic(1);

    TRY(FSREG_LoadProgram(programHandle, title));
    if (*programHandle >> 32 == 0xFFFF0000)
        return 0;

    res = FSREG_CheckHostLoadId(*programHandle);
    //if ((res >= 0 && (unsigned)res >> 27) || (res < 0 && ((unsigned)res >> 27)-32))
    if (R_FAILED(res) || (R_SUCCEEDED(res) && R_LEVEL(res) != RL_SUCCESS))
        panic(2);

    return res;
}

static Result UnregisterProgram(u64 programHandle)
{
    Result res;

    if (programHandle >> 32 == 0xFFFF0000)
        return FSREG_UnloadProgram(programHandle);
    else
    {
        res = FSREG_CheckHostLoadId(programHandle);
        //if ((res >= 0 && (unsigned)res >> 27) || (res < 0 && ((unsigned)res >> 27)-32))
        if (R_FAILED(res) || (R_SUCCEEDED(res) && R_LEVEL(res) != RL_SUCCESS))
            return PXIPM_UnregisterProgram(programHandle);
        else
            return FSREG_UnloadProgram(programHandle);
    }
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
            if (programHandle != g_cached_programHandle || hbldrIs3dsxTitle(g_exheaderInfo.aci.local_caps.title_id))
            {
                res = GetProgramInfo(&g_exheaderInfo, programHandle);
                g_cached_programHandle = R_SUCCEEDED(res) ? programHandle : 0;
            }
            memcpy(&g_ret_buf, &g_exheaderInfo, sizeof(ExHeader_Info));
            cmdbuf[0] = IPC_MakeHeader(4, 1, 2);
            cmdbuf[1] = res;
            cmdbuf[2] = IPC_Desc_StaticBuffer(sizeof(ExHeader_Info), 0); //0x1000002;
            cmdbuf[3] = (u32)&g_ret_buf;
            break;
        default: // error
            cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
            cmdbuf[1] = 0xD900182F;
            break;
    }
}
