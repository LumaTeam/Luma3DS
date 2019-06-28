#include <3ds.h>
#include <string.h>
#include "launch.h"
#include "info.h"
#include "manager.h"
#include "reslimit.h"
#include "exheader_info_heap.h"
#include "task_runner.h"
#include "util.h"
#include "luma.h"

static bool g_debugNextApplication = false;

// Note: official PM has two distinct functions for sysmodule vs. regular app. We refactor that into a single function.
static Result launchTitleImpl(Handle *outDebug, ProcessData **outProcessData, const FS_ProgramInfo *programInfo,
    const FS_ProgramInfo *programInfoUpdate, u32 launchFlags, ExHeader_Info *exheaderInfo);

// Note: official PM doesn't include svcDebugActiveProcess in this function, but rather in the caller handling dependencies
static Result loadWithoutDependencies(Handle *outDebug, ProcessData **outProcessData, u64 programHandle, const FS_ProgramInfo *programInfo,
    u32 launchFlags, const ExHeader_Info *exheaderInfo)
{
    Result res = 0;
    Handle processHandle = 0;
    u32 pid;
    ProcessData *process;
    const ExHeader_Arm11SystemLocalCapabilities *localcaps = &exheaderInfo->aci.local_caps;

    if (outDebug != NULL) {
        *outDebug = 0;
    }

    if (outProcessData != NULL) {
        *outProcessData = NULL;
    }

    if (programInfo->programId & (1ULL << 35)) {
        // RequireBatchUpdate?
        return 0xD8E05803;
    }

    TRY(LOADER_LoadProcess(&processHandle, programHandle));
    TRY(svcGetProcessId(&pid, processHandle));

    // Note: bug in official PM: it seems not to panic/cleanup properly if the function calls below fail,
    // svcTerminateProcess won't be called, it's possible to trigger NULL derefs if you crash fs/sm/whatever,
    // leaks dependencies, and so on...
    // This can be solved by interesting the new process in the list earlier, etc. etc., allowing us to simplify the logic greatly.

    ProcessList_Lock(&g_manager.processList);
    process = ProcessList_New(&g_manager.processList);
    if (process == NULL) {
        panic(1);
    }

    process->handle = processHandle;
    process->pid = pid;
    process->titleId = exheaderInfo->aci.local_caps.title_id;;
    process->programHandle = programHandle;
    process->flags = 0; // will be filled later
    process->terminatedNotificationVariation = (launchFlags & 0xF0) >> 4;
    process->terminationStatus = TERMSTATUS_RUNNING;
    process->refcount = 1;

    ProcessList_Unlock(&g_manager.processList);
    svcSignalEvent(g_manager.newProcessEvent);

    if (outProcessData != NULL) {
        *outProcessData = process;
    }

    u32 serviceCount;
    for(serviceCount = 0; serviceCount < 34 && *(u64 *)localcaps->service_access[serviceCount] != 0; serviceCount++);

    TRY(FSREG_Register(pid, programHandle, programInfo, &localcaps->storage_info));
    TRY(SRVPM_RegisterProcess(pid, serviceCount, localcaps->service_access));

    if (localcaps->reslimit_category <= RESLIMIT_CATEGORY_OTHER) {
        TRY(svcSetProcessResourceLimits(processHandle, g_manager.reslimits[localcaps->reslimit_category]));
    }

    // Yes, even numberOfCores=2 on N3DS. On the 3DS, the affinity mask doesn't play the role of an access limiter,
    // it's only useful for cpuId < 0. thread->affinityMask = process->affinityMask | (cpuId >= 0 ? 1 << cpuId : 0)
    u8 affinityMask = localcaps->core_info.affinity_mask;
    TRY(svcSetProcessAffinityMask(processHandle, &affinityMask, 2));
    TRY(svcSetProcessIdealProcessor(processHandle, localcaps->core_info.ideal_processor));

    if (launchFlags & PMLAUNCHFLAG_NORMAL_APPLICATION) {
        setAppCpuTimeLimitAndSchedModeFromDescriptor(localcaps->title_id, localcaps->reslimits[0]);
        (*outProcessData)->flags |= PROCESSFLAG_NORMAL_APPLICATION; // not in official PM
    }

    if (outDebug != NULL) {
        TRY(svcDebugActiveProcess(outDebug, pid));
    }

    return res;
}

