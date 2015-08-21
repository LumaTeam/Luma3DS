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
const u32 firmSize = 0xF1000;
firmSectionHeader *section;
u32 emuOffset = 0;
u32 emuHeader = 0;
u32 kversion = 0;

//Load firm into FCRAM
void loadFirm(int mode){
    //Sysnand mode
    if(mode == 0 || getEmunand(&emuOffset, &emuHeader) == 0){
        //Read FIRM from NAND and write to FCRAM
        nandFirm0((u8*)firmLocation, firmSize);
        section = firmLocation->section;
        kversion = 0x04; //TODO: make this not hard coded
        decryptArm9Bin((u8*)firmLocation + section[2].offset, kversion);
    }
    //Emunand mode
    else{
        //Read FIRM from SD card and write to FCRAM
        fileRead((u8*)firmLocation, "/rei/firmware.bin", firmSize);
        section = firmLocation->section;
        kversion = 0x0F; //TODO: make this not hard coded
        loadEmu();
    }
}

//Nand redirection
void loadEmu(void){
    
    //Read emunand code from SD
    u32 code = emuCode();
    fileRead((u8*)code, "/rei/emunand/emunand.bin", 0);
    u32 *pos_offset = memsearch((u8*)code, "NAND", 0x218, 4);
    u32 *pos_header = memsearch((u8*)code, "NCSD", 0x218, 4);
    memcpy((void *)pos_offset, (void *)emuOffset, 4);
    memcpy((void *)pos_header, (void *)emuHeader, 4);

    //Add emunand hooks
    memcpy((u8*)emuHook(1), eh1, sizeof(eh1));
    memcpy((u8*)emuHook(2), eh2, sizeof(eh2));
    memcpy((u8*)emuHook(3), eh3, sizeof(eh3));
}

//Patches
void patchFirm(){
    
    //Part1: Set MPU for payload area
    memcpy((u8*)mpuCode(kversion), mpu, sizeof(mpu));
    
    //Part2: Disable signature checks
    memcpy((u8*)sigPatch(1, kversion), p1, sizeof(p1));
    memcpy((u8*)sigPatch(2, kversion), p2, sizeof(p2));
    
    //Part3: Create arm9 thread
    fileRead((u8*)threadCode(kversion), "/rei/thread/arm9.bin", 0);
    if(kversion == 0x0F){ //TODO: 0F only untill i can figure out why the hell this doesnt work on sysnand anymore. 
        memcpy((u8*)threadHook(1, kversion), th1, sizeof(th1));
        memcpy((u8*)threadHook(2, kversion), th2, sizeof(th2));
    }
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
    ((void (*)())0x801B01C)();
}