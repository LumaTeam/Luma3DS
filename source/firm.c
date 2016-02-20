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

firmHeader *firmLocation = (firmHeader *)0x24000000;
firmSectionHeader *section;
u32 firmSize = 0;
u8  mode = 1,
    console = 1,
    a9lhSetup = 0,
    updatedSys = 0;
u16 pressed;

//Load firm into FCRAM
u8 loadFirm(u8 a9lhBoot){

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

    section = firmLocation->section;

    /* If L is pressed, and on an updated SysNAND setup the SAFE MODE combo
       is not pressed, boot 9.0 FIRM */
    if((pressed & BUTTON_L1) && !(updatedSys && pressed == SAFEMODE)) mode = 0;

    //If not using an A9LH setup, do so by decrypting FIRM0
    if(!a9lhSetup && !mode){
        //Read FIRM from NAND and write to FCRAM
        firmSize = console ? 0xF2000 : 0xE9000;
        nandFirm0((u8*)firmLocation, firmSize, console);
        if(memcmp((u8*)firmLocation, "FIRM", 4) != 0) return 1;
    }
    //Load FIRM from SD
    else{
        char firmPath[] = "/rei/firmware.bin";
        char firmPath2[] = "/rei/firmware90.bin";
        char *pathPtr = mode ? firmPath : firmPath2;
        firmSize = fileSize(pathPtr);
        if (!firmSize) return 1;
        fileRead((u8*)firmLocation, pathPtr, firmSize);
    }
    if((((u32)section[2].address >> 8) & 0xFF) != (console ? 0x60 : 0x68)) return 1;

    if(console) arm9loader((u8*)firmLocation + section[2].offset, mode);

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

    /* If L or R aren't pressed on a 9.0/9.2 SysNAND, or the 9.0 FIRM is selected
       or R is pressed on a > 9.2 SysNAND, boot emuNAND */
    if((updatedSys && (!mode || ((pressed & BUTTON_R1) && pressed != SAFEMODE))) ||
       (!updatedSys && mode && !(pressed & (BUTTON_L1 | BUTTON_R1)))){
        if (loadEmu()) return 1;
    }
    else if(a9lhSetup){
        //Patch FIRM partitions writes on SysNAND to protect A9LH
        u32 writeOffset = 0;
        getFIRMWrite(firmLocation, firmSize, &writeOffset);
        memcpy((u8*)writeOffset, FIRMblock, sizeof(FIRMblock));
    }

    u32 sigOffset = 0,
        sigOffset2 = 0;

    //Disable signature checks
    getSignatures(firmLocation, firmSize, &sigOffset, &sigOffset2);
    memcpy((u8*)sigOffset, sigPat1, sizeof(sigPat1));
    memcpy((u8*)sigOffset2, sigPat2, sizeof(sigPat2));

    //Apply FIRM reboot patch. Not needed on N3DS
    if(!console && mode && pressed != SAFEMODE &&
       fileExists("/rei/reversereboot") == (pressed & BUTTON_A)){
        u32 rebootOffset = 0,
            rebootOffset2 = 0;

        //Read reboot code from SD and write patched FIRM path in memory
        char path[] = "/rei/reboot/reboot1.bin";
        u32 size = fileSize(path);
        if (!size) return 1;
        getReboot(firmLocation, firmSize, &rebootOffset, &rebootOffset2);
        fileRead((u8*)rebootOffset, path, size);
        memcpy((u8*)rebootOffset + size, L"sdmc:", 10);
        memcpy((u8*)rebootOffset + size + 10, L"" PATCHED_FIRM_PATH, sizeof(PATCHED_FIRM_PATH) * 2);
        path[18] = '2';
        size = fileSize(path);
        if (!size) return 1;
        fileRead((u8*)rebootOffset2, path, size);

        //Write patched FIRM to SD
        if (fileWrite((u8*)firmLocation, PATCHED_FIRM_PATH, firmSize) != 0) return 1;
    }

    return 0;
}

//Firmlaunchhax
void launchFirm(void){

    //Set MPU
    __asm__ (
        "msr cpsr_c, #0xDF\n\t"         //Set system mode, disable interrupts
        "ldr r0, =0x10000035\n\t"       //Memory area 0x10000000-0x18000000, enabled, 128MB
        "ldr r4, =0x18000035\n\t"       //Memory area 0x18000000-0x20000000, enabled, 128MB
        "mcr p15, 0, r0, c6, c3, 0\n\t" //Set memory area 3 (0x10000000-0x18000000)
        "mcr p15, 0, r4, c6, c4, 0\n\t" //Set memory area 4 (0x18000000-0x20000000)
        "mrc p15, 0, r0, c2, c0, 0\n\t" //read data cacheable bit
        "mrc p15, 0, r4, c2, c0, 1\n\t" //read inst cacheable bit
        "mrc p15, 0, r1, c3, c0, 0\n\t" //read data writeable
        "mrc p15, 0, r2, c5, c0, 2\n\t" //read data access permission
        "mrc p15, 0, r3, c5, c0, 3\n\t" //read inst access permission
        "orr r0, r0, #0x30\n\t"
        "orr r4, r4, #0x30\n\t"
        "orr r1, r1, #0x30\n\t"
        "bic r2, r2, #0xF0000\n\t"
        "bic r3, r3, #0xF0000\n\t"
        "orr r2, r2, #0x30000\n\t"
        "orr r3, r3, #0x30000\n\t"
        "mcr p15, 0, r0, c2, c0, 0\n\t" //write data cacheable bit
        "mcr p15, 0, r4, c2, c0, 1\n\t" //write inst cacheable bit
        "mcr p15, 0, r1, c3, c0, 0\n\t" //write data writeable
        "mcr p15, 0, r2, c5, c0, 2\n\t" //write data access permission
        "mcr p15, 0, r3, c5, c0, 3\n\t" //write inst access permission
        ::: "r0", "r1", "r2", "r3", "r4"
    );
    
    //Copy firm partitions to respective memory locations
    memcpy(section[0].address, (u8*)firmLocation + section[0].offset, section[0].size);
    memcpy(section[1].address, (u8*)firmLocation + section[1].offset, section[1].size);
    memcpy(section[2].address, (u8*)firmLocation + section[2].offset, section[2].size);
    *(u32 *)0x1FFFFFF8 = (u32)firmLocation->arm11Entry;
    
    //Final jump to arm9 binary
    console ? ((void (*)())0x801B01C)() : ((void (*)())firmLocation->arm9Entry)();
}