static Result loadWithDependencies(Handle *outDebug, ProcessData **outProcessData, u64 programHandle, const FS_ProgramInfo *programInfo,
    u32 launchFlags, const ExHeader_Info *exheaderInfo)
{
    Result res = 0;

    u64 dependencies[48];
    u32 remrefcounts[48] = {0};
    ProcessData *depProcs[48] = {NULL};
    u32 numUnique = 0;

    FS_ProgramInfo depProgramInfo;

    res = loadWithoutDependencies(outDebug, outProcessData, programHandle, programInfo, launchFlags, exheaderInfo);
    ProcessData *process = *outProcessData;

    if (R_FAILED(res)) {
        if (outDebug != NULL) {
            svcCloseHandle(*outDebug);
            *outDebug = 0;
        }

        if (process != NULL) {
            svcTerminateProcess(process->handle);
        }

        return res;
    }

    ExHeader_Info *depExheaderInfo = ExHeaderInfoHeap_New();
    if (depExheaderInfo == NULL) {
        panic(0);
    }

    listMergeUniqueDependencies(depProcs, dependencies, remrefcounts, &numUnique, exheaderInfo);

    if (numUnique > 0) {
        process->flags |= PROCESSFLAG_DEPENDENCIES_LOADED;
    }

    /*
        Official pm does this:
            for each dependency:
                if dep already loaded: if autoloaded increase refcount // note: not autoloaded = not autoterminated
                else: load new sysmodule w/o its deps (then process its deps), set flag "autoloaded"  return early from entire function if it fails
        Naturally, it forgets to incref all subsequent dependencies here & also when it factors the duplicate entries in,
        and has a few other bugs (actually I'm not entirely sure... I think it doesn't clear dependencies on termination if it fails)
        It also has a buffer overflow bug if the flattened dep tree has more than 48 elements (but this can never happen in practice)
    */

    for (u32 i = 0; i < numUnique; i++) {
        if (depProcs[i] != NULL) {
            continue;
        }
        // Note: numUnique is changed within the loop
        depProgramInfo.programId = dependencies[i];
        depProgramInfo.mediaType = MEDIATYPE_NAND;

        res = launchTitleImpl(NULL, &process, &depProgramInfo, NULL, 0, depExheaderInfo);
        depProcs[i] = process;
        if (R_SUCCEEDED(res)) {
            process->flags |= PROCESSFLAG_AUTOLOADED | PROCESSFLAG_DEPENDENCIES_LOADED;
            ProcessData_Incref(process, remrefcounts[i] - 1);
            remrefcounts[i] = 0;
            listMergeUniqueDependencies(depProcs, dependencies, remrefcounts, &numUnique, depExheaderInfo); // does some incref too
        } else if (process != NULL) {
            if (outDebug != NULL) {
                svcCloseHandle(*outDebug);
                *outDebug = 0;
            }

            svcTerminateProcess(process->handle);
            ExHeaderInfoHeap_Delete(depExheaderInfo);
            return res;
        }
    }


    ExHeaderInfoHeap_Delete(depExheaderInfo);
    return res;
}

