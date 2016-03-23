/*
*   buttons.h
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

#define HID_PAD       ((~*(vu16 *)0x10146000) & 0xFFF)
#define BUTTON_R1     (1 << 8)
#define BUTTON_L1     (1 << 9)
#define BUTTON_L1R1   (BUTTON_R1 | BUTTON_L1)
#define BUTTON_A      1
#define BUTTON_B      (1 << 1)
#define BUTTON_UP     (1 << 6)
#define BUTTON_DOWN   (1 << 7)
#define BUTTON_START  (1 << 3)
#define BUTTON_SELECT (1 << 2)
#define SAFEMODE      (BUTTON_L1R1 | BUTTON_A | (1 << 6))