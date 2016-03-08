/*
*   firm.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "firm.h"
#include "patches.h"
#include "memory.h"
#include "fs.h"
#include "emunand.h"
#include "crypto.h"
#include "draw.h"
#include "loader.h"

firmHeader *firmLocation = (firmHeader *)0x24000000;
firmSectionHeader *section;
u32 firmSize = 0,
    mode = 1,
    console = 1,
    emuNAND = 0,
    a9lhSetup = 0,
    updatedSys = 0,
    usePatchedFirm = 0;
u16 pressed;
char *firmPathPatched = NULL;

void setupCFW(void){

    //Determine if booting with A9LH via PDN_SPI_CNT
    u8 a9lhBoot = (*(u8 *)0x101401C0 == 0x0) ? 1 : 0;
    //Retrieve the last booted FIRM via CFG_BOOTENV
    u8 previousFirm = *(u8 *)0x10010000;
    u32 overrideConfig = 0;
    const char lastConfigPath[] = "rei/lastbootcfg";

    //Detect the console being used
    if(PDN_MPCORE_CFG == 1) console = 0;

    //Get pressed buttons
    pressed = HID_PAD;

    //Determine if A9LH is installed
    if(a9lhBoot || fileExists("/rei/installeda9lh")){
        a9lhSetup = 1;
        //Check flag for > 9.2 SysNAND
        if(fileExists("/rei/updatedsysnand")) updatedSys = 1;
    }

    //If booting with A9LH and it's a MCU reboot, try to force boot options
    if(a9lhBoot && previousFirm && fileExists(lastConfigPath)){
        u8 tempConfig;
        fileRead(&tempConfig, lastConfigPath, 1);

        //Always force a sysNAND boot when quitting AGB_FIRM
        if(previousFirm == 0x7) {
            if(!updatedSys) mode = tempConfig & 0x1;
            overrideConfig = 1;
        //Else, force the last used boot options unless A is pressed
        } else if(!(pressed & BUTTON_A)) {
            mode = tempConfig & 0x1;
            emuNAND = (tempConfig >> 1) & 0x1;
            overrideConfig = 1;
        }
    }

    if(!overrideConfig){

        //If L and R are pressed, chainload an external payload
        if(a9lhBoot && (pressed & BUTTON_L1R1) == BUTTON_L1R1) loadPayload();

        /* If L is pressed, and on an updated SysNAND setup the SAFE MODE combo
           is not pressed, boot 9.0 FIRM */
        if((pressed & BUTTON_L1) && !(updatedSys && pressed == SAFEMODE)) mode = 0;

        /* If L or R aren't pressed on a 9.0/9.2 SysNAND, or the 9.0 FIRM is selected
           or R is pressed on a > 9.2 SysNAND, boot emuNAND */
        if((updatedSys && (!mode || ((pressed & BUTTON_R1) && pressed != SAFEMODE))) ||
           (!updatedSys && mode && !(pressed & BUTTON_R1))) emuNAND = 1;

        //Write the current boot options on A9LH
        if(a9lhBoot){
            u8 tempConfig = (mode | (emuNAND << 1)) & 0x3;
            fileWrite(&tempConfig, lastConfigPath, 1);
        }
    }

    if(mode) firmPathPatched = emuNAND ? "/rei/patched_firmware_emu.bin" :
                                         "/rei/patched_firmware_sys.bin";

    //Skip decrypting and patching FIRM
    if(fileExists("/rei/usepatchedfw")){
        //Only needed with this flag
        if(!mode) firmPathPatched = "/rei/patched_firmware90.bin";
        if(fileExists(firmPathPatched)) usePatchedFirm = 1;
    }
}

//Load firm into FCRAM
u32 loadFirm(void){

    //If not using an A9LH setup or the patched FIRM, load 9.0 FIRM from NAND
    if(!usePatchedFirm && !a9lhSetup && !mode){
        //Read FIRM from NAND and write to FCRAM
        firmSize = console ? 0xF2000 : 0xE9000;
        nandFirm0((u8 *)firmLocation, firmSize, console);
        //Check for correct decryption
        if(memcmp(firmLocation, "FIRM", 4) != 0) return 0;
    }
    //Load FIRM from SD
    else{
        const char *path = usePatchedFirm ? firmPathPatched :
                                (mode ? "/rei/firmware.bin" : "/rei/firmware90.bin");
        firmSize = fileSize(path);
        if(!firmSize) return 0;
        fileRead((u8 *)firmLocation, path, firmSize);
    }

    section = firmLocation->section;

    //Check that the loaded FIRM matches the console
    if((((u32)section[2].address >> 8) & 0xFF) != (console ? 0x60 : 0x68)) return 0;

    if(console && !usePatchedFirm)
        decArm9Bin((u8 *)firmLocation + section[2].offset, mode);

    return 1;
}

