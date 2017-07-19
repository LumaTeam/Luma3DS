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

struct KExtParameters
{
    u32 ALIGN(0x400) L2MMUTableFor0x40000000[256];
    u32 basePA;
    void *originalHandlers[4];
    u32 L1MMUTableAddrs[4];

    CfwInfo cfwInfo;
} kExtParameters = { .basePA = 0x12345678 }; // place this in .data

void relocateAndSetupMMU(u32 coreId, u32 *L1Table)
{
    struct KExtParameters *p0 = (struct KExtParameters *)((u32)&kExtParameters - 0x40000000 + 0x18000000);
    struct KExtParameters *p = (struct KExtParameters *)((u32)&kExtParameters - 0x40000000 + p0->basePA);

    if(coreId == 0)
    {
        // Relocate ourselves, and clear BSS
        memcpy((void *)p0->basePA, (const void *)0x18000000, __bss_start__ - __start__);
        memset32((u32 *)(p0->basePA + (__bss_start__ - __start__)), 0, __bss_end__ - __bss_start__);

        // Map the kernel ext to 0x40000000
        // 4KB extended small pages: [SYS:RW USR:-- X  TYP:NORMAL SHARED OUTER NOCACHE, INNER CACHED WB WA]
        for(u32 offset = 0; offset < (u32)(__end__ - __start__); offset += 0x1000)
            p->L2MMUTableFor0x40000000[offset >> 12] = (p0->basePA + offset) | 0x516;

        __asm__ __volatile__ ("sev");
    }
    else
        __asm__ __volatile__ ("wfe");

    // bit31 idea thanks to SALT
    // Maps physmem so that, if addr is in physmem(0, 0x30000000), it can be accessed uncached&rwx as addr|(1<<31)
    u32 attribs = 0x40C02; // supersection (rwx for all) of strongly ordered memory, shared
    for(u32 PA = 0; PA < 0x30000000; PA += 0x01000000)
    {
        u32 VA = (1 << 31) | PA;
        for(u32 i = 0; i < 16; i++)
            L1Table[i + (VA >> 20)] = PA | attribs;
    }

    L1Table[0x40000000 >> 20] = (u32)p->L2MMUTableFor0x40000000 | 1;

    p->L1MMUTableAddrs[coreId] = (u32)L1Table;
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
}

