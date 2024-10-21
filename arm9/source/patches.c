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

/*
*   Signature patches by an unknown author
*   Signature patches for old FIRMs by SciresM
*   firmlaunches patching code originally by delebile
*   FIRM partition writes patches by delebile
*   Idea for svcBreak patches from yellows8 and others on #3dsdev
*   TWL_FIRM patches by Steveice10 and others
*/

#include "patches.h"
#include "fs.h"
#include "exceptions.h"
#include "memory.h"
#include "config.h"
#include "utils.h"
#include "arm9_exception_handlers.h"
#include "large_patches.h"

#define K11EXT_VA         0x70000000

extern u16 launchedPath[];

u8 *getProcess9Info(u8 *pos, u32 size, u32 *process9Size, u32 *process9MemAddr)
{
    u8 *temp = memsearch(pos, "NCCH", size, 4);

    if(temp == NULL) error("Failed to get Process9 data.");

    Cxi *off = (Cxi *)(temp - 0x100);

    *process9Size = (off->ncch.exeFsSize - 1) * 0x200;
    *process9MemAddr = off->exHeader.systemControlInfo.textCodeSet.address;

    return (u8 *)off + (off->ncch.exeFsOffset + 1) * 0x200;
}

u32 *getKernel11Info(u8 *pos, u32 size, u32 *baseK11VA, u8 **freeK11Space, u32 **arm11SvcHandler, u32 **arm11ExceptionsPage)
{
    static const u8 pattern[] = {0x00, 0xB0, 0x9C, 0xE5};
    *arm11ExceptionsPage = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(*arm11ExceptionsPage == NULL) error("Failed to get Kernel11 data.");

    u32 *arm11SvcTable;

    *arm11ExceptionsPage -= 0xB;
    u32 svcOffset = (-(((*arm11ExceptionsPage)[2] & 0xFFFFFF) << 2) & (0xFFFFFF << 2)) - 8; //Branch offset + 8 for prefetch
    u32 pointedInstructionVA = 0xFFFF0008 - svcOffset;
    *baseK11VA = pointedInstructionVA & 0xFFFF0000; //This assumes that the pointed instruction has an offset < 0x10000, iirc that's always the case
    arm11SvcTable = *arm11SvcHandler = (u32 *)(pos + *(u32 *)(pos + pointedInstructionVA - *baseK11VA + 8) - *baseK11VA); //SVC handler address
    while(*arm11SvcTable) arm11SvcTable++; //Look for SVC0 (NULL)

    u32 *freeSpace;
    for(freeSpace = *arm11ExceptionsPage; freeSpace < *arm11ExceptionsPage + 0x400 && *freeSpace != 0xFFFFFFFF; freeSpace++);
    *freeK11Space = (u8 *) freeSpace;

    return arm11SvcTable;
}

// For Arm prologs in the form of: push {regs} ... sub sp, #off (this obviously doesn't intend to cover all cases)
static inline u32 computeArmFrameSize(const u32 *prolog)
{
    const u32 *off;

    for(off = prolog; (*off >> 16) != 0xE92D; off++); // look for stmfd sp! = push
    u32 nbPushedRegs = 0;
    for(u32 val = *off & 0xFFFF; val != 0; val >>= 1) // 1 bit = 1 pushed register
        nbPushedRegs += val & 1;
    for(; (*off >> 8) != 0xE24DD0; off++); // look for sub sp, #offset
    u32 localVariablesSpaceSize = *off & 0xFF;

    return 4 * nbPushedRegs + localVariablesSpaceSize;
}

static inline u32 *getKernel11HandlerVAPos(u8 *pos, u32 *arm11ExceptionsPage, u32 baseK11VA, u32 id)
{
    u32 off = ((-((arm11ExceptionsPage[id] & 0xFFFFFF) << 2)) & (0xFFFFFF << 2)) - 8;
    u32 pointedInstructionVA = 0xFFFF0000 + 4 * id - off;
    return (u32 *)(pos + pointedInstructionVA - baseK11VA + 8);
}

