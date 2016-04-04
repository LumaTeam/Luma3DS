/*
*   firm.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "firm.h"
#include "patches.h"
#include "memory.h"
#include "fs.h"
#include "emunand.h"
#include "crypto.h"
#include "draw.h"
#include "screeninit.h"
#include "loader.h"
#include "utils.h"
#include "buttons.h"
#include "../build/patches.h"

//FIRM patches version
#define PATCH_VER 3

static firmHeader *const firm = (firmHeader *)0x24000000;
static const firmSectionHeader *section;
static u8 *arm9Section;
static const char *patchedFirms[] = { "/aurei/patched_firmware_sys.bin",
                                     "/aurei/patched_firmware_emu.bin",
                                     "/aurei/patched_firmware_em2.bin",
                                     "/aurei/patched_firmware90.bin",
                                     "/aurei/patched_firmware_twl.bin",
                                     "/aurei/patched_firmware_agb.bin" };

static u32 firmSize,
           console,
           mode,
           emuNAND,
           a9lhSetup,
           selectedFirm,
           usePatchedFirm,
           emuOffset,
           emuHeader,
           config;

void setupCFW(void)
{
    //Determine if booting with A9LH
    u32 a9lhBoot = !PDN_SPI_CNT ? 1 : 0;

    //Retrieve the last booted FIRM
    u32 previousFirm = CFG_BOOTENV;

    //Detect the console being used
    console = (PDN_MPCORE_CFG == 1) ? 0 : 1;

    //Get pressed buttons
    u32 pressed = HID_PAD;

    //Attempt to read the configuration file
    const char configPath[] = "/aurei/config.bin";
    u32 needConfig;
    if(fileRead(&config, configPath, 3)) needConfig = 1;
    else
    {
        config = 0;
        needConfig = 2;
    }

    //Determine if A9LH is installed and the user has an updated sysNAND
    u32 updatedSys;

    if(a9lhBoot || (config >> 2) & 1)
    {
        if(pressed == SAFE_MODE)
            error("Using Safe Mode would brick you, or remove A9LH!");

        a9lhSetup = 1;

        //Check setting for > 9.2 sysNAND
        updatedSys = config & 1;
    }
    else
    {
        a9lhSetup = 0;
        updatedSys = 0;
    }

    u32 tempConfig = (PATCH_VER << 17) | (a9lhSetup << 16);

    /* If booting with A9LH, it's a MCU reboot and a previous configuration exists,
       try to force boot options */
    if(a9lhBoot && previousFirm && needConfig == 1)
    {
        //Always force a sysNAND boot when quitting AGB_FIRM
        if(previousFirm == 7)
        {
            mode = updatedSys ? 1 : (config >> 12) & 1;
            emuNAND = 0;
            needConfig = 0;

            //Flag to prevent multiple boot options-forcing
            tempConfig |= 1 << 15;
        }
        /* Else, force the last used boot options unless a payload button or A/L/R are pressed
           or the no-forcing flag is set */
        else if(!(pressed & OVERRIDE_BUTTONS) && !((config >> 15) & 1))
        {
            mode = (config >> 12) & 1;
            emuNAND = (config >> 13) & 3;
            needConfig = 0;
        }
    }

    //Boot options aren't being forced
    if(needConfig)
    {
        /* If L and R/Select or one of the single payload buttons are pressed and, if not using A9LH,
           the Safe Mode combo is not pressed, chainload an external payload */
        if(((pressed & SINGLE_PAYLOAD_BUTTONS) || ((pressed & BUTTON_L1) && (pressed & L_PAYLOAD_BUTTONS)))
           && pressed != SAFE_MODE)
            loadPayload();

        //If no configuration file exists or SELECT is held, load configuration menu
        if(needConfig == 2 || (pressed & BUTTON_SELECT))
            configureCFW(configPath, patchedFirms);

        //If screens are inited, load splash screen
        if(PDN_GPU_CNT != 1) loadSplash();

        /* If L is pressed, or L or R are not pressed when it is the default FIRM,
           boot 9.0 FIRM */
        mode = ((config >> 3) & 1) ? ((!(pressed & BUTTON_L1R1)) ? 0 : 1) :
                                     ((pressed & BUTTON_L1) ? 0 : 1);

        /* If L or R aren't pressed on a 9.0/9.2 sysNAND, or the 9.0 FIRM is selected
           or R is pressed on a > 9.2 sysNAND, boot emuNAND */
        if((updatedSys && (!mode || (pressed & BUTTON_R1))) || (!updatedSys && mode && !(pressed & BUTTON_R1)))
        {
            /* If not 9.0 FIRM and the second emuNAND is set as default and B isn't pressed, or vice-versa,
               attempt to boot it */ 
            emuNAND = (mode && ((!(pressed & BUTTON_B)) == ((config >> 4) & 1))) ? 2 : 1;
        }
        else emuNAND = 0;

        /* If tha FIRM patches version is different or user switched to/from A9LH,
           delete all patched FIRMs */
        if((tempConfig & 0xFF0000) != (config & 0xFF0000))
            deleteFirms(patchedFirms, sizeof(patchedFirms) / sizeof(char *));
    }

    u32 usePatchedFirmSet = ((config >> 1) & 1);

    while(1)
    {
        /* Determine which patched FIRM we need to write or attempt to use (if any).
           Patched 9.0 FIRM is only needed if "Use pre-patched FIRMs" is set */
        selectedFirm = mode ? (emuNAND ? (emuNAND == 1 ? 2 : 3) : 1) :
                              (usePatchedFirmSet ? 4 : 0);

        //If "Use pre-patched FIRMs" is set and the appropriate FIRM exists, use it
        if(usePatchedFirmSet && fileExists(patchedFirms[selectedFirm - 1]))
            usePatchedFirm = 1;

        /* If the user is booting EmuNAND but the chosen one is not found,
           force 9.6/10.x FIRM and re-detect the patched FIRMs */
        else if(emuNAND && !getEmunandSect(&emuOffset, &emuHeader, &emuNAND))
        {
            mode = 1;
            continue;
        }

        break;
    }

    tempConfig |= (emuNAND << 13) | (mode << 12);

    //If the boot configuration is different from previously, overwrite it
    if(tempConfig != (config & 0xFFF000))
    {
        //Preserve user settings (first 12 bits)
        tempConfig |= config & 0xFFF;

        fileWrite(&tempConfig, configPath, 3);
    }
}

