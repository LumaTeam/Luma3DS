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

#include "utils.h"
#include "csvc.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <3ds.h>

void formatMemoryPermission(char *outbuf, MemPerm perm)
{
    if (perm == MEMPERM_DONTCARE)
    {
        strcpy(outbuf, "???");
        return;
    }

    outbuf[0] = perm & MEMPERM_READ ? 'r' : '-';
    outbuf[1] = perm & MEMPERM_WRITE ? 'w' : '-';
    outbuf[2] = perm & MEMPERM_EXECUTE ? 'x' : '-';
    outbuf[3] = '\0';
}

void formatUserMemoryState(char *outbuf, MemState state)
{
    static const char *states[12] =
    {
        "Free",
        "Reserved",
        "IO",
        "Static",
        "Code",
        "Private",
        "Shared",
        "Continuous",
        "Aliased",
        "Alias",
        "AliasCode",
        "Locked"
    };

    strcpy(outbuf, state > 11 ? "Unknown" : states[state]);
}

u32 formatMemoryMapOfProcess(char *outbuf, u32 bufLen, Handle handle)
{
    u32         maxLineSize = 35 + (handle == CUR_PROCESS_HANDLE ? 15 : 0);
    u32         address = 0;
    u32         posInBuffer = 0;
    u32         maxPosInBuffer = bufLen - maxLineSize; // 35 is the maximum length of a formatted region
    MemInfo     memi;
    PageInfo    pagei;
    char        pabuf[32];
    char        permbuf[8];
    char        statebuf[16];

    s64 TTBCR;
    svcGetSystemInfo(&TTBCR, 0x10002, 0);

    while (address < (1u << (32 - (u32)TTBCR)) // Limit to check for regions
        && posInBuffer < maxPosInBuffer
        && R_SUCCEEDED(svcQueryProcessMemory(&memi, &pagei, handle, address)))
    {
        // Update the address for next region
        address = memi.base_addr + memi.size;

        // If region isn't FREE then add it to the list
        if (memi.state != MEMSTATE_FREE)
        {
            if (handle == CUR_PROCESS_HANDLE)
            {
                u32 pa = svcConvertVAToPA((void *)memi.base_addr, false);
                sprintf(pabuf, " (PA %08lx)", pa);
            }
            else
                pabuf[0] = '\0';

            formatMemoryPermission(permbuf, memi.perm);
            formatUserMemoryState(statebuf, memi.state);

            posInBuffer += sprintf(outbuf + posInBuffer, "%08lx - %08lx%s %s %s\n",
                memi.base_addr, address, pabuf, permbuf, statebuf);
        }
    }

    svcCloseHandle(handle);
    return posInBuffer;
}




// Returns year/month/day triple in civil calendar
// daysSince1970 is number of days since 1970-01-01
// Output year is e.g. 2024, month is 1-12, day is 1-31
// Adapted from: https://howardhinnant.github.io/date_algorithms.html#civil_from_days
// (original author Howard Hinnant)
void civil_from_days(u32 daysSince1970, u32 *y, u32 *m, u32 *d)
{
    daysSince1970 += 719468;
    int era = daysSince1970 / 146097;
    unsigned doe = daysSince1970 - era * 146097;
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    *y = yoe + era * 400;
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp = (5*doy + 2) / 153;
    *d = doy - (153*mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y += (*m <= 2);
}
 

int dateTimeToString(char *out, u64 msSince1900, DateTimeFormat dateTimeFormat)
{
    // Time unit conversion adapted from https://stackoverflow.com/questions/21593692/convert-unix-timestamp-to-date-without-system-libs
    // (original author @gnif under CC-BY-SA 4.0)
    u32 seconds, minutes, hours, daysSince1900, year, month, weekDay;
    u64 milliseconds = msSince1900;
    seconds = milliseconds/1000;
    milliseconds %= 1000;
    minutes = seconds / 60;
    seconds %= 60;
    hours = minutes / 60;
    minutes %= 60;
    daysSince1900 = hours / 24;
    hours %= 24;

    
    weekDay = (daysSince1900 + 1) % 7; // 1900-01-01 was a Monday (1)

    // Convert to year/month/day
    u32 daysSince1970 = daysSince1900 - 25567; // 25567 days between 1900-01-01 and 1970-01-01
    civil_from_days(daysSince1970, &year, &month, &daysSince1900);

    if (dateTimeFormat == DATE_TIME_FILENAME)
        return sprintf(out, "%04lu-%02lu-%02lu_%02lu-%02lu-%02lu.%03llu", year, month, daysSince1900, hours, minutes, seconds, milliseconds);
    else if (dateTimeFormat == DATE_TIME_ISO)
        return sprintf(out, "%04lu-%02lu-%02lu %02lu:%02lu:%02lu", year, month, daysSince1900, hours, minutes, seconds);
    else {
        char *daysHuman[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        char *monthsHuman[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        return sprintf(out, "%s, %lu %s %lu %02lu:%02lu", daysHuman[weekDay], daysSince1900, monthsHuman[month - 1], year, hours, minutes);
    }
}

int floatToString(char *out, float f, u32 precision, bool pad)
{
    // Floating point stuff is cringe
    if (isnanf(f))
        return sprintf(out, "NaN");
    else if (isinff(f) && f >= -0.0f)
        return sprintf(out, "inf");
    else if (isinff(f))
        return sprintf(out, "-inf");

    static const u64 pow10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };
    precision = precision >= 6 ? 6 : precision; // result inaccurate after 1e-6

    u64 mult = pow10[precision];
    double f2 = fabs((double)f) * mult + 0.5;
    u64 f3 = (u64)f2;
    const char *sign = (f >= 0.0f || f3 == 0) ? "" : "-";

    u64 intPart = f3 / mult;
    u64 fracPart = f3 % mult;

    if (pad)
        return sprintf(out, "%s%llu.%0*llu", sign, intPart, (int)precision, fracPart);
    else
    {
        int n = sprintf(out, "%s%llu", sign, intPart);
        if (fracPart == 0)
            return n;

        n += sprintf(out + n, ".%0*llu", (int)precision, fracPart);

        int n2 = n - 1;
        while (out[n2] == '0')
            out[n2--] = '\0';
        return n2;
    }
}