u32 installK11Extension(u8 *pos, u32 size, bool needToInitSd, u32 baseK11VA, u32 *arm11ExceptionsPage, u8 **freeK11Space)
{
    //The parameters to be passed on to the kernel ext
    //Please keep that in sync with the definition in k11_extension/source/main.c
    struct KExtParameters
    {
        u32 basePA;
        u32 stolenSystemMemRegionSize;
        void *originalHandlers[4];
        u32 L1MMUTableAddrs[4];

        volatile bool done;

        struct CfwInfo
        {
            char magic[4];

            u8 versionMajor;
            u8 versionMinor;
            u8 versionBuild;
            u8 flags;

            u32 commitHash;

            u16 configFormatVersionMajor, configFormatVersionMinor;
            u32 config, multiConfig, bootConfig;
            u32 splashDurationMsec;
            s8 volumeSliderOverride;
            u64 hbldr3dsxTitleId;
            u32 rosalinaMenuCombo;
            u32 pluginLoaderFlags;
            s16 ntpTzOffetMinutes;

            ScreenFiltersCfgData topScreenFilter;
            ScreenFiltersCfgData bottomScreenFilter;

            u64 autobootTwlTitleId;
            u8 autobootCtrAppmemtype;

            u16 launchedPath[80+1];
        } info;
    };

    static const u8 patternHook1[] = {0x02, 0xC2, 0xA0, 0xE3, 0xFF}; //MMU setup hook
    static const u8 patternHook2[] = {0x08, 0x00, 0xA4, 0xE5, 0x02, 0x10, 0x80, 0xE0, 0x08, 0x10, 0x84, 0xE5}; //FCRAM layout setup hook
    static const u8 patternHook3_4[] = {0x00, 0x00, 0xA0, 0xE1, 0x03, 0xF0, 0x20, 0xE3, 0xFD, 0xFF, 0xFF, 0xEA}; //SGI0 setup code, etc.

    //Our kernel11 extension is initially loaded in VRAM
    u32 kextTotalSize = *(u32 *)0x18000020 - K11EXT_VA;
    u32 stolenSystemMemRegionSize = kextTotalSize; // no need to steal any more mem on N3DS. Currently, everything fits in BASE on O3DS too (?)
    u32 dstKextPA = (ISN3DS ? 0x2E000000 : 0x26C00000) - stolenSystemMemRegionSize; // start of BASE memregion (note: linear heap ---> <--- the rest)

    u32 *hookVeneers = (u32 *)*freeK11Space;
    u32 relocBase = 0xFFFF0000 + (*freeK11Space - (u8 *)arm11ExceptionsPage);

    hookVeneers[0] = 0xE51FF004; //ldr pc, [pc, #-8+4]
    hookVeneers[1] = 0x18000004;
    hookVeneers[2] = 0xE51FF004;
    hookVeneers[3] = K11EXT_VA;
    hookVeneers[4] = 0xE51FF004;
    hookVeneers[5] = K11EXT_VA + 8;
    hookVeneers[6] = 0xE51FF004;
    hookVeneers[7] = K11EXT_VA + 0xC;

    (*freeK11Space) += 32;

    //MMU setup hook
    u32 *off = (u32 *)memsearch(pos, patternHook1, size, sizeof(patternHook1));
    if(off == NULL) return 1;
    *off = MAKE_BRANCH_LINK(off, hookVeneers);

    //Most important hook: FCRAM layout setup hook
    off = (u32 *)memsearch(pos, patternHook2, size, sizeof(patternHook2));
    if(off == NULL) return 1;
    off += 2;
    *off = MAKE_BRANCH_LINK(baseK11VA + ((u8 *)off - pos), relocBase + 8);

    //Bind SGI0 hook
    //Look for cpsie i and place our hook in the nop 2 instructions before
    off = (u32 *)memsearch(pos, patternHook3_4, size, 12);
    if(off == NULL) return 1;
    for(; *off != 0xF1080080; off--);
    off -= 2;
    *off = MAKE_BRANCH_LINK(baseK11VA + ((u8 *)off - pos), relocBase + 16);

    //Config hook (after the configuration memory fields have been filled)
    for(; *off != 0xE1A00000; off++);
    off += 4;
    *off = MAKE_BRANCH_LINK(baseK11VA + ((u8 *)off - pos), relocBase + 24);

    struct KExtParameters *p = (struct KExtParameters *)(*(u32 *)0x18000024 - K11EXT_VA + 0x18000000);
    p->basePA = dstKextPA;
    p->done = false;
    p->stolenSystemMemRegionSize = stolenSystemMemRegionSize;

    for(u32 i = 0; i < 4; i++)
    {
        u32 *handlerPos = getKernel11HandlerVAPos(pos, arm11ExceptionsPage, baseK11VA, 1 + i);
        p->originalHandlers[i] = (void *)*handlerPos;
        *handlerPos = K11EXT_VA + 0x10 + 4 * i;
    }

    struct CfwInfo *info = &p->info;
    memcpy(&info->magic, "LUMA", 4);
    info->commitHash = COMMIT_HASH;
    info->configFormatVersionMajor = configData.formatVersionMajor;
    info->configFormatVersionMinor = configData.formatVersionMinor;
    info->config = configData.config;
    info->multiConfig = configData.multiConfig;
    info->bootConfig = configData.bootConfig;
    info->splashDurationMsec = configData.splashDurationMsec;
    info->volumeSliderOverride = configData.volumeSliderOverride;
    info->hbldr3dsxTitleId = configData.hbldr3dsxTitleId;
    info->rosalinaMenuCombo = configData.rosalinaMenuCombo;
    info->pluginLoaderFlags = configData.pluginLoaderFlags;
    info->ntpTzOffetMinutes = configData.ntpTzOffetMinutes;
    info->topScreenFilter = configData.topScreenFilter;
    info->bottomScreenFilter = configData.bottomScreenFilter;
    info->autobootTwlTitleId = configData.autobootTwlTitleId;
    info->autobootCtrAppmemtype = configData.autobootCtrAppmemtype;
    info->versionMajor = VERSION_MAJOR;
    info->versionMinor = VERSION_MINOR;
    info->versionBuild = VERSION_BUILD;

    if(ISRELEASE) info->flags = 1;
    if(ISN3DS) info->flags |= 1 << 4;
    if(needToInitSd) info->flags |= 1 << 5;
    if(isSdMode) info->flags |= 1 << 6;

    memcpy(info->launchedPath, launchedPath, sizeof(info->launchedPath));

    return 0;
}

