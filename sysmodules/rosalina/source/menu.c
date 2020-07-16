/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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

#include <3ds.h>
#include "menu.h"
#include "draw.h"
#include "fmt.h"
#include "memory.h"
#include "ifile.h"
#include "menus.h"
#include "utils.h"
#include "menus/n3ds.h"
#include "menus/cheats.h"
#include "minisoc.h"

bool isHidInitialized = false;

// libctru redefinition:

bool hidShouldUseIrrst(void)
{
    // ir:rst exposes only two sessions :(
    return false;
}

static inline u32 convertHidKeys(u32 keys)
{
    // Nothing to do yet
    return keys;
}

u32 waitInputWithTimeout(s32 msec)
{
    s32 n = 0;
    u32 keys;

    do
    {
        svcSleepThread(1 * 1000 * 1000LL);
        Draw_Lock();
        if (!isHidInitialized || menuShouldExit)
        {
            keys = 0;
            Draw_Unlock();
            break;
        }
        n++;

        hidScanInput();
        keys = convertHidKeys(hidKeysDown()) | (convertHidKeys(hidKeysDownRepeat()) & DIRECTIONAL_KEYS);
        Draw_Unlock();
    } while (keys == 0 && !menuShouldExit && isHidInitialized && (msec < 0 || n < msec));


    return keys;
}

u32 waitInput(void)
{
    return waitInputWithTimeout(-1);
}

static u32 scanHeldKeys(void)
{
    u32 keys;

    Draw_Lock();

    if (!isHidInitialized || menuShouldExit)
        keys = 0;
    else
    {
        hidScanInput();
        keys = convertHidKeys(hidKeysHeld());
    }

    Draw_Unlock();
    return keys;
}

u32 waitComboWithTimeout(s32 msec)
{
    s32 n = 0;
    u32 keys = 0;
    u32 tempKeys = 0;

    // Wait for nothing to be pressed
    while (scanHeldKeys() != 0 && !menuShouldExit && isHidInitialized && (msec < 0 || n < msec))
    {
        svcSleepThread(1 * 1000 * 1000LL);
        n++;
    }

    if (menuShouldExit || !isHidInitialized || !(msec < 0 || n < msec))
        return 0;

    do
    {
        svcSleepThread(1 * 1000 * 1000LL);
        n++;

        tempKeys = scanHeldKeys();

        for (u32 i = 0x10000; i > 0; i--)
        {
            if (tempKeys != scanHeldKeys()) break;
            if (i == 1) keys = tempKeys;
        }
    }
    while((keys == 0 || scanHeldKeys() != 0) && !menuShouldExit && isHidInitialized && (msec < 0 || n < msec));

    return keys;
}

u32 waitCombo(void)
{
    return waitComboWithTimeout(-1);
}

static MyThread menuThread;
static u8 ALIGN(8) menuThreadStack[0x1000];
static u8 batteryLevel = 255;

static inline u32 menuAdvanceCursor(u32 pos, u32 numItems, s32 displ)
{
    return (pos + numItems + displ) % numItems;
}

static inline bool menuItemIsHidden(const MenuItem *item)
{
    return item->visibility != NULL && !item->visibility();
}

bool menuCheckN3ds(void)
{
    return isN3DS;
}

u32 menuCountItems(const Menu *menu)
{
    u32 n;
    for (n = 0; menu->items[n].action_type != MENU_END; n++);
    return n;
}