//Nand redirection
u32 loadEmu(void){

    u32 emuOffset = 0,
        emuHeader = 0,
        emuRead = 0,
        emuWrite = 0,
        sdmmcOffset = 0,
        mpuOffset = 0,
        emuCodeOffset = 0;

    //Read emunand code from SD
    const char path[] = "/rei/emunand/emunand.bin";
    u32 size = fileSize(path);
    if(!size) return 0;
    if(!console || !mode) nandRedir[5] = 0xA4;
    //Find offset for emuNAND code from the offset in nandRedir
    emuCodeOffset = *(u32 *)(nandRedir + 4) - (u32)section[2].address +
                    section[2].offset + (u32)firmLocation;
    fileRead((u8 *)emuCodeOffset, path, size);

    //Find and patch emunand related offsets
    u32 *pos_sdmmc = (u32 *)memsearch((void *)emuCodeOffset, "SDMC", size, 4);
    u32 *pos_offset = (u32 *)memsearch((void *)emuCodeOffset, "NAND", size, 4);
    u32 *pos_header = (u32 *)memsearch((void *)emuCodeOffset, "NCSD", size, 4);
    getSDMMC(firmLocation, &sdmmcOffset, firmSize);
    getEmunandSect(&emuOffset, &emuHeader);
    getEmuRW(firmLocation, firmSize, &emuRead, &emuWrite);
    getMPU(firmLocation, &mpuOffset, firmSize);
    *pos_sdmmc = sdmmcOffset;
    *pos_offset = emuOffset;
    *pos_header = emuHeader;

    //Patch emuNAND code in memory for O3DS and 9.0 N3DS
    if(!console || !mode){
        void *pos_instr = memsearch((void *)emuCodeOffset, "\xA6\x01\x08\x30", size, 4);
        memcpy(pos_instr, emuInstr, sizeof(emuInstr));
    }

    //Add emunand hooks
    memcpy((void *)emuRead, nandRedir, sizeof(nandRedir));
    memcpy((void *)emuWrite, nandRedir, sizeof(nandRedir));

    //Set MPU for emu code region
    memcpy((void *)mpuOffset, mpu, sizeof(mpu));

    return 1;
}

//Patches
u32 patchFirm(void){

    //Skip patching
    if(usePatchedFirm) return 1;

    //Apply emuNAND patches
    if(emuNAND){
        if(!loadEmu()) return 0;
    }
    else if(a9lhSetup){
        //Patch FIRM partitions writes on SysNAND to protect A9LH
        u32 writeOffset = 0;
        getFIRMWrite(firmLocation, firmSize, &writeOffset);
        memcpy((void *)writeOffset, FIRMblock, sizeof(FIRMblock));
    }

    //Disable signature checks
    u32 sigOffset = 0,
        sigOffset2 = 0;

    getSignatures(firmLocation, firmSize, &sigOffset, &sigOffset2);
    memcpy((void *)sigOffset, sigPat1, sizeof(sigPat1));
    memcpy((void *)sigOffset2, sigPat2, sizeof(sigPat2));

    //Patch ARM9 entrypoint on N3DS to skip arm9loader
    if(console){
        u32 *arm9 = (u32 *)&firmLocation->arm9Entry;
        *arm9 = 0x801B01C;
    }

    //Patch FIRM reboots, not on 9.0 FIRM as it breaks firmlaunchhax
    if(mode){
        u32 rebootOffset = 0,
            fOpenOffset = 0;

        //Read reboot code from SD
        const char path[] = "/rei/reboot/reboot.bin";
        u32 size = fileSize(path);
        if(!size) return 0;
        getReboot(firmLocation, firmSize, &rebootOffset);
        fileRead((u8 *)rebootOffset, path, size);

        //Calculate the fOpen offset and put it in the right location
        u32 *pos_fopen = (u32 *)memsearch((void *)rebootOffset, "OPEN", size, 4);
        getfOpen(firmLocation, firmSize, &fOpenOffset);
        *pos_fopen = fOpenOffset;

        //Patch path for emuNAND-patched FIRM
        if(emuNAND){
            void *pos_path = memsearch((void *)rebootOffset, L"sy", size, 4);
            memcpy(pos_path, L"emu", 5);
        }
    }

    //Write patched FIRM to SD if needed
    if(firmPathPatched)
        if(!fileWrite((u8 *)firmLocation, firmPathPatched, firmSize)) return 0;

    return 1;
}

void launchFirm(void){

    if(console && mode) setKeyXs((u8 *)firmLocation + section[2].offset);

    //Copy firm partitions to respective memory locations
    memcpy(section[0].address, (u8 *)firmLocation + section[0].offset, section[0].size);
    memcpy(section[1].address, (u8 *)firmLocation + section[1].offset, section[1].size);
    memcpy(section[2].address, (u8 *)firmLocation + section[2].offset, section[2].size);

    //Run ARM11 screen stuff
    vu32 *arm11 = (vu32 *)0x1FFFFFF8;
    *arm11 = (u32)shutdownLCD;
    while(*arm11);
    
    //Set ARM11 kernel
    *arm11 = (u32)firmLocation->arm11Entry;
    
    //Final jump to arm9 binary
    ((void (*)())firmLocation->arm9Entry)();
}