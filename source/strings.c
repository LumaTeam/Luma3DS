/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
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
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

#include "strings.h"
#include "memory.h"

int strlen(const char *string)
{
    char *stringEnd = (char *)string;

    while(*stringEnd) stringEnd++;

    return stringEnd - string;
}

void concatenateStrings(char *destination, const char *source)
{
    int i = strlen(source),
        j = strlen(destination);

    memcpy(&destination[j], source, i + 1);
}

void hexItoa(u32 number, char *out)
{
    const char hexDigits[] = "0123456789ABCDEF";
    u32 i = 0;

    while(number > 0)
    {
        out[7 - i++] = hexDigits[number & 0xF];
        number >>= 4;
    }

    for(; i < 8; i++) out[7 - i] = '0';
}