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

#include "strings.h"
#include "memory.h"

u32 strlen(const char *string)
{
    char *stringEnd = (char *)string;

    while(*stringEnd != 0) stringEnd++;

    return stringEnd - string;
}

u32 strnlen(const char *string, u32 maxlen)
{
    u32 size;

    for(size = 0; size < maxlen && *string; string++, size++);

    return size;
}

u32 hexAtoi(const char *in, u32 digits)
{
    u32 res = 0;
    char *tmp = (char *)in;

    for(u32 i = 0; i < digits && *tmp != 0; tmp++, i++)
        res = (*tmp > '9' ? *tmp - 'A' + 10 : *tmp - '0') + (res << 4);

    return res;
}

u32 decAtoi(const char *in, u32 digits)
{
    u32 res = 0;
    char *tmp = (char *)in;

    for(u32 i = 0; i < digits && *tmp != 0; tmp++, i++)
        res = *tmp - '0' + res * 10;

    return res;
}
