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
#include <string.h>
#include "utils.h"
#include "globals.h"
#include "synchronization.h"
#include "fatalExceptionHandlers.h"
#include "svc.h"
#include "svc/ConnectToPort.h"
#include "svcHandler.h"

#define K11EXT_VA         0x70000000

struct KExtParameters
{
    u32 basePA;
    u32 stolenSystemMemRegionSize;
    void *originalHandlers[4];
    u32 L1MMUTableAddrs[4];

    volatile bool done;

    CfwInfo cfwInfo;
} kExtParameters = { .basePA = 0x12345678 }; // place this in .data

static ALIGN(1024) u32 g_L2Table[256] = {0};

void relocateAndSetupMMU(u32 coreId, u32 *L1Table)
{
    struct KExtParameters *p0 = (struct KExtParameters *)((u32)&kExtParameters - K11EXT_VA + 0x18000000);
    struct KExtParameters *p = (struct KExtParameters *)((u32)&kExtParameters - K11EXT_VA + p0->basePA);
    u32 *L2Table = (u32 *)((u32)g_L2Table - K11EXT_VA + p0->basePA);

    if(coreId == 0)
    {
        // Relocate ourselves, and clear BSS
        // This is only OK because the jumps will be relative & there's no mode switch...
        memcpy((void *)p0->basePA, (const void *)0x18000000, __bss_start__ - __start__);
        memset((u32 *)(p0->basePA + (__bss_start__ - __start__)), 0, __bss_end__ - __bss_start__);

        // Map the kernel ext at K11EXT_VA
        // 4KB extended small pages:
        // Outer Write-Through cached, No Allocate on Write, Buffered
        // Inner Cached Write-Back Write-Allocate, Buffered
        // This was changed at some point (8.0 maybe?), it was outer noncached before
        for(u32 offset = 0; offset < (u32)(__end__ - __start__); offset += 0x1000)
            L2Table[offset >> 12] = (p0->basePA + offset) | 0x596;

        p0->done = true;

        // DSB, Flush Prefetch Buffer (more or less "isb")
        __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
        __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" :: "r" (0) : "memory");

        __asm__ __volatile__ ("sev");
    }
    else {
        do
        {
            __asm__ __volatile__ ("wfe");
        } while(!p0->done);

        // DSB, Flush Prefetch Buffer (more or less "isb")
        __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
        __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" :: "r" (0) : "memory");
    }
    // bit31 idea thanks to SALT
    // Maps physmem so that, if addr is in physmem(0, 0x30000000), it can be accessed uncached&rwx as addr|(1<<31)
    u32 attribs = 0x40C02; // supersection (rwx for all) of strongly ordered memory, shared
    for(u32 PA = 0; PA < 0x30000000; PA += 0x01000000)
    {
        u32 VA = (1 << 31) | PA;
        for(u32 i = 0; i < 16; i++)
            L1Table[i + (VA >> 20)] = PA | attribs;
    }

    L1Table[K11EXT_VA >> 20] = (u32)L2Table | 1;

    p->L1MMUTableAddrs[coreId] = (u32)L1Table;

    // DSB, Flush Prefetch Buffer (more or less "isb")
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" :: "r" (0) : "memory");
}

void bindSGI0Hook(void)
{
    if(InterruptManager__MapInterrupt(interruptManager, customInterruptEvent, 0, getCurrentCoreID(), 0, false, false) != 0)
        __asm__ __volatile__ ("bkpt 0xdead");
}

void configHook(vu8 *cfgPage)
{
    configPage = cfgPage;

    kernelVersion = *(vu32 *)configPage;
    *(vu32 *)(configPage + 0x40) = fcramLayout.applicationSize;
    *(vu32 *)(configPage + 0x44) = fcramLayout.systemSize;
    *(vu32 *)(configPage + 0x48) = fcramLayout.baseSize;
    *isDevUnit = true; // enable debug features

    pidOffsetKProcess = KPROCESS_OFFSETOF(processId);
    hwInfoOffsetKProcess = KPROCESS_OFFSETOF(hwInfo);
    codeSetOffsetKProcess = KPROCESS_OFFSETOF(codeSet);
    handleTableOffsetKProcess = KPROCESS_OFFSETOF(handleTable);
    debugOffsetKProcess = KPROCESS_OFFSETOF(debug);
}

void KProcessHwInfo__MapL1Section_Hook(void);
void KProcessHwInfo__MapL2Section_Hook(void);

