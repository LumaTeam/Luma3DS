/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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

#include <3ds/types.h>
#include <3ds/services/hid.h>
#include "MyThread.h"
#include "utils.h"

#define HID_PAD           (REG32(0x10146000) ^ 0xFFF)

#define DEFAULT_MENU_COMBO      (KEY_L | KEY_DDOWN | KEY_SELECT)
#define DIRECTIONAL_KEYS        (KEY_DOWN | KEY_UP | KEY_LEFT | KEY_RIGHT)

#define CORE_APPLICATION  0
#define CORE_SYSTEM       1

#define FLOAT_CONV_MULT 1e8 // for screen filters

typedef enum MenuItemAction {
    MENU_END = 0,
    METHOD = 1,
    MENU = 2,
} MenuItemAction;

typedef struct MenuItem {
    const char *title;

    MenuItemAction action_type;
    union {
        struct Menu *menu;
        void (*method)(void);
    };

    bool (*visibility)(void);
} MenuItem;

typedef struct Menu {
    const char *title;

    MenuItem items[16];
} Menu;

extern u32 menuCombo;
extern bool isHidInitialized;
extern u32 mcuFwVersion;

// From main.c
extern bool isN3DS;
extern bool menuShouldExit;
extern bool preTerminationRequested;
extern Handle preTerminationEvent;

u32 waitInputWithTimeout(s32 msec);
u32 waitInputWithTimeoutEx(u32 *outHeldKeys, s32 msec);
u32 waitInput(void);

u32 waitComboWithTimeout(s32 msec);
u32 waitCombo(void);

bool menuCheckN3ds(void);
u32 menuCountItems(const Menu *menu);

MyThread *menuCreateThread(void);
void menuEnter(void);
void menuLeave(void);
void menuThreadMain(void);
void menuShow(Menu *root);
void menuResetSelectedItem(void);
