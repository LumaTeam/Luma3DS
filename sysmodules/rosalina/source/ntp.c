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

#include <3ds.h>
#include <arpa/inet.h>
#include <string.h>
#include "utils.h"
#include "minisoc.h"

#define NUM2BCD(n)  ((n<99) ? (((n/10)*0x10)|(n%10)) : 0x99)

#define NTP_TIMESTAMP_DELTA 2208988800ull

#ifndef NTP_IP
#define NTP_IP 0xD8EF2300
#endif

typedef struct RtcTime {
        // From 3dbrew
        u8 seconds;
        u8 minutes;
        u8 hours;
        u8 dayofweek;
        u8 dayofmonth;
        u8 month;
        u8 year;
        u8 leapcount;
} RtcTime;

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

void rtcToBcd(u8 *out, const RtcTime *in)
{
    memcpy(out, in, 8);
    for (u32 i = 0; i < 8; i++)
    {
        u8 units = out[i] % 10;
        u8 tens = (out[i] - units) / 10;
        out[i] = (tens << 4) | units;
    }
}

Result ntpGetTimeStamp(time_t *outTimestamp)
{
    Result res = 0;
    struct linger linger;
    res = miniSocInit();
    if(R_FAILED(res))
        return res;

    int sock = socSocket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servAddr = {0}; // Server address data structure.
    NtpPacket packet = {0};

    // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.
    packet.li_vn_mode = 0x1b;

    // Zero out the server address structure.
    servAddr.sin_family = AF_INET;

    // Copy the server's IP address to the server address structure.

    servAddr.sin_addr.s_addr = htonl(NTP_IP); // 216.239.35.0 time1.google.com
    // Convert the port number integer to network big-endian style and save it to the server address structure.

    servAddr.sin_port = htons(123);

    // Call up the server using its IP address and port number.
    res = -1;
    if(socConnect(sock, (struct sockaddr *)&servAddr, sizeof(struct sockaddr_in)) < 0)
        goto cleanup;

    if(soc_send(sock, &packet, sizeof(NtpPacket), 0) < 0)
        goto cleanup;

    if(soc_recv(sock, &packet, sizeof(NtpPacket), 0) < 0)
        goto cleanup;

    res = 0;

    // These two fields contain the time-stamp seconds as the packet left the NTP server.
    // The number of seconds correspond to the seconds passed since 1900.
    // ntohl() converts the bit/byte order from the network's to host's "endianness".

    packet.txTm_s = ntohl(packet.txTm_s); // Time-stamp seconds.
    packet.txTm_f = ntohl(packet.txTm_f); // Time-stamp fraction of a second.

    // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
    // Subtract 70 years worth of seconds from the seconds since 1900.
    // This leaves the seconds since the UNIX epoch of 1970.
    // (1900)------------------(1970)**************************************(Time Packet Left the Server)
    *outTimestamp = (time_t)(packet.txTm_s - NTP_TIMESTAMP_DELTA);

cleanup:
    linger.l_onoff = 1;
    linger.l_linger = 0;
    socSetsockopt(sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));

    socClose(sock);
    miniSocExit();

    return res;
}

Result ntpSetTimeDate(const struct tm *localt)
{
    Result res = mcuHwcInit();
    if (R_FAILED(res)) return res;


    res = cfguInit();
    if (R_FAILED(res)) goto cleanup;

    // First, set the config RTC offset to 0
    u8 rtcOff = 0;
    u8 rtcOff2[8] = {0};
    res = CFG_SetConfigInfoBlk4(1, 0x10000, &rtcOff);
    if (R_FAILED(res)) goto cleanup;
    res = CFG_SetConfigInfoBlk4(8, 0x30001, rtcOff2);
    if (R_FAILED(res)) goto cleanup;

    u8 yr = (u8)(localt->tm_year - 100);
    // Update the RTC
    u8 bcd[8];
    RtcTime lt = {
        .seconds    = (u8)localt->tm_sec,
        .minutes    = (u8)localt->tm_min,
        .hours      = (u8)localt->tm_hour,
        .dayofweek  = (u8)localt->tm_wday,
        .dayofmonth = (u8)localt->tm_mday,
        .month      = (u8)(localt->tm_mon + 1),
        .year       = yr,
        .leapcount  = 0,
    };
    rtcToBcd(bcd, &lt);

    res = MCUHWC_WriteRegister(0x30, bcd, 7);
    if (R_FAILED(res)) goto cleanup;

    // Save the config changes
    res = CFG_UpdateConfigSavegame();
    cleanup:
    mcuHwcExit();
    cfguExit();
    return res;
}