u32 patchKernel11(u8 *pos, u32 size, u32 baseK11VA, u32 *arm11SvcTable, u32 *arm11ExceptionsPage)
{
    static const u8 patternKPanic[] = {0x02, 0x0B, 0x44, 0xE2};
    static const u8 patternKThreadDebugReschedule[] = {0x34, 0x20, 0xD4, 0xE5, 0x00, 0x00, 0x55, 0xE3, 0x80, 0x00, 0xA0, 0x13};

    //Assumption: ControlMemory, DebugActiveProcess and KernelSetState are in the first 0x20000 bytes
    //Patch ControlMemory
    u8 *instrPos = pos + (arm11SvcTable[1] + 20 - baseK11VA);
    s32 displ = (*(u32 *)instrPos & 0xFFFFFF) << 2;
    displ = (displ << 6) >> 6; // sign extend

    u8 *ControlMemoryPos = instrPos + 8 + displ;
    u32 *off;

    /*
        Here we replace currentProcess->processID == 1 by additionnalParameter == 1.
        This patch should be generic enough to work even on firmware version 5.0.

        It effectively changes the prototype of the ControlMemory function which
        only caller is the svc 0x01 handler on OFW.
    */
    for(off = (u32 *)ControlMemoryPos; (off[0] & 0xFFF0FFFF) != 0xE3500001 || (off[1] & 0xFFFF0FFF) != 0x13A00000; off++);
    off -= 2;
    *off = 0xE59D0000 | (*off & 0x0000F000) | (8 + computeArmFrameSize((u32 *)ControlMemoryPos)); // ldr r0, [sp, #(frameSize + 8)]

    //Patch DebugActiveProcess
    for(off = (u32 *)(pos + (arm11SvcTable[0x60] - baseK11VA)); *off != 0xE3110001; off++);
    *off = 0xE3B01001; // tst r1, #1 -> movs r1, #1

    for(off = (u32 *)(pos + (arm11SvcTable[0x7C] - baseK11VA)); off[0] != 0xE5D00001 || off[1] != 0xE3500000; off++);
    off[2] = 0xE1A00000; // in case 6: beq -> nop

    //Patch kernelpanic
    off = (u32 *)memsearch(pos, patternKPanic, size, sizeof(patternKPanic));
    if(off == NULL)
        return 1;

    off[-6] = 0xE12FFF7E;

    //Redirect enableUserExceptionHandlersForCPUExc (= true)
    for(off = arm11ExceptionsPage; *off != 0x96007F9; off++);
    off[1] = K11EXT_VA + 0x28;

    off = (u32 *)memsearch(pos, patternKThreadDebugReschedule, size, sizeof(patternKThreadDebugReschedule));
    if(off == NULL)
        return 1;

    off[-5] = 0xE51FF004;
    off[-4] = K11EXT_VA + 0x2C;

    if (ISN3DS)
    {
        // Patch SvcSetProcessIdealProcessor and SvcCreate thread to always allow
        // for core2 and core3 to be used. Normally, processes with the 0x2000 kernel flag
        // have access to core2, and BASE processes have access to both core2 and core3.
        // We're patching the if (memory region == BASE) check to be always true.
        off = (u32 *)pos;
        for (u32 i = 0; i < 2 && (u8 *)off < pos + size; i++)
        {
            // cmp r2, #0x300; beq...
            for (; (off[0] != 0xE3520C03 || off[1] != 0x0A000003) && (u8 *)off < pos + size; off++);
            if ((u8 *)off > pos + size)
                return 1;
            off[1] = 0xEA000003;
        }
    }

    return 0;
}

