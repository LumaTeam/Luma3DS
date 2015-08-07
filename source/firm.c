/*
*   firm.c
*       by Reisyukaku
*/

#include "firm.h"
#include "patches.h"
#include "memory.h"
#include "types.h"
#include "fs.h"
#include "emunand.h"

firmHeader *firmLocation = (firmHeader *)0x24000000;
const u32 firmSize = 0xF1000;
firmSectionHeader *section;
u32 emuOffset = 0;
u32 emuHeader = 0;

void loadFirm(void){
    //Read FIRM from SD card and write to FCRAM
    fileRead((u8*)firmLocation, "/rei/firmware.bin", firmSize);
    section = firmLocation->section;
}

void loadSys(void){
    memcpy((u8*)mpuCode, mpu, sizeof(mpu));
}

void loadEmu(void){
    fileRead((u8*)emuCode, "/rei/emunand/emunand.bin", 0);
    u32 *pos_offset = memsearch((u8*)emuCode, "NAND", 0x218, 4);
    u32 *pos_header = memsearch((u8*)emuCode, "NCSD", 0x218, 4);
    memcpy((void *)pos_offset, (void *)emuOffset, 4);
    memcpy((void *)pos_header, (void *)emuHeader, 4);

    //Add emunand hooks
    memcpy((u8*)mpuCode, mpu, sizeof(mpu));
    memcpy((u8*)emuHook2, eh2, sizeof(eh2));
    memcpy((u8*)emuHook3, eh3, sizeof(eh3));
    memcpy((u8*)emuHook4, eh4, sizeof(eh4));
}

void patchFirm(void){
    
    //Part1: Get Emunand
    if(getEmunand(&emuOffset, &emuHeader) == 1)
        loadEmu();
    else
        loadSys();
    
    //Part2: Disable signature checks
    memcpy((u8*)patch1, p1, sizeof(p1));
    memcpy((u8*)patch2, p2, sizeof(p2));
    
    //Part3: Create arm9 thread
    fileRead((u8*)threadCode, "/rei/thread/arm9.bin", 0);
    memcpy((u8*)threadHook1, th1, sizeof(th1));
    memcpy((u8*)threadHook2, th2, sizeof(th2));
}

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