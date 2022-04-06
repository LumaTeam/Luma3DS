/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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

#include "globals.h"
#include "utils.h"
#include "ipc.h"

KRecursiveLock *criticalSectionLock;
KObjectList *threadList;
KObjectMutex *synchronizationMutex;

void (*KRecursiveLock__Lock)(KRecursiveLock *this);
void (*KRecursiveLock__Unlock)(KRecursiveLock *this);

void (*KAutoObject__AddReference)(KAutoObject *this);
KProcess * (*KProcessHandleTable__ToKProcess)(KProcessHandleTable *this, Handle processHandle);
KThread * (*KProcessHandleTable__ToKThread)(KProcessHandleTable *this, Handle threadHandle);
KAutoObject * (*KProcessHandleTable__ToKAutoObject)(KProcessHandleTable *this, Handle handle);
void (*KSynchronizationObject__Signal)(KSynchronizationObject *this, bool isPulse);
Result (*WaitSynchronization1)(void *this_unused, KThread *thread, KSynchronizationObject *syncObject, s64 timeout);
Result (*KProcessHandleTable__CreateHandle)(KProcessHandleTable *this, Handle *out, KAutoObject *obj, u8 token);
Result (*KProcessHwInfo__QueryMemory)(KProcessHwInfo *this, MemoryInfo *memoryInfo, PageInfo *pageInfo, void *address);
Result (*KProcessHwInfo__MapProcessMemory)(KProcessHwInfo *this, KProcessHwInfo *other, void *dst, void *src, u32 nbPages);
Result (*KProcessHwInfo__UnmapProcessMemory)(KProcessHwInfo *this, void *addr, u32 nbPages);
Result (*KProcessHwInfo__CheckVaState)(KProcessHwInfo *hwInfo, u32 va, u32 size, u32 state, u32 perm);
Result (*KProcessHwInfo__GetListOfKBlockInfoForVA)(KProcessHwInfo *hwInfo, KLinkedList *list, u32 va, u32 sizeInPage);
Result (*KProcessHwInfo__MapListOfKBlockInfo)(KProcessHwInfo *this, u32 va, KLinkedList *list, u32 state, u32 perm, u32 sbz);
Result (*KEvent__Clear)(KEvent *this);
Result (*KEvent__Signal)(KEvent *this);

void (*KObjectMutex__WaitAndAcquire)(KObjectMutex *this);
void (*KObjectMutex__ErrorOccured)(void);

void (*KScheduler__AdjustThread)(KScheduler *this, KThread *thread, u32 oldSchedulingMask);
void (*KScheduler__AttemptSwitchingThreadContext)(KScheduler *this);

void (*KLinkedList_KBlockInfo__Clear)(KLinkedList *list);

Result (*ControlMemory)(u32 *addrOut, u32 addr0, u32 addr1, u32 size, MemOp op, MemPerm perm, bool isLoader);
Result (*doControlMemory)(KProcessHwInfo *this, u32 addr, u32 requestedNbPages, u32 pa, u32 state, u32 perm, u32 a7, u32 region);

void (*SleepThread)(s64 ns);
Result (*CreateEvent)(Handle *out, ResetType resetType);
Result (*CloseHandle)(Handle handle);
Result (*GetHandleInfo)(s64 *out, Handle handle, u32 type);
Result (*GetSystemInfo)(s64 *out, s32 type, s32 param);
Result (*GetProcessInfo)(s64 *out, Handle processHandle, u32 type);
Result (*GetThreadInfo)(s64 *out, Handle threadHandle, u32 type);
Result (*ConnectToPort)(Handle *out, const char *name);
Result (*SendSyncRequest)(Handle handle);
Result (*OpenProcess)(Handle *out, u32 processId);
Result (*GetProcessId)(u32 *out, Handle process);
Result (*DebugActiveProcess)(Handle *out, u32 processId);
Result (*SignalEvent)(Handle event);
Result (*UnmapProcessMemory)(Handle processHandle, void *dst, u32 size);
Result (*KernelSetState)(u32 type, u32 varg1, u32 varg2, u32 varg3);

