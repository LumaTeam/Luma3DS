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

#include <3ds.h>
#include "memory.h"
#include "menu.h"
#include "timelock.h"
#include "service_manager.h"
#include "errdisp.h"
#include "utils.h"
#include "MyThread.h"
#include "menus/miscellaneous.h"
#include "menus/debugger.h"
#include "menus/screen_filters.h"
#include "menus/cheats.h"
#include "menus/sysconfig.h"
#include "menus/timelock_config.h"
#include "input_redirection.h"
#include "minisoc.h"
#include "draw.h"
#include "bootdiag.h"
#include "shell.h"

#include "task_runner.h"

bool isN3DS;

Result __sync_init(void);
Result __sync_fini(void);
void __libc_init_array(void);
void __libc_fini_array(void);

void __ctru_exit(int rc) { (void)rc; } // needed to avoid linking error

// this is called after main exits
void __wrap_exit(int rc)
{
    (void)rc;
    // TODO: make pm terminate rosalina
    __libc_fini_array();

    // Kernel will take care of it all
    /*
    pmDbgExit();
    fsExit();
    svcCloseHandle(*fsRegGetSessionHandle());
    srvExit();
    __sync_fini();*/

    svcExitProcess();
}

// this is called before main
void initSystem(void)
{
    s64 out;
    Result res;
    __sync_init();
    mappableInit(0x10000000, 0x14000000);

    isN3DS = svcGetSystemInfo(&out, 0x10001, 0) == 0;

    svcGetSystemInfo(&out, 0x10000, 0x101);
    menuCombo = out == 0 ? DEFAULT_MENU_COMBO : (u32)out;

    svcGetSystemInfo(&out, 0x10000, 0x103);
    lastNtpTzOffset = (s16)out;

    for(res = 0xD88007FA; res == (Result)0xD88007FA; svcSleepThread(500 * 1000LL))
    {
        res = srvInit();
        if(R_FAILED(res) && res != (Result)0xD88007FA)
            svcBreak(USERBREAK_PANIC);
    }

    if (R_FAILED(pmAppInit()) || R_FAILED(pmDbgInit()))
        svcBreak(USERBREAK_PANIC);

    if (R_FAILED(fsInit()))
        svcBreak(USERBREAK_PANIC);

    if (R_FAILED(FSUSER_SetPriority(-16)))
        svcBreak(USERBREAK_PANIC);

    miscellaneousMenu.items[0].title = Luma_SharedConfig->selected_hbldr_3dsx_tid == HBLDR_DEFAULT_3DSX_TID ?
        "Switch the hb. title to the current app." :
        "Switch the hb. title to " HBLDR_DEFAULT_3DSX_TITLE_NAME;

    // **** DO NOT init services that don't come from KIPs here ****
    // Instead, init the service only where it's actually init (then deinit it).

    __libc_init_array();

    // ROSALINA HACKJOB END

    // Rosalina specific:
    u32 *tls = (u32 *)getThreadLocalStorage();
    memset(tls, 0, 0x80);
    tls[0] = 0x21545624;

    // ROSALINA HACKJOB BEGIN
    // NORMAL APPS SHOULD NOT DO THIS, EVER
    srvSetBlockingPolicy(true); // GetServiceHandle nonblocking if service port is full
}

bool menuShouldExit = false;
bool preTerminationRequested = false;
Handle preTerminationEvent;

static void handleTermNotification(u32 notificationId)
{
    (void)notificationId;
}

static void handleSleepNotification(u32 notificationId)
{
    ptmSysmInit();
    s32 ackValue = ptmSysmGetNotificationAckValue(notificationId);
    switch (notificationId)
    {
        case PTMNOTIFID_SLEEP_REQUESTED:
            menuShouldExit = true;
            PTMSYSM_ReplyToSleepQuery(miniSocEnabled); // deny sleep request if we have network stuff running
            break;
        case PTMNOTIFID_GOING_TO_SLEEP:
        case PTMNOTIFID_SLEEP_ALLOWED:
        case PTMNOTIFID_FULLY_WAKING_UP:
        case PTMNOTIFID_HALF_AWAKE:
            PTMSYSM_NotifySleepPreparationComplete(ackValue);
            break;
        case PTMNOTIFID_SLEEP_DENIED:
        case PTMNOTIFID_FULLY_AWAKE:
            menuShouldExit = false;
            break;
        default:
            break;
    }
    ptmSysmExit();
}

