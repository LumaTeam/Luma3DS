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

firmHeader *firmLocation = (firmHeader *)0x24000000;
firmSectionHeader *section;
u32 firmSize = 0;
u8  mode = 1,
    console = 1,
    emuNAND = 0,
    a9lhSetup = 0,
    updatedSys = 0,
    usePatchedFirm = 0;
u16 pressed;
char *firmPathPatched = NULL;

void setupCFW(void){

    //Detect the console being used
    if(PDN_MPCORE_CFG == 1) console = 0;

    //Get pressed buttons
    pressed = HID_PAD;

    //Determine if A9LH is installed via PDN_SPI_CNT and an user flag
    if((*((u8*)0x101401C0) == 0x0) || fileExists("/rei/installeda9lh")){
        a9lhSetup = 1;
        //Check flag for > 9.2 SysNAND
        if(fileExists("/rei/updatedsysnand")) updatedSys = 1;
    }

    /* If L is pressed, and on an updated SysNAND setup the SAFE MODE combo
       is not pressed, boot 9.0 FIRM */
    if((pressed & BUTTON_L1) && !(updatedSys && pressed == SAFEMODE)) mode = 0;

    /* If L or R aren't pressed on a 9.0/9.2 SysNAND, or the 9.0 FIRM is selected
       or R is pressed on a > 9.2 SysNAND, boot emuNAND */
    if((updatedSys && (!mode || ((pressed & BUTTON_R1) && pressed != SAFEMODE))) ||
       (!updatedSys && mode && !(pressed & BUTTON_R1))) emuNAND = 1;

    if(mode) firmPathPatched = emuNAND ? "/rei/patched_firmware_emu.bin" :
                                         "/rei/patched_firmware_sys.bin";

    //Skip decrypting and patching FIRM
    if(fileExists("/rei/usepatchedfw")){
        //Only needed with this flag
        if(!mode) firmPathPatched = "/rei/patched_firmware90.bin";
        if (fileExists(firmPathPatched)) usePatchedFirm = 1;
    }
}

//Load firm into FCRAM
u8 loadFirm(void){

    //If not using an A9LH setup or the patched FIRM, load 9.0 FIRM from NAND
    if(!usePatchedFirm && !a9lhSetup && !mode){
        //Read FIRM from NAND and write to FCRAM
        firmSize = console ? 0xF2000 : 0xE9000;
        nandFirm0((u8*)firmLocation, firmSize, console);
        //Check for correct decryption
        if(memcmp((u8*)firmLocation, "FIRM", 4) != 0) return 1;
    }
    //Load FIRM from SD
    else{
        char *path = usePatchedFirm ? firmPathPatched :
                                      (mode ? "/rei/firmware.bin" : "/rei/firmware90.bin");
        firmSize = fileSize(path);
        if (!firmSize) return 1;
        fileRead((u8*)firmLocation, path, firmSize);
    }

    section = firmLocation->section;

    //Check that the loaded FIRM matches the console
    if((((u32)section[2].address >> 8) & 0xFF) != (console ? 0x60 : 0x68)) return 1;

    if(console && !usePatchedFirm)
        decArm9Bin((u8*)firmLocation + section[2].offset, mode);

    return 0;
}

