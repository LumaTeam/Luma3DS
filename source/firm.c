/*
*   firm.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "firm.h"
#include "fs.h"
#include "patches.h"
#include "memory.h"
#include "crypto.h"
#include "draw.h"
#include "screeninit.h"
#include "buttons.h"
#include "../build/patches.h"
#include "i2c.h"

static firmHeader *const firm = (firmHeader *)0x24000000;
static const firmSectionHeader *section;

u32 console;

void main(void)
{
    u32 bootType, firmType;

    //Detect the console being used
    console = (PDN_MPCORE_CFG == 7); //if the result is 0111 (have 4 cores)

    //Mount filesystems. CTRNAND will be mounted only if/when needed
    mountFs();

    //Determine if this is a firmlaunch boot
    if(*(vu8 *)0x23F00005)
    {
        bootType = 1;
        //'0' = NATIVE_FIRM, '1' = TWL_FIRM, '2' = AGB_FIRM
        firmType = *(vu8 *)0x23F00009 == '3' ? 3 : *(vu8 *)0x23F00005 - '0';
    }
    else
    {
        bootType = 0;
        firmType = 0;

        if(PDN_GPU_CNT != 1) loadSplash();
    }

    loadFirm(firmType, !firmType);

    switch(firmType)
    {
        case 0:
            patchNativeFirm();
            break;
        case 3:
            patchSafeFirm();
            break;
        default:
            patchLegacyFirm(firmType);
            break;
    }

    launchFirm(bootType);
}

// Load FIRM into FCRAM
static inline void loadFirm(u32 firmType, u32 externalFirm)
{
    section = firm->section;

    /* If the conditions to load the external FIRM aren't met, or reading fails, or the FIRM
       doesn't match the console, load it from CTRNAND */

    u32 externalFirmLoaded = externalFirm &&
                             fileRead(firm, "/firmware.bin", 0) &&
                             (((u32)section[2].address >> 8) & 0xFF) == (console ? 0x60 : 0x68);

    if(!externalFirmLoaded)
    {
        const char *firmFolders[4][2] = {{ "00000002", "20000002" },
                                         { "00000102", "20000102" },
                                         { "00000202", "20000202" },
                                         { "00000003", "20000003" }};

        firmRead(firm, firmFolders[firmType][console]);
        decryptExeFs((u8 *)firm);
    }
}

static inline void patchNativeFirm()
{
    u8 *arm9Section = (u8 *)firm + section[2].offset;

    if(console)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section, 1);
        firm->arm9Entry = (u8 *)0x801B01C;
    }

    //Find the Process9 NCCH location
    u8 *proc9Offset = getProc9(arm9Section, section[2].size);

    //Apply FIRM reboot patches, not on 9.0 FIRM as it breaks firmlaunchhax
    patchReboots(arm9Section, proc9Offset);

    //Apply FIRM0/1 writes patches on sysNAND to protect A9LH
    patchFirmWrites(arm9Section, 1);

    //Apply signature checks patches
    u32 sigOffset,
    sigOffset2;

    getSigChecks(arm9Section, section[2].size, &sigOffset, &sigOffset2);
    *(u16 *)sigOffset = sigPatch[0];
    *(u16 *)sigOffset2 = sigPatch[0];
    *((u16 *)sigOffset2 + 1) = sigPatch[1];

    //Replace the FIRM loader with the injector
    injectLoader();
}

static inline void patchReboots(u8 *arm9Section, u8 *proc9Offset)
{
    //Calculate offset for the firmlaunch code
    void *rebootOffset = getReboot(arm9Section, section[2].size);

    //Calculate offset for the fOpen function
    u32 fOpenOffset = getfOpen(proc9Offset, rebootOffset);

    //Copy firmlaunch code
    memcpy(rebootOffset, reboot, reboot_size);

    //Put the fOpen offset in the right location
    u32 *pos_fopen = (u32 *)memsearch(rebootOffset, "OPEN", reboot_size, 4);
    *pos_fopen = fOpenOffset;
}

static inline void injectLoader(void)
{
    u32 loaderSize;

    void *loaderOffset = getLoader((u8 *)firm + section[0].offset, section[0].size, &loaderSize);

    //Check that the injector CXI isn't larger than the original
    if((u32)injector_size <= loaderSize)
    {
        memcpy(loaderOffset, injector, injector_size);

        //Patch content size and ExeFS size to match the repaced loader's ones
        *((u32 *)loaderOffset + 0x41) = loaderSize / 0x200;
        *((u32 *)loaderOffset + 0x69) = loaderSize / 0x200 - 5;
    }
}

static inline void patchLegacyFirm(u32 firmType)
{
    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
    if(console)
    {
        arm9Loader((u8 *)firm + section[3].offset, 0);
        firm->arm9Entry = (u8 *)0x801301C;
    }

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
    u32 numPatches = firmType == 1 ? (sizeof(twlPatches) / sizeof(patchData)) : (sizeof(agbPatches) / sizeof(patchData) - 1);
    const patchData *patches = firmType == 1 ? twlPatches : agbPatches;

    //Patch
    for(u32 i = 0; i < numPatches; i++)
    {
        switch(patches[i].type)
        {
            case 0:
            memcpy((u8 *)firm + patches[i].offset[console], patches[i].patch.type0 + 1, patches[i].patch.type0[0]);
            break;
            case 2:
            *(u16 *)((u8 *)firm + patches[i].offset[console] + 2) = 0;
            case 1:
            *(u16 *)((u8 *)firm + patches[i].offset[console]) = patches[i].patch.type1;
            break;
        }
    }
}

static inline void patchSafeFirm(void)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset;

    if(console)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section, 0);
        firm->arm9Entry = (u8 *)0x801B01C;
    }

    //Apply FIRM0/1 writes patches to protect A9LH
    patchFirmWrites(arm9Section, console);
}

static void patchFirmWrites(u8 *arm9Section, u32 mode)
{
    if(mode)
    {
        u16 *writeOffset = getFirmWrite(arm9Section, section[2].size);
        *writeOffset = writeBlock[0];
        *(writeOffset + 1) = writeBlock[1];
    }
    else
    {
        u16 *writeOffset = getFirmWriteSafe(arm9Section, section[2].size);
        *writeOffset = writeBlockSafe[0];
        *(writeOffset + 1) = writeBlockSafe[1];
    }
}

static inline void launchFirm(u32 bootType)
{
    //Copy firm partitions to respective memory locations
    for(u32 i = 0; i < 4 && section[i].size; i++)
        memcpy(section[i].address, (u8 *)firm + section[i].offset, section[i].size);

    //Determine the ARM11 entry to use
    vu32 *arm11;
    if(bootType) arm11 = (u32 *)0x1FFFFFFC;
    else
    {
        deinitScreens();
        arm11 = (u32 *)0x1FFFFFF8;
    }

    //Set ARM11 kernel entrypoint
    *arm11 = (u32)firm->arm11Entry;

    //Final jump to arm9 kernel
    ((void (*)())firm->arm9Entry)();
}