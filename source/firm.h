/*
*   firm.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/
#ifndef FIRM_INC
#define FIRM_INC

#include "types.h"

#define PDN_MPCORE_CFG     (*(u8*)0x10140FFC)
#define HID_PAD            ((~*(u16*)0x10146000) & 0xFFF)
#define BUTTON_L1R1        (3 << 8)
#define BUTTON_L1          (1 << 9)
#define BUTTON_A           1
#define PATCHED_FIRM_PATH  "/rei/patched_firmware.bin"

u8 loadFirm(u8 a9lh);
u8 loadEmu(void);
u8 patchFirm(void);
void launchFirm(void);

#endif