// Note: official PM has two distinct functions for sysmodule vs. regular app. We refactor that into a single function.
static Result launchTitleImpl(Handle *debug, ProcessData **outProcessData, const FS_ProgramInfo *programInfo,
    const FS_ProgramInfo *programInfoUpdate, u32 launchFlags, ExHeader_Info *exheaderInfo)
{
    *outProcessData = NULL;

    if (debug != NULL) {
        *debug = 0;
    }

    if (isTitleLaunchPrevented(programInfo->programId)) {
        return 0;
    }

    if (launchFlags & PMLAUNCHFLAG_NORMAL_APPLICATION) {
        launchFlags |= PMLAUNCHFLAG_LOAD_DEPENDENCIES;
    } else {
        // NEW: allow non-apps to be debugged with the help of pm
        //launchFlags &= ~(PMLAUNCHFLAG_USE_UPDATE_TITLE | PMLAUNCHFLAG_QUEUE_DEBUG_APPLICATION);
        launchFlags &= ~PMLAUNCHFLAG_USE_UPDATE_TITLE;
        launchFlags &= ~(PMLAUNCHFLAG_FORCE_USE_O3DS_APP_MEM | PMLAUNCHFLAG_FORCE_USE_O3DS_MAX_APP_MEM);
    }

    Result res = 0;
    u64 programHandle;
    StartupInfo si = {0};

    programInfoUpdate = (launchFlags & PMLAUNCHFLAG_USE_UPDATE_TITLE) ? programInfoUpdate : programInfo;
    TRY(registerProgram(&programHandle, programInfo, programInfoUpdate));

    res = LOADER_GetProgramInfo(exheaderInfo, programHandle);
    res = R_SUCCEEDED(res) && exheaderInfo->aci.local_caps.core_info.core_version != SYSCOREVER ? (Result)0xC8A05800 : res;

    if (R_FAILED(res)) {
        LOADER_UnregisterProgram(programHandle);
        return res;
    }

    // Change APPMEMALLOC if needed
    if (IS_N3DS && APPMEMTYPE == 6 && (launchFlags & PMLAUNCHFLAG_NORMAL_APPLICATION) != 0) {
        u32 limitMb;
        SystemMode n3dsSystemMode = exheaderInfo->aci.local_caps.core_info.n3ds_system_mode;
        if ((launchFlags & PMLAUNCHFLAG_FORCE_USE_O3DS_APP_MEM) || n3dsSystemMode == SYSMODE_O3DS_PROD) {
            if ((launchFlags & PMLAUNCHFLAG_FORCE_USE_O3DS_APP_MEM) & PMLAUNCHFLAG_FORCE_USE_O3DS_MAX_APP_MEM) {
                limitMb = 96;
            } else {
                switch (exheaderInfo->aci.local_caps.core_info.o3ds_system_mode) {
                    case SYSMODE_O3DS_PROD: limitMb = 64; break;
                    case SYSMODE_DEV1:      limitMb = 96; break;
                    case SYSMODE_DEV2:      limitMb = 80; break;
                    default:                limitMb = 0;  break;
                }
            }

            setAppMemLimit(limitMb << 20);
        }
    }

    if (launchFlags & PMLAUNCHFLAG_LOAD_DEPENDENCIES) {
        TRYG(loadWithDependencies(debug, outProcessData, programHandle, programInfo, launchFlags, exheaderInfo), cleanup);
        // note: official pm doesn't terminate the process if this fails (dependency loading)...
        // This may be intentional, but I believe this is a bug since the 0xD8A05805 and svcRun failure codepaths terminate the process...
        // It also forgets to clear PROCESSFLAG_NOTIFY_TERMINATION in the process...
    } else {
        TRYG(loadWithoutDependencies(debug, outProcessData, programHandle, programInfo, launchFlags, exheaderInfo), cleanup);
        // note: official pm doesn't terminate the proc. if it fails here either, but will because of the svcCloseHandle and the svcRun codepath
    }

    ProcessList_Lock(&g_manager.processList);
    ProcessData *process = *outProcessData;
    if (launchFlags & PMLAUNCHFLAG_QUEUE_DEBUG_APPLICATION) {
        // saved field is different in official pm
        // this also means official pm can't launch a title with a debug flag and an application
        if (g_manager.debugData == NULL) {
            g_manager.debugData = process;
        } else {
            res = 0xD8A05805;
        }
    } else {
        si.priority = exheaderInfo->aci.local_caps.core_info.priority;
        si.stack_size = exheaderInfo->sci.codeset_info.stack_size;
        res = svcRun(process->handle, &si);
        if (R_SUCCEEDED(res) && (launchFlags & PMLAUNCHFLAG_NORMAL_APPLICATION) != 0) {
            g_manager.runningApplicationData = process;
            notifySubscribers(0x10C);
        }
    }

    cleanup:
    process = *outProcessData;
    if (process != NULL && R_FAILED(res)) {
        svcTerminateProcess(process->handle);
    } else if (process != NULL) {
        // official PM sets it but forgets to clear it on failure...
        process->flags |= (launchFlags & PMLAUNCHFLAG_NOTIFY_TERMINATION) ? PROCESSFLAG_NOTIFY_TERMINATION : 0;
    }

    ProcessList_Unlock(&g_manager.processList);
    return res;
}

static Result launchTitleImplWrapper(Handle *outDebug, u32 *outPid, const FS_ProgramInfo *programInfo, const FS_ProgramInfo *programInfoUpdate, u32 launchFlags)
{
    ExHeader_Info *exheaderInfo = ExHeaderInfoHeap_New();
    if (exheaderInfo == NULL) {
        panic(0);
    }

    ProcessData *process = NULL;
    Result res = launchTitleImpl(outDebug, &process, programInfo, programInfoUpdate, launchFlags, exheaderInfo);

    if (outPid != NULL && process != NULL) {
        *outPid = process->pid;
    }

    ExHeaderInfoHeap_Delete(exheaderInfo);

    return res;
}

static void LaunchTitleAsync(void *argdata)
{
    struct {
        FS_ProgramInfo programInfo, programInfoUpdate;
        u32 launchFlags;
    } *args = argdata;

    launchTitleImplWrapper(NULL, NULL, &args->programInfo, &args->programInfoUpdate, args->launchFlags);
}

