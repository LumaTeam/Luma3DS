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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <3ds.h>
#include "draw.h"
#include "ifile.h"
#include "timelock.h"
#include "sha256.h"


static MyThread timelockThread;
static u8 CTR_ALIGN(8) timelockThreadStack[0x1000];

timelockConfig timelockData;
const char PIN_ZERO[PIN_LENGTH] = {'0', '0', '0', '0'};

bool isTimeLocked = false;
int pinIndex = 0;
char enteredPIN[PIN_LENGTH] = {0};


MyThread *timelockCreateThread(void)
{
    if (R_FAILED(MyThread_Create(&timelockThread, timelockThreadMain, timelockThreadStack, 0x1000, 52, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);

    return &timelockThread;
}

// ---- hashing / salt ---------------------------------------------------------

void timelockHashPin(const char pin[PIN_LENGTH], const u8 salt[TIMELOCK_SALT_LEN], u8 outHash[TIMELOCK_HASH_LEN])
{
    u8 buf[TIMELOCK_SALT_LEN + PIN_LENGTH];
    memcpy(buf, salt, TIMELOCK_SALT_LEN);
    memcpy(buf + TIMELOCK_SALT_LEN, pin, PIN_LENGTH);
    sha256(buf, sizeof(buf), outHash);
}

bool timelockVerifyPin(const char pin[PIN_LENGTH], const u8 storedHash[TIMELOCK_HASH_LEN], const u8 storedSalt[TIMELOCK_SALT_LEN])
{
    u8 candidate[TIMELOCK_HASH_LEN];
    timelockHashPin(pin, storedSalt, candidate);

    // Constant-time compare to avoid leaking position via timing on the menu thread.
    u8 diff = 0;
    for (int i = 0; i < TIMELOCK_HASH_LEN; ++i)
        diff |= candidate[i] ^ storedHash[i];
    return diff == 0;
}

void timelockGenerateSalt(u8 outSalt[TIMELOCK_SALT_LEN])
{
    // Prefer the hardware PRNG via the PS service. Fall back to a
    // tick-driven LCG if PS is unavailable — quality is fine for a
    // per-install salt (not a key).
    if (R_SUCCEEDED(psInit())) {
        Result r = PS_GenerateRandomBytes(outSalt, TIMELOCK_SALT_LEN);
        psExit();
        if (R_SUCCEEDED(r))
            return;
    }

    u64 seed = svcGetSystemTick();
    for (int i = 0; i < TIMELOCK_SALT_LEN; ++i) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        outSalt[i] = (u8)(seed >> 33);
        seed ^= svcGetSystemTick();
    }
}

void timelockInitDefaults(timelockConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic              = TIMELOCK_MAGIC;
    cfg->version            = TIMELOCK_VERSION;
    cfg->minutes            = 60;
    cfg->elapsedMinutes     = 0;
    cfg->isEnabled          = false;
    cfg->dailyResetEnabled  = false;
    cfg->resetHourLocal     = 0;
    timelockGenerateSalt(cfg->pinSalt);
    timelockHashPin(PIN_ZERO, cfg->pinSalt, cfg->pinHash);
    cfg->nextResetEligibleAtMs = 0;  // first tick will arm it if dailyResetEnabled is later turned on
}

// ---- RTC --------------------------------------------------------------------

s64 timelockGetRtcMs(void)
{
    s64 ms = 0;
    if (R_FAILED(ptmSysmInit()))
        return 0;
    Result r = PTMSYSM_GetRtcTime(&ms);
    ptmSysmExit();
    return R_SUCCEEDED(r) ? ms : 0;
}

s64 timelockComputeNextResetMs(s64 nowMs, u8 hourLocal)
{
    if (nowMs <= 0)
        return 0;
    s64 dayStart = (nowMs / MS_PER_DAY) * MS_PER_DAY;
    s64 todaysReset = dayStart + ((s64)hourLocal) * MS_PER_HOUR;
    return (nowMs < todaysReset) ? todaysReset : (todaysReset + MS_PER_DAY);
}

// ---- file I/O ---------------------------------------------------------------

static FS_ArchiveID timelockArchive(void)
{
    s64 out = 0;
    if (R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203)))
        svcBreak(USERBREAK_ASSERT);
    return ((bool)out) ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
}

