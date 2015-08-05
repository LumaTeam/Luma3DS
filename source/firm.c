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

void loadSplash(void){
    fileRead((u8 *)0x20047000, "/rei/splash.bin", 0x46500); //ghetto because address varies i think
    while(((~*(unsigned *)0x10146000) & 0xFFF) == (1 << 3) ? 0 : 1); //Press start to boot
}

void loadFirm(void){
    //Read FIRM from SD card and write to FCRAM
    fileRead((u8*)firmLocation, "/rei/firmware.bin", firmSize);
    section = firmLocation->section;
}

void patchFirm(void){
    //Part1: Add emunand parsing code
    u32 offset = 0;
    u32 header = 0;
    if(getEmunand(&offset, &header) == 1){
        fileRead((u8*)emuCode, "/rei/emunand/emunand.bin", 0);
        u32 *pos_offset = memsearch((u8*)emuCode, "NAND", 0x218, 4);
        u32 *pos_header = memsearch((u8*)emuCode, "NCSD", 0x218, 4);
        memcpy((void *)pos_offset, (void *)offset, 4);
        memcpy((void *)pos_header, (void *)header, 4);
    }
    //Part2: Add emunand hooks
    memcpy((u8*)emuHook1, eh1, sizeof(eh1));
    memcpy((u8*)emuHook2, eh2, sizeof(eh2));
    memcpy((u8*)emuHook3, eh3, sizeof(eh3));
    memcpy((u8*)emuHook4, eh4, sizeof(eh4));
    
    //Part3: Disable signature checks
    memcpy((u8*)patch1, p1, sizeof(p1));
    memcpy((u8*)patch2, p2, sizeof(p2));
    
    //Part4: Create arm9 thread
    fileRead((u8*)threadCode, "/rei/thread/arm9.bin", 0);
    memcpy((u8*)threadHook1, th1, sizeof(th1));
    memcpy((u8*)threadHook2, th2, sizeof(th2));
}

void launchFirm(void){
    //Set MPU
        __asm__ (
        "msr cpsr_c, #0xDF\n\t"
        "ldr r0, =0x10000035\n\t"
        "ldr r4, =0x18000035\n\t"
        "mcr p15, 0, r0, c6, c3, 0\n\t"
        "mcr p15, 0, r4, c6, c4, 0\n\t"
        "mrc p15, 0, r0, c2, c0, 0\n\t"
        "mrc p15, 0, r4, c2, c0, 1\n\t"
        "mrc p15, 0, r1, c3, c0, 0\n\t"
        "mrc p15, 0, r2, c5, c0, 2\n\t"
        "mrc p15, 0, r3, c5, c0, 3\n\t"
        "orr r0, r0, #0x30\n\t"
        "orr r4, r4, #0x30\n\t"
        "orr r1, r1, #0x30\n\t"
        "bic r2, r2, #0xF0000\n\t"
        "bic r3, r3, #0xF0000\n\t"
        "orr r2, r2, #0x30000\n\t"
        "orr r3, r3, #0x30000\n\t"
        "mcr p15, 0, r0, c2, c0, 0\n\t"
        "mcr p15, 0, r4, c2, c0, 1\n\t"
        "mcr p15, 0, r1, c3, c0, 0\n\t"
        "mcr p15, 0, r2, c5, c0, 2\n\t"
        "mcr p15, 0, r3, c5, c0, 3\n\t"
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