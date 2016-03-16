/*
*   firm.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#pragma once

#include "types.h"

#define PDN_MPCORE_CFG     (*(vu8 *)0x10140FFC)
#define HID_PAD            ((~*(vu16 *)0x10146000) & 0xFFF)
#define PDN_SPI_CNT        (*(vu8 *)0x101401C0)
#define CFG_BOOTENV        (*(vu8 *)0x10010000)
#define PDN_GPU_CNT        (*(vu8 *)0x10141200)
#define BUTTON_R1          (1 << 8)
#define BUTTON_L1          (1 << 9)
#define BUTTON_L1R1        (BUTTON_R1 | BUTTON_L1)
#define BUTTON_A           1
#define BUTTON_B           (1 << 1)
#define SAFEMODE           (BUTTON_L1R1 | BUTTON_A | (1 << 6))

//FIRM Header layout
typedef struct firmSectionHeader {
    u32 offset;
    u8 *address;
    u32 size;
    u32 procType;
    u8 hash[0x20];
} firmSectionHeader;

typedef struct firmHeader {
    u32 magic;
    u32 reserved1;
    u8 *arm11Entry;
    u8 *arm9Entry;
    u8 reserved2[0x30];
    firmSectionHeader section[4];
} firmHeader;

void setupCFW(void);
u32 loadFirm(void);
u32 patchFirm(void);
void launchFirm(void);