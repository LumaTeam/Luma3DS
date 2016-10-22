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

u32 strlen(const char *string)
{
    char *stringEnd = (char *)string;

    while(*stringEnd != 0) stringEnd++;

    return stringEnd - string;
}

void concatenateStrings(char *destination, const char *source)
{
    u32 i = strlen(source),
        j = strlen(destination);

    memcpy(&destination[j], source, i + 1);
}

void hexItoa(u32 number, char *out, u32 digits, bool fillString)
{
    const char hexDigits[] = "0123456789ABCDEF";
    u32 i;

    for(i = 0; number > 0; i++)
    {
        out[digits - 1 - i] = hexDigits[number & 0xF];
        number >>= 4;
    }

    if(fillString) while(i < digits) out[digits - 1 - i++] = '0';
}

void decItoa(u32 number, char *out, u32 digits)
{
    for(u32 i = 0; number > 0; i++)
    {
        out[digits - 1 - i] = '0' + number % 10;
        number /= 10;
    }
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