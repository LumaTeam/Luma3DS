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
#include "plgloader.h"
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
static u32 homeBtnPressed = 0;

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
    svcKernelSetState(0x10007, &homeBtnPressed);
    if(R_FAILED(MyThread_Create(&menuThread, menuThreadMain, menuThreadStack, 0x3000, 52, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &menuThread;
}

u32 menuCombo;

u32     DispWarningOnHome(void);

void    menuThreadMain(void)
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

        if ((scanHeldKeys() & menuCombo) == menuCombo)
        {
            menuEnter();
            if(isN3DS) N3DSMenu_UpdateStatus();
            PluginLoader__UpdateMenu();
            menuShow(&rosalinaMenu);
            menuLeave();
        }
        

        // Check for home button on O3DS Mode3 with plugin loaded
        if (homeBtnPressed != 0)
        {
            if (DispWarningOnHome())
                svcKernelSetState(7); ///< reboot is fine since exiting a mode3 game reboot anyway

            homeBtnPressed = 0;
        }

        svcSleepThread(50 * 1000 * 1000LL);
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

static const char *__press_b_to_close = "Press [B] to close";

void    DispMessage(const char *title, const char *message)
{
    menuEnter();

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();

    Draw_DrawString(10, 10, COLOR_TITLE, title);

    Draw_DrawString(30, 30, COLOR_WHITE, message);
    Draw_DrawString(200, 220, COLOR_TITLE, __press_b_to_close);


    u32 keys = 0;

    do
    {
        keys = waitComboWithTimeout(1000);
    }while (!preTerminationRequested && !(keys & KEY_B));

    Draw_Unlock(); ///< Keep it locked until we exit the message
    menuLeave();
}

u32    DispErrMessage(const char *title, const char *message, const Result error)
{
    char buf[100];

    sprintf(buf, "Error code: 0x%08X", (unsigned int)error);
    menuEnter();

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();

    Draw_DrawString(10, 10, COLOR_TITLE, title);

    u32 posY = Draw_DrawString(30, 30, COLOR_WHITE, message);
    Draw_DrawString(30, posY + 20, COLOR_RED, buf);
    Draw_DrawString(200, 220, COLOR_TITLE, __press_b_to_close);

    u32 keys = 0;

    do
    {
        keys = waitComboWithTimeout(1000);
    }while (!preTerminationRequested && !(keys & KEY_B));

    Draw_Unlock();  ///< Keep it locked until we exit the message
    menuLeave();
    return error;
}

u32     DispWarningOnHome(void)
{
    menuEnter();

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();

    Draw_DrawString(10, 10, COLOR_TITLE, "Warning");

    u32 posY = Draw_DrawString(30, 40, COLOR_WHITE, "Due to memory shortage the home button\nis disabled.");
    Draw_DrawString(30, posY + 20, COLOR_WHITE, "Press [DPAD UP + B] to exit the application.");
    Draw_DrawString(200, 220, COLOR_TITLE, __press_b_to_close);


    u32 keys = 0;

    do
    {
        keys = waitComboWithTimeout(1000);
    }while (!preTerminationRequested && !(keys & KEY_B));

    Draw_Unlock(); ///< Keep it locked until we exit the message
    menuLeave();

    return (keys & KEY_UP) > 0;
}


typedef char string[50];
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void    DisplayPluginMenu(u32   *cmdbuf)
{
    u32             cursor = 0;
    u32             nbItems = cmdbuf[1];
    u8              *states = (u8 *)cmdbuf[3];
    char            buffer[60];
    const char      *title = (const char *)cmdbuf[5];
    const string    *items = (const string *)cmdbuf[7];
    const string    *hints = (const string *)cmdbuf[9];

    menuEnter();
    Draw_Lock();

    do
    {
        // Draw the menu
        {
            // Clear screen
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();

            // Draw title
            Draw_DrawString(10, 10, COLOR_TITLE, title);

            // Draw items
            u32 i = MAX(0, (int)cursor - 7);
            u32 end = MIN(nbItems, i + 16);
            u32 posY = 30;

            for (; i < end; ++i, posY += 10)
            {
                sprintf(buffer, "[ ] %s", items[i]);
                Draw_DrawString(30, posY, COLOR_WHITE, buffer);

                if (i == cursor) Draw_DrawCharacter(10, posY, COLOR_TITLE, '>');
                if (states[i]) Draw_DrawCharacter(36, posY, COLOR_LIME, 'x');
            }

            // Draw hint
            if (hints[cursor])
                Draw_DrawString(10, 200, COLOR_TITLE, hints[cursor]);
        }

        // Wait for input
        u32 pressed = waitInput();

        if (pressed & KEY_A)
            states[cursor] = !states[cursor];

        if (pressed & KEY_B)
            break;

        if (pressed & KEY_DOWN)
            if (++cursor >= nbItems)
                cursor = 0;

        if (pressed & KEY_UP)
            if (--cursor >= nbItems)
                cursor = nbItems - 1;

    } while (true);

    Draw_Unlock();
    menuLeave();
}
