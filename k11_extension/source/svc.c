/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include "memory.h"
#include "synchronization.h"
#include "svc.h"
#include "svc/ControlMemory.h"
#include "svc/GetHandleInfo.h"
#include "svc/GetSystemInfo.h"
#include "svc/GetProcessInfo.h"
#include "svc/GetThreadInfo.h"
#include "svc/GetCFWInfo.h"
#include "svc/ConnectToPort.h"
#include "svc/SendSyncRequest.h"
#include "svc/Break.h"
#include "svc/SetGpuProt.h"
#include "svc/SetWifiEnabled.h"
#include "svc/Backdoor.h"
#include "svc/KernelSetState.h"
#include "svc/CustomBackdoor.h"
#include "svc/MapProcessMemoryEx.h"
#include "svc/UnmapProcessMemoryEx.h"
#include "svc/ControlService.h"
#include "svc/CopyHandle.h"
#include "svc/TranslateHandle.h"

void *officialSVCs[0x7E] = {NULL};

void signalSvcEntry(u8 *pageEnd)
{
    u32 svcId = (u32) *(u8 *)(pageEnd - 0xB5);
    KProcess *currentProcess = currentCoreContext->objectContext.currentProcess;

    if(svcId == 0xFE)
        svcId = *(u32 *)(pageEnd - 0x110 + 8 * 4); // r12 ; note: max theortical SVC atm: 0x3FFFFFFF. We don't support catching svcIds >= 0x100 atm either

    // Since DBGEVENT_SYSCALL_ENTRY is non blocking, we'll cheat using EXCEVENT_UNDEFINED_SYSCALL (debug->svcId is fortunately an u16!)
    if(debugOfProcess(currentProcess) != NULL && shouldSignalSyscallDebugEvent(currentProcess, svcId))
        SignalDebugEvent(DBGEVENT_OUTPUT_STRING, 0xFFFFFFFE, svcId);
}

void signalSvcReturn(u8 *pageEnd)
{
    u32 svcId = (u32) *(u8 *)(pageEnd - 0xB5);
    KProcess *currentProcess = currentCoreContext->objectContext.currentProcess;

    if(svcId == 0xFE)
        svcId = *(u32 *)(pageEnd - 0x110 + 8 * 4); // r12 ; note: max theortical SVC atm: 0x1FFFFFFF. We don't support catching svcIds >= 0x100 atm either

    // Since DBGEVENT_SYSCALL_RETURN is non blocking, we'll cheat using EXCEVENT_UNDEFINED_SYSCALL (debug->svcId is fortunately an u16!)
    if(debugOfProcess(currentProcess) != NULL && shouldSignalSyscallDebugEvent(currentProcess, svcId))
        SignalDebugEvent(DBGEVENT_OUTPUT_STRING, 0xFFFFFFFF, svcId);
}

void postprocessSvc(void)
{
    KThread *currentThread = currentCoreContext->objectContext.currentThread;
    if(!currentThread->shallTerminate && rosalinaThreadLockPredicate(currentThread))
        rosalinaRescheduleThread(currentThread, true);

    officialPostProcessSvc();
}

static bool doingVeryShittyPmResLimitWorkaround = false; // I feel dirty

void *svcHook(u8 *pageEnd)
{
    KProcess *currentProcess = currentCoreContext->objectContext.currentProcess;

    u32 svcId = *(u8 *)(pageEnd - 0xB5);
    if(svcId == 0xFE)
        svcId = *(u32 *)(pageEnd - 0x110 + 8 * 4); // r12 ; note: max theortical SVC atm: 0x3FFFFFFF. We don't support catching svcIds >= 0x100 atm either
    switch(svcId)
    {
        case 0x01:
            return ControlMemoryHookWrapper;
        case 0x17:
            if(strcmp(codeSetOfProcess(currentProcess)->processName, "pm") == 0) // only called twice in pm, by the same function
            {
                *(vu32 *)(configPage + 0x44) += __end__ - __start__;
                doingVeryShittyPmResLimitWorkaround = true;
            }
            return officialSVCs[0x17];
        case 0x29:
            return GetHandleInfoHookWrapper;
        case 0x2A:
            return GetSystemInfoHookWrapper;
        case 0x2B:
            return GetProcessInfoHookWrapper;
        case 0x2C:
            return GetThreadInfoHookWrapper;
        case 0x2D:
            return ConnectToPortHookWrapper;
        case 0x2E:
            return GetCFWInfo; // DEPRECATED
        case 0x32:
            return SendSyncRequestHook;
        case 0x3C:
            return (debugOfProcess(currentProcess) != NULL) ? officialSVCs[0x3C] : (void *)Break;
        case 0x59:
            return SetGpuProt;
        case 0x5A:
            return SetWifiEnabled;
        case 0x79:
            if(doingVeryShittyPmResLimitWorkaround)
            {
                *(vu32 *)(configPage + 0x44) -= __end__ - __start__;
                doingVeryShittyPmResLimitWorkaround = false;
            }
            return officialSVCs[0x79];
        case 0x7B:
            return Backdoor;
        case 0x7C:
            return KernelSetStateHook;


        case 0x80:
            return CustomBackdoor;

        case 0x90:
            return convertVAToPA;
        case 0x91:
            return flushDataCacheRange;
        case 0x92:
            return flushEntireDataCache;
        case 0x93:
            return invalidateInstructionCacheRange;
        case 0x94:
            return invalidateEntireInstructionCache;

        case 0xA0:
            return MapProcessMemoryEx;
        case 0xA1:
            return UnmapProcessMemoryEx;
        case 0xA2:
            return ControlMemoryEx;

        case 0xB0:
            return ControlService;
        case 0xB1:
            return CopyHandleWrapper;
        case 0xB2:
            return TranslateHandleWrapper;

        default:
            return (svcId <= 0x7D) ? officialSVCs[svcId] : NULL;
    }
}
