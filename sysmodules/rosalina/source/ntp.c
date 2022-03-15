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

#include <3ds.h>
#include <arpa/inet.h>
#include <string.h>
#include "utils.h"
#include "minisoc.h"
#include "ntp.h"

#define NUM2BCD(n)          ((n<99) ? (((n/10)*0x10)|(n%10)) : 0x99)

//#define NTP_TIMESTAMP_DELTA 2208988800ull

#define MSEC_DELTA_1900_2000 3155673600000ull

#define MAKE_IPV4(a,b,c,d)  ((a) << 24 | (b) << 16 | (c) << 8 | (d))

#ifndef NTP_IP
#define NTP_IP              MAKE_IPV4(51, 137, 137, 111) // time.windows.com
#endif

// From https://github.com/lettier/ntpclient/blob/master/source/c/main.c

typedef struct NtpPacket
{

    u8 li_vn_mode;      // Eight bits. li, vn, and mode.
                             // li.   Two bits.   Leap indicator.
                             // vn.   Three bits. Version number of the protocol.
                             // mode. Three bits. Client will pick mode 3 for client.

    u8 stratum;         // Eight bits. Stratum level of the local clock.
    u8 poll;            // Eight bits. Maximum interval between successive messages.
    u8 precision;       // Eight bits. Precision of the local clock.

    u32 rootDelay;      // 32 bits. Total round trip delay time.
    u32 rootDispersion; // 32 bits. Max error aloud from primary clock source.
    u32 refId;          // 32 bits. Reference clock identifier.

    u32 refTm_s;        // 32 bits. Reference time-stamp seconds.
    u32 refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

    u32 origTm_s;       // 32 bits. Originate time-stamp seconds.
    u32 origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

    u32 rxTm_s;         // 32 bits. Received time-stamp seconds.
    u32 rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

    u32 txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
    u32 txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

} NtpPacket;            // Total: 384 bits or 48 bytes.

Result ntpGetTimeStamp(u64 *msSince1900, u64 *samplingTick)
{
    Result res = 0;
    struct linger linger;
    *msSince1900 = 0;
    *samplingTick = 0;

    res = miniSocInit();
    if(R_FAILED(res))
        return res;

    int sock = socSocket(AF_INET, SOCK_DGRAM, 0);
    if (sock < -10000) {
        // Socket services broken
        return sock;
    }

    struct sockaddr_in servAddr = {0}; // Server address data structure.
    NtpPacket packet = {0};

    // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.
    packet.li_vn_mode = 0x1b;

    // Zero out the server address structure.
    servAddr.sin_family = AF_INET;

    // Copy the server's IP address to the server address structure.

    servAddr.sin_addr.s_addr = htonl(NTP_IP);
    // Convert the port number integer to network big-endian style and save it to the server address structure.

    servAddr.sin_port = htons(123);

    // Call up the server using its IP address and port number.
    res = -1;
    if(socConnect(sock, (struct sockaddr *)&servAddr, sizeof(struct sockaddr_in)) < 0)
        goto cleanup;

    u64 roundTripTicks = svcGetSystemTick();
    if(socSend(sock, &packet, sizeof(NtpPacket), 0) < 0)
        goto cleanup;

    if(socRecv(sock, &packet, sizeof(NtpPacket), 0) < 0)
        goto cleanup;
    roundTripTicks = svcGetSystemTick() - roundTripTicks;
    u64 dt = 1000 * 1000 * roundTripTicks / (2 * SYSCLOCK_ARM11); // avg = round trip time / 2
    res = 0;

    // These two fields contain the time-stamp seconds as the packet left the NTP server.
    // The number of seconds correspond to the seconds passed since 1900.
    // ntohl() converts the bit/byte order from the network's to host's "endianness".

    packet.txTm_s = ntohl(packet.txTm_s); // Time-stamp seconds.
    packet.txTm_f = ntohl(packet.txTm_f); // Time-stamp fraction of a second. txTm is 32.32 fixed point.
    u64 txTmUsec = (1000 * 1000 * (u64)packet.txTm_f) >> 32; // convert txTm to usec (truncate; end result is in ms anyway)
    txTmUsec += 1000 * 1000 * (u64)packet.txTm_s;
    *msSince1900 = (txTmUsec + dt + 500u) / 1000u;
    *samplingTick = svcGetSystemTick();

cleanup:
    linger.l_onoff = 1;
    linger.l_linger = 0;
    socSetsockopt(sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));

    socClose(sock);
    miniSocExit();

    return res;
}

static Result ntpSetTimeDateImpl(u64 msSince1900, u64 samplingTick, bool syncRtc)
{
    Result res = ptmSysmInit();
    if (R_FAILED(res)) return res;
    res = ptmSetsInit();
    if (R_FAILED(res))
    {
        ptmSysmExit();
        return res;
    }
    res = cfguInit();
    if (R_FAILED(res))
    {
        ptmSetsExit();
        ptmSysmExit();
        return res;
    }

    u64 dt = 1000 * (svcGetSystemTick() - samplingTick) / SYSCLOCK_ARM11;
    s64 msY2k = msSince1900 + dt - MSEC_DELTA_1900_2000;

    if (syncRtc)
    {
        u64 samplingTick2 = svcGetSystemTick();

        s64 timeOff = 0;
        res = CFG_SetConfigInfoBlk4(8, 0x30001, &timeOff);
        if (R_SUCCEEDED(res)) res = CFG_SetConfigInfoBlk4(8, 0x30002, &timeOff);

        // Save the config changes
        if (R_SUCCEEDED(res)) res = CFG_UpdateConfigSavegame();

        // Wait till next second
        msY2k += 1000 * (svcGetSystemTick() - samplingTick2) / SYSCLOCK_ARM11;
        if (msY2k % 1000u != 0)
        {
            u64 dt2 = 1000u - msY2k % 1000u;
            svcSleepThread(dt2);
            msY2k += dt2;
        }
        if (R_SUCCEEDED(res)) res = PTMSYSM_SetRtcTime(msY2k);
    }
    else
    {
        if (R_SUCCEEDED(res)) res = PTMSYSM_SetUserTime(msY2k);
        if (R_SUCCEEDED(res)) res = PTMSETS_SetSystemTime(msY2k);
    }


    cfguExit();
    ptmSetsExit();
    ptmSysmExit();
    return res;
}

Result ntpSetTimeDate(u64 msSince1900, u64 samplingTick)
{
    return ntpSetTimeDateImpl(msSince1900, samplingTick, false);
}

// Not actually used for NTP, but...
Result ntpNullifyUserTimeOffset(void)
{
    u64 msSince1900 = osGetTime();
    u64 samplingTick = svcGetSystemTick();
    return ntpSetTimeDateImpl(msSince1900, samplingTick, true);
}
