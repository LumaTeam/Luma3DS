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

// For accessing physmem uncached (and directly)
#define PA_PTR(addr)            (void *)((u32)(addr) | 1u << 31)
#define PA_FROM_VA_PTR(addr)    PA_PTR(convertVAToPA(addr, false))

static inline u32 makeARMBranch(const void *src, const void *dst, bool link) // the macros for those are ugly and buggy
{
    u32 instrBase = link ? 0xEB000000 : 0xEA000000;
    u32 off = (u32)((const u8 *)dst - ((const u8 *)src + 8)); // the PC is always two instructions ahead of the one being executed

    return instrBase | ((off >> 2) & 0xFFFFFF);
}

static inline void *decodeARMBranch(const void *src)
{
    u32 instr = *(const u32 *)src;
    s32 off = (instr & 0xFFFFFF) << 2;
    off = (off << 6) >> 6; // sign extend

    return (void *)((const u8 *)src + 8 + off);
}

// For ARM prologs in the form of: push {regs} ... sub sp, #off (this obviously doesn't intend to cover all cases)
static inline u32 computeARMFrameSize(const u32 *prolog)
{
    const u32 *off;

    for(off = prolog; (*off >> 16) != 0xE92D; off++); // look for stmfd sp! = push
    u32 nbPushedRegs = 0;
    for(u32 val = *off & 0xFFFF; val != 0; val >>= 1) // 1 bit = 1 pushed register
        nbPushedRegs += val & 1;
    for(; (*off >> 8) != 0xE24DD0; off++); // look for sub sp, #offset
    u32 localVariablesSpaceSize = *off & 0xFF;

    return 4 * nbPushedRegs + localVariablesSpaceSize;
}

static inline u32 getNumberOfCores(void)
{
    return (MPCORE_SCU_CFG & 3) + 1;
}

static inline u32 getCurrentCoreID(void)
{
    u32 coreId;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(coreId));
    return coreId & 3;
}

u32 convertVAToPA(const void *addr, bool writeCheck);

u32 safecpy(void *dst, const void *src, u32 len);
void KObjectMutex__Acquire(KObjectMutex *this);
void KObjectMutex__Release(KObjectMutex *this);

void flushEntireDataCache(void);
void invalidateEntireInstructionCache(void);
