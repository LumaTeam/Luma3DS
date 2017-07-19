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

#include "globals.h"

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
Result (*KProcessHwInfo__MapProcessMemory)(KProcessHwInfo *this, KProcessHwInfo *other, void *dst, void *src, u32 nbPages);
Result (*KProcessHwInfo__UnmapProcessMemory)(KProcessHwInfo *this, void *addr, u32 nbPages);
Result (*KEvent__Clear)(KEvent *this);
void (*KObjectMutex__WaitAndAcquire)(KObjectMutex *this);
void (*KObjectMutex__ErrorOccured)(void);

void (*KScheduler__AdjustThread)(KScheduler *this, KThread *thread, u32 oldSchedulingMask);
void (*KScheduler__AttemptSwitchingThreadContext)(KScheduler *this);

Result (*ControlMemory)(u32 *addrOut, u32 addr0, u32 addr1, u32 size, MemOp op, MemPerm perm, bool isLoader);
void (*SleepThread)(s64 ns);
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
KCoreContext *coreCtxs;

void *originalHandlers[8] = {NULL};

u32 nbSection0Modules;

Result (*InterruptManager__MapInterrupt)(InterruptManager *manager, KBaseInterruptEvent *iEvent, u32 interruptID,
                                         u32 coreID, u32 priority, bool disableUponReceipt, bool levelHighActive);
InterruptManager *interruptManager;
KBaseInterruptEvent *customInterruptEvent;

void (*initFPU)(void);
void (*mcuReboot)(void);
void (*coreBarrier)(void);

CfwInfo cfwInfo;

vu32 rosalinaState;
bool hasStartedRosalinaNetworkFuncsOnce;
