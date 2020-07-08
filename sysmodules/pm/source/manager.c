#include <3ds.h>
#include <string.h>
#include "manager.h"
#include "reslimit.h"
#include "util.h"
#include "luma.h"

Manager g_manager;

static void giveAllFsArchiveAccessToKip(u32 pid, u64 tid)
{
    static const ExHeader_Arm11StorageInfo storageInfo = {
        .fs_access_info = 0xFFFFFFFF,
    };
    static const u64 programHandle = 0xFFFF000000000000LL;

    FS_ProgramInfo info = {
        .programId = tid,
        .mediaType = MEDIATYPE_NAND,
    };

    assertSuccess(FSREG_Register(pid, programHandle, &info, &storageInfo));
}

void Manager_Init(void *procBuf, size_t numProc)
{
    memset(&g_manager, 0, sizeof(Manager));
    ProcessList_Init(&g_manager.processList, procBuf, numProc);
    assertSuccess(svcCreateEvent(&g_manager.newProcessEvent, RESET_ONESHOT));
    assertSuccess(svcCreateEvent(&g_manager.allNotifiedTerminationEvent , RESET_ONESHOT));
}

void Manager_RegisterKips(void)
{
    s64 numKips = 0;
    ProcessData *process;
    Handle processHandle;

    svcGetSystemInfo(&numKips, 26, 0);

    ProcessList_Lock(&g_manager.processList);
    for (u32 i = 0; i < (u32)numKips; i++) {
        process = ProcessList_New(&g_manager.processList);
        if (process == NULL) {
            panic(1);
        }

        assertSuccess(svcOpenProcess(&processHandle, i));
        process->handle = processHandle;
        process->pid = i;
        process->refcount = 1;
        process->titleId = 0x0004000100001000ULL; // note: same internal TID for all builtins
        process->flags = PROCESSFLAG_KIP;
        process->terminationStatus = TERMSTATUS_RUNNING;

        if (i < 5) {
            // Exempt rosalina from being resource-limited at all
            assertSuccess(svcSetProcessResourceLimits(processHandle, g_manager.reslimits[RESLIMIT_CATEGORY_OTHER]));
        }
    }
    ProcessList_Unlock(&g_manager.processList);

    // Give full archive access to us (PM) and Rosalina (real PIDs don't matter, they just have to be unique (?))
    // Loader doesn't depend on PM and has its own fs:REG handle so it must do it itself.
    giveAllFsArchiveAccessToKip(2, 0x0004013000001202LL); // PM
    if (numKips > 5) {
        giveAllFsArchiveAccessToKip(5, 0x0004013000006902LL); // Rosalina
    }
}

Result UnregisterProcess(u64 titleId)
{
    ProcessData *foundProcess = NULL;

    ProcessList_Lock(&g_manager.processList);
    foundProcess = ProcessList_FindProcessByTitleId(&g_manager.processList, titleId & ~N3DS_TID_MASK);
    if (foundProcess != NULL) {
        if (foundProcess == g_manager.runningApplicationData) {
            g_manager.runningApplicationData = NULL;
        }

        if (foundProcess == g_manager.debugData) {
            g_manager.debugData = NULL;
        }

        svcCloseHandle(foundProcess->handle);
        ProcessList_Delete(&g_manager.processList, foundProcess);
    }

    ProcessList_Unlock(&g_manager.processList);
    return 0;
}

Result PrepareToChainloadHomebrew(u64 titleId)
{
    // Note: I'm allowing this command to be called for non-applications, maybe that'll be useful
    // in the future...

    ProcessData *foundProcess = NULL;
    Result res;
    ProcessList_Lock(&g_manager.processList);
    foundProcess = ProcessList_FindProcessByTitleId(&g_manager.processList, titleId & ~N3DS_TID_MASK);
    if (foundProcess != NULL) {
        // Clear the "notify on termination, don't cleanup" flag, so that for ex. APT isn't notified & no need for UnregisterProcess,
        // and the "dependencies loaded" flag, so that the dependencies aren't killed (for ex. when
        // booting hbmenu instead of Home Menu, in which case the same title is going to be launched...)

        foundProcess->flags &= ~(PROCESSFLAG_DEPENDENCIES_LOADED | PROCESSFLAG_NOTIFY_TERMINATION);
        res = 0;
    } else {
        res = MAKERESULT(RL_TEMPORARY, RS_NOTFOUND, RM_PM, 0x100);
    }

    ProcessList_Unlock(&g_manager.processList);
    return res;
}