//Load FIRM into FCRAM
void loadFirm(void)
{
    //If not using an A9LH setup or the patched FIRM, load 9.0 FIRM from NAND
    if(!usePatchedFirm && !a9lhSetup && !mode)
    {
        //Read FIRM from NAND and write to FCRAM
        firmSize = console ? 0xF2000 : 0xE9000;
        nandFirm0((u8 *)firm, firmSize, console);

        //Check for correct decryption
        if(memcmp(firm, "FIRM", 4) != 0)
            error("Couldn't decrypt NAND FIRM0 (O3DS not on 9.x?)");
    }

    //Load FIRM from SD
    else
    {
        const char *path = usePatchedFirm ? patchedFirms[selectedFirm - 1] :
                                            (mode ? "/aurei/firmware.bin" : "/aurei/firmware90.bin");
        firmSize = fileSize(path);
        if(!firmSize) error(mode ? "aurei/firmware.bin doesn't exist" :
                                   "aurei/firmware90.bin doesn't exist");
        fileRead(firm, path, firmSize);
    }

    section = firm->section;

    //Check that the loaded FIRM matches the console
    if((((u32)section[2].address >> 8) & 0xFF) != (console ? 0x60 : 0x68))
        error(mode ? "aurei/firmware.bin doesn't match this console,\nor it's encrypted" :
                     "aurei/firmware90.bin doesn't match this console,\nor it's encrypted");

    arm9Section = (u8 *)firm + section[2].offset;

    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
    if(console && !usePatchedFirm)
    {
        decryptArm9Bin(arm9Section, mode);
        firm->arm9Entry = (u8 *)0x801B01C;
    }
}

