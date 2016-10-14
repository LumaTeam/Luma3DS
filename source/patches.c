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

/*
*   ARM11 modules patching code originally by Subv
*/

#include "patches.h"
#include "fs.h"
#include "memory.h"
#include "config.h"
#include "utils.h"
#include "../build/bundled.h"

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
    const u8 pattern[] = {0x00, 0xB0, 0x9C, 0xE5};
    bool ret = true;

    *arm11ExceptionsPage = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));

    u32 *arm11SvcTable;

    if(*arm11ExceptionsPage == NULL) ret = false;
    else
    {
        *arm11ExceptionsPage -= 0xB;
        u32 svcOffset = (-(((*arm11ExceptionsPage)[2] & 0xFFFFFF) << 2) & (0xFFFFFF << 2)) - 8; //Branch offset + 8 for prefetch
        u32 pointedInstructionVA = 0xFFFF0008 - svcOffset;
        *baseK11VA = pointedInstructionVA & 0xFFFF0000; //This assumes that the pointed instruction has an offset < 0x10000, iirc that's always the case
        arm11SvcTable = *arm11SvcHandler = (u32 *)(pos + *(u32 *)(pos + pointedInstructionVA - *baseK11VA + 8) - *baseK11VA); //SVC handler address
        while(*arm11SvcTable) arm11SvcTable++; //Look for SVC0 (NULL)
    }

    const u8 pattern2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    *freeK11Space = memsearch(pos, pattern2, size, sizeof(pattern2));

    if(*freeK11Space == NULL) ret = false;
    else (*freeK11Space)++;

    if(!ret) error("Failed to get Kernel11 data.");

    return arm11SvcTable;
}

u32 patchSignatureChecks(u8 *pos, u32 size)
{
    //Look for signature checks
    const u8 pattern[] = {0xC0, 0x1C, 0x76, 0xE7},
             pattern2[] = {0xB5, 0x22, 0x4D, 0x0C};
    u32 ret;

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));
    u8 *temp = memsearch(pos, pattern2, size, sizeof(pattern2));

    if(off == NULL || temp == NULL) ret = 1;
    else
    {
        u16 *off2 = (u16 *)(temp - 1);

        *off = off2[0] = 0x2000;
        off2[1] = 0x4770;

        ret = 0;
    }

    return ret;
}

u32 patchFirmlaunches(u8 *pos, u32 size, u32 process9MemAddr, bool isSdMode)
{
    //Look for firmlaunch code
    const u8 pattern[] = {0xE2, 0x20, 0x20, 0x90};
    u32 ret;

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) ret = 1;
    else
    {
        off -= 0x13;

        //Firmlaunch function offset - offset in BLX opcode (A4-16 - ARM DDI 0100E) + 1
        u32 fOpenOffset = (u32)(off + 9 - (-((*(u32 *)off & 0x00FFFFFF) << 2) & (0xFFFFFF << 2)) - pos + process9MemAddr);

        //Copy firmlaunch code
        memcpy(off, reboot_bin, reboot_bin_size);

        //Put the fOpen offset in the right location
        u32 *pos_fopen = (u32 *)memsearch(off, "OPEN", reboot_bin_size, 4);
        *pos_fopen = fOpenOffset;

        ret = 0;

        if(CONFIG(USECUSTOMPATH))
        {
            const char *pathFile = "path.txt";

            u32 pathSize = getFileSize(pathFile);

            if(pathSize > 5 && pathSize < 58)
            {
                u8 path[pathSize];
                fileRead(path, pathFile, pathSize);
                if(path[pathSize - 1] == 0xA) pathSize--;
                if(path[pathSize - 1] == 0xD) pathSize--;

                if(pathSize > 5 && pathSize < 56 && path[0] == '/' && memcmp(&path[pathSize - 4], ".bin", 4) == 0)
                {
                    u16 finalPath[pathSize];
                    for(u32 i = 0; i < pathSize; i++)
                        finalPath[i] = (u16)path[i];

                    u8 *pos_path = memsearch(off, isSdMode ? u"sd" : u"na", reboot_bin_size, 4) + 0xA;
                    memcpy(pos_path, finalPath, pathSize * 2);
                }
            }
        }
    }

    return ret;
}

