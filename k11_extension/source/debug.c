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
#include "debug.h"
#include "synchronization.h"

KRecursiveLock dbgParamsLock = { NULL };
u32 dbgParamWatchpointId, dbgParamDVA, dbgParamWCR, dbgParamContextId;

KSchedulableInterruptEvent *enableMonitorModeDebugging(KBaseInterruptEvent *this CTR_UNUSED, u32 interruptID CTR_UNUSED)
{
    coreBarrier();

    u32 DSCR;
    __asm__ __volatile__("mrc p14, 0, %[val], c0, c1, 0" : [val] "=r" (DSCR));
    DSCR |= 0x8000;
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c1, 0" :: [val] "r" (DSCR));

    __dsb();
    coreBarrier();

    return NULL;
}

static void disableWatchpoint0(void)
{
    u32 control;

    // WCR0
    __asm__ __volatile__("mrc p14, 0, %[val], c0, c0, 7" : [val] "=r" (control));
    control &= ~1;
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c0, 7" :: [val] "r" (control));

    // BCR4
    __asm__ __volatile__("mrc p14, 0, %[val], c0, c4, 5" : [val] "=r" (control));
    control &= ~1;
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c4, 5" :: [val] "r" (control));
}

static void disableWatchpoint1(void)
{
    u32 control;

    // WCR1
    __asm__ __volatile__("mrc p14, 0, %[val], c0, c1, 7" : [val] "=r" (control));
    control &= ~1;
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c1, 7" :: [val] "r" (control));

    // BCR5
    __asm__ __volatile__("mrc p14, 0, %[val], c0, c5, 5" : [val] "=r" (control));
    control &= ~1;
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c5, 5" :: [val] "r" (control));
}

KSchedulableInterruptEvent *disableWatchpoint(KBaseInterruptEvent *this CTR_UNUSED, u32 interruptID CTR_UNUSED)
{
    coreBarrier();

    if(dbgParamWatchpointId == 0)
        disableWatchpoint0();
    else
        disableWatchpoint1();

    __dsb();
    coreBarrier();

    return NULL;
}

static void setWatchpoint0WithContextId(u32 DVA, u32 WCR, u32 contextId)
{
    // http://infocenter.arm.com/help/topic/com.arm.doc.ddi0360f/CEGCFFDF.html
    u32 BCR =
            (1   << 21) | /* compare with context ID */
            (1   << 20) | /* linked (with a WRP in our case) */
            (0xf <<  5) | /* byte address select, +0 to +3 as mandated when linking with a WRP */
            (3   <<  1) | /* either privileged modes or user mode, as mandated when linking with a WRP */
            (1   <<  0) ; /* enabled */

    disableWatchpoint0();

    __asm__ __volatile__("mcr p14, 0, %[val], c0, c0, 6" :: [val] "r" (DVA));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c4, 4" :: [val] "r" (contextId));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c0, 7" :: [val] "r" (WCR));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c4, 5" :: [val] "r" (BCR));

    __asm__ __volatile__("mcr p15, 0, %[val], c7, c10, 5" :: [val] "r" (0) : "memory"); // DMB
}

static void setWatchpoint1WithContextId(u32 DVA, u32 WCR, u32 contextId)
{
    // http://infocenter.arm.com/help/topic/com.arm.doc.ddi0360f/CEGCFFDF.html
    u32 BCR =
            (1   << 21) | /* compare with context ID */
            (1   << 20) | /* linked (with a WRP in our case) */
            (0xf <<  5) | /* byte address select, +0 to +3 as mandated when linking with a WRP */
            (3   <<  1) | /* either privileged modes or user mode, as mandated when linking with a WRP */
            (1   <<  0) ; /* enabled */

    disableWatchpoint1();

    __asm__ __volatile__("mcr p14, 0, %[val], c0, c1, 6" :: [val] "r" (DVA));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c5, 4" :: [val] "r" (contextId));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c1, 7" :: [val] "r" (WCR));
    __asm__ __volatile__("mcr p14, 0, %[val], c0, c5, 5" :: [val] "r" (BCR));

    __asm__ __volatile__("mcr p15, 0, %[val], c7, c10, 5" :: [val] "r" (0) : "memory"); // DMB
}

KSchedulableInterruptEvent *setWatchpointWithContextId(KBaseInterruptEvent *this CTR_UNUSED, u32 interruptID CTR_UNUSED)
{
    coreBarrier();

    if(dbgParamWatchpointId == 0)
        setWatchpoint0WithContextId(dbgParamDVA, dbgParamWCR, dbgParamContextId);
    else
        setWatchpoint1WithContextId(dbgParamDVA, dbgParamWCR, dbgParamContextId);

    __dsb();
    coreBarrier();

    return NULL;
}