u32 patchSignatureChecks(u8 *pos, u32 size)
{
    //Look for signature checks
    static const u8 pattern[] = {0xC0, 0x1C, 0x76, 0xE7},
                    pattern2[] = {0xB5, 0x22, 0x4D, 0x0C};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));
    u8 *temp = memsearch(pos, pattern2, size, sizeof(pattern2));

    if(off == NULL || temp == NULL) return 1;

    u16 *off2 = (u16 *)(temp - 1);
    *off = off2[0] = 0x2000;
    off2[1] = 0x4770;

    return 0;
}

u32 patchOldSignatureChecks(u8 *pos, u32 size)
{
    // Look for signature checks
    static const u8 pattern[] = {0xC0, 0x1C, 0xBD, 0xE7},
                    pattern2[] = {0xB5, 0x23, 0x4E, 0x0C};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));
    u8 *temp = memsearch(pos, pattern2, size, sizeof(pattern2));

    if(off == NULL || temp == NULL) return 1;

    u16 *off2 = (u16 *)(temp - 1);
    *off = off2[0] = 0x2000;
    off2[1] = 0x4770;

    return 0;
}

u32 patchFirmlaunches(u8 *pos, u32 size, u32 process9MemAddr)
{
    //Look for firmlaunch code
    static const u8 pattern[] = {0xE2, 0x20, 0x20, 0x90};

    u32 pathLen;
    for(pathLen = 0; pathLen < sizeof(launchedPath)/2 && launchedPath[pathLen] != 0; pathLen++);

    if(launchedPath[pathLen] != 0) return 1;

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    off -= 0x13;

    //Firmlaunch function offset - offset in BLX opcode (A4-16 - Arm DDI 0100E) + 1
    u32 fOpenOffset = (u32)(off + 9 - (-((*(u32 *)off & 0x00FFFFFF) << 2) & (0xFFFFFF << 2)) - pos + process9MemAddr);

    //Put the fOpen offset in the right location
    rebootPatchFopenPtr = fOpenOffset;

    //Copy the launched path
    memcpy(rebootPatchFileName, launchedPath, 2 * (1 + pathLen));

    //Copy firmlaunch code
    memcpy(off, rebootPatch, rebootPatchSize);

    return 0;
}

u32 patchFirmWrites(u8 *pos, u32 size)
{
    //Look for FIRM writing code
    u8 *off = memsearch(pos, "exe:", size, 4);

    if(off == NULL) return 1;

    static const u8 pattern[] = {0x00, 0x28, 0x01, 0xDA};

    u16 *off2 = (u16 *)memsearch(off - 0x100, pattern, 0x100, sizeof(pattern));

    if(off2 == NULL) return 1;

    off2[0] = 0x2000;
    off2[1] = 0x46C0;

    return 0;
}

u32 patchOldFirmWrites(u8 *pos, u32 size)
{
    //Look for FIRM writing code
    static const u8 pattern[] = {0x04, 0x1E, 0x1D, 0xDB};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    off[0] = 0x2400;
    off[1] = 0xE01D;

    return 0;
}

