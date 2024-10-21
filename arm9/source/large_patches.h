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

#pragma once

#include "types.h"

extern const u8 emunandPatch[];
extern const u32 emunandPatchSize;
extern u32 emunandPatchSdmmcStructPtr, emunandPatchNandOffset, emunandPatchNcsdHeaderOffset;

extern const u8 rebootPatch[];
extern const u32 rebootPatchSize;
extern u32 rebootPatchFopenPtr;
extern u16 rebootPatchFileName[80+1];

extern const u8 readFileSHA256Vtab11Patch[];
extern const u32 readFileSHA256Vtab11PatchSize;
extern u32 readFileSHA256Vtab11PatchCtorPtr, readFileSHA256Vtab11PatchInitPtr, readFileSHA256Vtab11PatchProcessPtr;