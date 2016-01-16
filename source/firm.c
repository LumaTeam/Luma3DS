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

const firmHeader *firmLocation = (firmHeader *)0x24000000;
const u32 firmSize = 0xF3000;
firmSectionHeader *section;
u32 emuOffset = 0;
u32 emuHeader = 0;

//Load firm into FCRAM
void loadFirm(void){
    //Read FIRM from SD card and write to FCRAM
    fileRead((u8*)firmLocation, "/rei/firmware.bin", firmSize);
    section = firmLocation->section;
    arm9loader((u8*)firmLocation + section[2].offset);
}

//Nand redirection
void loadEmu(void){
    
    //Read emunand code from SD
    u32 code = emuCode();
    fileRead(code, "/rei/emunand/emunand.bin", 0);
    u32 *pos_offset = memsearch(code, "NAND", 0x218, 4);
    u32 *pos_header = memsearch(code, "NCSD", 0x218, 4);
    if (pos_offset && pos_header) {
        *pos_offset = emuOffset;
        *pos_header = emuHeader;
    }
    
    //Add emunand hooks
    memcpy((u8*)emuHook(1), nandRedir, sizeof(nandRedir));
    memcpy((u8*)emuHook(2), nandRedir, sizeof(nandRedir));
}

//Patches
void patchFirm(){
    
    //Part1: Set MPU for payload area
    memcpy((u8*)mpuCode(), mpu, sizeof(mpu));
    
    //Part2: Disable signature checks
    memcpy((u8*)sigPatch(1), sigPat1, sizeof(sigPat1));
    memcpy((u8*)sigPatch(2), sigPat2, sizeof(sigPat2));
    
    //Part3: Create arm9 thread
    fileRead((u8*)threadCode(), "/rei/thread/arm9.bin", 0);
    memcpy((u8*)threadHook(1), th1, sizeof(th1));
    memcpy((u8*)threadHook(2), th2, sizeof(th2));
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