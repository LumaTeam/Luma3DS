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

#pragma once

#include "types.h"
#include "kernel.h"

typedef KSchedulableInterruptEvent* (*SGI0Handler_t)(KBaseInterruptEvent *this, u32 interruptID);

// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0360f/CCHDIFIJ.html
void executeFunctionOnCores(SGI0Handler_t func, u8 targetList, u8 targetListFilter);

void KScheduler__TriggerCrossCoreInterrupt(KScheduler *this);
void KThread__DebugReschedule(KThread *this, bool lock);
bool rosalinaThreadLockPredicate(KThread *thread);
void rosalinaRescheduleThread(KThread *thread, bool lock);
void rosalinaLockThread(KThread *thread);
void rosalinaLockAllThreads(void);
void rosalinaUnlockAllThreads(void);

// Taken from ctrulib:

static inline void __dsb(void)
{
	__asm__ __volatile__("mcr p15, 0, %[val], c7, c10, 4" :: [val] "r" (0) : "memory");
}

static inline void __clrex(void)
{
    __asm__ __volatile__("clrex" ::: "memory");
}

static inline s32 __ldrex(s32* addr)
{
    s32 val;
    __asm__ __volatile__("ldrex %[val], %[addr]" : [val] "=r" (val) : [addr] "Q" (*addr));
    return val;
}

static inline bool __strex(s32* addr, s32 val)
{
    bool res;
    __asm__ __volatile__("strex %[res], %[val], %[addr]" : [res] "=&r" (res) : [val] "r" (val), [addr] "Q" (*addr));
    return res;
}

static inline s8 __ldrex8(s8* addr)
{
    s8 val;
    __asm__ __volatile__("ldrexb %[val], %[addr]" : [val] "=r" (val) : [addr] "Q" (*addr));
    return val;
}

static inline bool __strex8(s8* addr, s8 val)
{
    bool res;
    __asm__ __volatile__("strexb %[res], %[val], %[addr]" : [res] "=&r" (res) : [val] "r" (val), [addr] "Q" (*addr));
    return res;
}

static inline s16 __ldrex16(s16* addr)
{
    s16 val;
    __asm__ __volatile__("ldrexh %[val], %[addr]" : [val] "=r" (val) : [addr] "Q" (*addr));
    return val;
}

static inline bool __strex16(s16* addr, s16 val)
{
    bool res;
    __asm__ __volatile__("strexh %[res], %[val], %[addr]" : [res] "=&r" (res) : [val] "r" (val), [addr] "Q" (*addr));
    return res;
}

static inline u32 __get_cpsr(void)
{
    u32 cpsr;
    __asm__ __volatile__("mrs %0, cpsr" : "=r"(cpsr));
    return cpsr;
}

static inline void __set_cpsr_cx(u32 cpsr)
{
    __asm__ __volatile__("msr cpsr_cx, %0" :: "r"(cpsr));
}

static inline void __enable_irq(void)
{
    __asm__ __volatile__("cpsie i");
}

static inline void __disable_irq(void)
{
    __asm__ __volatile__("cpsid i");
}
