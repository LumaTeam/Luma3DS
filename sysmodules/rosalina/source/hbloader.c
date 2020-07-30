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

/* This file was mostly written by fincs */

#include <3ds.h>
#include "hbloader.h"
#include "3dsx.h"
#include "menu.h"
#include "csvc.h"
#include "memory.h"

#include "gdb/server.h"
#include "pmdbgext.h"

extern GDBContext *nextApplicationGdbCtx;
extern GDBServer gdbServer;

static const char serviceList[32][8] =
{
    "APT:U",
    "ac:u",
    "am:net",
    "boss:P",
    "cam:u",
    "cfg:nor",
    "cfg:u",
    "csnd:SND",
    "dsp::DSP",
    "fs:USER",
    "gsp::Lcd",
    "gsp::Gpu",
    "hid:USER",
    "http:C",
    "ir:USER",
    "ir:rst",
    "ir:u",
    "ldr:ro",
    "mic:u",
    "ndm:u",
    "news:s",
    "nim:s",
    "ns:s",
    "nwm::UDS",
    "nwm::EXT",
    "ptm:u",
    "ptm:sysm",
    "pxi:dev",
    "qtm:u",
    "soc:U",
    "ssl:C",
    "y2r:u",
};

static const u64 dependencyListNativeFirm[] =
{
    0x0004013000002402LL, //ac
    0x0004013000001502LL, //am
    0x0004013000001702LL, //cfg
    0x0004013000001802LL, //codec
    0x0004013000002702LL, //csnd
    0x0004013000001A02LL, //dsp
    0x0004013000001B02LL, //gpio
    0x0004013000001C02LL, //gsp
    0x0004013000001D02LL, //hid
    0x0004013000002902LL, //http
    0x0004013000001E02LL, //i2c
    0x0004013000003302LL, //ir
    0x0004013000001F02LL, //mcu
    0x0004013000002C02LL, //nim
    0x0004013000002D02LL, //nwm
    0x0004013000002102LL, //pdn
    0x0004013000003102LL, //ps
    0x0004013000002202LL, //ptm
    0x0004013000002E02LL, //socket
    0x0004013000002302LL, //spi
    0x0004013000002F02LL, //ssl

    // Not present on SAFE_FIRM:
    0x0004013000003402LL, //boss
    0x0004013000001602LL, //camera
    0x0004013000002802LL, //dlp
    0x0004013000002002LL, //mic
    0x0004013000002B02LL, //ndm
    0x0004013000003502LL, //news
    0x0004013000003702LL, //ro
};

static const u64 dependencyListSafeFirm[] =
{
    0x0004013000002403LL, //ac
    0x0004013000001503LL, //am
    0x0004013000001703LL, //cfg
    0x0004013000001803LL, //codec
    0x0004013000002703LL, //csnd
    0x0004013000001A03LL, //dsp
    0x0004013000001B03LL, //gpio
    0x0004013000001C03LL, //gsp
    0x0004013000001D03LL, //hid
    0x0004013000002903LL, //http
    0x0004013000001E03LL, //i2c
    0x0004013000003303LL, //ir
    0x0004013000001F03LL, //mcu
    0x0004013000002C03LL, //nim
    0x0004013000002D03LL, //nwm
    0x0004013000002103LL, //pdn
    0x0004013000003103LL, //ps
    0x0004013000002203LL, //ptm
    0x0004013000002E03LL, //socket
    0x0004013000002303LL, //spi
    0x0004013000002F03LL, //ssl

    0x0004013000003203LL, //friends (wouldn't be launched otherwise)
};

