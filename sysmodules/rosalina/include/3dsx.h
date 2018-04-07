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

/* File entirely written by fincs */

#pragma once
#include <3ds/types.h>
#include "ifile.h"

// File layout:
// - File header
// - Code, rodata and data relocation table headers
// - Code segment
// - Rodata segment
// - Loadable (non-BSS) part of the data segment
// - Code relocation table
// - Rodata relocation table
// - Data relocation table

// Memory layout before relocations are applied:
// [0..codeSegSize)             -> code segment
// [codeSegSize..rodataSegSize) -> rodata segment
// [rodataSegSize..dataSegSize) -> data segment

// Memory layout after relocations are applied: well, however the loader sets it up :)
// The entrypoint is always the start of the code segment.
// The BSS section must be cleared manually by the application.

// File header
#define _3DSX_MAGIC 0x58534433 // '3DSX'
typedef struct
{
    u32 magic;
    u16 headerSize, relocHdrSize;
    u32 formatVer;
    u32 flags;

    // Sizes of the code, rodata and data segments +
    // size of the BSS section (uninitialized latter half of the data segment)
    u32 codeSegSize, rodataSegSize, dataSegSize, bssSize;
} _3DSX_Header;

// Relocation header: all fields (even extra unknown fields) are guaranteed to be relocation counts.
typedef struct
{
    u32 cAbsolute; // # of absolute relocations (that is, fix address to post-relocation memory layout)
    u32 cRelative; // # of cross-segment relative relocations (that is, 32bit signed offsets that need to be patched)
    // more?

    // Relocations are written in this order:
    // - Absolute relocs
    // - Relative relocs
} _3DSX_RelocHdr;

// Relocation entry: from the current pointer, skip X words and patch Y words
typedef struct
{
    u16 skip, patch;
} _3DSX_Reloc;

// _prm structure
#define _PRM_MAGIC 0x6D72705F // '_prm'
typedef struct
{
    u32 magic;
    u32 pSrvOverride;
    u32 aptAppId;
    u32 heapSize, linearHeapSize;
    u32 pArgList;
    u32 runFlags;
} PrmStruct;

// Service override structure
typedef struct
{
    u32 count;
    struct
    {
        char name[8];
        Handle handle;
    } services[];
} SrvOverride;

#define ARGVBUF_SIZE 0x400
extern u32 ldrArgvBuf[ARGVBUF_SIZE/4];

bool Ldr_Get3dsxSize(u32* pSize, IFile *file);
Handle Ldr_CodesetFrom3dsx(const char* name, u32* codePages, u32 baseAddr, IFile *file, u64 tid);
