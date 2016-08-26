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

#include "patches.h"
#include "memory.h"
#include "config.h"
#include "../build/rebootpatch.h"
#include "../build/svcGetCFWInfopatch.h"
#include "../build/twl_k11modulespatch.h"

static u32 *arm11ExceptionsPage = NULL;
static u32 *arm11SvcTable = NULL;
static u32 *arm11SvcHandler = NULL;

static u8 *freeK11Space = NULL; //other than the one used for svcBackdoor

static void findArm11ExceptionsPageAndSvcHandlerAndTable(u8 *pos, u32 size)
{
    const u8 arm11ExceptionsPagePattern[] = {0x00, 0xB0, 0x9C, 0xE5};
    
    if(arm11ExceptionsPage == NULL) arm11ExceptionsPage = (u32 *)memsearch(pos, arm11ExceptionsPagePattern, size, 4) - 0xB;
    if((arm11SvcTable == NULL || arm11SvcHandler == NULL) && arm11ExceptionsPage != NULL)
    {
        u32 svcOffset = (-((arm11ExceptionsPage[2] & 0xFFFFFF) << 2) & (0xFFFFFF << 2)) - 8; //Branch offset + 8 for prefetch
        arm11SvcHandler = arm11SvcTable = (u32 *)(pos + *(u32 *)(pos + 0xFFFF0008 - svcOffset - 0xFFF00000 + 8) - 0xFFF00000); //SVC handler address
        while(*arm11SvcTable) arm11SvcTable++; //Look for SVC0 (NULL)
    }
}

static void findFreeK11Space(u8 *pos, u32 size)
{
    if(freeK11Space == NULL)
    {
        const u8 bogus_pattern[] = { 0x1E, 0xFF, 0x2F, 0xE1, 0x1E, 0xFF, 0x2F, 0xE1, 0x1E, 0xFF, 
                                     0x2F, 0xE1, 0x00, 0x10, 0xA0, 0xE3, 0x00, 0x10, 0xC0, 0xE5, 
                                     0x1E, 0xFF, 0x2F, 0xE1 };
        
        u32 *someSpace = (u32 *)memsearch(pos, bogus_pattern, size, 24);

        // We couldn't find the place where to begin our search of an empty block
        if (someSpace == NULL)
            return;

        // Advance until we reach the padding area (filled with 0xFF)
        u32 *freeSpace;
        for(freeSpace = someSpace; *freeSpace != 0xFFFFFFFF; freeSpace++);

        freeK11Space = (u8 *)freeSpace;
    }
}

u8 *getProcess9(u8 *pos, u32 size, u32 *process9Size, u32 *process9MemAddr)
{
    u8 *off = memsearch(pos, "ess9", size, 4);

    *process9Size = *(u32 *)(off - 0x60) * 0x200;
    *process9MemAddr = *(u32 *)(off + 0xC);

    //Process9 code offset (start of NCCH + ExeFS offset + ExeFS header size)
    return off - 0x204 + (*(u32 *)(off - 0x64) * 0x200) + 0x200;
}

void patchSignatureChecks(u8 *pos, u32 size)
{
    const u16 sigPatch[2] = {0x2000, 0x4770};

    //Look for signature checks
    const u8 pattern[] = {0xC0, 0x1C, 0x76, 0xE7},
             pattern2[] = {0xB5, 0x22, 0x4D, 0x0C};

    u16 *off = (u16 *)memsearch(pos, pattern, size, 4),
        *off2 = (u16 *)(memsearch(pos, pattern2, size, 4) - 1);

    *off = sigPatch[0];
    off2[0] = sigPatch[0];
    off2[1] = sigPatch[1];
}

