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
