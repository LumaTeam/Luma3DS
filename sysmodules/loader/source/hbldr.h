/*
*   This file is part of Luma3DS
*   Copyright (C) 2022 Aurora Wright, TuxSH
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

#include <3ds.h>
#include "luma_shared_config.h"

Result hbldrLoadProcess(Handle *outProcessHandle, const ExHeader_Info *exhi);
void hbldrPatchExHeaderInfo(ExHeader_Info *exhi);
void hbldrHandleCommands(void *ctx);

static inline bool hbldrIs3dsxTitle(u64 tid)
{
    if (!Luma_SharedConfig->use_hbldr)
        return false;
    u64 hbldrTid = Luma_SharedConfig->hbldr_3dsx_tid;

    // Just like p9 clears them, ignore platform/N3DS bits
    return ((tid ^ hbldrTid) & ~0xF0000000ull) == 0;
}
