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

#include <3ds/types.h>
#include "exheader.h"

Result fsregInit(void);
void fsregExit(void);
Result FSREG_CheckHostLoadId(u64 prog_handle);
Result FSREG_LoadProgram(u64 *prog_handle, FS_ProgramInfo *title);
Result FSREG_GetProgramInfo(exheader_header *exheader, u32 entry_count, u64 prog_handle);
Result FSREG_UnloadProgram(u64 prog_handle);
Result FSREG_Unregister(u32 pid);
Result FSREG_Register(u32 pid, u64 prog_handle, FS_ProgramInfo *info, void *storageinfo);
Result fsregSetupPermissions(void);
Handle fsregGetHandle(void);