void (*flushDataCacheRange)(void *addr, u32 len);
void (*invalidateInstructionCacheRange)(void *addr, u32 len);

bool (*usrToKernelMemcpy8)(void *dst, const void *src, u32 len);
bool (*usrToKernelMemcpy32)(u32 *dst, const u32 *src, u32 len);
s32 (*usrToKernelStrncpy)(char *dst, const char *src, u32 len);
bool (*kernelToUsrMemcpy8)(void *dst, const void *src, u32 len);
bool (*kernelToUsrMemcpy32)(u32 *dst, const u32 *src, u32 len);
s32 (*kernelToUsrStrncpy)(char *dst, const char *src, u32 len);

void (*svcFallbackHandler)(u8 svcId);
void (*kernelpanic)(void);
void (*officialPostProcessSvc)(void);

Result (*SignalDebugEvent)(DebugEventType type, u32 info, ...);

bool isN3DS;
u32 *exceptionStackTop;

u32 TTBCR;
u32 L1MMUTableAddrs[4];

void *kernelUsrCopyFuncsStart, *kernelUsrCopyFuncsEnd;

bool *isDevUnit;

vu8 *configPage;
u32 kernelVersion;
FcramLayout fcramLayout;
FcramDescriptor *fcramDescriptor;
KCoreContext *coreCtxs;

void *originalHandlers[8] = {NULL};

u32 nbSection0Modules;

Result (*InterruptManager__MapInterrupt)(InterruptManager *manager, KBaseInterruptEvent *iEvent, u32 interruptID,
                                         u32 coreID, u32 priority, bool disableUponReceipt, bool levelHighActive);
InterruptManager *interruptManager;

void  (*initFPU)(void);
void  (*mcuReboot)(void);
void  (*coreBarrier)(void);
void* (*kAlloc)(FcramDescriptor *fcramDesc, u32 nbPages, u32 alignment, u32 region);

CfwInfo cfwInfo;
u32 kextBasePa;
u32 stolenSystemMemRegionSize;

vu32 rosalinaState;
bool hasStartedRosalinaNetworkFuncsOnce;
KEvent* signalPluginEvent = NULL;
u32 pidOffsetKProcess, hwInfoOffsetKProcess, codeSetOffsetKProcess, handleTableOffsetKProcess, debugOffsetKProcess;

KLinkedList*    KLinkedList__Initialize(KLinkedList *list)
{
    list->size = 0;
    list->nodes.first = list->nodes.last = (KLinkedListNode *)&list->nodes;
    return list;
}

void     PLG_SignalEvent(u32 event)
{
    KThread     *currentThread = currentCoreContext->objectContext.currentThread;

    // Set configuration memory field with event
    *(vu32 *)PA_FROM_VA_PTR((u32 *)0x1FF800F0) |= event;

    // Send notification 0x1001
    {
        u32     *cmdbuf = (u32 *)((u8 *)currentCoreContext->objectContext.currentThread->threadLocalStorage + 0x80);
        u32     backup[3] = { cmdbuf[0], cmdbuf[1], cmdbuf[2] };
        Handle  srvHandle;
        SessionInfo *info = SessionInfo_FindFirst("srv:");

        Result  res = createHandleForThisProcess(&srvHandle, &info->session->clientSession.syncObject.autoObject);

        if (res >= 0)
        {
            cmdbuf[0] = 0x000C0080;
            cmdbuf[1] = 0x1001;
            cmdbuf[2] = 0;

            SendSyncRequest(srvHandle);
            CloseHandle(srvHandle);
        }

        cmdbuf[0] = backup[0]; cmdbuf[1] = backup[1]; cmdbuf[2] = backup[2];
    }
    // Wait for notification 0x1002
    WaitSynchronization1(NULL, currentThread, (KSynchronizationObject *)signalPluginEvent, U64_MAX);
}

void    PLG__WakeAppThread(void)
{
    KEvent__Signal(signalPluginEvent);
}

u32      PLG_GetStatus(void)
{
    return (*(vu32 *)PA_FROM_VA_PTR((u32 *)0x1FF800F0)) & 0xFFFF;
}
