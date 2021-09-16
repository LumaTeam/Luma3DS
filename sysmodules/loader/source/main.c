#include <3ds.h>
#include "memory.h"
#include "patcher.h"
#include "ifile.h"
#include "util.h"
#include "loader.h"
#include "service_manager.h"

u32 config, multiConfig, bootConfig;
bool isN3DS, isSdMode, nextGamePatchDisabled;

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

    if(svcGetSystemInfo(&out, 0x20000, 0) != 1) panic(0xDEADCAFE);

    svcGetSystemInfo(&out, 0x10000, 3);
    config = (u32)out;
    svcGetSystemInfo(&out, 0x10000, 4);
    multiConfig = (u32)out;
    svcGetSystemInfo(&out, 0x10000, 5);
    bootConfig = (u32)out;

    svcGetSystemInfo(&out, 0x10000, 0x201);
    isN3DS = (bool)out;
    svcGetSystemInfo(&out, 0x10000, 0x203);
    isSdMode = (bool)out;
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
    { NULL },
};

static const ServiceManagerNotificationEntry notifications[] = {
    { 0x000, NULL },
};

int main(void)
{
    nextGamePatchDisabled = false;
    loadCFWInfo();
    assertSuccess(ServiceManager_Run(services, notifications, NULL));
    return 0;
}