u32 patchTitleInstallMinVersionChecks(u8 *pos, u32 size, u32 firmVersion)
{
    static const u8 pattern[] = {0xFF, 0x00, 0x00, 0x02};

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return firmVersion == 0xFFFFFFFF ? 0 : 1;

    off++;

    //Zero out the first TitleID in the list
    memset(off, 0, 8);

    return 0;
}

u32 patchZeroKeyNcchEncryptionCheck(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x28, 0x2A, 0xD0, 0x08};

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) return 1;

    u16 *off = (u16 *)(temp - 1);
    *off = 0x2001; //mov r0, #1

    return 0;
}

u32 patchNandNcchEncryptionCheck(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x07, 0xD1, 0x28, 0x7A};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    off--;
    *off = 0x2001; //mov r0, #1

    return 0;
}

u32 patchCheckForDevCommonKey(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x03, 0x7C, 0x28, 0x00};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    *off = 0x2301; //mov r3, #1

    return 0;
}

u32 patchK11ModuleLoading(u32 oldKipSectionSize, u32 newKipSectionSize, u32 numKips, u8 *pos, u32 size)
{
    static const u8 moduleLoadingPattern[]  = {0xE2, 0x05, 0x00, 0x57},
                    modulePidPattern[] = {0x06, 0xA0, 0xE1, 0xF2}; //GetSystemInfo

    u8 *off = memsearch(pos, moduleLoadingPattern, size, 4);

    if(off == NULL) return 1;

    off[1] = (u8)numKips;

    u32 *off32;
    for(off32 = (u32 *)(off - 3); *off32 != 0xE59F0000; off32++);
    off32 += 2;
    off32[1] = off32[0] + newKipSectionSize;
    for(; *off32 != oldKipSectionSize; off32++);
    *off32 = ((newKipSectionSize + 0x1FF) >> 9) << 9;

    off = memsearch(pos, modulePidPattern, size, 4);

    if(off == NULL) return 1;

    off[0xB] = (u8)numKips;

    return 0;
}

u32 patchK11ModuleLoadingLgy(u32 newKipSectionSize, u8 *pos, u32 size)
{
    // Patch the function where TwlBg/AgbBg is copied from 18000000 (VRAM) to 21000000 (FCRAM).
    // This is where we can also automatically obtain the section size

    u16 *off = (u16 *)pos;
    for (; (u8 *)off < pos + size && (off[0] != 0x06C9 || off[1] != 0x0600); off++);
    if ((u8 *)off >= pos + size)
        return 3;

    off += 7;
    u32 oldKipSectionSize = *(u32 *)off;
    *(u32 *)off = newKipSectionSize;
    off += 2;

    u32 *off2 = (u32 *)off;
    for (; (u8 *)off2 < pos + size && *off2 != oldKipSectionSize; off2++);
    if ((u8 *)off2 >= pos + size)
        return 4;
    *off2 = newKipSectionSize;

    return 0;
}

u32 patchArm9ExceptionHandlersInstall(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x80, 0xE5, 0x40, 0x1C};

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) return 1;

    u32 *off;

    for(off = (u32 *)(temp - 2); *off != 0xE5801000; off--); //Until str r1, [r0]

    for(u32 r0 = 0x08000000; *off != 0xE3A01040; off++) //Until mov r1, #0x40
    {
        //Discard everything that's not str rX, [r0, #imm](!)
        if((*off & 0xFE5F0000) != 0xE4000000) continue;

        u32 rD = (*off >> 12) & 0xF,
            offset = (*off & 0xFFF) * ((((*off >> 23) & 1) == 0) ? -1 : 1);
        bool writeback = ((*off >> 21) & 1) != 0,
             pre = ((*off >> 24) & 1) != 0;

        u32 addr = r0 + ((pre || !writeback) ? offset : 0);
        if((addr & 7) != 0 && addr != 0x08000014 && addr != 0x08000004) *off = 0xE1A00000; //nop
        else *off = 0xE5800000 | (rD << 12) | (addr & 0xFFF); //Preserve IRQ and SVC handlers

        if(!pre) addr += offset;
        if(writeback) r0 = addr;
    }

    return 0;
}

