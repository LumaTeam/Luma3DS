/*
*   patches.c
*/

#include "patches.h"
#include "memory.h"
#include "config.h"
#include "../build/rebootpatch.h"

static u32 *arm11ExceptionsPage = NULL;
static u32 *arm11SvcTable = NULL;

static void findArm11ExceptionsPageAndSvcTable(u8 *pos, u32 size)
{
    const u8 arm11ExceptionsPagePattern[] = {0x00, 0xB0, 0x9C, 0xE5};
    
    if(arm11ExceptionsPage == NULL) arm11ExceptionsPage = (u32 *)memsearch(pos, arm11ExceptionsPagePattern, size, 4) - 0xB;
    if(arm11SvcTable == NULL && arm11ExceptionsPage != NULL)
    {
        u32 svcOffset = (-((arm11ExceptionsPage[2] & 0xFFFFFF) << 2) & (0xFFFFFF << 2)) - 8; //Branch offset + 8 for prefetch
        arm11SvcTable = (u32 *)(pos + *(u32 *)(pos + 0xFFFF0008 - svcOffset - 0xFFF00000 + 8) - 0xFFF00000); //SVC handler address
        while(*arm11SvcTable) arm11SvcTable++; //Look for SVC0 (NULL)
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

u32* getInfoForArm11ExceptionHandlers(u8 *pos, u32 size, u32 *stackAddr, u32 *codeSetOffset)
{
    //This function has to succeed. Crash if it doesn't (we'll get an exception dump of it anyways)
    
    const u8 callExceptionDispatcherPattern[] = {0x0F, 0x00, 0xBD, 0xE8, 0x13, 0x00, 0x02, 0xF1};   
    const u8 getTitleIDFromCodeSetPattern[] = {0xDC, 0x05, 0xC0, 0xE1, 0x20, 0x04, 0xA0, 0xE1};
    
    *stackAddr = *((u32 *)memsearch(pos, callExceptionDispatcherPattern, size, 8) + 3);
    
    u32 *loadCodeSet = (u32 *)memsearch(pos, getTitleIDFromCodeSetPattern, size, 8);
    while((*loadCodeSet >> 20) != 0xE59 || ((*loadCodeSet >> 12) & 0xF) != 0) //ldr r0, [rX, #offset]
        loadCodeSet--;
    *codeSetOffset = *loadCodeSet & 0xFFF;
    
    findArm11ExceptionsPageAndSvcTable(pos, size);
    return arm11ExceptionsPage;
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

void patchExceptionHandlersInstall(u8 *pos, u32 size)
{
    const u8 pattern[] = {
        0x18, 0x10, 0x80, 0xE5, 
        0x10, 0x10, 0x80, 0xE5, 
        0x20, 0x10, 0x80, 0xE5, 
        0x28, 0x10, 0x80, 0xE5,
    }; //i.e when it stores ldr pc, [pc, #-4]
    
    u32* off = (u32 *)(memsearch(pos, pattern, size, sizeof(pattern)));
    if(off == NULL) return;
    off += sizeof(pattern)/4;

    u32 r0 = 0x08000000;

    for(; *off != 0xE3A01040; off++) //Until mov r1, #0x40
    {
        if((*off >> 26) != 0x39 || ((*off >> 16) & 0xF) != 0 || ((*off >> 25) & 1) != 0 || ((*off >> 20) & 5) != 0)
            continue; //Discard everything that's not str rX, [r0, #imm](!)

        int rD = (*off >> 12) & 0xF,
            offset = (*off & 0xFFF) * ((((*off >> 23) & 1) == 0) ? -1 : 1),
            writeback = (*off >> 21) & 1,
            pre = (*off >> 24) & 1;

        u32 addr = r0 + ((pre || !writeback) ? offset : 0);
        if(addr != 0x08000014 && addr != 0x08000004)
            *off = 0xE1A00000; //nop
        else
            *off = 0xE5800000 | (rD << 12) | (addr & 0xFFF); //Preserve IRQ and SVC handlers

        if(!pre) addr += offset;
        if(writeback) r0 = addr;
    }
}

void patchSvcBreak9(u8 *pos, u32 size, u32 k9addr)
{
    //Stub svcBreak with "bkpt 65535" so we can debug the panic.
    //Thanks @yellows8 and others for mentioning this idea on #3dsdev.
    const u8 svcHandlerPattern[] = {0x00, 0xE0, 0x4F, 0xE1}; //mrs lr, spsr
    
    u32 *arm9SvcTable = (u32 *)memsearch(pos, svcHandlerPattern, size, 4);
    while(*arm9SvcTable) arm9SvcTable++; //Look for SVC0 (NULL)
    *(u32 *)(pos + arm9SvcTable[0x3C] - k9addr) = 0xE12FFF7F;
}

void patchSvcBreak11(u8 *pos, u32 size)
{
    //Same as above, for NFIRM arm11
    
    findArm11ExceptionsPageAndSvcTable(pos, size);
    *(u32 *)(pos + arm11SvcTable[0x3C] - 0xFFF00000) = 0xE12FFF7F;
}

void patchUnitInfoValueSet(u8 *pos, u32 size)
{
    //Look for UNITINFO value being set
    const u8 pattern[] = {0x01, 0x10, 0xA0, 0x13};

    u8 *off = memsearch(pos, pattern, size, 4);

    off[3] = 0xE3;
}

void patchKernelFCRAMAndVRAMMappingPermissions(u8 *pos, u32 size)
{
    //Look for MMU config
    const u8 pattern[] = {0x97, 0x05, 0x00, 0x00, 0x15, 0xE4, 0x00, 0x00};

    u32 *off = (u32 *)memsearch(pos, pattern, size, 8);
    while(off != NULL && *off != 0x16416) off--;

    if(off != NULL) *off &= ~(1 << 4); //Clear XN bit
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

    findArm11ExceptionsPageAndSvcTable(pos, size);

    if(!arm11SvcTable[0x7B])
    {
        u32 *freeSpace;
        for(freeSpace = arm11ExceptionsPage; *freeSpace != 0xFFFFFFFF; freeSpace++);

        memcpy(freeSpace, svcBackdoor, 40);

        arm11SvcTable[0x7B] = 0xFFFF0000 + ((u8 *)freeSpace - (u8 *)arm11ExceptionsPage);
    }
}

void patchTitleInstallMinVersionCheck(u8 *pos, u32 size)
{
    const u8 pattern[] = {0x0A, 0x81, 0x42, 0x02};
    
    u8 *off = memsearch(pos, pattern, size, 4);
    
    if(off != NULL) off[4] = 0xE0;
}

void applyLegacyFirmPatches(u8 *pos, u32 firmType, u32 console)
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
    u32 numPatches = firmType == 1 ? (sizeof(twlPatches) / sizeof(patchData)) :
                                     (sizeof(agbPatches) / sizeof(patchData) - !CONFIG(6));
    const patchData *patches = firmType == 1 ? twlPatches : agbPatches;

    //Patch
    for(u32 i = 0; i < numPatches; i++)
    {
        switch(patches[i].type)
        {
            case 0:
                memcpy(pos + patches[i].offset[console], patches[i].patch.type0 + 1, patches[i].patch.type0[0]);
                break;
            case 2:
                *(u16 *)(pos + patches[i].offset[console] + 2) = 0;
            case 1:
                *(u16 *)(pos + patches[i].offset[console]) = patches[i].patch.type1;
                break;
        }
    }
}

u32 getLoader(u8 *pos, u32 *loaderSize)
{
    u8 *off = pos;
    u32 size;

    while(1)
    {
        size = *(u32 *)(off + 0x104) * 0x200;
        if(*(u32 *)(off + 0x200) == 0x64616F6C) break;
        off += size;
    }

    *loaderSize = size;

    return (u32)(off - pos);
}