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
#include "3dsheaders.h"

u32 loadNintendoFirm(FirmwareType *firmType, FirmwareSource nandType, bool loadFromStorage, bool isSafeMode);
void loadHomebrewFirm(u32 pressed);
u32 patchNativeFirm(u32 firmVersion, FirmwareSource nandType, bool loadFromStorage, bool isFirmProtEnabled, bool needToInitSd, bool doUnitinfoPatch);
u32 patchTwlFirm(u32 firmVersion, bool loadFromStorage, bool doUnitinfoPatch);
u32 patchAgbFirm(bool loadFromStorage, bool doUnitinfoPatch);
u32 patch1x2xNativeAndSafeFirm(void);
u32 patchPrototypeNative(FirmwareSource nandType);
void launchFirm(int argc, char **argv);
