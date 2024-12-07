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

/*
*   Signature patches by an unknown author
*   Signature patches for old FIRMs by SciresM
*   firmlaunches patching code originally by delebile
*   FIRM partition writes patches by delebile
*   Idea for svcBreak patches from yellows8 and others on #3dsdev
*   TWL_FIRM patches by Steveice10 and others
*   Signature patches for prototype FW by PabloMK7
*/

#pragma once

#include "types.h"

u8 *getProcess9Info(u8 *pos, u32 size, u32 *process9Size, u32 *process9MemAddr);
u32 *getKernel11Info(u8 *pos, u32 size, u32 *baseK11VA, u8 **freeK11Space, u32 **arm11SvcHandler, u32 **arm11ExceptionsPage);
u32 installK11Extension(u8 *pos, u32 size, bool needToInitSd, u32 baseK11VA, u32 *arm11ExceptionsPage, u8 **freeK11Space);
u32 patchKernel11(u8 *pos, u32 size, u32 baseK11VA, u32 *arm11SvcTable, u32 *arm11ExceptionsPage);
u32 patchSignatureChecks(u8 *pos, u32 size);
u32 patchOldSignatureChecks(u8 *pos, u32 size);
u32 patchFirmlaunches(u8 *pos, u32 size, u32 process9MemAddr);
u32 patchFirmWrites(u8 *pos, u32 size);
u32 patchOldFirmWrites(u8 *pos, u32 size);
u32 patchTitleInstallMinVersionChecks(u8 *pos, u32 size, u32 firmVersion);
u32 patchZeroKeyNcchEncryptionCheck(u8 *pos, u32 size);
u32 patchNandNcchEncryptionCheck(u8 *pos, u32 size);
u32 patchCheckForDevCommonKey(u8 *pos, u32 size);
u32 patchK11ModuleLoading(u32 oldKipSectionSize, u32 newKipSectionSize, u32 numKips, u8 *pos, u32 size);
u32 patchK11ModuleLoadingLgy(u32 newKipSectionSize, u8 *pos, u32 size);
u32 patchArm9ExceptionHandlersInstall(u8 *pos, u32 size);
u32 patchSvcBreak9(u8 *pos, u32 size, u32 kernel9Address);
u32 patchKernel9Panic(u8 *pos, u32 size);
u32 patchP9AccessChecks(u8 *pos, u32 size);
u32 patchUnitInfoValueSet(u8 *pos, u32 size);
u32 patchP9AMTicketWrapperZeroKeyIV(u8 *pos, u32 size, u32 firmVersion);
u32 patchLgySignatureChecks(u8 *pos, u32 size);
u32 patchTwlInvalidSignatureChecks(u8 *pos, u32 size);
u32 patchTwlNintendoLogoChecks(u8 *pos, u32 size);
u32 patchTwlWhitelistChecks(u8 *pos, u32 size);
u32 patchTwlFlashcartChecks(u8 *pos, u32 size, u32 firmVersion);
u32 patchOldTwlFlashcartChecks(u8 *pos, u32 size);
u32 patchTwlShaHashChecks(u8 *pos, u32 size);
u32 patchAgbBootSplash(u8 *pos, u32 size);
void patchTwlBg(u8 *pos, u32 size); // silently fails
u32 patchLgyK11(u8 *section1, u32 section1Size, u8 *section2, u32 section2Size);
u32 patchProtoNandSignatureCheck(u8 *pos, u32 size);