static const u32 kernelCaps[] =
{
    0xFC00022C, // Kernel release version 8.0 is necessary for using the new linear mapping. Modified below.
    0xFF81FF50, // RW static mapping: 0x1FF50000
    0xFF81FF58, // RW static mapping: 0x1FF58000
    0xFF81FF70, // RW static mapping: 0x1FF70000
    0xFF81FF78, // RW static mapping: 0x1FF78000
    0xFF91F000, // RO static mapping: 0x1F000000
    0xFF91F600, // RO static mapping: 0x1F600000
    0xFF002109, // Exflags: APPLICATION memtype + "Shared page writing" + "Allow debug" + "Access core2"
    0xFE000200, // Handle table size: 0x200
};

static inline void assertSuccess(Result res)
{
    if(R_FAILED(res))
        svcBreak(USERBREAK_PANIC);
}

static u16 hbldrTarget[PATH_MAX+1];

static inline void error(u32* cmdbuf, Result rc)
{
    cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
    cmdbuf[1] = rc;
}

static u16 *u16_strncpy(u16 *dest, const u16 *src, u32 size)
{
    u32 i;
    for (i = 0; i < size && src[i] != 0; i++)
        dest[i] = src[i];
    while (i < size)
        dest[i++] = 0;

    return dest;
}

void HBLDR_RestartHbApplication(void *p)
{
    (void)p;
    // Don't crash if we fail

    FS_ProgramInfo programInfo;
    u32 pid;
    u32 launchFlags;

    Result res = PMDBG_GetCurrentAppInfo(&programInfo, &pid, &launchFlags);
    if (R_FAILED(res)) return;
    res = PMDBG_PrepareToChainloadHomebrew(programInfo.programId);
    if (R_FAILED(res)) return;
    res = PMAPP_TerminateCurrentApplication(3 * 1000 * 1000 *1000LL); // 3s, like what NS uses
    if (R_FAILED(res)) return;
    if (R_SUCCEEDED(res))
    {
        do
        {
            svcSleepThread(100 * 1000 * 1000LL);
            res = PMAPP_LaunchTitle(&programInfo, PMLAUNCHFLAGEXT_FAKE_DEPENDENCY_LOADING | launchFlags);
        } while (res == (Result)0xC8A05BF0);
    }
}

