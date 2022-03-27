/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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

#define PATTERN(a) a "_*.firm"

bool mountFs(bool isSd, bool switchToCtrNand);
u32 fileRead(void *dest, const char *path, u32 maxSize);
u32 getFileSize(const char *path);
bool fileWrite(const void *buffer, const char *path, u32 size);
bool fileDelete(const char *path);
bool fileCopy(const char *pathSrc, const char *pathDst, bool replace, void *tmpBuffer, size_t bufferSize);
bool findPayload(char *path, u32 pressed);
bool payloadMenu(char *path, bool *hasDisplayedMenu);
u32 firmRead(void *dest, u32 firmType);
void findDumpFile(const char *folderPath, char *fileName);

bool doLumaUpgradeProcess(void);
