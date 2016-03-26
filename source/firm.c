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
#define PATCH_VER 1

static firmHeader *const firmLocation = (firmHeader *)0x24000000;
static const firmSectionHeader *section;
static u8 *arm9Section;
static const char *patchedFirms[] = { "/aurei/patched_firmware_sys.bin",
                                     "/aurei/patched_firmware_emu.bin",
                                     "/aurei/patched_firmware_em2.bin",
                                     "/aurei/patched_firmware90.bin" };
static u32 firmSize,
           mode = 1,
           console = 1,
           emuNAND = 0,
           a9lhSetup = 0,
           usePatchedFirm = 0,
           selectedFirm = 0;

void setupCFW(void){

    //Determine if booting with A9LH
    u32 a9lhBoot = (PDN_SPI_CNT == 0x0) ? 1 : 0;
    //Retrieve the last booted FIRM
    u8 previousFirm = CFG_BOOTENV;
    //Detect the console being used
    if(PDN_MPCORE_CFG == 1) console = 0;
    //Get pressed buttons
    u16 pressed = HID_PAD;
    u32 updatedSys = 0;

    //Attempt to read the configuration file
    const char configPath[] = "aurei/config.bin";
    u16 config = 0;
    u32 needConfig = fileRead(&config, configPath, 2) ? 1 : 2;

    //Determine if A9LH is installed
    if(a9lhBoot || (config >> 2) & 0x1){
        if(pressed == SAFE_MODE)
            error("Using Safe Mode would brick you, or remove A9LH!");

        a9lhSetup = 1;
        //Check setting for > 9.2 sysNAND
        updatedSys = config & 0x1;
    }

    /* If booting with A9LH, it's a MCU reboot and a previous configuration exists,
       try to force boot options */
    if(a9lhBoot && previousFirm && needConfig == 1){
        //Always force a sysNAND boot when quitting AGB_FIRM
        if(previousFirm == 0x7){
            if(!updatedSys) mode = (config >> 8) & 0x1;
            needConfig = 0;
        //Else, force the last used boot options unless A is pressed
        } else if(!(pressed & BUTTON_A)){
            mode = (config >> 8) & 0x1;
            emuNAND = (config >> 9) & 0x1;
            needConfig = 0;
        }
    }

    if(needConfig){

        /* If L and one of the payload buttons are pressed, and if not using A9LH
           the Safe Mode combo is not pressed, chainload an external payload */
        if((pressed & BUTTON_L1) && (pressed & PAYLOAD_BUTTONS) &&
           pressed != SAFE_MODE) loadPayload();

        //If no configuration file exists or SELECT is held, load configuration menu
        if(needConfig == 2 || (pressed & BUTTON_SELECT))
            configureCFW(configPath);

        //If screens are inited, load splash screen
        if(PDN_GPU_CNT != 0x1) loadSplash();

        /* If L is pressed, boot 9.0 FIRM */
        if(pressed & BUTTON_L1) mode = 0;

        /* If L or R aren't pressed on a 9.0/9.2 sysNAND, or the 9.0 FIRM is selected
           or R is pressed on a > 9.2 sysNAND, boot emuNAND */
        if((updatedSys && (!mode || (pressed & BUTTON_R1))) ||
           (!updatedSys && mode && !(pressed & BUTTON_R1))){
            //If not 9.0 FIRM and B is pressed, attempt booting the second emuNAND
            if(mode && (pressed & BUTTON_B)) emuNAND = 2;
            else emuNAND = 1;
        }

        /* If tha FIRM patches version is different or user switched to/from A9LH,
           and "Use pre-patched FIRMs" is set, delete all patched FIRMs */
        u16 bootConfig = (PATCH_VER << 11) | (a9lhSetup << 10);
        if ((config >> 1) & 0x1 && bootConfig != (config & 0xFC00))
            deleteFirms(patchedFirms, sizeof(patchedFirms) / sizeof(char *));

        //We also need to remember the used boot mode on A9LH
        if(a9lhBoot) bootConfig |= (mode << 8) | (emuNAND << 9);

        //If the boot configuration is different from previously, overwrite it
        if(bootConfig != (config & 0xFF00)){
            //Preserve user settings (first byte)
            u16 tempConfig = ((config & 0xFF) | bootConfig);
            fileWrite(&tempConfig, configPath, 2);
        }
    }

    if(mode) selectedFirm = emuNAND ? (emuNAND == 1 ? 2 : 3) : 1;

    //Skip decrypting and patching FIRM
    if((config >> 1) & 0x1){
        //Only needed with this setting
        if(!mode) selectedFirm = 4;
        if(fileExists(patchedFirms[selectedFirm - 1])) usePatchedFirm = 1;
    }
}