u32 patchSvcBreak9(u8 *pos, u32 size, u32 kernel9Address)
{
    //Stub svcBreak with "bkpt 65535" so we can debug the panic

    //Look for the svc handler
    static const u8 pattern[] = {0x00, 0xE0, 0x4F, 0xE1}; //mrs lr, spsr

    u32 *arm9SvcTable = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(arm9SvcTable == NULL) return 1;

    while(*arm9SvcTable != 0) arm9SvcTable++; //Look for SVC0 (NULL)

    u32 *addr = (u32 *)(pos + arm9SvcTable[0x3C] - kernel9Address);

    /*
        mov r8, sp
        bkpt 0xffff
    */
    addr[0] = 0xE1A0800D;
    addr[1] = 0xE12FFF7F;

    arm9ExceptionHandlerSvcBreakAddress = arm9SvcTable[0x3C]; //BreakPtr

    return 0;
}

u32 patchKernel9Panic(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x00, 0x20, 0x92, 0x15};

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) return 1;

    u32 *off = (u32 *)(temp - 0x34);
    *off = 0xE12FFF7E;

    return 0;
}

u32 patchP9AccessChecks(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x00, 0x08, 0x49, 0x68};

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) return 1;

    u16 *off = (u16 *)(temp - 3);
    off[0] = 0x2001; //mov r0, #1
    off[1] = 0x4770; //bx lr

    return 0;
}

u32 patchUnitInfoValueSet(u8 *pos, u32 size)
{
    //Look for UNITINFO value being set during kernel sync
    static const u8 pattern[] = {0x01, 0x10, 0xA0, 0x13};

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    off[0] = ISDEVUNIT ? 0 : 1;
    off[3] = 0xE3;

    return 0;
}

u32 patchP9AMTicketWrapperZeroKeyIV(u8 *pos, u32 size, u32 firmVersion)
{
    static const u8 __rt_memclr_pattern[] = {0x00, 0x20, 0xA0, 0xE3, 0x04, 0x00, 0x51, 0xE3, 0x07, 0x00, 0x00, 0x3A};
    static const u8 pattern[] = {0x20, 0x21, 0xA6, 0xA8};

    u32 function = (u32)memsearch(pos, __rt_memclr_pattern, size, sizeof(__rt_memclr_pattern));
    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(function == 0 || off == NULL) return firmVersion == 0xFFFFFFFF ? 0 : 1;

    //After the found code it's a BL call (at &off[2]), that will be replaced
    //From Thumb, op distance for setting in BLX can be got with,
    //(Destination_offset - blx_op_offset+2) / 2
    s32 opjumpdistance = (s32)(function - ((u32)&off[3])) / 2;

    //Beyond limit
    if(opjumpdistance < -0x1fffff || opjumpdistance > 0x1fffff) return 1;

    //r0 and r1 for old call are already correct for this one
    //BLX __rt_memclr
    u32 op = (0xE800F000U | (((u32)opjumpdistance & 0x7FF) << 16) | (((u32)opjumpdistance >> 11) & 0x3FF) | (((u32)opjumpdistance >> 21) & 0x400)) & ~(1<<16);

    off[2] = op;
    off[3] = op >> 16;

    return 0;
}

u32 patchLgySignatureChecks(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x47, 0xC1, 0x17, 0x49};

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) return 1;

    u16 *off = (u16 *)(temp + 1);
    off[0] = 0x2000;
    off[1] = 0xB04E;
    off[2] = 0xBD70;

    return 0;
}

u32 patchTwlInvalidSignatureChecks(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x20, 0xF6, 0xE7, 0x7F};

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) return 1;

    u16 *off = (u16 *)(temp - 1);
    *off = 0x2001; //mov r0, #1

    return 0;
}

u32 patchTwlNintendoLogoChecks(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0xC0, 0x30, 0x06, 0xF0};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    off[1] = 0x2000;
    off[2] = 0;

    return 0;
}

u32 patchTwlWhitelistChecks(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x22, 0x00, 0x20, 0x30};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    off[2] = 0x2000;
    off[3] = 0;

    return 0;
}

u32 patchTwlFlashcartChecks(u8 *pos, u32 size, u32 firmVersion)
{
    static const u8 pattern[] = {0x25, 0x20, 0x00, 0x0E};

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL)
    {
        if(firmVersion == 0xFFFFFFFF) return patchOldTwlFlashcartChecks(pos, size);

        return 1;
    }

    u16 *off = (u16 *)(temp + 3);
    off[0] = off[6] = off[0xC] = 0x2001; //mov r0, #1
    off[1] = off[7] = off[0xD] = 0; //nop

    return 0;
}

