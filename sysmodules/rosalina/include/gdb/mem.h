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

#include "gdb.h"

Result GDB_ReadMemoryInPage(void *out, GDBContext *ctx, u32 addr, u32 len);
Result GDB_WriteMemoryInPage(GDBContext *ctx, const void *in, u32 addr, u32 len);
int GDB_SendMemory(GDBContext *ctx, const char *prefix, u32 prefixLen, u32 addr, u32 len);
int GDB_WriteMemory(GDBContext *ctx, const void *buf, u32 addr, u32 len);
u32 GDB_SearchMemory(bool *found, GDBContext *ctx, u32 addr, u32 len, const void *pattern, u32 patternLen);

GDB_DECLARE_HANDLER(ReadMemory);
GDB_DECLARE_HANDLER(WriteMemory);
GDB_DECLARE_HANDLER(WriteMemoryRaw);
GDB_DECLARE_QUERY_HANDLER(SearchMemory);
