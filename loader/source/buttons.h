#pragma once

#include "types.h"

#define HID_PAD       (*(vu32 *)0x10146000 ^ 0xFFF)
#define BUTTON_RIGHT  (1 << 4)
#define BUTTON_LEFT   (1 << 5)
#define BUTTON_UP     (1 << 6)
#define BUTTON_DOWN   (1 << 7)
#define BUTTON_X      (1 << 10)
#define BUTTON_Y      (1 << 11)
#define BUTTON_R1     (1 << 8)
#define BUTTON_SELECT (1 << 2)