void patchFirmlaunches(u8 *pos, u32 size, u32 process9MemAddr)
{
    //Look for firmlaunch code
    const u8 pattern[] = {0xDE, 0x1F, 0x8D, 0xE2};

    u8 *off = memsearch(pos, pattern, size, 4) - 0x10;

    //Firmlaunch function offset - offset in BLX opcode (A4-16 - ARM DDI 0100E) + 1
    u32 fOpenOffset = (u32)(off + 9 - (-((*(u32 *)off & 0x00FFFFFF) << 2) & (0xFFFFFF << 2)) - pos + process9MemAddr);

    //Copy firmlaunch code
    memcpy(off, reboot, reboot_size);

    //Put the fOpen offset in the right location
    u32 *pos_fopen = (u32 *)memsearch(off, "OPEN", reboot_size, 4);
    *pos_fopen = fOpenOffset;
}

void patchFirmWrites(u8 *pos, u32 size)
{
    const u16 writeBlock[2] = {0x2000, 0x46C0};

    //Look for FIRM writing code
    u8 *const off1 = memsearch(pos, "exe:", size, 4);
    const u8 pattern[] = {0x00, 0x28, 0x01, 0xDA};

    u16 *off2 = (u16 *)memsearch(off1 - 0x100, pattern, 0x100, 4);

    off2[0] = writeBlock[0];
    off2[1] = writeBlock[1];
}

void patchFirmWriteSafe(u8 *pos, u32 size)
{
    const u16 writeBlockSafe[2] = {0x2400, 0xE01D};

    //Look for FIRM writing code
    const u8 pattern[] = {0x04, 0x1E, 0x1D, 0xDB};

    u16 *off = (u16 *)memsearch(pos, pattern, size, 4);

    off[0] = writeBlockSafe[0];
    off[1] = writeBlockSafe[1];
}

void reimplementSvcBackdoor(u8 *pos, u32 size)
{
    //Official implementation of svcBackdoor
    const u8  svcBackdoor[40] = {0xFF, 0x10, 0xCD, 0xE3,  //bic   r1, sp, #0xff
                                 0x0F, 0x1C, 0x81, 0xE3,  //orr   r1, r1, #0xf00
                                 0x28, 0x10, 0x81, 0xE2,  //add   r1, r1, #0x28
                                 0x00, 0x20, 0x91, 0xE5,  //ldr   r2, [r1]
                                 0x00, 0x60, 0x22, 0xE9,  //stmdb r2!, {sp, lr}
                                 0x02, 0xD0, 0xA0, 0xE1,  //mov   sp, r2
                                 0x30, 0xFF, 0x2F, 0xE1,  //blx   r0
                                 0x03, 0x00, 0xBD, 0xE8,  //pop   {r0, r1}
                                 0x00, 0xD0, 0xA0, 0xE1,  //mov   sp, r0
                                 0x11, 0xFF, 0x2F, 0xE1}; //bx    r1

    findArm11ExceptionsPageAndSvcHandlerAndTable(pos, size);

    if(!arm11SvcTable[0x7B])
    {
        u32 *freeSpace;
        for(freeSpace = arm11ExceptionsPage; *freeSpace != 0xFFFFFFFF; freeSpace++);

        memcpy(freeSpace, svcBackdoor, 40);

        arm11SvcTable[0x7B] = 0xFFFF0000 + ((u8 *)freeSpace - (u8 *)arm11ExceptionsPage);
    }
}

void implementSvcGetCFWInfo(u8 *pos, u32 size)
{
    const char *rev = REVISION;
    bool isRelease;

    findArm11ExceptionsPageAndSvcHandlerAndTable(pos, size);
    findFreeK11Space(pos, size);
    
    memcpy(freeK11Space, svcGetCFWInfo, svcGetCFWInfo_size);

    CFWInfo *info = (CFWInfo *)memsearch(freeK11Space, "LUMA", svcGetCFWInfo_size, 4);

    info->commitHash = COMMIT_HASH;
    info->config = config;
    info->versionMajor = (u8)(rev[1] - '0');
    info->versionMinor = (u8)(rev[3] - '0');
    if(rev[4] == '.')
    {
        info->versionBuild = (u8)(rev[5] - '0');
        isRelease = rev[6] == 0;
    }
    else
        isRelease = rev[4] == 0;

    info->flags = 0 /* master branch */ | (((isRelease) ? 1 : 0) << 1) /* is release */;

    arm11SvcTable[0x2E] = 0xFFF00000 + freeK11Space - pos; //stubbed svc
    freeK11Space += svcGetCFWInfo_size;
}