static void handleShellNotification(u32 notificationId)
{
    if (notificationId == 0x213) {
        // Shell opened
        // Note that this notification is also fired on system init.
        // Sequence goes like this: MCU fires notif. 0x200 on shell open
        // and shell close, then NS demuxes it and fires 0x213 and 0x214.
        handleShellOpened();
        menuShouldExit = false;
    } else {
        // Shell closed
        menuShouldExit = true;
    }

}

static void handlePreTermNotification(u32 notificationId)
{
    (void)notificationId;
    // Might be subject to a race condition, but heh.

    miniSocUnlockState(true);

    // Disable input redirection
    InputRedirection_Disable(100 * 1000 * 1000LL);

    // Ask the debugger to terminate in approx 2 * 100ms
    debuggerDisable(100 * 1000 * 1000LL);

    // Kill the ac session if needed
    if(isConnectionForced)
    {
        acExit();
        isConnectionForced = false;
        SysConfigMenu_UpdateStatus(true);
    }

    Draw_Lock();
    if (isHidInitialized)
        hidExit();

    // Termination request
    menuShouldExit = true;
    preTerminationRequested = true;
    svcSignalEvent(preTerminationEvent);
    Draw_Unlock();
}

static void handleNextApplicationDebuggedByForce(u32 notificationId)
{
    (void)notificationId;
    // Following call needs to be async because pm -> Loader depends on rosalina hb:ldr, handled in this very thread.
    TaskRunner_RunTask(debuggerFetchAndSetNextApplicationDebugHandleTask, NULL, 0);
}

#if 0
static void handleRestartHbAppNotification(u32 notificationId)
{
    (void)notificationId;
    TaskRunner_RunTask(HBLDR_RestartHbApplication, NULL, 0);
}
#endif

static const ServiceManagerServiceEntry services[] = {
    { NULL },
};

static const ServiceManagerNotificationEntry notifications[] = {
    { 0x100 ,                       handleTermNotification                  },
    { PTMNOTIFID_SLEEP_REQUESTED,   handleSleepNotification                 },
    { PTMNOTIFID_SLEEP_DENIED,      handleSleepNotification                 },
    { PTMNOTIFID_SLEEP_ALLOWED,     handleSleepNotification                 },
    { PTMNOTIFID_GOING_TO_SLEEP,    handleSleepNotification                 },
    { PTMNOTIFID_FULLY_WAKING_UP,   handleSleepNotification                 },
    { PTMNOTIFID_FULLY_AWAKE,       handleSleepNotification                 },
    { PTMNOTIFID_HALF_AWAKE,        handleSleepNotification                 },
    { 0x213,                        handleShellNotification                 },
    { 0x214,                        handleShellNotification                 },
    { 0x1000,                       handleNextApplicationDebuggedByForce    },
    { 0x2000,                       handlePreTermNotification               },
    { 0x000, NULL },
};

int main(void)
{
    if(R_FAILED(svcCreateEvent(&preTerminationEvent, RESET_STICKY)))
        svcBreak(USERBREAK_ASSERT);

    Draw_Init();
    Cheat_SeedRng(svcGetSystemTick());
    TimelockMenu_LoadData();

    MyThread *menuThread = menuCreateThread();
    MyThread *taskRunnerThread = taskRunnerCreateThread();
    MyThread *errDispThread = errDispCreateThread();
    MyThread *timelockThread = timelockCreateThread();
    bootdiagCreateThread();

    if (R_FAILED(ServiceManager_Run(services, notifications, NULL)))
        svcBreak(USERBREAK_PANIC);

    TaskRunner_Terminate();

    MyThread_Join(menuThread, -1LL);

    MyThread_Join(taskRunnerThread, -1LL);
    MyThread_Join(errDispThread, -1LL);
    MyThread_Join(timelockThread, -1LL);

    return 0;
}
