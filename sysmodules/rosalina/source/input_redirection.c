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

#include <3ds.h>
#include <arpa/inet.h>
#include "utils.h" // for makeARMBranch
#include "minisoc.h"
#include "input_redirection.h"
#include "menus.h"
#include "memory.h"

bool inputRedirectionEnabled = false;
Handle inputRedirectionThreadStartedEvent;

static MyThread inputRedirectionThread;
static u8 ALIGN(8) inputRedirectionThreadStack[0x4000];

MyThread *inputRedirectionCreateThread(void)
{
    if(R_FAILED(MyThread_Create(&inputRedirectionThread, inputRedirectionThreadMain, inputRedirectionThreadStack, 0x4000, 0x20, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &inputRedirectionThread;
}

//                       local hid,  local tsrd  localcprd,  localtswr,  localcpwr,  remote hid, remote ts,  remote circle
static u32 hidData[] = { 0x00000FFF, 0x02000000, 0x007FF7FF, 0x00000000, 0x00000000, 0x00000FFF, 0x02000000, 0x007FF7FF };
//                      remote ir
static u32 irData[] = { 0x80800081 }; // Default: C-Stick at the center, no buttons.

int inputRedirectionStartResult;

void inputRedirectionThreadMain(void)
{
    Result res = 0;
    res = miniSocInit();
    if(R_FAILED(res))
        return;

    int sock = socSocket(AF_INET, SOCK_DGRAM, 0);
    while(sock == -1)
    {
        svcSleepThread(1000 * 0000 * 0000LL);
        sock = socSocket(AF_INET, SOCK_DGRAM, 0);
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(4950);
    saddr.sin_addr.s_addr = gethostid();
    res = socBind(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));
    if(res != 0)
    {
        socClose(sock);
        miniSocExit();
        inputRedirectionStartResult = res;
        return;
    }

    inputRedirectionEnabled = true;
    svcSignalEvent(inputRedirectionThreadStartedEvent);

    u32 *hidDataPhys = PA_FROM_VA_PTR(hidData);
    hidDataPhys += 5; // skip to +20

    u32 *irDataPhys = PA_FROM_VA_PTR(irData);

    char buf[20];
    u32 oldSpecialButtons = 0, specialButtons = 0;
    while(inputRedirectionEnabled && !terminationRequest)
    {
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int pollres = socPoll(&pfd, 1, 10);
        if(pollres > 0 && (pfd.revents & POLLIN))
        {
            int n = soc_recvfrom(sock, buf, 20, 0, NULL, 0);
            if(n < 0)
                break;
            else if(n < 12)
                continue;

            memcpy(hidDataPhys, buf, 12);
            if(n >= 20)
            {
                memcpy(irDataPhys, buf + 12, 4);

                oldSpecialButtons = specialButtons;
                memcpy(&specialButtons, buf + 16, 4);

                if(!(oldSpecialButtons & 1) && (specialButtons & 1)) // HOME button pressed
                    srvPublishToSubscriber(0x204, 0);
                else if((oldSpecialButtons & 1) && !(specialButtons & 1)) // HOME button released
                    srvPublishToSubscriber(0x205, 0);

                if(!(oldSpecialButtons & 2) && (specialButtons & 2)) // POWER button pressed
                    srvPublishToSubscriber(0x202, 0);

                if(!(oldSpecialButtons & 4) && (specialButtons & 4)) // POWER button held long
                    srvPublishToSubscriber(0x203, 0);
            }
        }
    }

    struct linger linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;

    socSetsockopt(sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));

    socClose(sock);
    miniSocExit();
}

void hidCodePatchFunc(void);
void irCodePatchFunc(void);

Result InputRedirection_DoOrUndoPatches(void)
{
    s64 startAddress, textTotalRoundedSize, rodataTotalRoundedSize, dataTotalRoundedSize;
    u32 totalSize;
    Handle processHandle;

    Result res = OpenProcessByName("hid", &processHandle);

    if(R_SUCCEEDED(res))
    {
        svcGetProcessInfo(&textTotalRoundedSize, processHandle, 0x10002); // only patch .text + .data
        svcGetProcessInfo(&rodataTotalRoundedSize, processHandle, 0x10003);
        svcGetProcessInfo(&dataTotalRoundedSize, processHandle, 0x10004);

        totalSize = (u32)(textTotalRoundedSize + rodataTotalRoundedSize + dataTotalRoundedSize);

        svcGetProcessInfo(&startAddress, processHandle, 0x10005);
        res = svcMapProcessMemoryEx(processHandle, 0x00100000, (u32) startAddress, totalSize);

        if(R_SUCCEEDED(res))
        {
            static const u32 hidOrigRegisterAndValue[] = { 0x1EC46000, 0x4001 };
            static const u32 hidOrigCode[] = {
                0xE92D4070, // push {r4-r6, lr}
                0xE1A05001, // mov  r5, r1
                0xEE1D4F70, // mrc  p15, 0, r4, c13, c0, 3
                0xE3A01801, // mov  r1, #0x10000
                0xE5A41080, // str  r1, [r4,#0x80]!
            };

            static u32 *hidRegPatchOffsets[2];
            static u32 *hidPatchJumpLoc;

            if(inputRedirectionEnabled)
            {
                memcpy(hidRegPatchOffsets[0], &hidOrigRegisterAndValue, sizeof(hidOrigRegisterAndValue));
                memcpy(hidRegPatchOffsets[1], &hidOrigRegisterAndValue, sizeof(hidOrigRegisterAndValue));
                memcpy(hidPatchJumpLoc, &hidOrigCode, sizeof(hidOrigCode));
            }
            else
            {
                u32 hidDataPhys = (u32)PA_FROM_VA_PTR(hidData);
                u32 hidCodePhys = (u32)PA_FROM_VA_PTR(&hidCodePatchFunc);
                u32 hidHook[] = {
                    0xE59F3004, // ldr r3,  [pc, #4]
                    0xE59FC004, // ldr r12, [pc, #4]
                    0xE12FFF1C, // bx  r12
                    hidDataPhys,
                    hidCodePhys,
                };

                u32 *off = (u32 *)memsearch((u8 *)0x00100000, &hidOrigRegisterAndValue, totalSize, sizeof(hidOrigRegisterAndValue));
                if(off == NULL)
                {
                    svcUnmapProcessMemoryEx(processHandle, 0x00100000, totalSize);
                    return -1;
                }

                u32 *off2 = (u32 *)memsearch((u8 *)off + sizeof(hidOrigRegisterAndValue), &hidOrigRegisterAndValue, totalSize - ((u32)off - 0x00100000), sizeof(hidOrigRegisterAndValue));
                if(off2 == NULL)
                {
                    svcUnmapProcessMemoryEx(processHandle, 0x00100000, totalSize);
                    return -2;
                }

                u32 *off3 = (u32 *)memsearch((u8 *)0x00100000, &hidOrigCode, totalSize, sizeof(hidOrigCode));
                if(off3 == NULL)
                {
                    svcUnmapProcessMemoryEx(processHandle, 0x00100000, totalSize);
                    return -3;
                }

                hidRegPatchOffsets[0] = off;
                hidRegPatchOffsets[1] = off2;
                hidPatchJumpLoc = off3;

                *off = *off2 = hidDataPhys;
                memcpy(off3, &hidHook, sizeof(hidHook));
            }
        }

        res = svcUnmapProcessMemoryEx(processHandle, 0x00100000, totalSize);
    }
    svcCloseHandle(processHandle);

    res = OpenProcessByName("ir", &processHandle);
    if(R_SUCCEEDED(res) && osGetKernelVersion() >= SYSTEM_VERSION(2, 44, 6))
    {
        svcGetProcessInfo(&textTotalRoundedSize, processHandle, 0x10002); // only patch .text + .data
        svcGetProcessInfo(&rodataTotalRoundedSize, processHandle, 0x10003);
        svcGetProcessInfo(&dataTotalRoundedSize, processHandle, 0x10004);

        totalSize = (u32)(textTotalRoundedSize + rodataTotalRoundedSize + dataTotalRoundedSize);

        svcGetProcessInfo(&startAddress, processHandle, 0x10005);
        res = svcMapProcessMemoryEx(processHandle, 0x00100000, (u32) startAddress, totalSize);

        if(R_SUCCEEDED(res))
        {
            static bool useOldSyncCode;
            static u32 irOrigReadingCode[5] = {
                0xE5940000, // ldr r0, [r4]
                0xE1A01005, // mov r1, r5
                0xE3A03005, // mov r3, #5
                0xE3A02011, // mov r2, #17
                0x00000000  // (bl i2c_read_raw goes here)
            };

            static const u32 irOrigWaitSyncCode[] = {
                0xEF000024, // svc 0x24 (WaitSynchronization)
                0xE1B01FA0, // movs r1, r0, lsr#31
                0xE1A0A000, // mov r10, r0
            }, irOrigWaitSyncCodeOld[] = {
                0xE0AC6000, // adc r6, r12, r0
                0xE5D70000, // ldrb r0, [r7]
            }; // pattern for 8.1

            static const u32 irOrigCppFlagCode[] = {
                0xE3550000, // cmp r5, #0
                0xE3A0B080, // mov r11, #0x80
            };

            static u32 *irHookLoc, *irWaitSyncLoc, *irCppFlagLoc;

            if(inputRedirectionEnabled)
            {
                memcpy(irHookLoc, &irOrigReadingCode, sizeof(irOrigReadingCode));
                if(useOldSyncCode)
                    memcpy(irWaitSyncLoc, &irOrigWaitSyncCodeOld, sizeof(irOrigWaitSyncCodeOld));
                else
                    memcpy(irWaitSyncLoc, &irOrigWaitSyncCode, sizeof(irOrigWaitSyncCode));
                memcpy(irCppFlagLoc, &irOrigCppFlagCode, sizeof(irOrigCppFlagCode));
            }
            else
            {
                u32 irDataPhys = (u32)PA_FROM_VA_PTR(irData);
                u32 irCodePhys = (u32)PA_FROM_VA_PTR(&irCodePatchFunc);

                u32 irHook[] = {
                    0xE5940000, // ldr r0, [r4]
                    0xE1A01005, // mov r1, r5
                    0xE59FC000, // ldr r12, [pc] (actually +8)
                    0xE12FFF3C, // blx r12
                    irCodePhys,
                };

                u32 *off = (u32 *)memsearch((u8 *)0x00100000, &irOrigReadingCode, totalSize, sizeof(irOrigReadingCode) - 4);
                if(off == NULL)
                {
                    svcUnmapProcessMemoryEx(processHandle, 0x00100000, totalSize);
                    return -4;
                }

                u32 *off2 = (u32 *)memsearch((u8 *)0x00100000, &irOrigWaitSyncCode, totalSize, sizeof(irOrigWaitSyncCode));
                if(off2 == NULL)
                {
                    off2 = (u32 *)memsearch((u8 *)0x00100000, &irOrigWaitSyncCodeOld, totalSize, sizeof(irOrigWaitSyncCodeOld));
                    if(off2 == NULL)
                    {
                        svcUnmapProcessMemoryEx(processHandle, 0x00100000, totalSize);
                        return -5;
                    }

                    useOldSyncCode = true;
                }
                else
                    useOldSyncCode = false;

                u32 *off3 = (u32 *)memsearch((u8 *)0x00100000, &irOrigCppFlagCode, totalSize, sizeof(irOrigCppFlagCode));
                if(off3 == NULL)
                {
                    svcUnmapProcessMemoryEx(processHandle, 0x00100000, totalSize);
                    return -6;
                }

                *(void **)(irCodePhys + 8) = decodeARMBranch(off + 4);
                *(void **)(irCodePhys + 12) = (void*)irDataPhys;

                irHookLoc = off;
                irWaitSyncLoc = off2;
                irCppFlagLoc = off3;

                irOrigReadingCode[4] = off[4]; // Copy the branch.

                memcpy(irHookLoc, &irHook, sizeof(irHook));

                // This "NOP"s out a WaitSynchronisation1 (on the event bound to the 'IR' interrupt) or the check of a previous one
                *irWaitSyncLoc = 0xE3A00000; // mov r0, #0

                // This NOPs out a flag check in ir:user's CPP emulation
                *irCppFlagLoc = 0xE3150000; // tst r5, #0
            }
        }

        res = svcUnmapProcessMemoryEx(processHandle, 0x00100000, totalSize);
    }
    svcCloseHandle(processHandle);

    return res;
}
