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

#define GDB_XFER_HANDLER(name)                  GDB_HANDLER(Xfer##name)
#define GDB_DECLARE_XFER_HANDLER(name)          int GDB_XFER_HANDLER(name)(GDBContext *ctx, bool write, const char *annex, u32 offset, u32 length)

#define GDB_XFER_OSDATA_HANDLER(name)           GDB_XFER_HANDLER(OsData##name)
#define GDB_DECLARE_XFER_OSDATA_HANDLER(name)   int GDB_XFER_OSDATA_HANDLER(name)(GDBContext *ctx, bool write, u32 offset, u32 length)

GDB_DECLARE_XFER_HANDLER(Features);

GDB_DECLARE_XFER_OSDATA_HANDLER(CfwVersion);
GDB_DECLARE_XFER_OSDATA_HANDLER(Memory);
GDB_DECLARE_XFER_OSDATA_HANDLER(Processes);

GDB_DECLARE_XFER_HANDLER(OsData);

GDB_DECLARE_QUERY_HANDLER(Xfer);
