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

#include "utils.h"
#include "globals.h"
#include "synchronization.h"
#include "fatalExceptionHandlers.h"
#include "svc.h"
#include "svc/ConnectToPort.h"
#include "svcHandler.h"
#include "memory.h"

static const u32 *const exceptionsPage = (const u32 *)0xFFFF0000;
void *originalHandlers[8] = {NULL};

enum VECTORS { RESET = 0, UNDEFINED_INSTRUCTION, SVC, PREFETCH_ABORT, DATA_ABORT, RESERVED, IRQ, FIQ };

static void setupSGI0Handler(void)
{
    for(u32 i = 0; i < getNumberOfCores(); i++)
        interruptManager->N3DS.privateInterrupts[i][0].interruptEvent = customInterruptEvent;
}

static inline void **getHandlerDestination(enum VECTORS vector)
{
    u32 *branch_dst = (u32 *)decodeARMBranch((u32 *)exceptionsPage + (u32)vector);
    return (void **)(branch_dst + 2);
}

static inline void swapHandlerInVeneer(enum VECTORS vector, void *handler)
{
    void **dst = getHandlerDestination(vector);
    originalHandlers[(u32)vector] = *dst;
    if(handler != NULL)
        *(void**)PA_FROM_VA_PTR(dst) = handler;
}

static u32 *trampo_;
static bool **enableUserExceptionHandlersForCPUExcLoc;
static bool enableUserExceptionHandlersForCPUExc = true;

static void setupSvcHandler(void)
{
    swapHandlerInVeneer(SVC, svcHandler);

    void **arm11SvcTable = (void**)originalHandlers[(u32)SVC];
    while(*arm11SvcTable != NULL) arm11SvcTable++; //Look for SVC0 (NULL)
    memcpy(officialSVCs, arm11SvcTable, 4 * 0x7E);

    u32 *off;
    for(off = (u32 *)officialSVCs[0x2D]; *off != 0x65736162; off++);
    *(void **)PA_FROM_VA_PTR(arm11SvcTable + 0x2D) = officialSVCs[0x2D] = (void *)off[1];
    trampo_ = (u32 *)PA_FROM_VA_PTR(off + 3);

    CustomBackdoor = (Result (*)(void *, ...))((u32 *)officialSVCs[0x2F] + 2);
    *(void **)PA_FROM_VA_PTR(arm11SvcTable + 0x2F) = officialSVCs[0x2F] = (void *)*((u32 *)officialSVCs[0x2F] + 1);

    off = (u32 *)originalHandlers[(u32) SVC];
    while(*off++ != 0xE1A00009);
    svcFallbackHandler = (void (*)(u8))decodeARMBranch(off);
    for(; *off != 0xE92D000F; off++);
    PostprocessSvc = (void (*)(void))decodeARMBranch(off + 1);
}

static void setupExceptionHandlers(void)
{
    swapHandlerInVeneer(FIQ, FIQHandler);
    swapHandlerInVeneer(UNDEFINED_INSTRUCTION, undefinedInstructionHandler);
    swapHandlerInVeneer(PREFETCH_ABORT, prefetchAbortHandler);
    swapHandlerInVeneer(DATA_ABORT, dataAbortHandler);

    setupSvcHandler();
}