static inline void patchTwlAgb(u32 whichFirm)
{
    static firmHeader *const twlAgbFirm = (firmHeader *)0x25000000;

    const char *path = whichFirm ? "/aurei/firmware_agb.bin" : "/aurei/firmware_twl.bin";
    u32 size = fileSize(path);

    //Skip patching if the file doesn't exist
    if(!size) return;

    fileRead(twlAgbFirm, path, size);

    static const firmSectionHeader *twlAgbSection = twlAgbFirm->section;

    //Check that the loaded FIRM matches the console
    if((((u32)twlAgbSection[3].address >> 8) & 0xFF) != (console ? 0x60 : 0x68))
        error(whichFirm ? "aurei/firmware_agb.bin doesn't match this\nconsole, or it's encrypted" :
                          "aurei/firmware_twl.bin doesn't match this\nconsole, or it's encrypted");

    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
    if(console)
    {
        decryptArm9Bin((u8 *)twlAgbFirm + twlAgbSection[3].offset, 0);
        twlAgbFirm->arm9Entry = (u8 *)0x801301C;
    }

    struct patchData {
        u32 offset[2];
        union {
            u8 type0[8];
            u16 type1;
        } patch;
        u32 type;
    };

    static const struct patchData twlPatches[] = {
        {{0x1650C0, 0x165D64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0x173A0E, 0x17474A}, { .type1 = 0x2001 }, 1},
        {{0x174802, 0x17553E}, { .type1 = 0x2000 }, 2},
        {{0x174964, 0x1756A0}, { .type1 = 0x2000 }, 2},
        {{0x174D52, 0x175A8E}, { .type1 = 0x2001 }, 2},
        {{0x174D5E, 0x175A9A}, { .type1 = 0x2001 }, 2},
        {{0x174D6A, 0x175AA6}, { .type1 = 0x2001 }, 2},
        {{0x174E56, 0x175B92}, { .type1 = 0x2001 }, 1},
        {{0x174E58, 0x175B94}, { .type1 = 0x4770 }, 1}
    };

    static const struct patchData agbPatches[] = {
        {{0x9D2A8, 0x9DF64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0xD7A12, 0xD8B8A}, { .type1 = 0xEF26 }, 1}
    };

    /* Calculate the amount of patches to apply. Only count the boot screen patch for AGB_FIRM
       if the matching option was enabled (keep it as last) */
    u32 numPatches = whichFirm ? (sizeof(agbPatches) / sizeof(struct patchData)) - !((config >> 6) & 1) :
                                 (sizeof(twlPatches) / sizeof(struct patchData));
    const struct patchData *patches = whichFirm ? agbPatches : twlPatches;

    //Patch
    for(u32 i = 0; i < numPatches; i++)
    {
        switch(patches[i].type)
        {
            case 0:
                memcpy((u8 *)twlAgbFirm + patches[i].offset[console], patches[i].patch.type0 + 1, patches[i].patch.type0[0]);
                break;
            case 2:
                *(u16 *)((u8 *)twlAgbFirm + patches[i].offset[console] + 2) = 0;
            case 1:
                *(u16 *)((u8 *)twlAgbFirm + patches[i].offset[console]) = patches[i].patch.type1;
                break;
        }
    }

    fileWrite(twlAgbFirm, whichFirm ? patchedFirms[5] : patchedFirms[4], size);
}

//NAND redirection
static inline void patchEmuNAND(u8 *proc9Offset)
{
    //Copy emuNAND code
    void *emuCodeOffset = getEmuCode(proc9Offset);
    memcpy(emuCodeOffset, emunand, emunand_size);

    //Add the data of the found emuNAND
    u32 *pos_offset = (u32 *)memsearch(emuCodeOffset, "NAND", emunand_size, 4);
    u32 *pos_header = (u32 *)memsearch(emuCodeOffset, "NCSD", emunand_size, 4);
    *pos_offset = emuOffset;
    *pos_header = emuHeader;

    //Find and add the SDMMC struct
    u32 *pos_sdmmc = (u32 *)memsearch(emuCodeOffset, "SDMC", emunand_size, 4);
    *pos_sdmmc = getSDMMC(arm9Section, section[2].size);

    //Calculate offset for the hooks
    u32 branchOffset = (u32)emuCodeOffset - (u32)firm -
                       section[2].offset + (u32)section[2].address;

    //Add emunand hooks
    u32 emuRead,
        emuWrite;

    getEmuRW(arm9Section, section[2].size, &emuRead, &emuWrite);
    *(u16 *)emuRead = nandRedir[0];
    *((u16 *)emuRead + 1) = nandRedir[1];
    *((u32 *)emuRead + 1) = branchOffset;
    *(u16 *)emuWrite = nandRedir[0];
    *((u16 *)emuWrite + 1) = nandRedir[1];
    *((u32 *)emuWrite + 1) = branchOffset;

    //Set MPU for emu code region
    u32 *mpuOffset = getMPU(arm9Section, section[2].size);
    *mpuOffset = mpuPatch[0];
    *(mpuOffset + 6) = mpuPatch[1];
    *(mpuOffset + 9) = mpuPatch[2];
}