static void findUsefulSymbols(void)
{
    u32 *off;

    for(off = (u32 *)0xFFFF0000; *off != 0xE1A0D002; off++);
    off += 3;
    initFPU = (void (*) (void))off;

    for(; *off != 0xE3A0A0C2; off++);
    mcuReboot = (void (*) (void))--off;
    coreBarrier = (void (*) (void))decodeARMBranch(off - 4);

    for(off = (u32 *)originalHandlers[2]; *off != 0xE1A00009; off++);
    svcFallbackHandler = (void (*)(u8))decodeARMBranch(off + 1);
    for(; *off != 0xE92D000F; off++);
    officialPostProcessSvc = (void (*)(void))decodeARMBranch(off + 1);

    KProcessHandleTable__ToKProcess = (KProcess * (*)(KProcessHandleTable *, Handle))decodeARMBranch(5 + (u32 *)officialSVCs[0x76]);

    for(off = (u32 *)KProcessHandleTable__ToKProcess; *off != 0xE1A00004; off++);
    KAutoObject__AddReference = (void (*)(KAutoObject *))decodeARMBranch(off + 1);

    for(; *off != 0xE320F000; off++);
    KProcessHandleTable__ToKAutoObject = (KAutoObject * (*)(KProcessHandleTable *, Handle))decodeARMBranch(off + 1);

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
    for(off = (u32 *)KEvent__Clear; *off != 0xE8BD8070; off++);
    synchronizationMutex = *(KObjectMutex **)(off + 1);

    for(off = (u32 *)officialSVCs[0x24]; *off != 0xE59F004C; off++);
    WaitSynchronization1 = (Result (*)(void *, KThread *, KSynchronizationObject *, s64))decodeARMBranch(off + 6);

    for(off = (u32 *)decodeARMBranch(3 + (u32 *)officialSVCs[0x33]) /* OpenProcess */ ; *off != 0xE1A05000; off++);
    KProcessHandleTable__CreateHandle = (Result (*)(KProcessHandleTable *, Handle *, KAutoObject *, u8))decodeARMBranch(off - 1);

    for(off = (u32 *)decodeARMBranch(3 + (u32 *)officialSVCs[0x34]) /* OpenThread */; *off != 0xD9001BF7; off++);
    threadList = *(KObjectList **)(off + 1);

    KProcessHandleTable__ToKThread = (KThread * (*)(KProcessHandleTable *, Handle))decodeARMBranch((u32 *)decodeARMBranch((u32 *)officialSVCs[0x37] + 3) /* GetThreadId */ + 5);

    for(off = (u32 *)officialSVCs[0x50]; off[0] != 0xE1A05000 || off[1] != 0xE2100102 || off[2] != 0x5A00000B; off++);
    InterruptManager__MapInterrupt = (Result (*)(InterruptManager *, KBaseInterruptEvent *, u32, u32, u32, bool, bool))decodeARMBranch(--off);
    interruptManager = *(InterruptManager **)(off - 4 + (off[-6] & 0xFFF) / 4);
    for(off = (u32 *)officialSVCs[0x54]; *off != 0xE8BD8008; off++);
    flushDataCacheRange = (void (*)(void *, u32))(*(u32 **)(off[1]) + 3);

    for(off = (u32 *)officialSVCs[0x71]; *off != 0xE2101102; off++);
    KProcessHwInfo__MapProcessMemory = (Result (*)(KProcessHwInfo *, KProcessHwInfo *, void *, void *, u32))decodeARMBranch(off - 1);

    // From 4.x to 6.x the pattern will match but the result will be wrong
    for(off = (u32 *)officialSVCs[0x72]; *off != 0xE2041102; off++);
    KProcessHwInfo__UnmapProcessMemory = (Result (*)(KProcessHwInfo *, void *, u32))decodeARMBranch(off - 1);

    for(off = (u32 *)officialSVCs[0x7C]; *off != 0x03530000; off++);
    KObjectMutex__WaitAndAcquire = (void (*)(KObjectMutex *))decodeARMBranch(++off);
    for(; *off != 0xE320F000; off++);
    KObjectMutex__ErrorOccured = (void (*)(void))decodeARMBranch(off + 1);

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
                    decodeARMBranch((u32 *)officialSVCs[0x01] + 5);
    SleepThread = (void (*)(s64))officialSVCs[0x0A];
    CloseHandle = (Result (*)(Handle))officialSVCs[0x23];
    GetHandleInfo = (Result (*)(s64 *, Handle, u32))decodeARMBranch((u32 *)officialSVCs[0x29] + 3);
    GetSystemInfo = (Result (*)(s64 *, s32, s32))decodeARMBranch((u32 *)officialSVCs[0x2A] + 3);
    GetProcessInfo = (Result (*)(s64 *, Handle, u32))decodeARMBranch((u32 *)officialSVCs[0x2B] + 3);
    GetThreadInfo = (Result (*)(s64 *, Handle, u32))decodeARMBranch((u32 *)officialSVCs[0x2C] + 3);
    ConnectToPort = (Result (*)(Handle *, const char*))decodeARMBranch((u32 *)officialSVCs[0x2D] + 3);
    SendSyncRequest = (Result (*)(Handle))officialSVCs[0x32];
    OpenProcess = (Result (*)(Handle *, u32))decodeARMBranch((u32 *)officialSVCs[0x33] + 3);
    GetProcessId = (Result (*)(u32 *, Handle))decodeARMBranch((u32 *)officialSVCs[0x35] + 3);
    DebugActiveProcess = (Result (*)(Handle *, u32))decodeARMBranch((u32 *)officialSVCs[0x60] + 3);
    UnmapProcessMemory = (Result (*)(Handle, void *, u32))officialSVCs[0x72];
    KernelSetState = (Result (*)(u32, u32, u32, u32))((u32 *)officialSVCs[0x7C] + 1);

    for(off = (u32 *)svcFallbackHandler; *off != 0xE8BD4010; off++);
    kernelpanic = (void (*)(void))decodeARMBranch(off + 1);

    for(off = (u32 *)0xFFFF0000; off[0] != 0xE3A01002 || off[1] != 0xE3A00004; off++);
    SignalDebugEvent = (Result (*)(DebugEventType type, u32 info, ...))decodeARMBranch(off + 2);

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
}

void main(FcramLayout *layout, KCoreContext *ctxs)
{
    struct KExtParameters *p = &kExtParameters;
    u32 TTBCR_;
    s64 nb;

    layout->systemSize -= __end__ - __start__;
    fcramLayout = *layout;
    coreCtxs = ctxs;

    __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 2" : "=r"(TTBCR_));
    TTBCR = TTBCR_;
    isN3DS = getNumberOfCores() == 4;
    memcpy(L1MMUTableAddrs, (const void *)p->L1MMUTableAddrs, 16);
    exceptionStackTop = (u32 *)0xFFFF2000 + (1 << (32 - TTBCR - 20));
    cfwInfo = p->cfwInfo;

    memcpy(originalHandlers + 1, p->originalHandlers, 16);
    void **arm11SvcTable = (void**)originalHandlers[2];
    while(*arm11SvcTable != NULL) arm11SvcTable++; //Look for SVC0 (NULL)
    memcpy(officialSVCs, arm11SvcTable, 4 * 0x7E);

    findUsefulSymbols();

    GetSystemInfo(&nb, 26, 0);
    nbSection0Modules = (u32)nb;

    rosalinaState = 0;
    hasStartedRosalinaNetworkFuncsOnce = false;
}