MyThread *menuCreateThread(void)
{
    if(R_FAILED(MyThread_Create(&menuThread, menuThreadMain, menuThreadStack, 0x1000, 52, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &menuThread;
}

u32 menuCombo;

void menuThreadMain(void)
{
    if(isN3DS)
        N3DSMenu_UpdateStatus();

    while (!isServiceUsable("ac:u") || !isServiceUsable("hid:USER"))
        svcSleepThread(500 * 1000 * 1000LL);

    hidInit(); // assume this doesn't fail
    isHidInitialized = true;

    while(!preTerminationRequested)
    {
        svcSleepThread(50 * 1000 * 1000LL);
        if (menuShouldExit)
            continue;

        Cheat_ApplyCheats();

        if((scanHeldKeys() & menuCombo) == menuCombo)
        {
            menuEnter();
            if(isN3DS) N3DSMenu_UpdateStatus();
            menuShow(&rosalinaMenu);
            menuLeave();
        }
    }
}

static s32 menuRefCount = 0;
void menuEnter(void)
{
    Draw_Lock();
    if(!menuShouldExit && menuRefCount == 0)
    {
        menuRefCount++;
        svcKernelSetState(0x10000, 2 | 1);
        svcSleepThread(5 * 1000 * 100LL);
        if (R_FAILED(Draw_AllocateFramebufferCache(FB_BOTTOM_SIZE)))
        {
            // Oops
            menuRefCount = 0;
            svcKernelSetState(0x10000, 2 | 1);
            svcSleepThread(5 * 1000 * 100LL);
        }
        else
            Draw_SetupFramebuffer();
    }
    Draw_Unlock();
}

void menuLeave(void)
{
    svcSleepThread(50 * 1000 * 1000);

    Draw_Lock();
    if(--menuRefCount == 0)
    {
        Draw_RestoreFramebuffer();
        Draw_FreeFramebufferCache();
        svcKernelSetState(0x10000, 2 | 1);
    }
    Draw_Unlock();
}

static void menuDraw(Menu *menu, u32 selected)
{
    char versionString[16];
    s64 out;
    u32 version, commitHash;
    bool isRelease;
    bool isMcuHwcRegistered;

    if(R_SUCCEEDED(srvIsServiceRegistered(&isMcuHwcRegistered, "mcu::HWC")) && isMcuHwcRegistered && R_SUCCEEDED(mcuHwcInit()))
    {
        if(R_FAILED(MCUHWC_GetBatteryLevel(&batteryLevel)))
            batteryLevel = 255;
        mcuHwcExit();
    }
    else
        batteryLevel = 255;

    svcGetSystemInfo(&out, 0x10000, 0);
    version = (u32)out;

    svcGetSystemInfo(&out, 0x10000, 1);
    commitHash = (u32)out;

    svcGetSystemInfo(&out, 0x10000, 0x200);
    isRelease = (bool)out;

    if(GET_VERSION_REVISION(version) == 0)
        sprintf(versionString, "v%lu.%lu", GET_VERSION_MAJOR(version), GET_VERSION_MINOR(version));
    else
        sprintf(versionString, "v%lu.%lu.%lu", GET_VERSION_MAJOR(version), GET_VERSION_MINOR(version), GET_VERSION_REVISION(version));

    Draw_DrawString(10, 10, COLOR_TITLE, menu->title);
    u32 numItems = menuCountItems(menu);
    u32 dispY = 0;

    for(u32 i = 0; i < numItems; i++)
    {
        if (menuItemIsHidden(&menu->items[i]))
            continue;

        Draw_DrawString(30, 30 + dispY, COLOR_WHITE, menu->items[i].title);
        Draw_DrawCharacter(10, 30 + dispY, COLOR_TITLE, i == selected ? '>' : ' ');
        dispY += SPACING_Y;
    }

    if(miniSocEnabled)
    {
        char ipBuffer[17];
        u32 ip = socGethostid();
        u8 *addr = (u8 *)&ip;
        int n = sprintf(ipBuffer, "%hhu.%hhu.%hhu.%hhu", addr[0], addr[1], addr[2], addr[3]);
        Draw_DrawString(SCREEN_BOT_WIDTH - 10 - SPACING_X * n, 10, COLOR_WHITE, ipBuffer);
    }

    Draw_DrawFormattedString(SCREEN_BOT_WIDTH - 10 - 4 * SPACING_X, SCREEN_BOT_HEIGHT - 20, COLOR_WHITE, "    ");

    if(batteryLevel != 255)
        Draw_DrawFormattedString(SCREEN_BOT_WIDTH - 10 - 4 * SPACING_X, SCREEN_BOT_HEIGHT - 20, COLOR_WHITE, "%02hhu%%", batteryLevel);
    else
        Draw_DrawString(SCREEN_BOT_WIDTH - 10 - 4 * SPACING_X, SCREEN_BOT_HEIGHT - 20, COLOR_WHITE, "    ");

    if(isRelease)
        Draw_DrawFormattedString(10, SCREEN_BOT_HEIGHT - 20, COLOR_TITLE, "Luma3DS %s", versionString);
    else
        Draw_DrawFormattedString(10, SCREEN_BOT_HEIGHT - 20, COLOR_TITLE, "Luma3DS %s-%08lx", versionString, commitHash);

    Draw_FlushFramebuffer();
}

void menuShow(Menu *root)
{
    u32 selectedItem = 0;
    Menu *currentMenu = root;
    u32 nbPreviousMenus = 0;
    Menu *previousMenus[0x80];
    u32 previousSelectedItems[0x80];

    u32 numItems = menuCountItems(currentMenu);
    if (menuItemIsHidden(&currentMenu->items[selectedItem]))
        selectedItem = menuAdvanceCursor(selectedItem, numItems, 1);

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    hidSetRepeatParameters(0, 0);
    menuDraw(currentMenu, selectedItem);
    Draw_Unlock();

    bool menuComboReleased = false;

    do
    {
        u32 pressed = waitInputWithTimeout(1000);
        numItems = menuCountItems(currentMenu);

        if(!menuComboReleased && (scanHeldKeys() & menuCombo) != menuCombo)
        {
            menuComboReleased = true;
            Draw_Lock();
            hidSetRepeatParameters(200, 100);
            Draw_Unlock();
        }

        if(pressed & KEY_A)
        {
            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();
            Draw_Unlock();

            switch(currentMenu->items[selectedItem].action_type)
            {
                case METHOD:
                    if(currentMenu->items[selectedItem].method != NULL)
                        currentMenu->items[selectedItem].method();
                    break;
                case MENU:
                    previousSelectedItems[nbPreviousMenus] = selectedItem;
                    previousMenus[nbPreviousMenus++] = currentMenu;
                    currentMenu = currentMenu->items[selectedItem].menu;
                    selectedItem = 0;
                    break;
                default:
                    __builtin_trap(); // oops
                    break;
            }

            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();
            Draw_Unlock();
        }
        else if(pressed & KEY_B)
        {
            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();
            Draw_Unlock();

            if(nbPreviousMenus > 0)
            {
                currentMenu = previousMenus[--nbPreviousMenus];
                selectedItem = previousSelectedItems[nbPreviousMenus];
            }
            else
                break;
        }
        else if(pressed & KEY_DOWN)
        {
            selectedItem = menuAdvanceCursor(selectedItem, numItems, 1);
            if (menuItemIsHidden(&currentMenu->items[selectedItem]))
                selectedItem = menuAdvanceCursor(selectedItem, numItems, 1);
        }
        else if(pressed & KEY_UP)
        {
            selectedItem = menuAdvanceCursor(selectedItem, numItems, -1);
            if (menuItemIsHidden(&currentMenu->items[selectedItem]))
                selectedItem = menuAdvanceCursor(selectedItem, numItems, -1);
        }

        Draw_Lock();
        menuDraw(currentMenu, selectedItem);
        Draw_Unlock();
    }
    while(!menuShouldExit);
}
