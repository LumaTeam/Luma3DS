#include <3ds.h>
#include <string.h>
#include "firmlaunch.h"
#include "termination.h"
#include "task_runner.h"
#include "util.h"

static void *const g_firmlaunchParameters = (void *)0x12000000;

void mapFirmlaunchParameters(void)
{
    assertSuccess(svcKernelSetState(3, 0, (u64)((uintptr_t)g_firmlaunchParameters)));
}

Result GetFirmlaunchParams(void *outParams, size_t size)
{
    size = size >= 0x1000 ? 0x1000 : size;
    memcpy(outParams, g_firmlaunchParameters, size);
    return 0;
}

Result SetFirmlaunchParams(const void *params, size_t size)
{
    size = size >= 0x1000 ? 0x1000 : size;
    memcpy(g_firmlaunchParameters, params, size);
    if (size < 0x1000) {
        memset(g_firmlaunchParameters + size, 0, 0x1000 - size);
    }
    return 0;
}

static void LaunchFirmAsync(void *argdata)
{
    struct {
        u32 firmTidLow;
    } *args = argdata;

    terminateAllProcesses((u32)-1, 4 * 1000 * 1000 * 1000LL);
    // pm has dead code there (notification 0x179, but there's no 'caller process'... (-1))

    u64 firmTid = args->firmTidLow; // note: tidHigh doesn't matter
    firmTid = IS_N3DS ? (firmTid & ~N3DS_TID_MASK) | N3DS_TID_BIT : firmTid;
    assertSuccess(svcKernelSetState(0, firmTid));
}

Result LaunchFirm(u32 firmTidLow, const void *params, size_t size)
{
    struct {
        u32 firmTidLow;
    } args = { firmTidLow };

    SetFirmlaunchParams(params, size);
    TaskRunner_RunTask(LaunchFirmAsync, &args, sizeof(args));

    return 0;
}
