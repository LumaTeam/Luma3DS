/*
*   firm.c
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

void error(const char *t){
    clearScreens();
    drawString(t, 0, 0, COLOR_RED);
    waitInput();
}

void main(void)
{
    u32 bootType = 0,
        firmType = 0,
        a9lhMode = 1,
        chronoStarted = 0;

    //Detect the console being used, 7 means New 3DS
    console = PDN_MPCORE_CFG == 7;

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
        //Get pressed buttons
        u32 pressed = HID_PAD;

        //If the SAFE MODE combo is held, force a sysNAND boot
        if(pressed == SAFE_MODE)
        {
            a9lhMode++;
            firmType = 3; // Gotta set this for proper patching.
        }

        //If screens are inited or the corresponding option is set, load splash screen
        if(PDN_GPU_CNT != 1 && loadSplash())
        {
            chronoStarted = 2;
            chrono(0);
        }
    }

    loadFirm(firmType, !firmType);

    switch(firmType)
    {
        case 0:
            patchNativeFirm(a9lhMode);
            break;
        case 3:
            patchSafeFirm();
            break;
        default:
            patchLegacyFirm(firmType);
            break;
    }

    if(chronoStarted)
    {
        if(chronoStarted == 2) chrono(3);
        stopChrono();
    }

    launchFirm(!firmType, bootType);
}

static inline void loadFirm(u32 firmType, u32 externalFirm)
{
    section = firm->section;

    u32 externalFirmLoaded = externalFirm &&
                             fileRead(firm, "/firmware.bin") &&
                             (((u32)section[2].address >> 8) & 0xFF) == (console ? 0x60 : 0x68);

    /* If the conditions to load the external FIRM aren't met, or reading fails, or the FIRM
       doesn't match the console, load FIRM from CTRNAND */
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

static inline void patchNativeFirm(u32 a9lhMode)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset;

    u32 nativeFirmType;

    if(console)
    {
        //Determine the NATIVE_FIRM version
        switch(arm9Section[0x53])
        {
            case 0xFF:
                nativeFirmType = 0;
                break;
            case '1':
                nativeFirmType = 2;
                break;
            default:
                nativeFirmType = 1;
                break;
        }

        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section, nativeFirmType);
        firm->arm9Entry = (u8 *)0x801B01C;
    }
    else
    {
        //Determine if we're booting the 9.0 FIRM
        u8 firm90Hash[0x10] = {0x27, 0x2D, 0xFE, 0xEB, 0xAF, 0x3F, 0x6B, 0x3B, 0xF5, 0xDE, 0x4C, 0x41, 0xDE, 0x95, 0x27, 0x6A};
        nativeFirmType = memcmp(section[2].hash, firm90Hash, 0x10) != 0;
    }

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9(arm9Section + 0x15000, section[2].size - 0x15000, &process9Size, &process9MemAddr);

    //Apply FIRM0/1 writes patches on sysNAND to protect A9LH
    if(a9lhMode) patchFirmWrites(process9Offset, process9Size, 1);

    //Apply FIRM reboot patches, not on 9.0 FIRM as it breaks firmlaunchhax
    if(nativeFirmType || a9lhMode == 2) patchReboots(process9Offset, process9Size, process9MemAddr);

    //Apply signature checks patches
    u16 *sigOffset,
        *sigOffset2;
    getSigChecks(process9Offset, process9Size, &sigOffset, &sigOffset2);
    *sigOffset = sigPatch[0];
    sigOffset2[0] = sigPatch[0];
    sigOffset2[1] = sigPatch[1];

    //Does nothing if svcBackdoor is still there
    if(nativeFirmType == 1) reimplementSvcBackdoor();

    //Replace the FIRM loader with the injector while copying section0
    copySection0AndInjectLoader();
}

static inline void patchReboots(u8 *process9Offset, u32 process9Size, u32 process9MemAddr)
{
    //Calculate offset for the firmlaunch code and fOpen
    u32 fOpenOffset;
    void *rebootOffset = getReboot(process9Offset, process9Size, process9MemAddr, &fOpenOffset);

    //Copy firmlaunch code
    memcpy(rebootOffset, reboot, reboot_size);

    //Put the fOpen offset in the right location
    u32 *pos_fopen = (u32 *)memsearch(rebootOffset, "OPEN", reboot_size, 4);
    *pos_fopen = fOpenOffset;
}

static inline void reimplementSvcBackdoor(void)
{
    u8 *arm11Section1 = (u8 *)firm + section[1].offset;

    u32 *exceptionsPage;
    u32 *svcTable = getSvcAndExceptions(arm11Section1, section[1].size, &exceptionsPage);

    if(!svcTable[0x7B])
    {
        u32 *freeSpace;
        for(freeSpace = exceptionsPage; *freeSpace != 0xFFFFFFFF; freeSpace++);

        memcpy(freeSpace, svcBackdoor, 40);

        svcTable[0x7B] = 0xFFFF0000 + ((u8 *)freeSpace - (u8 *)exceptionsPage);
    }
}

static inline void copySection0AndInjectLoader(void)
{
    u8 *arm11Section0 = (u8 *)firm + section[0].offset;

    u32 loaderSize;
    u32 loaderOffset = getLoader(arm11Section0, &loaderSize);

    memcpy(section[0].address, arm11Section0, loaderOffset);
    memcpy(section[0].address + loaderOffset, injector, injector_size);
    memcpy(section[0].address + loaderOffset + injector_size, arm11Section0 + loaderOffset + loaderSize, section[0].size - (loaderOffset + loaderSize));
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
    patchFirmWrites(arm9Section, section[2].size, console);
}

static void patchFirmWrites(u8 *offset, u32 size, u32 mode)
{
    if(mode)
    {
        u16 *writeOffset = getFirmWrite(offset, size);
        *writeOffset = writeBlock[0];
        *(writeOffset + 1) = writeBlock[1];
    }
    else
    {
        u16 *writeOffset = getFirmWriteSafe(offset, size);
        *writeOffset = writeBlockSafe[0];
        *(writeOffset + 1) = writeBlockSafe[1];
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
    u32 numPatches = firmType == 1 ? (sizeof(twlPatches) / sizeof(patchData)) :
                                     (sizeof(agbPatches) / sizeof(patchData) - 1);
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

static inline void launchFirm(u32 sectionNum, u32 bootType)
{
    //Copy FIRM sections to respective memory locations
    for(; sectionNum < 4 && section[sectionNum].size; sectionNum++)
        memcpy(section[sectionNum].address, (u8 *)firm + section[sectionNum].offset, section[sectionNum].size);

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

    //Final jump to ARM9 kernel
    ((void (*)())firm->arm9Entry)();
}