void timelockLoadData(void)
{
    IFile file;
    Result res;
    u64 bytesRead = 0;
    timelockConfig tmp;
    bool ok = false;

    res = IFile_Open(&file, timelockArchive(), fsMakePath(PATH_EMPTY, ""),
                     fsMakePath(PATH_ASCII, CONFIG_FILE_PATH), FS_OPEN_READ);
    if (R_SUCCEEDED(res))
        res = IFile_Read(&file, &bytesRead, &tmp, sizeof(tmp));
    IFile_Close(&file);

    if (R_SUCCEEDED(res) && bytesRead == sizeof(tmp)
        && tmp.magic == TIMELOCK_MAGIC && tmp.version == TIMELOCK_VERSION) {
        timelockData = tmp;
        ok = true;
    }

    if (!ok) {
        // First boot, missing file, or schema mismatch — start fresh.
        // (Old plaintext-PIN files from valx76's build will not match
        // magic/version and get replaced. Default PIN reverts to 0000.)
        timelockInitDefaults(&timelockData);
        timelockSaveData();
    }
}

void timelockSaveData(void)
{
    IFile file;
    Result res;
    u64 written = 0;

    res = IFile_Open(&file, timelockArchive(), fsMakePath(PATH_EMPTY, ""),
                     fsMakePath(PATH_ASCII, CONFIG_FILE_PATH), FS_OPEN_CREATE | FS_OPEN_WRITE);
    if (R_SUCCEEDED(res))
        res = IFile_SetSize(&file, sizeof(timelockData));
    if (R_SUCCEEDED(res))
        res = IFile_Write(&file, &written, &timelockData, sizeof(timelockData), 0);
    IFile_Close(&file);
}

void timelockFinalSaveOnShutdown(void)
{
    // Called from handlePreTermNotification in main.c.
    // Safe to call even if the thread never ran: timelockData starts zeroed
    // and timelockSaveData just writes whatever's in RAM.
    if (timelockData.magic == TIMELOCK_MAGIC)
        timelockSaveData();
}

// ---- main loop --------------------------------------------------------------

void timelockThreadMain(void)
{
    while (!isServiceUsable("hid:USER") || !isServiceUsable("fs:USER") || !isServiceUsable("APT:U"))
        svcSleepThread(1000 * 1000 * 500LL);

    hidInit();
    fsInit();
    aptInit();

    bool homeMenuLoaded;
    do {
        APT_GetAppletInfo(APPID_HOMEMENU, NULL, NULL, NULL, &homeMenuLoaded, NULL);
        svcSleepThread(1000 * 1000 * 1000 * 5LL);
    } while (!homeMenuLoaded);

    timelockLoadData();

    u32 ticksSinceHeartbeat = 0;

    while (!preTerminationRequested && timelockData.isEnabled) {
        if (isRosalinaMenuOpened) {
            svcSleepThread(1000 * 1000 * 1000 * 60LL);
            continue;
        }

        if (menuShouldExit) {
            svcSleepThread(1000 * 1000 * 1000 * 10LL);
            continue;
        }

        // Per-day reset: if armed and current RTC has crossed the threshold,
        // reset elapsed and rearm. Monotonic via the persisted timestamp:
        // rolling the clock back doesn't help, and rolling it forward only
        // grants ONE reset before nextResetEligibleAtMs is bumped to a new
        // future time.
        if (timelockData.dailyResetEnabled) {
            s64 nowMs = timelockGetRtcMs();
            if (nowMs > 0) {
                if (timelockData.nextResetEligibleAtMs == 0)
                    timelockData.nextResetEligibleAtMs = timelockComputeNextResetMs(nowMs, timelockData.resetHourLocal);

                if (nowMs >= timelockData.nextResetEligibleAtMs) {
                    timelockData.elapsedMinutes = 0;
                    timelockData.nextResetEligibleAtMs = timelockComputeNextResetMs(nowMs, timelockData.resetHourLocal);
                    timelockSaveData();
                    ticksSinceHeartbeat = 0;
                }
            }
        }

        if (timelockData.elapsedMinutes <= timelockData.minutes) {
            svcSleepThread(1000 * 1000 * 1000 * 60LL * MINUTES_TO_CHECK);

            timelockData.elapsedMinutes += MINUTES_TO_CHECK;
            ticksSinceHeartbeat += MINUTES_TO_CHECK;

            // Coarse heartbeat keeps SD writes to <=48/day of actual play
            // (vs valx76's 1440/day). Bounds state-loss on hard power-off
            // to HEARTBEAT_MINUTES.
            if (ticksSinceHeartbeat >= HEARTBEAT_MINUTES) {
                timelockSaveData();
                ticksSinceHeartbeat = 0;
            }
            continue;
        }

        resetEnteredPIN();

        // Lock screen — persist BEFORE showing it so a power-pull during
        // lock doesn't reset elapsed.
        timelockSaveData();
        timelockEnter();
        timelockShow();
        timelockLeave();

        if (menuShouldExit) {
            // Force-exited to enter sleep; do not treat as unlocked.
            continue;
        }

        timelockData.elapsedMinutes = 0;
        timelockSaveData();
        ticksSinceHeartbeat = 0;
    }

    fsExit();
    hidExit();
}