//Nand redirection
u8 loadEmu(void){

    u32 emuOffset = 0,
        emuHeader = 0,
        emuRead = 0,
        emuWrite = 0,
        sdmmcOffset = 0,
        mpuOffset = 0,
        emuCodeOffset = 0;

    //Read emunand code from SD
    char path[] = "/rei/emunand/emunand.bin";
    u32 size = fileSize(path);
    if (!size) return 1;
    if(!console || !mode) nandRedir[5] = 0xA4;
    //Find offset for emuNAND code from the offset in nandRedir
    u8 *emuCodeTmp = &nandRedir[4];
    emuCodeOffset = *(u32*)emuCodeTmp - (u32)section[2].address +
                    section[2].offset + (u32)firmLocation;
    fileRead((u8*)emuCodeOffset, path, size);

    //Find and patch emunand related offsets
    u32 *pos_sdmmc = memsearch((u32*)emuCodeOffset, "SDMC", size, 4);
    u32 *pos_offset = memsearch((u32*)emuCodeOffset, "NAND", size, 4);
    u32 *pos_header = memsearch((u32*)emuCodeOffset, "NCSD", size, 4);
    getSDMMC(firmLocation, &sdmmcOffset, firmSize);
    getEmunandSect(&emuOffset, &emuHeader);
    getEmuRW(firmLocation, firmSize, &emuRead, &emuWrite);
    getMPU(firmLocation, &mpuOffset, firmSize);
    *pos_sdmmc = sdmmcOffset;
    *pos_offset = emuOffset;
    *pos_header = emuHeader;

    //Patch emuNAND code in memory for O3DS and 9.0 N3DS
    if(!console || !mode){
        u32 *pos_instr = memsearch((u32*)emuCodeOffset, "\xA6\x01\x08\x30", size, 4);
        memcpy((u8*)pos_instr, emuInstr, sizeof(emuInstr));
    }

    //Add emunand hooks
    memcpy((u8*)emuRead, nandRedir, sizeof(nandRedir));
    memcpy((u8*)emuWrite, nandRedir, sizeof(nandRedir));

    //Set MPU for emu code region
    memcpy((u8*)mpuOffset, mpu, sizeof(mpu));

    return 0;
}

//Patches
u8 patchFirm(void){
    if(usePatchedFirm) return 0;

    if(emuNAND){
        if (loadEmu()) return 1;
    }
    else if (a9lhSetup){
        //Patch FIRM partitions writes on SysNAND to protect A9LH
        u32 writeOffset = 0;
        getFIRMWrite(firmLocation, firmSize, &writeOffset);
        memcpy((u8*)writeOffset, FIRMblock, sizeof(FIRMblock));
    }

    //Disable signature checks
    u32 sigOffset = 0,
        sigOffset2 = 0;

    getSignatures(firmLocation, firmSize, &sigOffset, &sigOffset2);
    memcpy((u8*)sigOffset, sigPat1, sizeof(sigPat1));
    memcpy((u8*)sigOffset2, sigPat2, sizeof(sigPat2));

    //Patch ARM9 entrypoint on N3DS to skip arm9loader
    if(console){
        u32 *arm9 = (u32*)&firmLocation->arm9Entry;
        *arm9 = 0x801B01C;
    }

    if(mode){
        //Patch FIRM reboots, not on 9.0 FIRM as it breaks firmlaunchhax
        u32 rebootOffset = 0,
            fOpenOffset = 0;

        //Read reboot code from SD
        char *path = "/rei/reboot/reboot.bin";
        u32 size = fileSize(path);
        if (!size) return 1;
        getReboot(firmLocation, firmSize, &rebootOffset);
        fileRead((u8*)rebootOffset, path, size);

        //Calculate the fOpen offset and put it in the right location
        u32 *pos_fopen = memsearch((u32*)rebootOffset, "OPEN", size, 4);
        getfOpen(firmLocation, firmSize, &fOpenOffset);
        *pos_fopen = fOpenOffset;

        //Patch path for emuNAND-patched FIRM
        if(emuNAND){
            u32 *pos_path = memsearch((u32*)rebootOffset, L"sys", size, 6);
            memcpy((u8*)pos_path, L"emu", 6);
        }
    }

    //Write patched FIRM to SD if needed
    if(firmPathPatched)
        if(fileWrite((u8*)firmLocation, firmPathPatched, firmSize) != 0) return 1;

    return 0;
}

void launchFirm(void){

    if(console && mode) setKeyXs((u8*)firmLocation + section[2].offset);

    //Copy firm partitions to respective memory locations
    memcpy(section[0].address, (u8*)firmLocation + section[0].offset, section[0].size);
    memcpy(section[1].address, (u8*)firmLocation + section[1].offset, section[1].size);
    memcpy(section[2].address, (u8*)firmLocation + section[2].offset, section[2].size);

    //Run ARM11 screen stuff
    vu32 *arm11 = (vu32*)0x1FFFFFF8;
    *arm11 = (u32)shutdownLCD;
    while (*arm11);
    
    //Set ARM11 kernel
    *arm11 = (u32)firmLocation->arm11Entry;
    
    //Final jump to arm9 binary
    ((void (*)())firmLocation->arm9Entry)();
}