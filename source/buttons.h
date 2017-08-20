/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#pragma once

#include "types.h"

#define HID_PAD                (*(vu32 *)0x10146000 ^ 0xFFF)

#define BUTTON_R1              (1 << 8)
#define BUTTON_L1              (1 << 9)
#define BUTTON_A               (1 << 0)
#define BUTTON_B               (1 << 1)
#define BUTTON_X               (1 << 10)
#define BUTTON_Y               (1 << 11)
#define BUTTON_SELECT          (1 << 2)
#define BUTTON_START           (1 << 3)
#define BUTTON_RIGHT           (1 << 4)
#define BUTTON_LEFT            (1 << 5)
#define BUTTON_UP              (1 << 6)
#define BUTTON_DOWN            (1 << 7)

#define DPAD_BUTTONS           (BUTTON_LEFT | BUTTON_RIGHT | BUTTON_UP | BUTTON_DOWN)
#define SAFE_MODE              (BUTTON_R1 | BUTTON_L1 | BUTTON_A | BUTTON_UP)
#define SINGLE_PAYLOAD_BUTTONS (BUTTON_B | BUTTON_X | BUTTON_Y)
#define L_PAYLOAD_BUTTONS      (BUTTON_R1 | BUTTON_A | BUTTON_START | BUTTON_SELECT)
#define MENU_BUTTONS           (DPAD_BUTTONS | BUTTON_A | BUTTON_START)
#define PIN_BUTTONS            (BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Y | DPAD_BUTTONS | BUTTON_START | BUTTON_SELECT)
#define NTRBOOT_BUTTONS        (BUTTON_START | BUTTON_SELECT | BUTTON_X)
