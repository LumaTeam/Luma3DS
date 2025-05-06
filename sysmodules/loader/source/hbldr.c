/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020, 2022 Aurora Wright, TuxSH, fincs
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
#include <string.h>
#include <assert.h>
#include "paslr.h"
#include "util.h"
#include "hbldr.h"
#include "3dsx.h"

extern bool isN3DS;

// Note: just switch to [[attribute]] once we use clangd and cmake
// vscode-cpptools has (or had?) some issues with C23 support
#if __GNUC__ >= 15
// Required by GCC 15+ but ignored (with warning) before
__attribute__((nonstring))
#endif
static const char serviceList[34][8] =
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

    // Discarded below 9.6:
    "nfc:u",
    "mvd:STD",
};

// N3DS-only titles are denoted as -x
// PM (official and reimpl) doesn't actually support recursive dependency
// loading, and some sysmodules don't even list all their dependencies, so
// we're flattening the dependency map here and listing all the sysmodules in
// an optimal boot order.
static const s16 dependencyListNativeFirmUids[] =
{
    0x1B,   // gpio
    0x1E,   // i2c
    0x21,   // pdn
    0x23,   // spi

    0x1F,   // mcu
    0x31,   // ps
    0x17,   // cfg

    0x22,   // ptm
    0x80,   // ns

    // The modules above were all previously launched
    // when launching NS.

    0x18,   // codec (implicit dep for hid)
    0x1D,   // hid
    0x27,   // csnd (for camera)
    0x16,   // camera
    -0x42,  // qtm
    0x1C,   // gsp

    0x1A,   // dsp
    0x20,   // mic
    0x33,   // ir

    0x2D,   // nwm
    0x2E,   // socket
    0x2F,   // ssl
    0x29,   // http
    0x24,   // ac

    0x32,   // friends

    0x15,   // am

    0x26,   // cecd
    0x34,   // boss
    0x2C,   // nim
    0x2B,   // ndm
    0x35,   // news

    -0x41,  // mvd
    -0x40,  // nfc

    0x38,   // act
    0x37,   // ro
    0x28,   // dlp
    0x2A,   // mp
};

static const s16 dependencyListSafeFirmUids[] =
{
    0x1B,   // gpio
    0x1E,   // i2c
    0x21,   // pdn
    0x23,   // spi

    0x1F,   // mcu
    0x31,   // ps
    0x17,   // cfg

    0x22,   // ptm
    0x80,   // ns

    // The modules above were all previously launched
    // when launching NS.

    0x18,   // codec (implicit dep for hid)
    0x1D,   // hid
    0x27,   // csnd
    0x1C,   // gsp

    0x1A,   // dsp
    0x33,   // ir

    0x2D,   // nwm
    0x2E,   // socket
    0x2F,   // ssl
    0x29,   // http
    0x24,   // ac

    0x32,   // friends

    0x15,   // am
};

static const u32 kernelCaps[] =
{
    0xFC00022C, // Kernel release version 8.0 is necessary for using the new linear mapping. Modified below.

    // Normal applications only have access to 0x1FF50000-0x1FF58000, 0x1FF70000-0x1FF78000 (both as IO),
    // however we can load unsigned DSP firmware binaries.
    0xFF81FF00, // RW static range mapping: 0x1FF00000 (DSP RAM, start)
    0xFF81FF80, // RW static range mapping: 0x1FF80000 (DSP RAM, end)

    0xFF91F000, // RO static range mapping: 0x1F000000 (VRAM, start)
    0xFF91F600, // RO static range mapping: 0x1F600000 (VRAM, end)

    // Give access to all Arm11-accessible IO
    0xFF81EC00, // RW IO range mapping: 0x1EC00000 (PA 0x10100000, start)
    0xFF81F000, // RW IO range mapping: 0x10500000 (PA 0x10500000, end)

    0xFF002109, // Exflags: APPLICATION memtype + "Shared page writing" + "Allow debug" + "Access core2"
    0xFE000200, // Handle table size: 0x200

    // In case kernel ext isn't loaded:
    0xFFE1EC40, // RW I/O page mapping: 0x1EC40000 (CONFIG11, for kernel takeover)
    0xF0FFFFFF, // SVC ACL 0 to 0x7F (even if some SVC numbers don't exist)...
    0xF1FFFFFF,
    0xF2FFFFFF,
    0xF3FFFFFF,
    0xF4FFFFFF,
    0xF5FFFFFF,
    0xF6FFFFFF,
    0xF7FFFFFF,
};

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