static void findUsefulSymbols(void)
{
    u32 *off;

    KProcessHandleTable__ToKProcess = (KProcess * (*)(KProcessHandleTable *, Handle))decodeARMBranch(5 + (u32 *)officialSVCs[0x76]);

    for(off = (u32 *)KProcessHandleTable__ToKProcess; *off != 0xE28DD014; off++);
    KAutoObject__AddReference = (void (*)(KAutoObject *))decodeARMBranch(off - 1);

    for(; *off != 0xE8BD80F0; off++);
    KProcessHandleTable__ToKAutoObject = (KAutoObject * (*)(KProcessHandleTable *, Handle))decodeARMBranch(off + 2);

    for(off = (u32 *)decodeARMBranch(3 + (u32 *)officialSVCs[9]); /* KThread::Terminate */ *off != 0xE5D42034; off++);
    off -= 2;
    criticalSectionLock = (KRecursiveLock *)off[2 + (off[0] & 0xFF) / 4];
    KRecursiveLock__Lock = (void (*)(KRecursiveLock *))decodeARMBranch(off + 1);
    off += 4;

    for(; (*off >> 16) != 0xE59F; off++);
    KRecursiveLock__Unlock = (void (*)(KRecursiveLock *))decodeARMBranch(off + 1);

    for(; *off != 0xE5C4007D; off++);
    KSynchronizationObject__Signal = (void (*)(KSynchronizationObject *, bool))decodeARMBranch(off + 3);

    for(off = (u32 *)officialSVCs[0x19]; *off != 0xE1A04005; off++);
    KEvent__Clear = (Result (*)(KEvent *))decodeARMBranch(off + 1);
    for(off = (u32 *)KEvent__Clear; *off != 0xE8BD8070; off++)
    synchronizationMutex = *(KObjectMutex **)(off + 1);

    for(off = (u32 *)officialSVCs[0x24]; *off != 0xE59F004C; off++);
    WaitSynchronization1 = (Result (*)(void *, KThread *, KSynchronizationObject *, s64))decodeARMBranch(off + 6);

    for(off = (u32 *)decodeARMBranch(3 + (u32 *)officialSVCs[0x33]) /* OpenProcess */ ; *off != 0xE20030FF; off++);
    KProcessHandleTable__CreateHandle = (Result (*)(KProcessHandleTable *, Handle *, KAutoObject *, u8))decodeARMBranch(off + 2);

    for(off = (u32 *)decodeARMBranch(3 + (u32 *)officialSVCs[0x34]) /* OpenThread */; *off != 0xD9001BF7; off++);
    threadList = *(KObjectList **)(off + 1);

    KProcessHandleTable__ToKThread = (KThread * (*)(KProcessHandleTable *, Handle))decodeARMBranch((u32 *)decodeARMBranch((u32 *)officialSVCs[0x37] + 3) /* GetThreadId */ + 5);

    for(off = (u32 *)officialSVCs[0x54]; *off != 0xE8BD8008; off++);
    flushDataCacheRange = (void (*)(void *, u32))(*(u32 **)(off[1]) + 3);

    for(off = (u32 *)officialSVCs[0x71]; *off != 0xE2101102; off++);
    KProcessHwInfo__MapProcessMemory = (Result (*)(KProcessHwInfo *, KProcessHwInfo *, void *, void *, u32))decodeARMBranch(off - 1);

    for(off = (u32 *)officialSVCs[0x72]; *off != 0xE2041102; off++);
    KProcessHwInfo__UnmapProcessMemory = (Result (*)(KProcessHwInfo *, void *, u32))decodeARMBranch(off - 1);

    for(off = (u32 *)officialSVCs[0x7C]; *off != 0x03530000; off++);
    KObjectMutex__WaitAndAcquire = (void (*)(KObjectMutex *))decodeARMBranch(++off);
    for(; *off != 0xE320F000; off++);
    KObjectMutex__ErrorOccured = (void (*)(void))decodeARMBranch(off + 1);

    for(off = (u32 *)originalHandlers[(u32) DATA_ABORT]; *off != (u32)exceptionStackTop; off++);
    kernelUsrCopyFuncsStart = (void *)off[1];
    kernelUsrCopyFuncsEnd = (void *)off[2];

    u32 n_cmp_0;
    for(off = (u32 *)kernelUsrCopyFuncsStart, n_cmp_0 = 1; n_cmp_0 <= 6; off++)
    {
        if(*off == 0xE3520000)
        {
            // We're missing some funcs
            switch(n_cmp_0)
            {
                case 1:
                    usrToKernelMemcpy8 = (bool (*)(void *, const void *, u32))off;
                    break;
                case 2:
                    usrToKernelMemcpy32 = (bool (*)(u32 *, const u32 *, u32))off;
                    break;
                case 3:
                    usrToKernelStrncpy = (s32 (*)(char *, const char *, u32))off;
                    break;
                case 4:
                    kernelToUsrMemcpy8 = (bool (*)(void *, const void *, u32))off;
                    break;
                case 5:
                    kernelToUsrMemcpy32 = (bool (*)(u32 *, const u32 *, u32))off;
                    break;
                case 6:
                    kernelToUsrStrncpy = (s32 (*)(char *, const char *, u32))off;
                    break;
                default: break;
            }
            n_cmp_0++;
        }
    }

    // The official prototype of ControlMemory doesn't have that extra param'
    ControlMemory = (Result (*)(u32 *, u32, u32, u32, MemOp, MemPerm, bool))
                    decodeARMBranch((u32 *)officialSVCs[0x01] + 5);
    SleepThread = (void (*)(s64))officialSVCs[0x0A];
    CloseHandle = (Result (*)(Handle))officialSVCs[0x23];
    GetSystemInfo = (Result (*)(s64 *, s32, s32))decodeARMBranch((u32 *)officialSVCs[0x2A] + 3);
    GetProcessInfo = (Result (*)(s64 *, Handle, u32))decodeARMBranch((u32 *)officialSVCs[0x2B] + 3);
    GetThreadInfo = (Result (*)(s64 *, Handle, u32))decodeARMBranch((u32 *)officialSVCs[0x2C] + 3);
    ConnectToPort = (Result (*)(Handle *, const char*))decodeARMBranch((u32 *)officialSVCs[0x2D] + 3);
    SendSyncRequest = (Result (*)(Handle))officialSVCs[0x32];
    OpenProcess = (Result (*)(Handle *, u32))decodeARMBranch((u32 *)officialSVCs[0x33] + 3);
    GetProcessId = (Result (*)(u32 *, Handle))decodeARMBranch((u32 *)officialSVCs[0x35] + 3);
    DebugActiveProcess = (Result (*)(Handle *, u32))decodeARMBranch((u32 *)officialSVCs[0x60] + 3);
    KernelSetState = (Result (*)(u32, u32, u32, u32))((u32 *)officialSVCs[0x7C] + 1);

    for(off = (u32 *)svcFallbackHandler; *off != 0xE8BD4010; off++);
    kernelpanic = (void (*)(void))off;

    for(off = (u32 *)0xFFFF0000; off[0] != 0xE3A01002 || off[1] != 0xE3A00004; off++);
    SignalDebugEvent = (Result (*)(DebugEventType type, u32 info, ...))decodeARMBranch(off + 2);

    for(; *off != 0x96007F9; off++);
    isDevUnit = *(bool **)(off - 1);
    enableUserExceptionHandlersForCPUExcLoc = (bool **)(off + 1);

    ///////////////////////////////////////////

    // Shitty/lazy heuristic but it works on even 4.5, so...
    u32 textStart = ((u32)originalHandlers[(u32) SVC]) & ~0xFFFF;
    u32 rodataStart = (u32)(interruptManager->N3DS.privateInterrupts[0][6].interruptEvent->vtable) & ~0xFFF;

    u32 textSize = rodataStart - textStart;
    for(off = (u32 *)textStart; off < (u32 *)(textStart + textSize) - 3; off++)
    {
        if(off[0] == 0xE5D13034 && off[1] == 0xE1530002)
            KScheduler__AdjustThread = (void (*)(KScheduler *, KThread *, u32))off;
        else if(off[0] == (u32)interruptManager && off[1] == (u32)&currentCoreContext->objectContext)
            KScheduler__AttemptSwitchingThreadContext = (void (*)(KScheduler *))(off - 2);
        else if(off[0] == 0xE3510B1A && off[1] == 0xE3A06000)
        {
            u32 *off2;
            for(off2 = off; *off2 != 0xE92D40F8; off2--);
            invalidateInstructionCacheRange = (void (*)(void *, u32))off2;
        }
    }
}

