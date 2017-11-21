#include "svc/KernelSetState.h"
#include "synchronization.h"
#include "ipc.h"
#include "memory.h"
#include "debug.h"

u32 *g_paArgs = 0;

static s32 K_GetCurrentCPU(void)
{
    s32 cpu;

    __asm__ __volatile__("mrc p15, 0, %[val], c0, c0, 5" : [val] "=r" (cpu));
    return (cpu & 3);
}

static void K_DataSynchronizationBarrier(void)
{
    u32 r0 = 0;
    __asm__ __volatile__("mcr p15, 0, %[val], c7, c10, 4" :: [val] "r" (r0));
}

static void K_EnableMonitorModeDebugging(void)
{
    __asm__ __volatile__("cpsid aif");

    u32 DSCR;
    __asm__ __volatile__("mrc p14, 0, %[val], c0, c1, 0" : [val] "=r" (DSCR));
    DSCR |= 0x8000;
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c1, 0" :: [val] "r" (DSCR));
}

static void K_DisableWatchpoint(u32 id)
{
    u32 control;

    __asm__ __volatile__("cpsid aif");

    if(id == 0)
    {
        // WCR0
        __asm__ __volatile__("mrc p14, 0, %[val], c0, c0, 7" : [val] "=r" (control));
        control &= ~1;
        __asm__ __volatile__("mcr p14, 0, %[val], c0, c0, 7" :: [val] "r" (control));

        // BCR4
        __asm__ __volatile__("mrc p14, 0, %[val], c0, c4, 5" : [val] "=r" (control));
        control &= ~1;
        __asm__ __volatile__("mcr p14, 0, %[val], c0, c4, 5" :: [val] "r" (control));
    }
    else if(id == 1)
    {
        // WCR1
        __asm__ __volatile__("mrc p14, 0, %[val], c0, c1, 7" : [val] "=r" (control));
        control &= ~1;
        __asm__ __volatile__("mcr p14, 0, %[val], c0, c1, 7" :: [val] "r" (control));

        // BCR5
        __asm__ __volatile__("mrc p14, 0, %[val], c0, c5, 5" : [val] "=r" (control));
        control &= ~1;
        __asm__ __volatile__("mcr p14, 0, %[val], c0, c5, 5" :: [val] "r" (control));
    }
}

static void K_SetWatchpoint0WithContextId(u32 DVA, u32 WCR, u32 contextId)
{
    // http://infocenter.arm.com/help/topic/com.arm.doc.ddi0360f/CEGCFFDF.html
    u32 BCR =
            (1   << 21) | /* compare with context ID */
            (1   << 20) | /* linked (with a WRP in our case) */
            (0xf <<  5) | /* byte address select, +0 to +3 as mandated when linking with a WRP */
            (3   <<  1) | /* either privileged modes or user mode, as mandated when linking with a WRP */
            (1   <<  0) ; /* enabled */

    __asm__ __volatile__("cpsid aif");

    K_DisableWatchpoint(0);

    __asm__ __volatile__("mcr p14, 0, %[val], c0, c0, 6" :: [val] "r" (DVA));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c4, 4" :: [val] "r" (contextId));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c0, 7" :: [val] "r" (WCR));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c4, 5" :: [val] "r" (BCR));

    __asm__ __volatile__("mcr p15, 0, %[val], c7, c10, 5" :: [val] "r" (0) : "memory"); // DMB
}

static void K_SetWatchpoint1WithContextId(u32 DVA, u32 WCR, u32 contextId)
{
    // http://infocenter.arm.com/help/topic/com.arm.doc.ddi0360f/CEGCFFDF.html
    u32 BCR =
            (1   << 21) | /* compare with context ID */
            (1   << 20) | /* linked (with a WRP in our case) */
            (0xf <<  5) | /* byte address select, +0 to +3 as mandated when linking with a WRP */
            (3   <<  1) | /* either privileged modes or user mode, as mandated when linking with a WRP */
            (1   <<  0) ; /* enabled */

    __asm__ __volatile__("cpsid aif");

    K_DisableWatchpoint(1);

    __asm__ __volatile__("mcr p14, 0, %[val], c0, c1, 6" :: [val] "r" (DVA));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c5, 4" :: [val] "r" (contextId));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c1, 7" :: [val] "r" (WCR));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c5, 5" :: [val] "r" (BCR));

    __asm__ __volatile__("mcr p15, 0, %[val], c7, c10, 5" :: [val] "r" (0) : "memory"); // DMB
}

KSchedulableInterruptEvent *K_SyncCP14(KBaseInterruptEvent *ievent, u32 id)
{
    u32 *SyncArgs = g_paArgs;

    if (SyncArgs)
    {
        u32 type, wpid, DVA, WCR, contextId;

        type = SyncArgs[0];
        switch(type)
        {
            case 0: // Init Hardware Debugger
                K_EnableMonitorModeDebugging();
                K_DisableWatchpoint(0);
                K_DisableWatchpoint(1);
                break;
            case 1: // Set HArdware Watchpoint
                wpid = SyncArgs[1];
                DVA = SyncArgs[2];
                WCR = SyncArgs[3];
                contextId = SyncArgs[4];
                if (wpid == 0)
                    K_SetWatchpoint0WithContextId(DVA, WCR, contextId);
                else
                    K_SetWatchpoint1WithContextId(DVA, WCR, contextId);
                break;
            case 2: // Disable Hardware Watchpoint
                wpid = SyncArgs[1];
                K_DisableWatchpoint(wpid);
                break;
            default:
                return NULL;
        }
    }
    K_DataSynchronizationBarrier();
    return NULL;
}

static u32 g_vtableBackup = 0;

u32    K_BindInterrupt(u32 *scheduler)
{
    static KBaseInterruptEvent  ievent;
    static Vtable__KBaseInterruptEvent vtable;

    ievent.vtable = &vtable;
    vtable.handleInterruptEvent = (void *)K_SyncCP14;

    if (scheduler)
    {
        // Restore vtable
        *scheduler = g_vtableBackup;

        u32 currentCore = K_GetCurrentCPU();

        InterruptManager__MapInterrupt(interruptManager, &ievent, 12, currentCore, 0, false, false);
    }
    return 1;
}

void    K_InstallSyncInterrupt(void)
{
    static u32 isInstalled = 0;
    static u32 vtable;

    if (isInstalled)
        return;

    vtable = (u32)K_BindInterrupt;

    K_DataSynchronizationBarrier();

    for (u32 i = 0; i < getNumberOfCores(); i++)
    {
        u32 *scheduler = (u32 *)coreCtxs[i].objectContext.currentScheduler;

        g_vtableBackup = *scheduler;
        *scheduler = (u32)&vtable;
    }

    isInstalled = 1;
}
