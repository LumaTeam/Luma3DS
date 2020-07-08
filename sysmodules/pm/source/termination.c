#include <3ds.h>
#include "termination.h"
#include "info.h"
#include "manager.h"
#include "util.h"
#include "exheader_info_heap.h"
#include "task_runner.h"

void forceMountSdCard(void)
{
    FS_Archive sdmcArchive;

    assertSuccess(fsInit());
    assertSuccess(FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")));
    // No need to clean up things as we will firmlaunch straight away
}

static Result terminateUnusedDependencies(const u64 *dependencies, u32 numDeps)
{
    ProcessData *process;
    Result res = 0;

    ProcessList_Lock(&g_manager.processList);
    FOREACH_PROCESS(&g_manager.processList, process) {
        if (process->terminationStatus != TERMSTATUS_RUNNING || !(process->flags & PROCESSFLAG_AUTOLOADED)) {
            continue;
        }

        u32 i;
        for (i = 0; i < numDeps && dependencies[i] != process->titleId; i++);

        if (i >= numDeps || --process->refcount > 0) {
            // Process not a listed dependency or still used
            continue;
        }

        res = ProcessData_SendTerminationNotification(process);
        res = R_SUMMARY(res) == RS_NOTFOUND ? 0 : res;

        if (R_FAILED(res)) {
            assertSuccess(svcTerminateProcess(process->handle));
        }
    }

    ProcessList_Unlock(&g_manager.processList);
    return res;
}

Result listAndTerminateDependencies(ProcessData *process, ExHeader_Info *exheaderInfo)
{
    Result res = 0;
    u64 dependencies[48]; // note: official pm reuses exheaderInfo to save space
    u32 numDeps = 0;

    TRY(getAndListDependencies(dependencies, &numDeps, process, exheaderInfo));
    return terminateUnusedDependencies(dependencies, numDeps);
}

static Result terminateProcessImpl(ProcessData *process, ExHeader_Info *exheaderInfo)
{
    // NOTE: list dependencies BEFORE sending the notification -- race condition material
    Result res = 0;
    u64 dependencies[48]; // note: official pm reuses exheaderInfo to save space
    u32 numDeps = 0;

    if (process->flags & PROCESSFLAG_DEPENDENCIES_LOADED) {
        TRY(getAndListDependencies(dependencies, &numDeps, process, exheaderInfo));
        process->flags &= ~PROCESSFLAG_DEPENDENCIES_LOADED;
        ProcessData_SendTerminationNotification(process);
        return terminateUnusedDependencies(dependencies, numDeps);
    } else {
        ProcessData_SendTerminationNotification(process);
        return 0;
    }
}

static void terminateProcessByIdChecked(u32 pid)
{
    ProcessData *process = ProcessList_FindProcessById(&g_manager.processList, pid);
    if (process != NULL) {
        ProcessData_SendTerminationNotification(process);
    } else {
        panic(0LL);
    }
}

static Result commitPendingTerminations(s64 timeout)
{
    // Wait for all of the processes that have received notification 0x100 to terminate
    // till the timeout, then actually terminate these processes, etc.

    Result res = 0;
    bool atLeastOneListener = false;
    ProcessList_Lock(&g_manager.processList);

    ProcessData *process;
    FOREACH_PROCESS(&g_manager.processList, process) {
        switch (process->terminationStatus) {
            case TERMSTATUS_NOTIFICATION_SENT:
                atLeastOneListener = true;
                break;
            case TERMSTATUS_NOTIFICATION_FAILED:
                res = svcTerminateProcess(process->handle); // official pm does not panic on failure here
                break;
            default:
                break;
        }
    }

    ProcessList_Unlock(&g_manager.processList);

    if (atLeastOneListener) {
        Result res = assertSuccess(svcWaitSynchronization(g_manager.allNotifiedTerminationEvent, timeout));

        if (R_DESCRIPTION(res) == RD_TIMEOUT) {
            ProcessList_Lock(&g_manager.processList);

            ProcessData *process;
            FOREACH_PROCESS(&g_manager.processList, process) {
                if (process->terminationStatus == TERMSTATUS_NOTIFICATION_SENT) {
                    res = svcTerminateProcess(process->handle);
                }
            }

            ProcessList_Unlock(&g_manager.processList);

            assertSuccess(svcWaitSynchronization(g_manager.allNotifiedTerminationEvent, -1LL));
        }
    } else {
        res = 0;
    }

    return res;
}

static void TerminateProcessOrTitleAsync(void *argdata)
{
    struct {
        u64 id;
        s64 timeout;
        bool useTitleId;
    } *args = argdata;

    ProcessData *process;
    bool notify = false;
    u8 variation;

    if (args->timeout >= 0) {
        assertSuccess(svcClearEvent(g_manager.allNotifiedTerminationEvent));
        g_manager.waitingForTermination = true;
    }

    ExHeader_Info *exheaderInfo = ExHeaderInfoHeap_New();
    if (exheaderInfo == NULL) {
        panic(0);
    }

    ProcessList_Lock(&g_manager.processList);
    FOREACH_PROCESS(&g_manager.processList, process) {
        // It's the only place where it uses the full titleId, and doesn't break after the first result.
        // Maybe it's to allow killing all the builtins at once with their dummy titleIds? Otherwise,
        // two processes can't have the same titleId.
        if ((args->useTitleId && process->titleId == args->id) || process->pid == args->id) {
            if (process->flags & PROCESSFLAG_NOTIFY_TERMINATION) {
                notify = true;
                variation = process->terminatedNotificationVariation;
                process->flags = (process->flags & ~PROCESSFLAG_NOTIFY_TERMINATION) | PROCESSFLAG_NOTIFY_TERMINATION_TERMINATED;
            }
            terminateProcessImpl(process, exheaderInfo);
            if (!args->useTitleId) {
                break;
            }
        }
    }
    ProcessList_Unlock(&g_manager.processList);

    ExHeaderInfoHeap_Delete(exheaderInfo);

    if (args->timeout >= 0) {
        commitPendingTerminations(args->timeout);
        g_manager.waitingForTermination = false;
        if (notify) {
            notifySubscribers(0x110 + variation);
        }
    }
}

static Result TerminateProcessOrTitle(u64 id, s64 timeout, bool useTitleId)
{
    ProcessData *process;

    if (g_manager.preparingForReboot) {
        if (timeout != 0) {
            return 0xC8A05801;
        }

        ProcessList_Lock(&g_manager.processList);
        FOREACH_PROCESS(&g_manager.processList, process) {
            if ((useTitleId && process->titleId == id) || process->pid == id) {
                assertSuccess(svcTerminateProcess(process->handle));
                if (!useTitleId) {
                    break;
                }
            }
        }
        ProcessList_Unlock(&g_manager.processList);

        return 0;
    } else {
        struct {
            u64 id;
            s64 timeout;
            bool useTitleId;
        } args = { id, timeout, useTitleId };

        TaskRunner_RunTask(TerminateProcessOrTitleAsync, &args, sizeof(args));
        return 0;
    }
}

Result TerminateApplication(s64 timeout)
{
    Result res = 0;

    if (g_manager.preparingForReboot) {
        return 0xC8A05801;
    }

    ExHeader_Info *exheaderInfo = ExHeaderInfoHeap_New();
    if (exheaderInfo == NULL) {
        panic(0);
    }

    assertSuccess(svcClearEvent(g_manager.allNotifiedTerminationEvent));
    g_manager.waitingForTermination = true;

    ProcessList_Lock(&g_manager.processList);
    if (g_manager.runningApplicationData != NULL) {
        terminateProcessImpl(g_manager.runningApplicationData, exheaderInfo);
    }
    ProcessList_Unlock(&g_manager.processList);

    res = commitPendingTerminations(timeout);

    ExHeaderInfoHeap_Delete(exheaderInfo);
    g_manager.waitingForTermination = false;

    return res;
}

Result TerminateTitle(u64 titleId, s64 timeout)
{
    return TerminateProcessOrTitle(titleId, timeout, true);
}

Result TerminateProcess(u32 pid, s64 timeout)
{
    return TerminateProcessOrTitle(pid, timeout, false);
}

ProcessData *terminateAllProcesses(u32 callerPid, s64 timeout)
{
    u64 dstTimePoint = svcGetSystemTick() + nsToTicks(timeout);
    ProcessData *process;
    ProcessData *callerProcess = NULL; // note: official pm returns the caller's handle instead

    u64 dependencies[48];
    u32 numDeps = 0;
    s64 numKips = 0;
    svcGetSystemInfo(&numKips, 26, 0);

    ExHeader_Info *exheaderInfo = ExHeaderInfoHeap_New();

    if (exheaderInfo == NULL) {
        panic(0);
    }

    assertSuccess(svcClearEvent(g_manager.allNotifiedTerminationEvent));
    g_manager.waitingForTermination = true;

    // List the dependencies of the caller
    if (callerPid != (u32)-1) {
        ProcessList_Lock(&g_manager.processList);
        process = ProcessList_FindProcessById(&g_manager.processList, callerPid);
        if (process != NULL) {
            callerProcess = process;
            getAndListDependencies(dependencies, &numDeps, process, exheaderInfo);
        }
        ProcessList_Unlock(&g_manager.processList);
    }

    ProcessList_Lock(&g_manager.processList);

    // Send custom notification to at least Rosalina to make it relinquish some non-KIP services handles, stop the debugger, etc.
    if (numKips >= 6) {
        notifySubscribers(0x2000);
    }

    // Send notification 0x100 to the currently running application
    if (g_manager.runningApplicationData != NULL) {
        g_manager.runningApplicationData->flags &= ~PROCESSFLAG_DEPENDENCIES_LOADED;
        ProcessData_SendTerminationNotification(g_manager.runningApplicationData);
    }

    // Send notification 0x100 to anything but the caller deps or the caller; and *increase* the refcount of the latter if autoloaded
    // Ignore KIPs
    FOREACH_PROCESS(&g_manager.processList, process) {
        if (process->flags & PROCESSFLAG_KIP) {
            continue;
        } else if (process == callerProcess && (process->flags & PROCESSFLAG_AUTOLOADED) != 0) {
            ProcessData_Incref(process, 1);
            continue;
        }

        u32 i;
        for (i = 0; i < numDeps && dependencies[i] != process->titleId; i++);

        if (i >= numDeps) {
            // Process not a listed dependency: send notification 0x100
            ProcessData_SendTerminationNotification(process);
        } else if (process->flags & PROCESSFLAG_AUTOLOADED){
            ProcessData_Incref(process, 1);
        }
    }
    ProcessList_Unlock(&g_manager.processList);
    ExHeaderInfoHeap_Delete(exheaderInfo);

    s64 timeoutTicks = dstTimePoint - svcGetSystemTick();
    commitPendingTerminations(timeoutTicks >= 0 ? ticksToNs(timeoutTicks) : 0LL);
    g_manager.waitingForTermination = false;

    if (callerPid == (u32)-1) {
        // On firmlaunch, try to force Process9 to mount the SD card to allow the Process9 firmlaunch patch to load boot.firm if needed
        // Need to do that before we tell PXI to terminate
        s64 out;
        if(R_SUCCEEDED(svcGetSystemInfo(&out, 0x10000, 0x203)) && out != 0) {
            // If boot.firm is on the SD card, then...
            forceMountSdCard();
        }
    }

    // Now, send termination notification to PXI (PID 4). Also do the same for Rosalina.
    assertSuccess(svcClearEvent(g_manager.allNotifiedTerminationEvent));
    g_manager.waitingForTermination = true;
    ProcessList_Lock(&g_manager.processList);

    if (numKips >= 6) {
        terminateProcessByIdChecked(5); // Rosalina
    }
    terminateProcessByIdChecked(4); // PXI

    ProcessList_Unlock(&g_manager.processList);

    // Allow 1.5 extra seconds for PXI and Rosalina (approx 402167783 ticks)
    timeoutTicks = dstTimePoint - svcGetSystemTick();
    commitPendingTerminations(1500 * 1000 * 1000LL + (timeoutTicks >= 0 ? ticksToNs(timeoutTicks) : 0LL));
    g_manager.waitingForTermination = false;

    return callerProcess;
}

static void PrepareForRebootAsync(void *argdata)
{
    struct {
        u32 pid;
        s64 timeout;
    } *args = argdata;

    ProcessData *caller = terminateAllProcesses(args->pid, args->timeout);
    if (caller != NULL) {
        ProcessData_Notify(caller, 0x179);
    }
}

Result PrepareForReboot(u32 pid, s64 timeout)
{
    struct {
        u32 pid;
        s64 timeout;
    } args = { pid, timeout };

    if (g_manager.preparingForReboot) {
        return 0xC8A05801;
    }

    g_manager.preparingForReboot = true;
    TaskRunner_RunTask(PrepareForRebootAsync, &args, sizeof(args));
    return 0;
}
