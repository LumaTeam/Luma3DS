/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2019 Aurora Wright, TuxSH
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

#include "memory.h"

//Boyer-Moore Horspool algorithm, adapted from http://www-igm.univ-mlv.fr/~lecroq/string/node18.html#SECTION00180
u8 *memsearch(u8 *startPos, const void *pattern, u32 size, u32 patternSize)
{
    const u8 *patternc = (const u8 *)pattern;
    u32 table[256];

    //Preprocessing
    for(u32 i = 0; i < 256; i++)
        table[i] = patternSize;
    for(u32 i = 0; i < patternSize - 1; i++)
        table[patternc[i]] = patternSize - i - 1;

    //Searching
    u32 j = 0;
    while(j <= size - patternSize)
    {
        u8 c = startPos[j + patternSize - 1];
        if(patternc[patternSize - 1] == c && memcmp(pattern, startPos + j, patternSize - 1) == 0)
            return startPos + j;
        j += table[c];
    }

    return NULL;
}

void hexItoa(u64 number, char *out, u32 digits, bool uppercase)
{
    const char hexDigits[] = "0123456789ABCDEF";
    const char hexDigitsLowercase[] = "0123456789abcdef";
    u32 i = 0;

    while(number > 0)
    {
        out[digits - 1 - i++] = uppercase ? hexDigits[number & 0xF] : hexDigitsLowercase[number & 0xF];
        number >>= 4;
    }

    while(i < digits) out[digits - 1 - i++] = '0';
}


// Copied from newlib, without the reent stuff + some other stuff
unsigned long int xstrtoul(const char *nptr, char **endptr, int base, bool allowPrefix, bool *ok)
{
    register const unsigned char *s = (const unsigned char *)nptr;
    register unsigned long acc;
    register int c;
    register unsigned long cutoff;
    register int neg = 0, any, cutlim;

    if(ok != NULL)
        *ok = true;
    /*
     * See strtol for comments as to the logic used.
     */
    do {
        c = *s++;
    } while ((c >= 9 && c <= 13) || c == ' ');
    if (c == '-') {
        if(!allowPrefix) {
            if(ok != NULL)
                *ok = false;
            return 0;
        }
        neg = 1;
        c = *s++;
    } else if (c == '+'){
        if(!allowPrefix) {
            if(ok != NULL)
                *ok = false;
            return 0;
        }

        c = *s++;
    }
    if ((base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X')) {

        if(!allowPrefix) {
            if(ok != NULL)
                *ok = false;
            return 0;
        }
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0) {
        if(!allowPrefix) {
            if(ok != NULL)
                *ok = false;
            return 0;
        }

        base = c == '0' ? 8 : 10;
    }
    cutoff = (unsigned long)(-1) / (unsigned long)base;
    cutlim = (unsigned long)(-1) % (unsigned long)base;
    for (acc = 0, any = 0;; c = *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            c -= c >= 'A' && c <= 'Z' ? 'A' - 10 : 'a' - 10;
        else
            break;
        if (c >= base)
            break;
               if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = (unsigned long)-1;
        if(ok != NULL)
            *ok = false;
//        rptr->_errno = ERANGE;
    } else if (neg)
        acc = -acc;
    if (endptr != 0)
        *endptr = (char *) (any ? (char *)s - 1 : nptr);
    return (acc);
}

// Copied from newlib, without the reent stuff + some other stuff
unsigned long long int xstrtoull(const char *nptr, char **endptr, int base, bool allowPrefix, bool *ok)
{
    register const unsigned char *s = (const unsigned char *)nptr;
    register unsigned long long acc;
    register int c;
    register unsigned long long cutoff;
    register int neg = 0, any, cutlim;

    if(ok != NULL)
        *ok = true;
    /*
     * See strtol for comments as to the logic used.
     */
    do {
        c = *s++;
    } while ((c >= 9 && c <= 13) || c == ' ');
    if (c == '-') {
        if(!allowPrefix) {
            if(ok != NULL)
                *ok = false;
            return 0;
        }
        neg = 1;
        c = *s++;
    } else if (c == '+'){
        if(!allowPrefix) {
            if(ok != NULL)
                *ok = false;
            return 0;
        }

        c = *s++;
    }
    if ((base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X')) {

        if(!allowPrefix) {
            if(ok != NULL)
                *ok = false;
            return 0;
        }
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0) {
        if(!allowPrefix) {
            if(ok != NULL)
                *ok = false;
            return 0;
        }

        base = c == '0' ? 8 : 10;
    }
    cutoff = (unsigned long long)(-1ull) / (unsigned long long)base;
    cutlim = (unsigned long long)(-1ull) % (unsigned long long)base;
    for (acc = 0, any = 0;; c = *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            c -= c >= 'A' && c <= 'Z' ? 'A' - 10 : 'a' - 10;
        else
            break;
        if (c >= base)
            break;
               if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = (unsigned long long)-1ull;
        if(ok != NULL)
            *ok = false;
//        rptr->_errno = ERANGE;
    } else if (neg)
        acc = -acc;
    if (endptr != 0)
        *endptr = (char *) (any ? (char *)s - 1 : nptr);
    return (acc);
}