u32 patchFirmWrites(u8 *pos, u32 size)
{
    u32 ret;

    //Look for FIRM writing code
    u8 *off = memsearch(pos, "exe:", size, 4);

    if(off == NULL) ret = 1;
    else
    {
        const u8 pattern[] = {0x00, 0x28, 0x01, 0xDA};

        u16 *off2 = (u16 *)memsearch(off - 0x100, pattern, 0x100, sizeof(pattern));

        if(off2 == NULL) ret = 1;
        else
        {
            off2[0] = 0x2000;
            off2[1] = 0x46C0;

            ret = 0;
        }
    }

    return ret;
}

u32 patchOldFirmWrites(u8 *pos, u32 size)
{
    //Look for FIRM writing code
    const u8 pattern[] = {0x04, 0x1E, 0x1D, 0xDB};
    u32 ret;

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) ret = 1;
    else
    {
        off[0] = 0x2400;
        off[1] = 0xE01D;

        ret = 0;
    }

    return ret;
}

u32 reimplementSvcBackdoor(u8 *pos, u32 *arm11SvcTable, u32 baseK11VA, u8 **freeK11Space)
{
    u32 ret = 0;

    //Official implementation of svcBackdoor
    const u8 svcBackdoor[] = {0xFF, 0x10, 0xCD, 0xE3,  //bic   r1, sp, #0xff
                              0x0F, 0x1C, 0x81, 0xE3,  //orr   r1, r1, #0xf00
                              0x28, 0x10, 0x81, 0xE2,  //add   r1, r1, #0x28
                              0x00, 0x20, 0x91, 0xE5,  //ldr   r2, [r1]
                              0x00, 0x60, 0x22, 0xE9,  //stmdb r2!, {sp, lr}
                              0x02, 0xD0, 0xA0, 0xE1,  //mov   sp, r2
                              0x30, 0xFF, 0x2F, 0xE1,  //blx   r0
                              0x03, 0x00, 0xBD, 0xE8,  //pop   {r0, r1}
                              0x00, 0xD0, 0xA0, 0xE1,  //mov   sp, r0
                              0x11, 0xFF, 0x2F, 0xE1}; //bx    r1

    if(!arm11SvcTable[0x7B])
    {
        if(*(u32 *)(*freeK11Space + sizeof(svcBackdoor) - 4) != 0xFFFFFFFF) ret = 1;
        else
        {
            memcpy(*freeK11Space, svcBackdoor, sizeof(svcBackdoor));

            arm11SvcTable[0x7B] = baseK11VA + *freeK11Space - pos;
            *freeK11Space += sizeof(svcBackdoor);
        }
    }

    return ret;
}

u32 implementSvcGetCFWInfo(u8 *pos, u32 *arm11SvcTable, u32 baseK11VA, u8 **freeK11Space)
{
    u32 ret;

    if(*(u32 *)(*freeK11Space + svcGetCFWInfo_bin_size - 4) != 0xFFFFFFFF) ret = 1;
    else
    {
        memcpy(*freeK11Space, svcGetCFWInfo_bin, svcGetCFWInfo_bin_size);

        struct CfwInfo
        {
            char magic[4];
        
            u8 versionMajor;
            u8 versionMinor;
            u8 versionBuild;
            u8 flags;

            u32 commitHash;

            u32 config;
        } __attribute__((packed)) *info = (struct CfwInfo *)memsearch(*freeK11Space, "LUMA", svcGetCFWInfo_bin_size, 4);

        const char *rev = REVISION;

        info->commitHash = COMMIT_HASH;
        info->config = configData.config;
        info->versionMajor = (u8)(rev[1] - '0');
        info->versionMinor = (u8)(rev[3] - '0');

        bool isRelease;

        if(rev[4] == '.')
        {
            info->versionBuild = (u8)(rev[5] - '0');
            isRelease = rev[6] == 0;
        }
        else isRelease = rev[4] == 0;

        info->flags = isRelease ? 1 : 0;

        arm11SvcTable[0x2E] = baseK11VA + *freeK11Space - pos; //Stubbed svc
        *freeK11Space += svcGetCFWInfo_bin_size;

        ret = 0;
    }

    return ret;
}

u32 patchTitleInstallMinVersionChecks(u8 *pos, u32 size, u32 firmVersion)
{
    const u8 pattern[] = {0x0A, 0x81, 0x42, 0x02};
    u32 ret = 0;

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL)
    {
        if(firmVersion != 0xFFFFFFFF) ret = 1;
    }
    else off[4] = 0xE0;

    return ret;
}

