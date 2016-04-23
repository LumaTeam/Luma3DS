/*
*   firm.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "firm.h"
#include "utils.h"
#include "fs.h"
#include "patches.h"
#include "memory.h"
#include "crypto.h"
#include "draw.h"
#include "screeninit.h"
#include "buttons.h"
#include "../build/patches.h"

static firmHeader *const firm = (firmHeader *)0x24000000;
static const firmSectionHeader *section;

u32 console;

void main(void)
{
    u32 bootType, firmType;

    //Detect the console being used
    console = (PDN_MPCORE_CFG == 1) ? 0 : 1; //3rd core exists ? n3ds : o3ds

    //Mount filesystems. CTRNAND will be mounted only if/when needed
    mountFs();

    //Determine if this is a firmlaunch boot
    if(*(vu8 *)0x23F00005)
    {
        bootType = 1;
        firmType = *(vu8 *)0x23F00005 - '0'; //'0' = NATIVE_FIRM, '1' = TWL_FIRM, '2' = AGB_FIRM
    }
    else
    {
        bootType = 0;
        firmType = 0;

        //Get pressed buttons
        u32 pressed = HID_PAD;

        //Prevent safe mode, as that can wipe a9lh or brick the device on N3DS.
        if(pressed == SAFE_MODE)
            error("Using Safe Mode would brick you, or remove A9LH!");

        if(PDN_GPU_CNT != 1) loadSplash();
    }

    loadFirm(firmType, !firmType);

    if(!firmType) patchNativeFirm();
    else patchTwlAgbFirm(firmType);

    launchFirm(bootType);
}

//Load FIRM into FCRAM
static inline void loadFirm(u32 firmType, u32 externalFirm)
{
    section = firm->section;

    u32 firmSize;

    if(externalFirm)
    {
        const char path[] = "/firmware.bin";
        firmSize = fileSize(path);

        if(firmSize)
        {
            fileRead(firm, path, firmSize);

            //Check that the loaded FIRM matches the console
            if((((u32)section[2].address >> 8) & 0xFF) != (console ? 0x60 : 0x68)) firmSize = 0;
        }
    }
    else firmSize = 0;

    if(!firmSize)
    {
        const char *firmFolders[3][2] = {{ "00000002", "20000002" },
                                         { "00000102", "20000102" },
                                         { "00000202", "20000202" }};

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
    u16 *writeOffset = getFirmWrite(arm9Section, section[2].size);
    *writeOffset = writeBlock[0];
    *(writeOffset + 1) = writeBlock[1];

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

static inline void patchTwlAgbFirm(u32 firmType)
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