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
#define _REENT_ONLY
#include <errno.h>

u8 GDB_ComputeChecksum(const char *packetData, u32 len);
void GDB_EncodeHex(char *dst, const void *src, u32 len);
u32 GDB_DecodeHex(void *dst, const char *src, u32 len);
u32 GDB_UnescapeBinaryData(void *dst, const void *src, u32 len);
const char *GDB_ParseIntegerList(u32 *dst, const char *src, u32 nb, char sep, char lastSep, u32 base, bool allowPrefix);
const char *GDB_ParseHexIntegerList(u32 *dst, const char *src, u32 nb, char lastSep);
int GDB_ReceivePacket(GDBContext *ctx);
int GDB_SendPacket(GDBContext *ctx, const char *packetData, u32 len);
int GDB_SendFormattedPacket(GDBContext *ctx, const char *packetDataFmt, ...);
int GDB_SendHexPacket(GDBContext *ctx, const void *packetData, u32 len);
int GDB_SendStreamData(GDBContext *ctx, const char *streamData, u32 offset, u32 length, u32 totalSize, bool forceEmptyLast);
int GDB_SendDebugString(GDBContext *ctx, const char *fmt, ...); // unsecure
int GDB_ReplyEmpty(GDBContext *ctx);
int GDB_ReplyOk(GDBContext *ctx);
int GDB_ReplyErrno(GDBContext *ctx, int no);