u32 patchArm9ExceptionHandlersInstall(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x80, 0xE5, 0x40, 0x1C};
    u32 ret;

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) ret = 1;
    else
    {
        u32 *off = (u32 *)(temp - 0xA);

        for(u32 r0 = 0x08000000; *off != 0xE3A01040; off++) //Until mov r1, #0x40
        {
            //Discard everything that's not str rX, [r0, #imm](!)
            if((*off & 0xFE5F0000) == 0xE4000000)
            {
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
        }

        ret = 0;
    }

    return ret;
}

u32 getInfoForArm11ExceptionHandlers(u8 *pos, u32 size, u32 *codeSetOffset)
{
    const u8 pattern[] = {0x1B, 0x50, 0xA0, 0xE3}, //Get TitleID from CodeSet
             pattern2[] = {0xE8, 0x13, 0x00, 0x02}; //Call exception dispatcher
    bool ret = true;

    u32 *loadCodeSet = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(loadCodeSet == NULL) ret = false;
    else
    {
        loadCodeSet -= 2;
        *codeSetOffset = *loadCodeSet & 0xFFF;
    }

    u8 *temp = memsearch(pos, pattern2, size, sizeof(pattern2));

    u32 stackAddress;

    if(temp == NULL) ret = false;
    else stackAddress = *(u32 *)(temp + 9);

    if(!ret) error("Failed to get ARM11 exception handlers data.");

    return stackAddress;
}

u32 patchSvcBreak9(u8 *pos, u32 size, u32 kernel9Address)
{
    /* Stub svcBreak with "bkpt 65535" so we can debug the panic.
       Thanks @yellows8 and others for mentioning this idea on #3dsdev */

    //Look for the svc handler
    const u8 pattern[] = {0x00, 0xE0, 0x4F, 0xE1}; //mrs lr, spsr
    u32 ret;

    u32 *arm9SvcTable = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(arm9SvcTable == NULL) ret = 1;
    else
    {
        while(*arm9SvcTable) arm9SvcTable++; //Look for SVC0 (NULL)

        u32 *addr = (u32 *)(pos + arm9SvcTable[0x3C] - kernel9Address);
        *addr = 0xE12FFF7F;

        ret = 0;
    }

    return ret;
}

void patchSvcBreak11(u8 *pos, u32 *arm11SvcTable)
{
    //Same as above, for NATIVE_FIRM ARM11
    u32 *addr = (u32 *)(pos + arm11SvcTable[0x3C] - 0xFFF00000);
    *addr = 0xE12FFF7F;
}

u32 patchKernel9Panic(u8 *pos, u32 size)
{
    const u8 pattern[] = {0xFF, 0xEA, 0x04, 0xD0};
    u32 ret;

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) ret = 1;
    else
    {
        u32 *off = (u32 *)(temp - 0x12);
        *off = 0xE12FFF7E;

        ret = 0;
    }

    return ret;
}

u32 patchKernel11Panic(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x02, 0x0B, 0x44, 0xE2};
    u32 ret;

    u32 *off = (u32 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) ret = 1;
    else
    {
        *off = 0xE12FFF7E;

        ret = 0;
    }

    return ret;
}

u32 patchP9AccessChecks(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x00, 0x08, 0x49, 0x68};
    u32 ret;

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) ret = 1;
    else
    {
        u16 *off = (u16 *)(temp - 3);

        off[0] = 0x2001; //mov r0, #1
        off[1] = 0x4770; //bx lr

        ret = 0;
    }

    return ret;
}

u32 patchArm11SvcAccessChecks(u32 *arm11SvcHandler, u32 *endPos)
{
    u32 ret;

    while(*arm11SvcHandler != 0xE11A0E1B && arm11SvcHandler < endPos) arm11SvcHandler++; //TST R10, R11,LSL LR

    if(arm11SvcHandler == endPos) ret = 1;
    else
    {
        *arm11SvcHandler = 0xE3B0A001; //MOVS R10, #1

        ret = 0;
    }

    return ret;
}

