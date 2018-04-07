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

void *memcpy(void *dest, const void *src, u32 size);
int memcmp(const void *buf1, const void *buf2, u32 size);
void *memset(void *dest, u32 value, u32 size) __attribute__((used));
void *memset32(void *dest, u32 value, u32 size);
u8 *memsearch(u8 *startPos, const void *pattern, u32 size, u32 patternSize);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, u32 size);
s32 strnlen(const char *string, s32 maxlen);
s32 strlen(const char *string);
s32 strcmp(const char *str1, const char *str2);
s32 strncmp(const char *str1, const char *str2, u32 size);
const char *strchr(const char *string, int c);
void hexItoa(u64 number, char *out, u32 digits, bool uppercase);
unsigned long int xstrtoul(const char *nptr, char **endptr, int base, bool allowPrefix, bool *ok);
