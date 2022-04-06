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

#pragma once

#include "config.h"
#include "kernel.h"

extern KRecursiveLock *criticalSectionLock;
extern KObjectList *threadList;
extern KObjectList *resourceLimitList;
extern KObjectMutex *synchronizationMutex;

extern void (*KRecursiveLock__Lock)(KRecursiveLock *this);
extern void (*KRecursiveLock__Unlock)(KRecursiveLock *this);

extern void (*KAutoObject__AddReference)(KAutoObject *this);

extern KProcess * (*KProcessHandleTable__ToKProcess)(KProcessHandleTable *this, Handle processHandle);
extern KThread * (*KProcessHandleTable__ToKThread)(KProcessHandleTable *this, Handle threadHandle);
extern KAutoObject * (*KProcessHandleTable__ToKAutoObject)(KProcessHandleTable *this, Handle handle);
extern void (*KSynchronizationObject__Signal)(KSynchronizationObject *this, bool isPulse);
extern Result (*WaitSynchronization1)(void *this_unused, KThread *thread, KSynchronizationObject *syncObject, s64 timeout);
extern Result (*KProcessHandleTable__CreateHandle)(KProcessHandleTable *this, Handle *out, KAutoObject *obj, u8 token);
extern Result (*KProcessHwInfo__QueryMemory)(KProcessHwInfo *this, MemoryInfo *memoryInfo, PageInfo *pageInfo, void *address);
extern Result (*KProcessHwInfo__MapProcessMemory)(KProcessHwInfo *this, KProcessHwInfo *other, void *dst, void *src, u32 nbPages);
extern Result (*KProcessHwInfo__UnmapProcessMemory)(KProcessHwInfo *this, void *addr, u32 nbPages);
extern Result (*KProcessHwInfo__CheckVaState)(KProcessHwInfo *hwInfo, u32 va, u32 size, u32 state, u32 perm);
extern Result (*KProcessHwInfo__GetListOfKBlockInfoForVA)(KProcessHwInfo *hwInfo, KLinkedList *list, u32 va, u32 sizeInPage);
extern Result (*KProcessHwInfo__MapListOfKBlockInfo)(KProcessHwInfo *this, u32 va, KLinkedList *list, u32 state, u32 perm, u32 sbz);
extern Result (*KEvent__Clear)(KEvent *this);
extern Result (*KEvent__Signal)(KEvent *this);
extern void (*KObjectMutex__WaitAndAcquire)(KObjectMutex *this);
extern void (*KObjectMutex__ErrorOccured)(void);

extern void (*KScheduler__AdjustThread)(KScheduler *this, KThread *thread, u32 oldSchedulingMask);
extern void (*KScheduler__AttemptSwitchingThreadContext)(KScheduler *this);

extern void (*KLinkedList_KBlockInfo__Clear)(KLinkedList *list);

extern Result (*ControlMemory)(u32 *addrOut, u32 addr0, u32 addr1, u32 size, MemOp op, MemPerm perm, bool isLoader);
extern Result (*doControlMemory)(KProcessHwInfo *this, u32 addr, u32 requestedNbPages, u32 pa, u32 state, u32 perm, u32 a7, u32 region);
extern void (*SleepThread)(s64 ns);
extern Result (*CreateEvent)(Handle *out, ResetType resetType);
extern Result (*CloseHandle)(Handle handle);
extern Result (*GetHandleInfo)(s64 *out, Handle handle, u32 type);
extern Result (*GetSystemInfo)(s64 *out, s32 type, s32 param);
extern Result (*GetProcessInfo)(s64 *out, Handle processHandle, u32 type);
extern Result (*GetThreadInfo)(s64 *out, Handle threadHandle, u32 type);
extern Result (*ConnectToPort)(Handle *out, const char *name);
extern Result (*SendSyncRequest)(Handle handle);
extern Result (*OpenProcess)(Handle *out, u32 processId);
extern Result (*GetProcessId)(u32 *out, Handle process);
extern Result (*DebugActiveProcess)(Handle *out, u32 processId);
extern Result (*SignalEvent)(Handle event);
extern Result (*UnmapProcessMemory)(Handle processHandle, void *dst, u32 size);
extern Result (*KernelSetState)(u32 type, u32 varg1, u32 varg2, u32 varg3);

