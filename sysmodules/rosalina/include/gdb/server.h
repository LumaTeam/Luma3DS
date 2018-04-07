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

#ifndef GDB_PORT_BASE
#define GDB_PORT_BASE 4000
#endif

typedef struct GDBServer
{
    sock_server super;
    s32 referenceCount;
    Handle statusUpdated;
    GDBContext ctxs[MAX_DEBUG];
} GDBServer;

Result GDB_InitializeServer(GDBServer *server);
void GDB_FinalizeServer(GDBServer *server);

void GDB_IncrementServerReferenceCount(GDBServer *server);
void GDB_DecrementServerReferenceCount(GDBServer *server);

void GDB_RunServer(GDBServer *server);

int GDB_AcceptClient(GDBContext *ctx);
int GDB_CloseClient(GDBContext *ctx);
GDBContext *GDB_GetClient(GDBServer *server, u16 port);
void GDB_ReleaseClient(GDBServer *server, GDBContext *ctx);
int GDB_DoPacket(GDBContext *ctx);
