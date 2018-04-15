/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH, panicbit
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

#include "shell_open.h"
#include "menus/screen_filters.h"
#include "draw.h"

#define INT_SHELL_OPEN 0x60
#define STACK_SIZE 0x3000

static MyThread shellOpenThread;
static u8 ALIGN(8) shellOpenStack[STACK_SIZE];
static Handle shellOpenEvent;

extern Handle terminationRequestEvent;

MyThread *shellOpenCreateThread(void)
{
    if (R_FAILED(MyThread_Create(&shellOpenThread, shellOpenThreadMain, shellOpenStack, STACK_SIZE, 0x3F, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &shellOpenThread;
}

void shellOpenThreadMain(void) {
    if (R_FAILED(svcCreateEvent(&shellOpenEvent, RESET_ONESHOT)))
        svcBreak(USERBREAK_ASSERT);

    if (R_FAILED(svcBindInterrupt(INT_SHELL_OPEN, shellOpenEvent, 0, false)))
        svcBreak(USERBREAK_ASSERT);

    Handle handles[2] = {terminationRequestEvent, shellOpenEvent};

    while (true) {
        s32 idx = -1;

        svcWaitSynchronizationN(&idx, handles, 2, false, U64_MAX);

        if (idx < 0) {
            continue;
        }

        if (handles[idx] == terminationRequestEvent) {
            break;
        }

        // Need to wait for the GPU to get initialized
        svcSleepThread(16 * 1000 * 1000LL);

        screenFiltersSetTemperature(screenFiltersCurrentTemperature);
    }

    svcUnbindInterrupt(INT_SHELL_OPEN, shellOpenEvent);
    svcCloseHandle(shellOpenEvent);
}