Result LaunchTitle(u32 *outPid, const FS_ProgramInfo *programInfo, u32 launchFlags)
{
    ProcessData *process, *foundProcess = NULL;
    bool originallyDebugged = launchFlags & PMLAUNCHFLAG_QUEUE_DEBUG_APPLICATION;

    launchFlags &= ~PMLAUNCHFLAG_USE_UPDATE_TITLE;
    launchFlags |= g_debugNextApplication && (launchFlags & PMLAUNCHFLAG_NORMAL_APPLICATION) ? PMLAUNCHFLAG_QUEUE_DEBUG_APPLICATION : 0;

    if (g_manager.preparingForReboot) {
        return 0xC8A05801;
    }

    u32 tidh = (u32)(programInfo->programId >> 32);
    u32 tidl = (u32)programInfo->programId;
    if ((tidh == 0x00040030 || tidh == 0x00040130) && (tidl & 0xFF) != SYSCOREVER) {
        // Panic if launching SAFE_MODE sysmodules or applets (note: exheader syscorever check above only done for applications in official PM)
        // Official PM also hardcodes SYSCOREVER = 2 here.
        panic(4);
    }

    ProcessList_Lock(&g_manager.processList);
    if (g_manager.runningApplicationData != NULL && (launchFlags & PMLAUNCHFLAG_NORMAL_APPLICATION) != 0) {
        ProcessList_Unlock(&g_manager.processList);
        return 0xC8A05BF0;
    }

    FOREACH_PROCESS(&g_manager.processList, process) {
        if ((process->titleId & ~0xFFULL) == (programInfo->programId & ~0xFFULL)) {
            foundProcess = process;
            break;
        }
    }
    ProcessList_Unlock(&g_manager.processList);

    if (foundProcess != NULL) {
        foundProcess->flags &= ~PROCESSFLAG_AUTOLOADED;
        if (outPid != NULL) {
            *outPid = foundProcess->pid;
        }
        return 0;
    } else {
        if (launchFlags & PMLAUNCHFLAG_QUEUE_DEBUG_APPLICATION || !(launchFlags & PMLAUNCHFLAG_NORMAL_APPLICATION)) {
            Result res = launchTitleImplWrapper(NULL, outPid, programInfo, programInfo, launchFlags);
            if (R_SUCCEEDED(res) && (launchFlags & PMLAUNCHFLAG_NORMAL_APPLICATION)) {
                g_debugNextApplication = false;
                if (!originallyDebugged) {
                    // Custom notification
                    notifySubscribers(0x1000);
                }
            }
            return res;
        } else {
            struct {
                FS_ProgramInfo programInfo, programInfoUpdate;
                u32 launchFlags;
            } args = { *programInfo, *programInfo, launchFlags };

            if (outPid != NULL) {
                *outPid = (u32)-1; // PM doesn't do that lol
            }
            TaskRunner_RunTask(LaunchTitleAsync, &args, sizeof(args));
            return 0;
        }
    }
}

Result LaunchTitleUpdate(const FS_ProgramInfo *programInfo, const FS_ProgramInfo *programInfoUpdate, u32 launchFlags)
{
    ProcessList_Lock(&g_manager.processList);
    if (g_manager.preparingForReboot) {
        return 0xC8A05801;
    }
    if (g_manager.runningApplicationData != NULL) {
        ProcessList_Unlock(&g_manager.processList);
        return 0xC8A05BF0;
    }
    if (!(launchFlags & ~PMLAUNCHFLAG_NORMAL_APPLICATION)) {
        return 0xD8E05802;
    }
    ProcessList_Unlock(&g_manager.processList);

    bool originallyDebugged = launchFlags & PMLAUNCHFLAG_QUEUE_DEBUG_APPLICATION;

    launchFlags |= PMLAUNCHFLAG_USE_UPDATE_TITLE;
    launchFlags |= g_debugNextApplication ? PMLAUNCHFLAG_QUEUE_DEBUG_APPLICATION : 0;

    if (launchFlags & PMLAUNCHFLAG_QUEUE_DEBUG_APPLICATION) {
        Result res = launchTitleImplWrapper(NULL, NULL, programInfo, programInfoUpdate, launchFlags);
        if (R_SUCCEEDED(res)) {
            g_debugNextApplication = false;
            if (!originallyDebugged) {
                // Custom notification
                notifySubscribers(0x1000);
            }
        }
        return res;
    } else {
        struct {
            FS_ProgramInfo programInfo, programInfoUpdate;
            u32 launchFlags;
        } args = { *programInfo, *programInfoUpdate, launchFlags };

        TaskRunner_RunTask(LaunchTitleAsync, &args, sizeof(args));
        return 0;
    }
}