static void installMmuHooks(void)
{
    u32 *mapL1Section = NULL;
    u32 *mapL2Section = NULL;
    u32 *off;

    for(off = (u32 *)officialSVCs[0x1F]; *off != 0xE1CD60F0; ++off);
    off = decodeArmBranch(off + 1);

    for (; *off != 0xE58D5000; ++off);
    off = decodeArmBranch(off + 1);

    for (; *off != 0xE58DC000; ++off);
    off = decodeArmBranch(off + 1);
    for (; *off != 0xE1A0000B; ++off);
    off = decodeArmBranch(off + 1);
    for (; *off != 0xE59D2030; ++off);
    off = decodeArmBranch(off + 1);

    for (; *off != 0xE88D1100; ++off);
    mapL2Section = (u32 *)PA_FROM_VA_PTR(decodeArmBranch(off + 1));

    do
    {
        for (; *off != 0xE58D8000; ++off);
        u32 *loc = (u32 *)PA_FROM_VA_PTR(decodeArmBranch(++off));
        if (loc != mapL2Section)
            mapL1Section = loc;
    } while (mapL1Section == NULL);

    mapL1Section[1] = 0xE28FE004; // add lr, pc, #4
    mapL1Section[2] = 0xE51FF004; // ldr pc, [pc, #-4]
    mapL1Section[3] = (u32)KProcessHwInfo__MapL1Section_Hook;

    mapL2Section[1] = 0xE28FE004; // add lr, pc, #4
    mapL2Section[2] = 0xE51FF004; // ldr pc, [pc, #-4]
    mapL2Section[3] = (u32)KProcessHwInfo__MapL2Section_Hook;
}