void hbldrPatchExHeaderInfo(ExHeader_Info *exhi)
{
    u32 stacksize = 4096; // 3dsx/libctru don't require anymore than this
    memcpy(exhi->sci.codeset_info.name, "3dsx_app", 8);
    memcpy(&exhi->sci.codeset_info.stack_size, &stacksize, 4);
    memset(&exhi->sci.dependencies, 0, sizeof(exhi->sci.dependencies));

    u32 coreVer = OS_KernelConfig->kernel_syscore_ver;
    u32 deplistSize;
    const s16 *deplist;
    switch (coreVer) {
        case 2:
            deplist = dependencyListNativeFirmUids;
            deplistSize = sizeof(dependencyListNativeFirmUids)/sizeof(dependencyListNativeFirmUids[0]);
            break;
        case 3:
            deplist = dependencyListSafeFirmUids;
            deplistSize = sizeof(dependencyListSafeFirmUids)/sizeof(dependencyListSafeFirmUids[0]);
            break;
        default:
            panic(0);
            break;
    }

    for (u32 i = 0; i < deplistSize; i++)
    {
        static const u64 mask = 0x0004013000000000ull;
        bool n3dsOnly = deplist[i] < 0;
        u32 n3dsMask = n3dsOnly ? 0x20000000 : 0;
        u32 uid = n3dsOnly ? -deplist[i] : deplist[i];

        // Enable O3DS NFC on 9.3 and above
        if (uid == 0x40 && GET_VERSION_MINOR(osGetKernelVersion()) >= 48)
            n3dsMask = 0;

        exhi->sci.dependencies[i] = mask | n3dsMask | (uid << 8) | coreVer;
    }

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
    // Only 32 services if not on 9.6+ NFIRM
    u32 numServices = GET_VERSION_MINOR(osGetKernelVersion()) < 50 || coreVer != 2 ? 32 : 34;
    memset(localcaps0->service_access, 0, sizeof(localcaps0->service_access));
    memcpy(localcaps0->service_access, serviceList, 8 * numServices);

    localcaps0->reslimit_category = RESLIMIT_CATEGORY_APPLICATION;

    ExHeader_Arm11KernelCapabilities* kcaps0 = &exhi->aci.kernel_caps;
    memset(kcaps0->descriptors, 0xFF, sizeof(kcaps0->descriptors));
    memcpy(kcaps0->descriptors, kernelCaps, sizeof(kernelCaps));

    // Set kernel release version to the current kernel version
    kcaps0->descriptors[0] = 0xFC000000 | (osGetKernelVersion() >> 16);
}

Result hbldrLoadProcess(Handle *outProcessHandle, const ExHeader_Info *exhi)
{
    IFile file;
    Result res;

    const ExHeader_CodeSetInfo *csi = &exhi->sci.codeset_info;

    u32 region = 0;
    u32 count;
    for (count = 0; count < 28; count++)
    {
        u32 desc = exhi->aci.kernel_caps.descriptors[count];
        if (0x1FE == desc >> 23)
            region = desc & 0xF00;
    }
    if (region == 0)
            return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, 1, 2);


    if (hbldrTarget[0] == 0)
    {
        u16_strncpy(hbldrTarget, u"/boot.3dsx", PATH_MAX);
        ldrArgvBuf[0] = 1;
        strncpy((char*)&ldrArgvBuf[1], "sdmc:/boot.3dsx", sizeof(ldrArgvBuf)-4);
    }

    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_UTF16, hbldrTarget), FS_OPEN_READ);
    hbldrTarget[0] = 0;
    if (R_FAILED(res))
        return res;

    u32 totalSize = 0;
    if (!Ldr_Get3dsxSize(&totalSize, &file))
    {
        IFile_Close(&file);
        return (Result)-1;
    }

    u32 tmp = 0;
    u32 addr = 0x10000000;
    res = allocateProgramMemory(exhi, addr, totalSize);
    if (R_FAILED(res))
    {
        IFile_Close(&file);
        return res;
    }

    Handle hCodeset = Ldr_CodesetFrom3dsx(csi->name, (u32 *)addr, csi->text.address, &file, exhi->aci.local_caps.title_id);
    IFile_Close(&file);

    if (hCodeset != 0)
    {
        // There are always 28 descriptors
        res = svcCreateProcess(outProcessHandle, hCodeset, exhi->aci.kernel_caps.descriptors, count);
        svcCloseHandle(hCodeset);
    }
    else
        res = MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_LDR, RD_NOT_FOUND);

    svcControlMemory(&tmp, addr, 0, totalSize, MEMOP_FREE, 0);
    return res;
}

void hbldrHandleCommands(void *ctx)
{
    (void)ctx;
    u32 *cmdbuf = getThreadCommandBuffer();
    switch (cmdbuf[0] >> 16)
    {
        case 2: // SetTarget
        {
            if (cmdbuf[0] != IPC_MakeHeader(2, 0, 2) || (cmdbuf[1] & 0x3FFF) != 0x0002)
            {
                error(cmdbuf, 0xD9001830);
                break;
            }
            size_t inSize = cmdbuf[1] >> 14;
            inSize = inSize > PATH_MAX ? inSize : PATH_MAX;
            ssize_t units = utf8_to_utf16(hbldrTarget, (const uint8_t*)cmdbuf[2], inSize);
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
            size_t inSize = cmdbuf[1] >> 14;
            inSize = inSize > ARGVBUF_SIZE ? inSize : ARGVBUF_SIZE;
            memcpy(ldrArgvBuf, (const u8 *)cmdbuf[2], inSize);
            cmdbuf[0] = IPC_MakeHeader(3, 1, 0);
            cmdbuf[1] = 0;
            break;
        }
        case 1: // LoadProcess (removed)
        case 4: // PatchExHeaderInfo (removed)
        case 5: // DebugNextApplicationByForce (removed)
        default:
        {
            error(cmdbuf, 0xD900182F);
            break;
        }
    }
}