u32 patchK11ModuleChecks(u8 *pos, u32 size, u8 **freeK11Space)
{
    /* We have to detour a function in the ARM11 kernel because builtin modules
       are compressed in memory and are only decompressed at runtime */

    u32 ret = 0;

    //Check that we have enough free space
    if(*(u32 *)(*freeK11Space + k11modules_bin_size - 4) == 0xFFFFFFFF)
    {
        //Look for the code that decompresses the .code section of the builtin modules
        const u8 pattern[] = {0xE5, 0x48, 0x00, 0x9D};

        u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

        if(temp == NULL) ret = 1;
        else
        {
            //Inject our code into the free space
            memcpy(*freeK11Space, k11modules_bin, k11modules_bin_size);

            u32 *off = (u32 *)(temp - 0xB);

            //Inject a jump (BL) instruction to our code at the offset we found
            *off = 0xEB000000 | (((((u32)*freeK11Space) - ((u32)off + 8)) >> 2) & 0xFFFFFF);

            *freeK11Space += k11modules_bin_size;
        }
    }

    return ret;
}

u32 patchUnitInfoValueSet(u8 *pos, u32 size)
{
    //Look for UNITINFO value being set during kernel sync
    const u8 pattern[] = {0xA0, 0x13, 0x01, 0x10};
    u32 ret;

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) ret = 1;
    else
    {
        off -= 2;

        off[0] = ISDEVUNIT ? 0 : 1;
        off[3] = 0xE3;

        ret = 0;
    }

    return ret;
}

u32 patchLgySignatureChecks(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x47, 0xC1, 0x17, 0x49};
    u32 ret;

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) ret = 1;
    else
    {
        u16 *off = (u16 *)(temp + 1);

        off[0] = 0x2000;
        off[1] = 0xB04E;
        off[2] = 0xBD70;

        ret = 0;
    }

    return ret;
}

u32 patchTwlInvalidSignatureChecks(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x20, 0xF6, 0xE7, 0x7F};
    u32 ret;

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) ret = 1;
    else
    {
        u16 *off = (u16 *)(temp - 1);

        *off = 0x2001; //mov r0, #1

        ret = 0;
    }

    return ret;
}

u32 patchTwlNintendoLogoChecks(u8 *pos, u32 size)
{
    const u8 pattern[] = {0xC0, 0x30, 0x06, 0xF0};
    u32 ret;

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) ret = 1;
    else
    {
        u16 *off = (u16 *)temp + 1;

        off[0] = 0x2000;
        off[1] = 0;

        ret = 0;
    }

    return ret;
}

u32 patchTwlWhitelistChecks(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x22, 0x00, 0x20, 0x30};
    u32 ret;

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL) ret = 1;
    else
    {
        u16 *off = (u16 *)temp + 2;

        off[0] = 0x2000;
        off[1] = 0;

        ret = 0;
    }

    return ret;
}

u32 patchTwlFlashcartChecks(u8 *pos, u32 size, u32 firmVersion)
{
    const u8 pattern[] = {0x25, 0x20, 0x00, 0x0E};
    u32 ret;

    u8 *temp = memsearch(pos, pattern, size, sizeof(pattern));

    if(temp == NULL)
    {
        if(firmVersion == 0xFFFFFFFF) ret = patchOldTwlFlashcartChecks(pos, size);
        else ret = 1;
    }
    else
    {
        u16 *off = (u16 *)(temp + 3);

        off[0] = 0x2001; //mov r0, #1
        off[1] = 0; //nop
        off[6] = 0x2001; //mov r0, #1
        off[7] = 0; //nop
        off[0xC] = 0x2001; //mov r0, #1
        off[0xD] = 0; //nop

        ret = 0;
    }

    return ret;
}

u32 patchOldTwlFlashcartChecks(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x06, 0xF0, 0xA0, 0xFD};
    u32 ret;

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) ret = 1;
    else
    {
        off[0] = 0x2001; //mov r0, #1
        off[1] = 0; //nop
        off[6] = 0x2001; //mov r0, #1
        off[7] = 0; //nop

        ret = 0;
    }

    return ret;
}

u32 patchTwlShaHashChecks(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x10, 0xB5, 0x14, 0x22};
    u32 ret;

    u16 *off = (u16 *)memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) ret = 1;
    else
    {
        off[0] = 0x2001; //mov r0, #1
        off[1] = 0x4770;

        ret = 0;
    }

    return ret;
}

u32 patchAgbBootSplash(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x00, 0x00, 0x01, 0xEF};
    u32 ret;

    u8 *off = memsearch(pos, pattern, size, sizeof(pattern));

    if(off == NULL) ret = 1;
    else
    {
        off[2] = 0x26;

        ret = 0;
    }

    return ret;
}