void patchTitleInstallMinVersionCheck(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x0A, 0x81, 0x42, 0x02};
    
    u8 *off = memsearch(pos, pattern, size, 4);
    
    if(off != NULL) off[4] = 0xE0;
}

void applyLegacyFirmPatches(u8 *pos, FirmwareType firmType)
{
    const patchData twlPatches[] = {
        {{0x1650C0, 0x165D64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0x173A0E, 0x17474A}, { .type1 = 0x2001 }, 1},
        {{0x174802, 0x17553E}, { .type1 = 0x2000 }, 2},
        {{0x174964, 0x1756A0}, { .type1 = 0x2000 }, 2},
        {{0x174D52, 0x175A8E}, { .type1 = 0x2001 }, 2},
        {{0x174D5E, 0x175A9A}, { .type1 = 0x2001 }, 2},
        {{0x174D6A, 0x175AA6}, { .type1 = 0x2001 }, 2},
        {{0x174E56, 0x175B92}, { .type1 = 0x2001 }, 1},
        {{0x174E58, 0x175B94}, { .type1 = 0x4770 }, 1}
    },
    agbPatches[] = {
        {{0x9D2A8, 0x9DF64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0xD7A12, 0xD8B8A}, { .type1 = 0xEF26 }, 1}
    };

    /* Calculate the amount of patches to apply. Only count the boot screen patch for AGB_FIRM
       if the matching option was enabled (keep it as last) */
    u32 numPatches = firmType == TWL_FIRM ? (sizeof(twlPatches) / sizeof(patchData)) :
                                            (sizeof(agbPatches) / sizeof(patchData) - !CONFIG(5));
    const patchData *patches = firmType == TWL_FIRM ? twlPatches : agbPatches;

    //Patch
    for(u32 i = 0; i < numPatches; i++)
    {
        switch(patches[i].type)
        {
            case 0:
                memcpy(pos + patches[i].offset[isN3DS ? 1 : 0], patches[i].patch.type0 + 1, patches[i].patch.type0[0]);
                break;
            case 2:
                *(u16 *)(pos + patches[i].offset[isN3DS ? 1 : 0] + 2) = 0;
            case 1:
                *(u16 *)(pos + patches[i].offset[isN3DS ? 1 : 0]) = patches[i].patch.type1;
                break;
        }
    }
}

void patchTwlBg(u8 *pos)
{
    u8 *dst = pos + ((isN3DS) ?  0xFEA4 : 0xFCA0);
    u16 *src1 = (u16 *)(pos + ((isN3DS) ? 0xE38 : 0xE3C)), *src2 = (u16 *)(pos + ((isN3DS) ? 0xE54 : 0xE58));
    memcpy(dst, twl_k11modules, twl_k11modules_size); //install k11 hook
    
    u32 *off;
    for(off = (u32 *)dst; *off != 0xABCDABCD; off++);
    *off = (isN3DS) ? 0xCDE88 : 0xCD5F8; //dev SRL launcher offset
    
    //Construct BLX instructions:
    src1[0] = 0xF000 | ((((u32)dst - (u32)src1 - 4) & (0xFFF << 11)) >> 12);
    src1[1] = 0xE800 | ((((u32)dst - (u32)src1 - 4) & 0xFFF) >> 1);

    src2[0] = 0xF000 | ((((u32)dst - (u32)src2 - 4) & (0xFFF << 11)) >> 12);
    src2[1] = 0xE800 | ((((u32)dst - (u32)src2 - 4) & 0xFFF) >> 1);
}