//Load FIRM into FCRAM
void loadFirm(void){

    //If not using an A9LH setup or the patched FIRM, load 9.0 FIRM from NAND
    if(!usePatchedFirm && !a9lhSetup && !mode){
        //Read FIRM from NAND and write to FCRAM
        firmSize = console ? 0xF2000 : 0xE9000;
        nandFirm0((u8 *)firmLocation, firmSize, console);
        //Check for correct decryption
        if(memcmp(firmLocation, "FIRM", 4) != 0)
            error("Couldn't decrypt NAND FIRM0 (O3DS not on 9.x?)");
    }
    //Load FIRM from SD
    else{
        const char *path = usePatchedFirm ? patchedFirms[selectedFirm - 1] :
                                (mode ? "/aurei/firmware.bin" : "/aurei/firmware90.bin");
        firmSize = fileSize(path);
        if(!firmSize) error("aurei/firmware(90).bin doesn't exist");
        fileRead(firmLocation, path, firmSize);
    }

    section = firmLocation->section;

    //Check that the loaded FIRM matches the console
    if((((u32)section[2].address >> 8) & 0xFF) != (console ? 0x60 : 0x68))
        error("aurei/firmware(90).bin doesn't match this\nconsole, or it's encrypted");

    arm9Section = (u8 *)firmLocation + section[2].offset;

    if(console && !usePatchedFirm) decryptArm9Bin(arm9Section, mode);
}

//NAND redirection
static void loadEmu(u8 *proc9Offset){

    u32 emuOffset,
        emuHeader = 0,
        emuRead,
        emuWrite;

    //Look for emuNAND
    getEmunandSect(&emuOffset, &emuHeader, emuNAND);

    //No emuNAND detected
    if(!emuHeader) error("No emuNAND has been detected");

    //Copy emuNAND code
    void *emuCodeOffset = getEmuCode(arm9Section, section[2].size, proc9Offset);
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
    u32 branchOffset = (u32)emuCodeOffset - (u32)firmLocation -
                       section[2].offset + (u32)section[2].address;

    //Add emunand hooks
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

//Patches
void patchFirm(void){

    //Skip patching
    if(usePatchedFirm) return;

    if(mode || emuNAND){
        //Find the Process9 NCCH location
        u8 *proc9Offset = getProc9(arm9Section, section[2].size);

        //Apply emuNAND patches
        if(emuNAND) loadEmu(proc9Offset);

        //Patch FIRM reboots, not on 9.0 FIRM as it breaks firmlaunchhax
        if(mode){
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
            if(emuNAND){
                void *pos_path = memsearch(rebootOffset, L"sy", reboot_size, 4);
                memcpy(pos_path, emuNAND == 1 ? L"emu" : L"em2", 5);
            }
        }
    }

    if(a9lhSetup && !emuNAND){
        //Patch FIRM partitions writes on sysNAND to protect A9LH
        u16 *writeOffset = getFirmWrite(arm9Section, section[2].size);
        *writeOffset = writeBlock[0];
        *(writeOffset + 1) = writeBlock[1];
    }

    //Disable signature checks
    u32 sigOffset,
        sigOffset2;

    getSignatures(firmLocation, firmSize, &sigOffset, &sigOffset2);
    *(u16 *)sigOffset = sigPatch[0];
    *(u16 *)sigOffset2 = sigPatch[0];
    *((u16 *)sigOffset2 + 1) = sigPatch[1];

    //Patch ARM9 entrypoint on N3DS to skip arm9loader
    if(console)
        firmLocation->arm9Entry = (u8 *)0x801B01C;

    //Write patched FIRM to SD if needed
    if(selectedFirm)
        if(!fileWrite(firmLocation, patchedFirms[selectedFirm - 1], firmSize))
            error("Couldn't write the patched FIRM (no free space?)");
}

void launchFirm(void){

    if(console && mode) setKeyXs(arm9Section);

    //Copy firm partitions to respective memory locations
    memcpy(section[0].address, (u8 *)firmLocation + section[0].offset, section[0].size);
    memcpy(section[1].address, (u8 *)firmLocation + section[1].offset, section[1].size);
    memcpy(section[2].address, arm9Section, section[2].size);

    //Fixes N3DS 3D
    deinitScreens();

    //Set ARM11 kernel entrypoint
    *(vu32 *)0x1FFFFFF8 = (u32)firmLocation->arm11Entry;

    //Final jump to arm9 kernel
    ((void (*)())firmLocation->arm9Entry)();
}