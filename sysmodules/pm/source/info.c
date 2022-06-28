#include <3ds.h>
#include <string.h>
#include "exheader_info_heap.h"
#include "manager.h"
#include "info.h"
#include "util.h"

Result registerProgram(u64 *programHandle, const FS_ProgramInfo *programInfo, const FS_ProgramInfo *programInfoUpdate)
{
    FS_ProgramInfo pi = *programInfo, piu = *programInfoUpdate;
    Result res = 0;

    if (IS_N3DS) {
        if (pi.programId >> 48 == 0xFFFF) {
            // Check for HIO TIDs. PM is a bit more lax, as the condition
            // in Loader and FS is (x >> 32) == 0xFFFF0000 instead.
            return LOADER_RegisterProgram(programHandle, &pi, &piu);
        }
        pi.programId = (pi.programId  & ~N3DS_TID_MASK) | N3DS_TID_BIT;
        piu.programId = (piu.programId & ~N3DS_TID_MASK) | N3DS_TID_BIT;
        res = LOADER_RegisterProgram(programHandle, &pi, &piu);
        if (R_FAILED(res)) {
            pi.programId  &= ~N3DS_TID_MASK;
            piu.programId &= ~N3DS_TID_MASK;
            res = LOADER_RegisterProgram(programHandle, &pi, &piu);
        }
    } else {
        res = LOADER_RegisterProgram(programHandle, &pi, &piu);
    }

    return res;
}

Result getAndListDependencies(u64 *dependencies, u32 *numDeps, ProcessData *process, ExHeader_Info *exheaderInfo)
{
    Result res = 0;
    TRY(LOADER_GetProgramInfo(exheaderInfo, process->programHandle));
    return listDependencies(dependencies, numDeps, exheaderInfo);
}

Result listDependencies(u64 *dependencies, u32 *numDeps, const ExHeader_Info *exheaderInfo)
{
    Result res = 0;

    u32 num = 0;
    for (u32 i = 0; i < 48 && exheaderInfo->sci.dependencies[i] != 0; i++) {
        u64 titleId = exheaderInfo->sci.dependencies[i];
        if (IS_N3DS || (titleId & N3DS_TID_MASK) == 0) {
            // On O3DS, ignore N3DS titles.
            // Then (on both) remove the N3DS titleId bits
            dependencies[num++] = titleId & ~N3DS_TID_MASK;
        }
    }

    *numDeps = num;
    return res;
}

Result listMergeUniqueDependencies(ProcessData **procs, u64 *dependencies, u32 *remrefcounts, u32 *numDeps, const ExHeader_Info *exheaderInfo)
{
    Result res = 0;
    u32 numDepsUnique = *numDeps;
    u32 num2;

    u64 deps[48];
    u32 newrefcounts[48] = {0};

    TRY(listDependencies(deps, &num2, exheaderInfo));

    ProcessList_Lock(&g_manager.processList);
    for (u32 i = 0; i < num2; i++) {
        // Filter duplicate results
        u32 j;
        for (j = 0; j < numDepsUnique && deps[i] != dependencies[j]; j++);
        if (j >= numDepsUnique) {
            if (numDepsUnique >= 48) {
                panic(2);
            }
            dependencies[numDepsUnique] = deps[i];
            newrefcounts[numDepsUnique] = 1;
            procs[numDepsUnique] = ProcessList_FindProcessByTitleId(&g_manager.processList, deps[i]);
            numDepsUnique++;
        } else {
            ++newrefcounts[j];
        }
    }

    // Apply refcounts
    for (u32 i = 0; i < numDepsUnique; i++) {
        if (procs[i] != NULL) {
            ProcessData_Incref(procs[i], newrefcounts[i]);
        } else {
            remrefcounts[i] += newrefcounts[i];
        }
    }
    ProcessList_Unlock(&g_manager.processList);

    *numDeps = numDepsUnique;
    return res;
}


Result GetTitleExHeaderFlags(ExHeader_Arm11CoreInfo *outCoreInfo, ExHeader_SystemInfoFlags *outSiFlags, const FS_ProgramInfo *programInfo)
{
    Result res = 0;
    u64 programHandle = 0;

    if (g_manager.preparingForReboot) {
        return 0xC8A05801;
    }

    ExHeader_Info *exheaderInfo = ExHeaderInfoHeap_New();
    if (exheaderInfo == NULL) {
        panic(0);
    }

    res = registerProgram(&programHandle, programInfo, programInfo);

    if (R_SUCCEEDED(res))
    {
        res = LOADER_GetProgramInfo(exheaderInfo, programHandle);
        if (R_SUCCEEDED(res)) {
            *outCoreInfo = exheaderInfo->aci.local_caps.core_info;
            *outSiFlags = exheaderInfo->sci.codeset_info.flags;
        }
        LOADER_UnregisterProgram(programHandle);
    }

    ExHeaderInfoHeap_Delete(exheaderInfo);

    return res;
}

Result GetCurrentAppInfo(FS_ProgramInfo *outProgramInfo, u32 *outPid, u32 *outLaunchFlags)
{
    ProcessList_Lock(&g_manager.processList);
    Result res;

    memset(outProgramInfo, 0, sizeof(FS_ProgramInfo));
    if (g_manager.runningApplicationData != NULL) {
        ProcessData *app = g_manager.runningApplicationData;
        outProgramInfo->programId = app->titleId;
        outProgramInfo->mediaType = app->mediaType;
        *outPid = app->pid;
        *outLaunchFlags = app->launchFlags;
        res = 0;
    } else {
        *outPid = 0;
        res = MAKERESULT(RL_TEMPORARY, RS_NOTFOUND, RM_PM, 0x100);
    }
    ProcessList_Unlock(&g_manager.processList);

    return res;
}