struct Parameters
{
    void (*SGI0HandlerCallback)(struct Parameters *, u32 *);
    InterruptManager *interruptManager;
    u32 *L2MMUTable; // bit31 mapping

    void (*initFPU)(void);
    void (*mcuReboot)(void);
    void (*coreBarrier)(void);

    u32 TTBCR;
    u32 L1MMUTableAddrs[4];

    u32 kernelVersion;

    CfwInfo cfwInfo;
};

static void enableDebugFeatures(void)
{
    *isDevUnit = true; // for debug SVCs and user exc. handlers, etc.
    *(bool **)PA_FROM_VA_PTR(enableUserExceptionHandlersForCPUExcLoc) = &enableUserExceptionHandlersForCPUExc;

    u32 *off;
    for(off = (u32 *)officialSVCs[0x7C]; off[0] != 0xE5D00001 || off[1] != 0xE3500000; off++);
    *(u32 *)PA_FROM_VA_PTR(off + 2) = 0xE1A00000; // in case 6: beq -> nop

    for(off = (u32 *)DebugActiveProcess; *off != 0xE3110001; off++);
    *(u32 *)PA_FROM_VA_PTR(off) = 0xE3B01001; // tst r1, #1 -> movs r1, #1
}

static void doOtherPatches(void)
{
    u32 *kpanic = (u32 *)kernelpanic;
    *(u32 *)PA_FROM_VA_PTR(kpanic) = 0xE12FFF7E; // bkpt 0xFFFE

    u32 *off;
    for(off = (u32 *)ControlMemory; (off[0] & 0xFFF0FFFF) != 0xE3500001 || (off[1] & 0xFFFF0FFF) != 0x13A00000; off++);
    off -= 2;

    /*
        Here we replace currentProcess->processID == 1 by additionnalParameter == 1.
        This patch should be generic enough to work even on firmware version 5.0.

        It effectively changes the prototype of the ControlMemory function which
        only caller is the svc 0x01 handler on OFW.
    */
    *(u32 *)PA_FROM_VA_PTR(off) = 0xE59D0000 | (*off & 0x0000F000) | (8 + computeARMFrameSize((u32 *)ControlMemory)); // ldr r0, [sp, #(frameSize + 8)]

}

void main(volatile struct Parameters *p)
{
    isN3DS = getNumberOfCores() == 4;
    interruptManager = p->interruptManager;

    initFPU = p->initFPU;
    mcuReboot = p->mcuReboot;
    coreBarrier = p->coreBarrier;

    TTBCR = p->TTBCR;
    memcpy(L1MMUTableAddrs, (const void *)p->L1MMUTableAddrs, 16);
    exceptionStackTop = (u32 *)0xFFFF2000 + (1 << (32 - TTBCR - 20));
    kernelVersion = p->kernelVersion;
    cfwInfo = p->cfwInfo;

    setupSGI0Handler();
    setupExceptionHandlers();
    findUsefulSymbols();
    enableDebugFeatures();
    doOtherPatches();

    rosalinaState = 0;
    hasStartedRosalinaNetworkFuncsOnce = false;
    *trampo_ = (u32)ConnectToPortHookWrapper;
}