u32 patchOldTwlFlashcartChecks(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x06, 0xF0, 0xA0, 0xFD};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    off[0] = off[6] = 0x2001; //mov r0, #1
    off[1] = off[7] = 0; //nop

    return 0;
}

u32 patchTwlShaHashChecks(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x10, 0xB5, 0x14, 0x22};

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    off[0] = 0x2001; //mov r0, #1
    off[1] = 0x4770;

    return 0;
}

u32 patchAgbBootSplash(u8 *pos, u32 size)
{
    static const u8 pattern[] = {0x00, 0x00, 0x01, 0xEF};

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) return 1;

    off[2] = 0x26;

    return 0;
}

void patchTwlBg(u8 *pos, u32 size)
{
    // You can use the following Python code to convert something like below
    // into twl_upscaling_filter.bin:
    // import struct; open("twl_upscaling_filter.bin", "wb+").write(struct.pack("<30H", [array contents]))
    static const u16 nintendoFilterTwl[] = {
        0x0000, 0x004E, 0x011D, 0x01E3, 0x01C1,
        0x0000, 0xFCA5, 0xF8D0, 0xF69D, 0xF873,
        0x0000, 0x0D47, 0x1E35, 0x2F08, 0x3B6F,
        0x4000, 0x3B6F, 0x2F08, 0x1E35, 0x0D47,
        0x0000, 0xF873, 0xF69D, 0xF8D0, 0xFCA5,
        0x0000, 0x01C1, 0x01E3, 0x011D, 0x004E,
    };

    // "error" func doesn't seem to work here
    if (CONFIG(ENABLEDSIEXTFILTER))
    {
        u16 filter[5*6] = { 0 };
        u32 rd = fileRead(filter, "twl_upscaling_filter.bin", sizeof(filter));
        if (rd == sizeof(filter))
        {
            // else error("Failed to apply enable_dsi_external_filter:\n\ntwl_upscaling_filter.bin is missing or invalid.");
            u8 *off = memsearch(pos, nintendoFilterTwl, size, sizeof(nintendoFilterTwl));
            if (off != NULL)
                memcpy(off, filter, sizeof(filter));
            // else error("Failed to apply enable_dsi_external_filter.");
        }
    }
}

u32 patchLgyK11(u8 *section1, u32 section1Size, u8 *section2, u32 section2Size)
{
    u32 *off;

    // Fix a bug where Legacy K11 maps user TLS with "user no access" permissions
    // Map it as RWX (just like the rest of other user-accessible pages) instead
    for (off = (u32 *)section1; (u8 *)off <= section1 + section1Size && *off != 0xE0100000; off++);

    if ((u8 *)off >= section1 + section1Size)
        return 1;

    ++off;

    *off &= ~0x231; // clear APX mask and XN
    *off |= 0x030; // re-set APX (to user/kernel RW)

    // Patch two pointer-to-bool to point to a non-zero byte, enabling user exception handling.
    // It is impossible to enable it by normal means, otherwise
    for (off = (u32 *)section2; (u8 *)off <= section2 + section2Size && *off != 0x100021F; off++);
    if ((u8 *)off >= section2 + section2Size)
        return 1;
    off[1] = 0xFFFF0F00;
    off[2] = 0xFFFF0F04;

    // Dispatch-to-user code checks for memory block type and permissions (etc.), but
    // LGY K11 doesn't do any memory management, so these checks will always fail.
    // Patch with b +0x38 to skip all those checks
    u16 *off2;
    for (off2 = (u16 *)section2; (u8 *)off2 <= section2 + section2Size && (off2[0] != 0xDB1F || off2[1] != 0x4915); off2++);
    if ((u8 *)off2 >= section2 + section2Size)
        return 1;
    *off2 = 0xE01A;

    return 0;
}

static bool getVtableAddress(u8 *pos, u32 size, const u8 *pattern, u32 patternSize, s32 patternOff, u32 memAddr, u32 *vtableAddr)
{
    u8 *tmp = memsearch(pos, pattern, size, patternSize);

    if(tmp == NULL) return false;

    u16 *instrPtr = (u16 *)(tmp + patternOff); //ldr rX, [PTR_TO_VTABLE]
    if((*instrPtr & 0xf800) != 0x4800) return false; //Check the instruction opcode

    *vtableAddr = *(u32 *)(((u32)instrPtr + 4 + 4*(*instrPtr & 0xff)) & ~0x3);

    if(*vtableAddr < memAddr || *vtableAddr >= memAddr + size) return false;

    return true;
}