// ---- PIN entry (lock screen) ------------------------------------------------

bool checkPIN(void)
{
    return timelockVerifyPin(enteredPIN, timelockData.pinHash, timelockData.pinSalt);
}

void resetEnteredPIN(void)
{
    for (int i = 0; i < PIN_LENGTH; i++)
        enteredPIN[i] = '0';
    pinIndex = 0;
}

void timelockInput(void)
{
    hidScanInput();
    const u32 kDown = hidKeysDown();

    if (kDown & KEY_DLEFT)
        pinIndex = (pinIndex == 0 ? (PIN_LENGTH - 1) : (pinIndex - 1));
    if (kDown & KEY_DRIGHT)
        pinIndex = (pinIndex == (PIN_LENGTH - 1) ? 0 : (pinIndex + 1));
    if (kDown & KEY_DDOWN)
        enteredPIN[pinIndex] = (enteredPIN[pinIndex] == '0' ? '9' : (enteredPIN[pinIndex] - 1));
    if (kDown & KEY_DUP)
        enteredPIN[pinIndex] = (enteredPIN[pinIndex] == '9' ? '0' : (enteredPIN[pinIndex] + 1));

    if (kDown & KEY_START)
        isTimeLocked = !checkPIN();
}

void timelockEnter(void)
{
    Draw_Lock();
    isTimeLocked = true;
    svcKernelSetState(0x10000, 2 | 1);
    svcSleepThread(5 * 1000 * 100LL);

    if (R_FAILED(Draw_AllocateFramebufferCache(FB_BOTTOM_SIZE))) {
        svcKernelSetState(0x10000, 2 | 1);
        svcSleepThread(5 * 1000 * 100LL);
    } else
        Draw_SetupFramebuffer();

    Draw_Unlock();
}

void timelockLeave(void)
{
    Draw_Lock();
    Draw_RestoreFramebuffer();
    Draw_FreeFramebufferCache();
    svcKernelSetState(0x10000, 2 | 1);
    Draw_Unlock();
}

static void timelockDraw(void)
{
    Draw_DrawString(10, 10, COLOR_TITLE, "Timelock");
    Draw_DrawString(10, 30, COLOR_WHITE, "Enter the PIN and press START.");

    for (int i = 0; i < PIN_LENGTH; i++) {
        Draw_DrawCharacter((i+1)*10, 70, COLOR_WHITE, enteredPIN[i]);
        Draw_DrawCharacter((i+1)*10, 90, COLOR_WHITE, (i == pinIndex ? '^' : ' '));
    }
    Draw_FlushFramebuffer();
}

void timelockShow(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    timelockDraw();
    Draw_Unlock();

    do {
        timelockInput();
        Draw_Lock();
        timelockDraw();
        Draw_Unlock();
    } while (isTimeLocked && !menuShouldExit);
}