void HBLDR_HandleCommands(void *ctx)
{
    (void)ctx;
    Result res = 0;
    IFile file;
    u32 *cmdbuf = getThreadCommandBuffer();
    switch (cmdbuf[0] >> 16)
    {
        case 1: // LoadProcess
        {
            if (cmdbuf[0] != IPC_MakeHeader(1, 6, 0))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }

            u32 baseAddr = cmdbuf[1];
            u32 flags = cmdbuf[2] & 0xF00;
            u64 tid = (u64)cmdbuf[3] | ((u64)cmdbuf[4]<<32);
            char name[8];
            memcpy(name, &cmdbuf[5], sizeof(name));

            if (hbldrTarget[0] == 0)
            {
                u16_strncpy(hbldrTarget, u"/boot.3dsx", PATH_MAX);
                ldrArgvBuf[0] = 1;
                strncpy((char*)&ldrArgvBuf[1], "sdmc:/boot.3dsx", sizeof(ldrArgvBuf)-4);
            }

            res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_UTF16, hbldrTarget), FS_OPEN_READ);
            hbldrTarget[0] = 0;
            if (R_FAILED(res))
            {
                error(cmdbuf, res);
                break;
            }

            u32 totalSize = 0;
            res = Ldr_Get3dsxSize(&totalSize, &file);
            if (R_FAILED(res))
            {
                IFile_Close(&file);
                error(cmdbuf, res);
                break;
            }

            // note: mappableFree doesn't do anything
            u32 tmp = 0;
            u32 *addr = mappableAlloc(totalSize);
            res = svcControlMemoryEx(&tmp, (u32)addr, 0, totalSize, MEMOP_ALLOC | flags, MEMPERM_READ | MEMPERM_WRITE, true);
            if (R_FAILED(res))
            {
                IFile_Close(&file);
                error(cmdbuf, res);
                break;
            }

            Handle hCodeset = Ldr_CodesetFrom3dsx(name, addr, baseAddr, &file, tid);
            IFile_Close(&file);

            if (!hCodeset)
            {
                svcControlMemory(&tmp, (u32)addr, 0, totalSize, MEMOP_FREE, 0);
                error(cmdbuf, MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_LDR, RD_NOT_FOUND));
                break;
            }

            cmdbuf[0] = IPC_MakeHeader(1, 1, 2);
            cmdbuf[1] = 0;
            cmdbuf[2] = IPC_Desc_MoveHandles(1);
            cmdbuf[3] = hCodeset;
            break;
        }
        case 2: // SetTarget
        {
            if (cmdbuf[0] != IPC_MakeHeader(2, 0, 2) || (cmdbuf[1] & 0x3FFF) != 0x0002)
            {
                error(cmdbuf, 0xD9001830);
                break;
            }
            ssize_t units = utf8_to_utf16(hbldrTarget, (const uint8_t*)cmdbuf[2], PATH_MAX);
            if (units < 0 || units > PATH_MAX)
            {
                hbldrTarget[0] = 0;
                error(cmdbuf, 0xD9001830);
                break;
            }

            hbldrTarget[units] = 0;
            cmdbuf[0] = IPC_MakeHeader(2, 1, 0);
            cmdbuf[1] = 0;
            break;
        }
        case 3: // SetArgv
        {
            if (cmdbuf[0] != IPC_MakeHeader(3, 0, 2) || (cmdbuf[1] & 0x3FFF) != (0x2 | (1<<10)))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }
            // No need to do anything - the kernel already copied the data to our buffer
            cmdbuf[0] = IPC_MakeHeader(3, 1, 0);
            cmdbuf[1] = 0;
            break;
        }
        case 4: // PatchExHeaderInfo
        {
            if (cmdbuf[0] != IPC_MakeHeader(4, 0, 2) || cmdbuf[1] != IPC_Desc_Buffer(sizeof(ExHeader_Info), IPC_BUFFER_RW))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }

            // Perform ExHeader patches
            ExHeader_Info* exhi = (ExHeader_Info*)cmdbuf[2];
            u32 stacksize = 4096; // 3dsx/libctru don't require anymore than this
            memcpy(exhi->sci.codeset_info.name, "3dsx_app", 8);
            memcpy(&exhi->sci.codeset_info.stack_size, &stacksize, 4);
            memset(&exhi->sci.dependencies, 0, sizeof(exhi->sci.dependencies));

            u32 coreVer = OS_KernelConfig->kernel_syscore_ver;
            if (coreVer == 2)
                memcpy(exhi->sci.dependencies, dependencyListNativeFirm, sizeof(dependencyListNativeFirm));
            else if (coreVer == 3)
                memcpy(exhi->sci.dependencies, dependencyListSafeFirm, sizeof(dependencyListSafeFirm));

            ExHeader_Arm11SystemLocalCapabilities* localcaps0 = &exhi->aci.local_caps;

            localcaps0->core_info.core_version = coreVer;
            localcaps0->core_info.use_cpu_clockrate_804MHz = false;
            localcaps0->core_info.enable_l2c = false;
            localcaps0->core_info.ideal_processor = 0;
            localcaps0->core_info.affinity_mask = BIT(0);
            localcaps0->core_info.priority = 0x30;

            u32 appmemtype = OS_KernelConfig->app_memtype;
            localcaps0->core_info.o3ds_system_mode = appmemtype < 6 ? (SystemMode)appmemtype : SYSMODE_O3DS_PROD;
            localcaps0->core_info.n3ds_system_mode = appmemtype >= 6 ? (SystemMode)(appmemtype - 6 + 1) : SYSMODE_N3DS_PROD;

            memset(localcaps0->reslimits, 0, sizeof(localcaps0->reslimits));

            // Set mode1 preemption mode for core1, max. 89% of CPU time (default 0, requires a APT_SetAppCpuTimeLimit call)
            // See the big comment in sysmodules/pm/source/reslimit.c for technical details.
            localcaps0->reslimits[0] = BIT(7) | 89;

            localcaps0->storage_info.fs_access_info = 0xFFFFFFFF; // Give access to everything
            localcaps0->storage_info.no_romfs = true;
            localcaps0->storage_info.use_extended_savedata_access = true; // Whatever

            // We have a patched SM, so whatever...
            memset(localcaps0->service_access, 0, sizeof(localcaps0->service_access));
            memcpy(localcaps0->service_access, serviceList, sizeof(serviceList));

            localcaps0->reslimit_category = RESLIMIT_CATEGORY_APPLICATION;

            ExHeader_Arm11KernelCapabilities* kcaps0 = &exhi->aci.kernel_caps;
            memset(kcaps0->descriptors, 0xFF, sizeof(kcaps0->descriptors));
            memcpy(kcaps0->descriptors, kernelCaps, sizeof(kernelCaps));

            // Set kernel release version to the current kernel version
            kcaps0->descriptors[0] = 0xFC000000 | (osGetKernelVersion() >> 16);

            if (GET_VERSION_MINOR(osGetKernelVersion()) >= 50 && coreVer == 2) // 9.6+ NFIRM
            {
                u64 lastdep = sizeof(dependencyListNativeFirm)/8;
                exhi->sci.dependencies[lastdep++] = 0x0004013000004002ULL; // nfc
                strncpy((char*)&localcaps0->service_access[0x20], "nfc:u", 8);
                s64 dummy = 0;
                bool isN3DS = svcGetSystemInfo(&dummy, 0x10001, 0) == 0;
                if (isN3DS)
                {
                    exhi->sci.dependencies[lastdep++] = 0x0004013020004102ULL; // mvd
                    strncpy((char*)&localcaps0->service_access[0x21], "mvd:STD", 8);
                }
            }

            cmdbuf[0] = IPC_MakeHeader(4, 1, 2);
            cmdbuf[1] = 0;
            cmdbuf[2] = IPC_Desc_Buffer(sizeof(ExHeader_Info), IPC_BUFFER_RW);
            cmdbuf[3] = (u32)exhi;
            break;
        }
        case 5: // DebugNextApplicationByForce
        {
            res = 0;
            if (gdbServer.referenceCount == 0)
                res = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_LDR, RD_NOT_INITIALIZED);
            else if (cmdbuf[1] == 0)
            {
                GDB_LockAllContexts(&gdbServer);
                if (nextApplicationGdbCtx != NULL)
                    res = MAKERESULT(RL_PERMANENT, RS_NOP, RM_LDR, RD_ALREADY_DONE);
                else
                {
                    nextApplicationGdbCtx = GDB_SelectAvailableContext(&gdbServer, GDB_PORT_BASE + 3, GDB_PORT_BASE + 4);
                    if (nextApplicationGdbCtx != NULL)
                    {
                        nextApplicationGdbCtx->debug = 0;
                        nextApplicationGdbCtx->pid = 0xFFFFFFFF;
                        res = PMDBG_DebugNextApplicationByForce(true);
                        if (R_FAILED(res))
                        {
                            nextApplicationGdbCtx->flags = 0;
                            nextApplicationGdbCtx->localPort = 0;
                            nextApplicationGdbCtx = NULL;
                        }
                    }
                    else
                        res = MAKERESULT(RL_PERMANENT, RS_OUTOFRESOURCE, RM_LDR, RD_OUT_OF_RANGE);
                }
                GDB_UnlockAllContexts(&gdbServer);
            }
            cmdbuf[1] = res;
            cmdbuf[0] = IPC_MakeHeader(5, 1, 0);
            break;
        }
        default:
        {
            error(cmdbuf, 0xD900182F);
            break;
        }
    }
}