u32 patchReadFileSHA256Vtab11(u8 *pos, u32 size, u32 process9MemAddr)
{
    static const u8 ncchVtableRefPattern[] = {0x77, 0x46, 0x06, 0x74, 0x47, 0x74};
    static const u8 shaVtable1RefPattern[] = {0x00, 0x1f, 0x01, 0x60, 0x20, 0x30};
    static const u8 shaVtable2RefPattern[] = {0x00, 0x1f, 0x01, 0x60, 0x01, 0x00, 0x20, 0x31};

    u32 ncchVtableAddr;
    if(!getVtableAddress(pos, size, ncchVtableRefPattern, sizeof(ncchVtableRefPattern), sizeof(ncchVtableRefPattern), process9MemAddr, &ncchVtableAddr)) return 1;

    u32 shaVtable1Addr;
    if(!getVtableAddress(pos, size, shaVtable1RefPattern, sizeof(shaVtable1RefPattern), -2, process9MemAddr, &shaVtable1Addr)) return 1;

    u32 shaVtable2Addr;
    if(!getVtableAddress(pos, size, shaVtable2RefPattern, sizeof(shaVtable2RefPattern), -2, process9MemAddr, &shaVtable2Addr)) return 1;

    u32 *ncchVtable11Ptr = (u32 *)(pos + (ncchVtableAddr - process9MemAddr)) + 11,
        *shaVtable1_11Ptr = (u32 *)(pos + (shaVtable1Addr - process9MemAddr)) + 11,
        *shaVtable2_11Ptr = (u32 *)(pos + (shaVtable2Addr - process9MemAddr)) + 11;

    if((*ncchVtable11Ptr & 0x1) == 0 || (*shaVtable1_11Ptr & 0x1) == 0 || (*shaVtable2_11Ptr & 0x1) == 0) return 1; //Must be Thumb

    //Find needed function addresses by inspecting all bl branch targets
    u16 *ncchWriteFnc = (u16 *)(pos + ((*ncchVtable11Ptr & ~0x1) - process9MemAddr));
    if(*(u32 *)ncchWriteFnc != 0x0005b5f0) return 1; //Check if we got the right function

    readFileSHA256Vtab11PatchCtorPtr = readFileSHA256Vtab11PatchInitPtr = readFileSHA256Vtab11PatchProcessPtr = 0;

    for(; ((*ncchWriteFnc) & 0xff00) != 0xbd00; ncchWriteFnc++) { //Stop whe encountering a pop {..., pc}
        if((ncchWriteFnc[0] & 0xf800) != 0xf000 || (ncchWriteFnc[1] & 0xf800) != 0xf800) continue; //Check the instruction opcode

        s32 callOff = ((ncchWriteFnc[0] & 0x07ff) << 11) | (ncchWriteFnc[1] & 0x07ff);
        callOff = (callOff & 0x1fffff) - (callOff & 0x200000);

        u32 callTargetAddr = process9MemAddr + ((u8 *)ncchWriteFnc - pos) + 4 + 2*callOff;

        if(callTargetAddr < process9MemAddr || callTargetAddr >= process9MemAddr + size) return false;

        switch(*(u32 *)(pos + (callTargetAddr - process9MemAddr))) {
            case 0x60422201: //DataChainProcessor::DataChainProcessor
                readFileSHA256Vtab11PatchCtorPtr = callTargetAddr | 0x1;
                break;
            case 0x000db538: //DataChainProcessor::Init
                readFileSHA256Vtab11PatchInitPtr = callTargetAddr | 0x1;
                break;
            case 0x0006b5f0: //DataChainProcessor::ProcessBytes
                readFileSHA256Vtab11PatchProcessPtr = callTargetAddr | 0x1;
                break;
        }
    }

    if(readFileSHA256Vtab11PatchCtorPtr == 0 || readFileSHA256Vtab11PatchInitPtr == 0 || readFileSHA256Vtab11PatchProcessPtr == 0) return 1;

    *shaVtable1_11Ptr = *shaVtable2_11Ptr = (u32) &readFileSHA256Vtab11Patch; //The patched vtable11 function is in ITCM, so we don't have to copy it somewhere else

    return 0;
}