static void findUsefulSymbols(void)
{
    u32 *off;

    // Get fcramDescriptor
    for (off = (u32 *)0xFFF00000; ; ++off)
    {
        if (  (off[0] >> 16) == 0xE59F
           && (off[1] >> 16) == 0xE3A0
           && (off[2] >> 16) == 0xE3A0
           && (off[3] >> 16) == 0xE1A0
           && (off[4] >> 16) == 0xEB00)
        {
            fcramDescriptor = (FcramDescriptor *)off[2 + (off[0] & 0xFFFF) / 4];
            break;
        }
    }

    // Get kAlloc
    for (; *off != 0xE1A00005 || *(off + 1) != 0xE320F000; ++off);
    off = decodeArmBranch(off + 2);
    for (; (*off >> 16) != 0xEB00; ++off);
    kAlloc = (void* (*)(FcramDescriptor *, u32, u32, u32))decodeArmBranch(off);

    // Patch ERRF__DumpException
    for(off = (u32 *)0xFFFF0000; *off != 0xE1A04005; ++off);
    ++off;
    *(u32 *)PA_FROM_VA_PTR(off) = makeArmBranch(off, off + 51, false);

    for(; *off != 0xE2100102; ++off);
    KProcessHwInfo__QueryMemory = (Result (*)(KProcessHwInfo *, MemoryInfo *, PageInfo *, void *))decodeArmBranch(off - 1);

    for(; *off != 0xE1A0D002; off++);
    off += 3;
    initFPU = (void (*) (void))off;

    for(; *off != 0xE3A0A0C2; off++);
    mcuReboot = (void (*) (void))--off;
    coreBarrier = (void (*) (void))decodeArmBranch(off - 4);

    for(off = (u32 *)originalHandlers[2]; *off != 0xE1A00009; off++);
    svcFallbackHandler = (void (*)(u8))decodeArmBranch(off + 1);
    for(; *off != 0xE92D000F; off++);
    officialPostProcessSvc = (void (*)(void))decodeArmBranch(off + 1);

    KProcessHandleTable__ToKProcess = (KProcess * (*)(KProcessHandleTable *, Handle))decodeArmBranch(5 + (u32 *)officialSVCs[0x76]);

    for(off = (u32 *)KProcessHandleTable__ToKProcess; *off != 0xE1A00004; off++);
    KAutoObject__AddReference = (void (*)(KAutoObject *))decodeArmBranch(off + 1);

    for(; *off != 0xE320F000; off++);
    KProcessHandleTable__ToKAutoObject = (KAutoObject * (*)(KProcessHandleTable *, Handle))decodeArmBranch(off + 1);

    for(off = (u32 *)decodeArmBranch(3 + (u32 *)officialSVCs[9]); /* KThread::Terminate */ *off != 0xE5D42034; off++);
    off -= 2;
    criticalSectionLock = (KRecursiveLock *)off[2 + (off[0] & 0xFF) / 4];
    KRecursiveLock__Lock = (void (*)(KRecursiveLock *))decodeArmBranch(off + 1);
    off += 4;

    for(; (*off >> 16) != 0xE59F; off++);
    KRecursiveLock__Unlock = (void (*)(KRecursiveLock *))decodeArmBranch(off + 1);

    for(; *off != 0xE5C4007D; off++);
    KSynchronizationObject__Signal = (void (*)(KSynchronizationObject *, bool))decodeArmBranch(off + 3);

    for(off = (u32 *)officialSVCs[0x19]; *off != 0xE1A04005; off++);
    KEvent__Clear = (Result (*)(KEvent *))decodeArmBranch(off + 1);
    for(off = (u32 *)KEvent__Clear; *off != 0xE8BD8070; off++);
    synchronizationMutex = *(KObjectMutex **)(off + 1);
    for(off = (u32 *)officialSVCs[0x18]; *off != 0xE1A04005; ++off);
    KEvent__Signal = (Result (*)(KEvent *))decodeArmBranch(off + 1);

    for(off = (u32 *)officialSVCs[0x24]; *off != 0xE59F004C; off++);
    WaitSynchronization1 = (Result (*)(void *, KThread *, KSynchronizationObject *, s64))decodeArmBranch(off + 6);

    for(off = (u32 *)decodeArmBranch(3 + (u32 *)officialSVCs[0x33]) /* OpenProcess */ ; *off != 0xE1A05000; off++);
    KProcessHandleTable__CreateHandle = (Result (*)(KProcessHandleTable *, Handle *, KAutoObject *, u8))decodeArmBranch(off - 1);

    for(off = (u32 *)decodeArmBranch(3 + (u32 *)officialSVCs[0x34]) /* OpenThread */; *off != 0xD9001BF7; off++);
    threadList = *(KObjectList **)(off + 1);

    off = (u32 *)decodeArmBranch((u32 *)officialSVCs[0x37] + 3) + 5; /* GetThreadId */
    KProcessHandleTable__ToKThread = (KThread * (*)(KProcessHandleTable *, Handle))decodeArmBranch((*off >> 16) == 0xEB00 ? off : off + 2);

    for(off = (u32 *)officialSVCs[0x50]; off[0] != 0xE1A05000 || off[1] != 0xE2100102 || off[2] != 0x5A00000B; off++);
    InterruptManager__MapInterrupt = (Result (*)(InterruptManager *, KBaseInterruptEvent *, u32, u32, u32, bool, bool))decodeArmBranch(--off);
    interruptManager = *(InterruptManager **)(off - 4 + (off[-6] & 0xFFF) / 4);
    for(off = (u32 *)officialSVCs[0x54]; *off != 0xE8BD8008; off++);
    flushDataCacheRange = (void (*)(void *, u32))(*(u32 **)(off[1]) + 3);

    for(off = (u32 *)officialSVCs[0x71]; *off != 0xE2101102; off++);
    KProcessHwInfo__MapProcessMemory = (Result (*)(KProcessHwInfo *, KProcessHwInfo *, void *, void *, u32))decodeArmBranch(off - 1);

    // From 4.x to 6.x the pattern will match but the result will be wrong
    for(off = (u32 *)officialSVCs[0x72]; *off != 0xE2041102; off++);
    KProcessHwInfo__UnmapProcessMemory = (Result (*)(KProcessHwInfo *, void *, u32))decodeArmBranch(off - 1);

    for (off = (u32 *)officialSVCs[0x70]; *off != 0xE8881200 && *off != 0xE8891900; ++off);
    for (off = (u32 *)decodeArmBranch(off + 1); *off != 0xE2101102; ++off);
    KProcessHwInfo__CheckVaState = (Result (*)(KProcessHwInfo *, u32, u32, u32, u32))decodeArmBranch(off - 1);
    for (; *off != 0xE28D1008; ++off);
    KProcessHwInfo__GetListOfKBlockInfoForVA = (Result (*)(KProcessHwInfo*, KLinkedList*, u32, u32))decodeArmBranch(off + 1);

    for (; *off != 0xE2000102; ++off);
    KProcessHwInfo__MapListOfKBlockInfo = (Result (*)(KProcessHwInfo*, u32, KLinkedList*, u32, u32, u32))decodeArmBranch(off - 1);

    for (; *off != 0xE8BD8FF0; ++off);
    KLinkedList_KBlockInfo__Clear = (void (*)(KLinkedList *))decodeArmBranch(off - 6);

    for(off = (u32 *)KProcessHwInfo__MapListOfKBlockInfo; *off != 0xE1A0000B; ++off);
    doControlMemory = (Result (*)(KProcessHwInfo*, u32, u32, u32, u32, u32, u32, u32))decodeArmBranch(off + 1);

    for(off = (u32 *)officialSVCs[0x7C]; *off != 0x03530000; off++);
    KObjectMutex__WaitAndAcquire = (void (*)(KObjectMutex *))decodeArmBranch(++off);
    for(; *off != 0xE320F000; off++);
    KObjectMutex__ErrorOccured = (void (*)(void))decodeArmBranch(off + 1);

    for(off = (u32 *)originalHandlers[4]; *off != (u32)exceptionStackTop; off++);
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
                    decodeArmBranch((u32 *)officialSVCs[0x01] + 5);
    SleepThread = (void (*)(s64))officialSVCs[0x0A];
    CreateEvent = (Result (*)(Handle *, ResetType))decodeArmBranch((u32 *)officialSVCs[0x17] + 3);
    CloseHandle = (Result (*)(Handle))officialSVCs[0x23];
    GetHandleInfo = (Result (*)(s64 *, Handle, u32))decodeArmBranch((u32 *)officialSVCs[0x29] + 3);
    GetSystemInfo = (Result (*)(s64 *, s32, s32))decodeArmBranch((u32 *)officialSVCs[0x2A] + 3);
    GetProcessInfo = (Result (*)(s64 *, Handle, u32))decodeArmBranch((u32 *)officialSVCs[0x2B] + 3);
    GetThreadInfo = (Result (*)(s64 *, Handle, u32))decodeArmBranch((u32 *)officialSVCs[0x2C] + 3);
    ConnectToPort = (Result (*)(Handle *, const char*))decodeArmBranch((u32 *)officialSVCs[0x2D] + 3);
    SendSyncRequest = (Result (*)(Handle))officialSVCs[0x32];
    OpenProcess = (Result (*)(Handle *, u32))decodeArmBranch((u32 *)officialSVCs[0x33] + 3);
    GetProcessId = (Result (*)(u32 *, Handle))decodeArmBranch((u32 *)officialSVCs[0x35] + 3);
    DebugActiveProcess = (Result (*)(Handle *, u32))decodeArmBranch((u32 *)officialSVCs[0x60] + 3);
    SignalEvent = (Result (*)(Handle event))officialSVCs[0x18];

    UnmapProcessMemory = (Result (*)(Handle, void *, u32))officialSVCs[0x72];
    KernelSetState = (Result (*)(u32, u32, u32, u32))((u32 *)officialSVCs[0x7C] + 1);

    for(off = (u32 *)svcFallbackHandler; *off != 0xE8BD4010; off++);
    kernelpanic = (void (*)(void))decodeArmBranch(off + 1);

    for(off = (u32 *)0xFFFF0000; off[0] != 0xE3A01002 || off[1] != 0xE3A00004; off++);
    SignalDebugEvent = (Result (*)(DebugEventType type, u32 info, ...))decodeArmBranch(off + 2);

    for(; *off != 0x96007F9; off++);
    isDevUnit = *(bool **)(off - 1);

    ///////////////////////////////////////////

    // Shitty/lazy heuristic but it works on even 4.5, so...
    u32 textStart = ((u32)originalHandlers[2]) & ~0xFFFF;
    u32 rodataStart = (u32)(interruptManager->N3DS.privateInterrupts[1][0x1D].interruptEvent->vtable) & ~0xFFF;
    u32 textSize = rodataStart - textStart;
    for(off = (u32 *)textStart; off < (u32 *)(textStart + textSize - 12); off++)
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

    installMmuHooks();
}

