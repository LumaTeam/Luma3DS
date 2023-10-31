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

// Taken from ctrulib

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/// The maximum value of a u64.
#define U64_MAX	UINT64_MAX

/// would be nice if newlib had this already
#ifndef SSIZE_MAX
#ifdef SIZE_MAX
#define SSIZE_MAX ((SIZE_MAX) >> 1)
#endif
#endif

typedef uint8_t u8;   ///<  8-bit unsigned integer
typedef uint16_t u16; ///< 16-bit unsigned integer
typedef uint32_t u32; ///< 32-bit unsigned integer
typedef uint64_t u64; ///< 64-bit unsigned integer

typedef int8_t s8;   ///<  8-bit signed integer
typedef int16_t s16; ///< 16-bit signed integer
typedef int32_t s32; ///< 32-bit signed integer
typedef int64_t s64; ///< 64-bit signed integer

typedef volatile u8 vu8;   ///<  8-bit volatile unsigned integer.
typedef volatile u16 vu16; ///< 16-bit volatile unsigned integer.
typedef volatile u32 vu32; ///< 32-bit volatile unsigned integer.
typedef volatile u64 vu64; ///< 64-bit volatile unsigned integer.

typedef volatile s8 vs8;   ///<  8-bit volatile signed integer.
typedef volatile s16 vs16; ///< 16-bit volatile signed integer.
typedef volatile s32 vs32; ///< 32-bit volatile signed integer.
typedef volatile s64 vs64; ///< 64-bit volatile signed integer.

typedef u32 Handle;                 ///< Resource handle.
typedef s32 Result;                 ///< Function result.

/// Creates a bitmask from a bit number.
#define BIT(n) (1U<<(n))

/// Aligns a struct (and other types?) to m, making sure that the size of the struct is a multiple of m.
#define CTR_ALIGN(m)   __attribute__((aligned(m)))
/// Packs a struct (and other types?) so it won't include padding bytes.
#define CTR_PACKED      __attribute__((packed))
#define CTR_USED        __attribute__((used))
#define CTR_UNUSED      __attribute__((unused))
/// Packs a system version from its components.
#define SYSTEM_VERSION(major, minor, revision) \
    (((major)<<24)|((minor)<<16)|((revision)<<8))

/// Retrieves the major version from a packed system version.
#define GET_VERSION_MAJOR(version)    ((version) >>24)

/// Retrieves the minor version from a packed system version.
#define GET_VERSION_MINOR(version)    (((version)>>16)&0xFF)

/// Retrieves the revision version from a packed system version.
#define GET_VERSION_REVISION(version) (((version)>> 8)&0xFF)

#define CUR_THREAD_HANDLE       0xFFFF8000
#define CUR_PROCESS_HANDLE      0xFFFF8001

#define MPCORE_REGS_BASE        (0x17E00000 | (1u << 31))
#define MPCORE_SCU_CFG          (*(vu32 *)(MPCORE_REGS_BASE + 4))
#define MPCORE_INT_ACK          (*(vu32 *)(MPCORE_REGS_BASE + 0x10C))

#define MPCORE_GID_REGS_BASE    (MPCORE_REGS_BASE + 0x1000)
#define MPCORE_GID_SGI          (*(vu32 *)(MPCORE_GID_REGS_BASE + 0xF00))

#define CFG11_REGS_BASE           (0x10140000 | (1u << 31))
#define CFG11_WIFICNT             (*(vu8  *)(CFG11_REGS_BASE + 0x180))
#define CFG11_MPCORE_CFG          (*(vu16 *)(CFG11_REGS_BASE + 0xFFC))
#define CFG11_MPCORE_CLKCNT       (*(vu16 *)(CFG11_REGS_BASE + 0x1300))

#define L2C_REGS_BASE           (0x17E10000 | (1u << 31))
#define L2C_CTRL                (*(vu32 *)(L2C_REGS_BASE + 0x100))
