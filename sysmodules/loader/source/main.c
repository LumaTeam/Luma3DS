#include <3ds.h>
#include <assert.h>
#include "memory.h"
#include "patcher.h"
#include "ifile.h"
#include "util.h"
#include "loader.h"
#include "service_manager.h"
#include "3dsx.h"
#include "hbldr.h"

u32 config, multiConfig, bootConfig;
bool isN3DS, isSdMode, nextGamePatchDisabled, isLumaWithKext;

// MAKE SURE fsreg has been init before calling this
static Result fsldrPatchPermissions(void)
{
    u32 pid;
    Result res = 0;
    FS_ProgramInfo info;
    ExHeader_Arm11StorageInfo storageInfo = {
        .fs_access_info = FSACCESS_NANDRW | FSACCESS_NANDRO_RO | FSACCESS_SDMC_RW,
    };

    info.programId = 0x0004013000001302LL; // loader PID
    info.mediaType = MEDIATYPE_NAND;
    TRY(svcGetProcessId(&pid, CUR_PROCESS_HANDLE));
    return FSREG_Register(pid, 0xFFFF000000000000LL, &info, &storageInfo);
}

static inline void loadCFWInfo(void)
{
    s64 out;
    u64 hbldrTid = 0;

    isLumaWithKext = svcGetSystemInfo(&out, 0x20000, 0) == 1;
    if (isLumaWithKext)
    {
        svcGetSystemInfo(&out, 0x10000, 3);
        config = (u32)out;
        svcGetSystemInfo(&out, 0x10000, 4);
        multiConfig = (u32)out;
        svcGetSystemInfo(&out, 0x10000, 5);
        bootConfig = (u32)out;

        svcGetSystemInfo(&out, 0x10000, 0x100);
        hbldrTid = (u64)out;
        svcGetSystemInfo(&out, 0x10000, 0x201);
        isN3DS = (bool)out;
        svcGetSystemInfo(&out, 0x10000, 0x203);
        isSdMode = (bool)out;
    }
    else
    {
        // Try to support non-Luma or builds where kext is disabled
        s64 numKips = 0;
        svcGetSystemInfo(&numKips, 26, 0);

        if (numKips >= 6)
            panic(0xDEADCAFE);

#ifndef BUILD_FOR_EXPLOIT_DEV
        // Most options 0, except select ones
        config = BIT(PATCHVERSTRING) | BIT(PATCHGAMES) | BIT(LOADEXTFIRMSANDMODULES);
#else
        config = 0;
#endif
        multiConfig = 0;
        bootConfig = 0;
        isN3DS = OS_KernelConfig->app_memtype >= 6;
        isSdMode = true;
    }

    hbldrTid = hbldrTid == 0 ? HBLDR_DEFAULT_3DSX_TID : hbldrTid;
    Luma_SharedConfig->hbldr_3dsx_tid = hbldrTid;
    Luma_SharedConfig->selected_hbldr_3dsx_tid = hbldrTid;
    Luma_SharedConfig->use_hbldr = true;
}

void __ctru_exit(int rc) { (void)rc; } // needed to avoid linking error

// this is called after main exits
void __wrap_exit(int rc)
{
    (void)rc;
    // Not supposed to terminate... kernel will clean up the handles if it does happen anyway
    svcExitProcess();
}

void __sync_init();
void __libc_init_array(void);

// called before main
void initSystem(void)
{
    __sync_init();
    loadCFWInfo();

    Result res;
    for(res = 0xD88007FA; res == (Result)0xD88007FA; svcSleepThread(500 * 1000LL))
    {
        res = srvInit();
        if(R_FAILED(res) && res != (Result)0xD88007FA)
            panic(res);
    }

    assertSuccess(fsRegInit());
    assertSuccess(fsldrPatchPermissions());

    //fsldrInit();
    assertSuccess(srvGetServiceHandle(fsGetSessionHandle(), "fs:LDR"));

    // Hackjob
    assertSuccess(FSUSER_InitializeWithSdkVersion(*fsGetSessionHandle(), 0x70200C8));
    assertSuccess(FSUSER_SetPriority(0));

    assertSuccess(pxiPmInit());

    //__libc_init_array();
}

static const ServiceManagerServiceEntry services[] = {
    { "Loader", 2, loaderHandleCommands, false },
    { "hb:ldr", 2, hbldrHandleCommands,  true  },
    { NULL },
};

static const ServiceManagerNotificationEntry notifications[] = {
    { 0x000, NULL },
};

static u8 CTR_ALIGN(4) staticBufferForHbldr[0x400];
static_assert(ARGVBUF_SIZE > 2 * PATH_MAX, "Wrong 3DSX argv buffer size");

int main(void)
{
    nextGamePatchDisabled = false;

    // Loader doesn't use any input static buffer, so we should be fine
    u32 *sbuf = getThreadStaticBuffers();
    sbuf[0] = IPC_Desc_StaticBuffer(sizeof(staticBufferForHbldr), 0);
    sbuf[1] = (u32)staticBufferForHbldr;
    sbuf[2] = IPC_Desc_StaticBuffer(sizeof(staticBufferForHbldr), 1);
    sbuf[3] = (u32)staticBufferForHbldr;

    assertSuccess(ServiceManager_Run(services, notifications, NULL));
    return 0;
}
