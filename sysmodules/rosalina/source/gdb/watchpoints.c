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

#include "gdb/watchpoints.h"
#include "csvc.h"

#define _REENT_ONLY
#include <errno.h>

/*
    There are only 2 Watchpoint Register Pairs on MPCORE ARM11 CPUs,
    and only 2 Breakpoint Register Pairs with context ID capabilities (BRP4-5) as well.

    We'll reserve and use all 4 of them
*/

RecursiveLock watchpointManagerLock;

typedef struct Watchpoint
{
    u32 address;
    u32 size;
    WatchpointKind kind;
    Handle debug; // => context ID
} Watchpoint;

typedef struct WatchpointManager
{
    u32 total;
    Watchpoint watchpoints[2];
} WatchpointManager;

static WatchpointManager manager;

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

void GDB_ResetWatchpoints(void)
{
    static bool lockInitialized = false;
    if(!lockInitialized)
    {
        RecursiveLock_Init(&watchpointManagerLock);
        lockInitialized = true;
    }
    RecursiveLock_Lock(&watchpointManagerLock);

    svcCustomBackdoor(K_EnableMonitorModeDebugging);
    svcCustomBackdoor(K_DisableWatchpoint, 0);
    svcCustomBackdoor(K_DisableWatchpoint, 1);

    memset(&manager, 0, sizeof(WatchpointManager));

    RecursiveLock_Unlock(&watchpointManagerLock);
}

int GDB_AddWatchpoint(GDBContext *ctx, u32 address, u32 size, WatchpointKind kind)
{
    RecursiveLock_Lock(&watchpointManagerLock);

    u32 offset = address - (address & ~3);

    if(manager.total == 2)
        return -EBUSY;

    if(size == 0 || (offset + size) > 4 || kind == WATCHPOINT_DISABLED)
        return -EINVAL;

    if(GDB_GetWatchpointKind(ctx, address) != WATCHPOINT_DISABLED)
        // Disallow duplicate watchpoints: the kernel doesn't give us sufficient info to differentiate them by kind (DFSR)
        return -EINVAL;

    u32 id = manager.watchpoints[0].kind == WATCHPOINT_DISABLED ? 0 : 1;
    u32 selectMask = ((1 << size) - 1) << offset;

    u32 WCR = (1          << 20) | /* linked */
              ((4 + id)   << 16) | /* ID of the linked BRP */
              (selectMask <<  5) | /* byte address select */
              ((u32)kind  <<  3) | /* kind */
              (2          <<  1) | /* user mode only */
              (1          <<  0) ; /* enabled */
    
    s64 out;

    Result r = svcGetHandleInfo(&out, ctx->debug, 0x10000); // context ID

    if(R_SUCCEEDED(r))
    {
        svcCustomBackdoor(id == 0 ? K_SetWatchpoint0WithContextId : K_SetWatchpoint1WithContextId, address, WCR, (u32)out);
        Watchpoint *watchpoint = &manager.watchpoints[id];
        manager.total++;
        watchpoint->address = address;
        watchpoint->size = size;
        watchpoint->kind = kind;
        watchpoint->debug = ctx->debug;
        ctx->watchpoints[ctx->nbWatchpoints++] = address;
        RecursiveLock_Unlock(&watchpointManagerLock);
        return 0;
    }
    else
    {
        RecursiveLock_Unlock(&watchpointManagerLock);
        return -EINVAL;
    }
}

int GDB_RemoveWatchpoint(GDBContext *ctx, u32 address, WatchpointKind kind)
{
    RecursiveLock_Lock(&watchpointManagerLock);

    u32 id;
    for(id = 0; id < 2 && manager.watchpoints[id].address != address && manager.watchpoints[id].debug != ctx->debug; id++);

    if(id == 2 || (kind != WATCHPOINT_DISABLED && manager.watchpoints[id].kind != kind))
    {
        RecursiveLock_Unlock(&watchpointManagerLock);
        return -EINVAL;
    }
    else
    {
        svcCustomBackdoor(K_DisableWatchpoint, id);

        memset(&manager.watchpoints[id], 0, sizeof(Watchpoint));
        manager.total--;

        if(ctx->watchpoints[0] == address)
        {
            ctx->watchpoints[0] = ctx->watchpoints[1];
            ctx->watchpoints[1] = 0;
            ctx->nbWatchpoints--;
        }
        else if(ctx->watchpoints[1] == address)
        {
            ctx->watchpoints[1] = 0;
            ctx->nbWatchpoints--;
        }

        RecursiveLock_Unlock(&watchpointManagerLock);

        return 0;
    }
}

WatchpointKind GDB_GetWatchpointKind(GDBContext *ctx, u32 address)
{
    u32 id;
    for(id = 0; id < 2 && manager.watchpoints[id].address != address && manager.watchpoints[id].debug != ctx->debug; id++);

    return id == 2 ? WATCHPOINT_DISABLED : manager.watchpoints[id].kind;
}