extern void (*flushDataCacheRange)(void *addr, u32 len);
extern void (*invalidateInstructionCacheRange)(void *addr, u32 len);

extern bool (*usrToKernelMemcpy8)(void *dst, const void *src, u32 len);
extern bool (*usrToKernelMemcpy32)(u32 *dst, const u32 *src, u32 len);
extern s32 (*usrToKernelStrncpy)(char *dst, const char *src, u32 len);
extern bool (*kernelToUsrMemcpy8)(void *dst, const void *src, u32 len);
extern bool (*kernelToUsrMemcpy32)(u32 *dst, const u32 *src, u32 len);
extern s32 (*kernelToUsrStrncpy)(char *dst, const char *src, u32 len);

extern void (*svcFallbackHandler)(u8 svcId);
extern void (*kernelpanic)(void);
extern void (*officialPostProcessSvc)(void);

extern Result (*SignalDebugEvent)(DebugEventType type, u32 info, ...);

extern bool isN3DS;
extern u32 *exceptionStackTop;

extern u32 TTBCR;
extern u32 L1MMUTableAddrs[4];

extern void *kernelUsrCopyFuncsStart, *kernelUsrCopyFuncsEnd;

extern bool *isDevUnit;

extern vu8 *configPage;
extern u32 kernelVersion;
extern FcramLayout fcramLayout;
extern FcramDescriptor *fcramDescriptor;

extern KCoreContext *coreCtxs;

extern void *originalHandlers[8];
extern u32 nbSection0Modules;

extern u8 __start__[], __end__[], __bss_start__[], __bss_end__[];

extern Result (*InterruptManager__MapInterrupt)(InterruptManager *manager, KBaseInterruptEvent *iEvent, u32 interruptID,
                                                u32 coreID, u32 priority, bool disableUponReceipt, bool levelHighActive);
extern InterruptManager *interruptManager;
extern KBaseInterruptEvent *customInterruptEvent;

extern void  (*initFPU)(void);
extern void  (*mcuReboot)(void);
extern void  (*coreBarrier)(void);
extern void* (*kAlloc)(FcramDescriptor *fcramDesc, u32 nbPages, u32 alignment, u32 region);

typedef struct CfwInfo
{
    char magic[4];

    u8 versionMajor;
    u8 versionMinor;
    u8 versionBuild;
    u8 flags;

    u32 commitHash;

    u16 configFormatVersionMajor, configFormatVersionMinor;
    u32 config, multiConfig, bootConfig;
    u32 splashDurationMsec;
    u64 hbldr3dsxTitleId;
    u32 rosalinaMenuCombo;
    u32 pluginLoaderFlags;
    u16 screenFiltersCct;
    s16 ntpTzOffetMinutes;
} CfwInfo;

extern CfwInfo cfwInfo;
extern u32 kextBasePa;
extern u32 stolenSystemMemRegionSize;

extern vu32 rosalinaState;
extern bool hasStartedRosalinaNetworkFuncsOnce;
extern KEvent* signalPluginEvent;

typedef enum
{
    PLG_CFG_NONE = 0,
    PLG_CFG_RUNNING = 1,
    PLG_CFG_SWAPPED = 2,

    PLG_CFG_SWAP_EVENT = 1 << 16,
    PLG_CFG_EXIT_EVENT = 2 << 16
}   PLG_CFG_STATUS;

void    PLG_SignalEvent(u32 event);
void    PLG__WakeAppThread(void);
u32     PLG_GetStatus(void);
KLinkedList*    KLinkedList__Initialize(KLinkedList *list);