void main(FcramLayout *layout, KCoreContext *ctxs)
{
    struct KExtParameters *p = &kExtParameters;
    u32 TTBCR_;
    s64 nb;

    cfwInfo = p->cfwInfo;
    kextBasePa = p->basePA;
    stolenSystemMemRegionSize = p->stolenSystemMemRegionSize;

    layout->systemSize -= stolenSystemMemRegionSize;
    fcramLayout = *layout;
    coreCtxs = ctxs;

    __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 2" : "=r"(TTBCR_));
    TTBCR = TTBCR_;
    isN3DS = getNumberOfCores() == 4;
    memcpy(L1MMUTableAddrs, (const void *)p->L1MMUTableAddrs, 16);
    exceptionStackTop = (u32 *)0xFFFF2000 + (1 << (32 - TTBCR - 20));

    memcpy(originalHandlers + 1, p->originalHandlers, 16);
    void **arm11SvcTable = (void**)originalHandlers[2];
    while(*arm11SvcTable != NULL) arm11SvcTable++; //Look for SVC0 (NULL)
    memcpy(officialSVCs, arm11SvcTable, 4 * 0x7E);

    findUsefulSymbols();
    buildAlteredSvcTable();

    GetSystemInfo(&nb, 26, 0);
    nbSection0Modules = (u32)nb;

    rosalinaState = 0;
    hasStartedRosalinaNetworkFuncsOnce = false;

    // DSB, Flush Prefetch Buffer (more or less "isb")
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" :: "r" (0) : "memory");
}