Result LaunchApp(const FS_ProgramInfo *programInfo, u32 launchFlags)
{
    ProcessList_Lock(&g_manager.processList);
    if (g_manager.runningApplicationData != NULL) {
        ProcessList_Unlock(&g_manager.processList);
        return 0xC8A05BF0;
    }

    assertSuccess(setAppCpuTimeLimit(0));
    ProcessList_Unlock(&g_manager.processList);

    return LaunchTitle(NULL, programInfo, launchFlags | PMLAUNCHFLAG_LOAD_DEPENDENCIES | PMLAUNCHFLAG_NORMAL_APPLICATION);
}

Result RunQueuedProcess(Handle *outDebug)
{
    Result res = 0;
    StartupInfo si = {0};

    ProcessList_Lock(&g_manager.processList);
    if (g_manager.debugData == NULL) {
        ProcessList_Unlock(&g_manager.processList);
        return 0xD8A05804;
    } else if ((g_manager.debugData->flags & PROCESSFLAG_NORMAL_APPLICATION) && g_manager.runningApplicationData != NULL) {
        // Not in official PM
        ProcessList_Unlock(&g_manager.processList);
        return 0xC8A05BF0;
    }

    ProcessData *process = g_manager.debugData;
    g_manager.debugData = NULL;

    ExHeader_Info *exheaderInfo = ExHeaderInfoHeap_New();
    if (exheaderInfo == NULL) {
        panic(0);
    }

    TRYG(LOADER_GetProgramInfo(exheaderInfo, process->programHandle), cleanup);
    TRYG(svcDebugActiveProcess(outDebug, process->pid), cleanup);

    si.priority = exheaderInfo->aci.local_caps.core_info.priority;
    si.stack_size = exheaderInfo->sci.codeset_info.stack_size;
    res = svcRun(process->handle, &si);
    if (R_SUCCEEDED(res) && process->flags & PROCESSFLAG_NORMAL_APPLICATION) {
        // Second operand not in official PM
        g_manager.runningApplicationData = process;
        notifySubscribers(0x10C);
    }

    cleanup:
    if (R_FAILED(res)) {
        process->flags &= ~PROCESSFLAG_NOTIFY_TERMINATION;
        svcTerminateProcess(process->handle);
    }

    ExHeaderInfoHeap_Delete(exheaderInfo);
    ProcessList_Unlock(&g_manager.processList);

    return res;
}

Result LaunchAppDebug(Handle *outDebug, const FS_ProgramInfo *programInfo, u32 launchFlags)
{
    ProcessList_Lock(&g_manager.processList);
    if (g_manager.debugData != NULL) {
        ProcessList_Unlock(&g_manager.processList);
        return RunQueuedProcess(outDebug);
    }

    if (g_manager.runningApplicationData != NULL) {
        ProcessList_Unlock(&g_manager.processList);
        return 0xC8A05BF0;
    }

    bool prevdbg = g_debugNextApplication;
    g_debugNextApplication = false;

    assertSuccess(setAppCpuTimeLimit(0));
    ProcessList_Unlock(&g_manager.processList);

    Result res = launchTitleImplWrapper(outDebug, NULL, programInfo, programInfo,
        (launchFlags & ~PMLAUNCHFLAG_USE_UPDATE_TITLE) | PMLAUNCHFLAG_NORMAL_APPLICATION);

    if (R_FAILED(res)) {
        g_debugNextApplication = prevdbg;
    }
    return res;
}

Result autolaunchSysmodules(void)
{
    Result res = 0;
    FS_ProgramInfo programInfo = { .mediaType = MEDIATYPE_NAND };

    // Launch NS
    if (NSTID != 0) {
        programInfo.programId = NSTID;
        TRY(launchTitleImplWrapper(NULL, NULL, &programInfo, &programInfo, PMLAUNCHFLAG_LOAD_DEPENDENCIES));
    }

    return res;
}

// Custom
Result DebugNextApplicationByForce(bool debug)
{
    g_debugNextApplication = debug;
    return 0;
}

Result LaunchTitleDebug(Handle *outDebug, const FS_ProgramInfo *programInfo, u32 launchFlags)
{
    if (launchFlags & PMLAUNCHFLAG_NORMAL_APPLICATION) {
        return LaunchAppDebug(outDebug, programInfo, launchFlags);
    }

    ProcessList_Lock(&g_manager.processList);
    if (g_manager.debugData != NULL) {
        ProcessList_Unlock(&g_manager.processList);
        return RunQueuedProcess(outDebug);
    }
    ProcessList_Unlock(&g_manager.processList);

    return launchTitleImplWrapper(outDebug, NULL, programInfo, programInfo, launchFlags & ~PMLAUNCHFLAG_USE_UPDATE_TITLE);
}