static inline void patchReboots(u8 *proc9Offset)
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

    //Patch path for emuNAND-patched FIRM
    if(emuNAND)
    {
        void *pos_path = memsearch(rebootOffset, L"sy", reboot_size, 4);
        memcpy(pos_path, emuNAND == 1 ? L"emu" : L"em2", 5);
    }
}

static inline void injectLoader(void)
{
    u32 loaderOffset,
        loaderSize;

    getLoader((u8 *)firm + section[0].offset, section[0].size, &loaderOffset, &loaderSize);

    //Check that the injector CXI isn't larger than the original
    if(injector_size <= (int)loaderSize)
    {
        memset((void *)loaderOffset, 0, loaderSize);
        memcpy((void *)loaderOffset, injector, injector_size);

        //Patch content size and ExeFS size to match the repaced loader's ones
        *((u32 *)loaderOffset + 0x41) = loaderSize / 0x200;
        *((u32 *)loaderOffset + 0x69) = loaderSize / 0x200 - 5;
    }
}

//Patches
void patchFirm(void)
{
    if(mode)
    {
        //Only patch AGB_FIRM/TWL_FIRM if the patched ones don't already exist
        if(!fileExists(patchedFirms[4])) patchTwlAgb(0);
        if(!fileExists(patchedFirms[5])) patchTwlAgb(1);
    }

    //Skip patching
    if(usePatchedFirm) return;

    if(mode || emuNAND)
    {
        //Find the Process9 NCCH location
        u8 *proc9Offset = getProc9(arm9Section, section[2].size);

        //Apply emuNAND patches
        if(emuNAND) patchEmuNAND(proc9Offset);

        //Apply FIRM reboot patches, not on 9.0 FIRM as it breaks firmlaunchhax
        if(mode) patchReboots(proc9Offset);
    }

    //Apply FIRM0/1 writes patches on sysNAND to protect A9LH
    if(a9lhSetup && !emuNAND)
    {
        u16 *writeOffset = getFirmWrite(arm9Section, section[2].size);
        *writeOffset = writeBlock[0];
        *(writeOffset + 1) = writeBlock[1];
    }

    //Apply signature checks patches
    u32 sigOffset,
        sigOffset2;

    getSigChecks(arm9Section, section[2].size, &sigOffset, &sigOffset2);
    *(u16 *)sigOffset = sigPatch[0];
    *(u16 *)sigOffset2 = sigPatch[0];
    *((u16 *)sigOffset2 + 1) = sigPatch[1];

    //Replace the FIRM loader with the injector
    injectLoader();

    //Write patched FIRM to SD if needed
    if(selectedFirm)
        if(!fileWrite(firm, patchedFirms[selectedFirm - 1], firmSize))
            error("Couldn't write the patched FIRM (no free space?)");
}

void launchFirm(void)
{
    if(console && mode) setKeyXs(arm9Section);

    //Copy firm partitions to respective memory locations
    memcpy(section[0].address, (u8 *)firm + section[0].offset, section[0].size);
    memcpy(section[1].address, (u8 *)firm + section[1].offset, section[1].size);
    memcpy(section[2].address, arm9Section, section[2].size);

    //Fixes N3DS 3D
    deinitScreens();

    //Set ARM11 kernel entrypoint
    *(vu32 *)0x1FFFFFF8 = (u32)firm->arm11Entry;

    //Final jump to arm9 kernel
    ((void (*)())firm->arm9Entry)();
}